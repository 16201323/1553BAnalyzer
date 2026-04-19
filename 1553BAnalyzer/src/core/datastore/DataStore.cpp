/**
 * @file DataStore.cpp
 * @brief 数据存储管理类实现
 * 
 * 本文件实现了DataStore类的所有方法，包括：
 * - 数据加载和索引构建
 * - 筛选条件管理
 * - 统计信息计算
 * - 数据排序
 * - 数据库后端支持
 */

#include "DataStore.h"
#include "DatabaseManager.h"
#include "core/parser/PacketStruct.h"
#include "utils/Logger.h"
#include "utils/Qt5Compat.h"
#include <algorithm>
#include <QDebug>
#include <cstring>
#include <climits>
#include <QtConcurrent>

/**
 * @brief 构造函数
 * @param parent 父对象指针
 * 
 * 初始化数据存储对象，设置默认参数：
 * - 无时间范围筛选
 * - 时间范围初始值为0
 * - 默认升序排序
 * - 默认每页100条
 * - 默认第0页
 * - 默认内存模式
 */
DataStore::DataStore(QObject *parent)
    : QObject(parent)
    , m_hasTimeRange(false)
    , m_startTime(0)
    , m_endTime(0)
    , m_hasPacketLenFilter(false)
    , m_packetLenMin(0)
    , m_packetLenMax(0)
    , m_hasDateRangeFilter(false)
    , m_dateYearStart(0)
    , m_dateMonthStart(0)
    , m_dateDayStart(0)
    , m_dateYearEnd(0)
    , m_dateMonthEnd(0)
    , m_dateDayEnd(0)
    , m_hasStatusBitFilter(false)
    , m_statusBitField(1)
    , m_statusBitPosition(0)
    , m_statusBitValue(true)
    , m_hasWordCountFilter(false)
    , m_wordCountMin(0)
    , m_wordCountMax(0)
    , m_errorFlagFilter(-1)
    , m_sortOrder(Qt::AscendingOrder)
    , m_pageSize(100)
    , m_currentPage(0)
    , m_cancelFilter(false)
    , m_storageMode(StorageMode::Memory)
    , m_autoSwitchThreshold(50000)
    , m_currentFileId(-1)
    , m_useDatabase(false)
    , m_filteredCountForDb(0)
    , m_batchFilterUpdate(false)
    , m_dataScope(DataScope::AllData)
    , m_cachedMsgIndex(-1)
    , m_chsttFilter(-1)
    , m_mpuIdFilter(0)
{
}

/**
 * @brief 析构函数
 * 
 * 清理数据存储资源
 */
DataStore::~DataStore()
{
}

// ========== 存储模式管理 ==========

/**
 * @brief 设置存储模式
 * @param mode 存储模式
 */
void DataStore::setStorageMode(StorageMode mode)
{
    if (m_storageMode == mode) {
        LOG_INFO("DataStore", QString::fromUtf8(u8"[数据存储] 已处于该模式: %1").arg(static_cast<int>(mode)));
        return;
    }
    
    LOG_INFO("DataStore", QString::fromUtf8(u8"[数据存储] 切换模式 - 从: %1 到: %2")
             .arg(static_cast<int>(m_storageMode)).arg(static_cast<int>(mode)));
    
    m_storageMode = mode;
    m_useDatabase = (mode == StorageMode::Database);
    
    if (m_useDatabase) {
        bool ok = DatabaseManager::instance()->initialize();
        LOG_INFO("DataStore", QString::fromUtf8(u8"[数据存储] 数据库初始化结果: %1").arg(ok));
    }
}

/**
 * @brief 从文件导入数据（数据库模式）
 */
int DataStore::importFromData(
    const QVector<PacketParser::SMbiMonPacketMsg>& data,
    const QString& filePath,
    std::function<void(int current, int total, const QString& status)> progressCallback)
{
    LOG_INFO("DataStore", QString::fromUtf8(u8"[数据存储] 开始导入数据, 使用数据库: %1, 数据量: %2, 文件路径: %3")
             .arg(m_useDatabase).arg(data.size()).arg(filePath));
    
    if (!m_useDatabase) {
        LOG_INFO("DataStore", QString::fromUtf8(u8"[数据存储] 非数据库模式, 使用内存模式"));
        setData(data);
        return 0;
    }
    
    LOG_INFO("DataStore", QString::fromUtf8(u8"[数据存储] 调用数据库管理器并行导入..."));
    int fileId = DatabaseManager::instance()->importDataParallel(data, filePath, progressCallback);
    LOG_INFO("DataStore", QString::fromUtf8(u8"[数据存储] 并行导入返回文件ID: %1").arg(fileId));
    
    if (fileId > 0) {
        m_currentFileId = fileId;
        m_rawData.clear();
        m_allRecords.clear();
        m_filteredRecords.clear();
        m_filteredCountForDb = DatabaseManager::instance()->queryPacketCount(fileId);
        m_currentPage = 0;
        LOG_INFO("DataStore", QString::fromUtf8(u8"[数据存储] 导入后 - 筛选计数: %1, 总页数: %2")
                 .arg(m_filteredCountForDb).arg(totalPages()));
        updateCurrentPageCache();
        LOG_INFO("DataStore", QString::fromUtf8(u8"[数据存储] 当前页记录数: %1").arg(m_currentPageRecords.size()));
        emit dataChanged();
        emit pageChanged(m_currentPage, totalPages(), static_cast<int>(m_filteredCountForDb));
    } else if (DatabaseManager::instance()->isImportCanceled()) {
        LOG_INFO("DataStore", QString::fromUtf8(u8"[数据存储] 导入已取消"));
    } else {
        LOG_ERROR("DataStore", QString::fromUtf8(u8"[数据存储] 并行导入失败, 文件ID: %1, 数据库错误: %2")
                 .arg(fileId).arg(DatabaseManager::instance()->lastError()));
    }
    
    return fileId;
}

/**
 * @brief 切换到指定文件
 */
bool DataStore::switchToFile(int fileId)
{
    if (!m_useDatabase) {
        return false;
    }
    
    QVariantMap fileInfo = DatabaseManager::instance()->getFileInfo(fileId);
    if (fileInfo.isEmpty()) {
        return false;
    }
    
    m_currentFileId = fileId;
    m_rawData.clear();
    m_allRecords.clear();
    m_filteredRecords.clear();
    m_cachedMsgIndex = -1;
    m_filteredCountForDb = DatabaseManager::instance()->queryPacketCount(fileId);
    m_currentPage = 0;
    
    applyFilters();
    emit dataChanged();
    
    return true;
}

/**
 * @brief 设置数据
 * 
 * 将解析后的原始数据加载到存储中，执行以下步骤：
 * 1. 清空现有数据
 * 2. 检查数据量，自动选择存储模式
 * 3. 存储原始数据
 * 4. 重建索引（内存模式）或导入数据库（数据库模式）
 * 5. 应用筛选条件
 * 6. 发出数据变化信号
 * 
 * @param data 原始消息数据数组
 */
void DataStore::setData(const QVector<PacketParser::SMbiMonPacketMsg>& data)
{
    clear();
    
    qint64 estimatedRecords = 0;
    for (const auto& msg : data) {
        estimatedRecords += msg.packetDatas.size();
    }
    
    if (estimatedRecords > m_autoSwitchThreshold && m_storageMode == StorageMode::Memory) {
        qInfo() << "Data count" << estimatedRecords << "exceeds threshold" << m_autoSwitchThreshold 
                << ", auto-switching to database mode";
        setStorageMode(StorageMode::Database);
    }
    
    if (m_useDatabase) {
        int fileId = DatabaseManager::instance()->importDataParallel(data, "", nullptr);
        if (fileId > 0) {
            m_currentFileId = fileId;
            // 数据库模式下不存储原始数据，避免内存浪费
        }
    } else {
        m_rawData = data;
        rebuildIndex();
    }
    
    applyFilters();
    emit dataChanged();
}

/**
 * @brief 清空数据
 * 
 * 清空所有存储的数据和索引，重置所有筛选条件
 */
void DataStore::clear()
{
    m_rawData.clear();
    m_allRecords.clear();
    m_filteredRecords.clear();
    m_currentPageRecords.clear();
    m_filters.clear();
    m_terminalFilter.clear();
    m_subAddressFilter.clear();
    m_messageTypeFilter.clear();
    m_hasTimeRange = false;
    m_currentPage = 0;
    m_filteredCountForDb = 0;
    m_currentFileId = -1;
    emit dataCleared();
}

/**
 * @brief 获取总记录数
 * @return 所有记录的总数（未筛选）
 */
int DataStore::totalCount() const
{
    if (m_useDatabase && m_currentFileId > 0) {
        return static_cast<int>(DatabaseManager::instance()->queryPacketCount(m_currentFileId));
    }
    return m_allRecords.size();
}

/**
 * @brief 获取筛选后的记录总数
 * @return 筛选后的记录总数
 */
int DataStore::filteredCount() const
{
    // 数据库模式+算式筛选时，数据在m_filteredRecords中
    if (m_useDatabase && m_currentFileId > 0 && !m_columnExpressionFilters.isEmpty()) {
        return m_filteredRecords.size();
    }
    if (m_useDatabase && m_currentFileId > 0) {
        return static_cast<int>(m_filteredCountForDb);
    }
    return m_filteredRecords.size();
}

/**
 * @brief 获取消息数
 * @return 原始消息的数量
 */
int DataStore::messageCount() const
{
    if (m_useDatabase && m_currentFileId > 0) {
        return static_cast<int>(DatabaseManager::instance()->getTotalMessageCount(m_currentFileId));
    }
    return m_rawData.size();
}

/**
 * @brief 获取指定索引的记录
 * @param index 记录索引
 * @return 数据记录，索引无效时返回空记录
 */
DataRecord DataStore::getRecord(int index) const
{
    if (m_useDatabase && m_currentFileId > 0) {
        // 数据库模式：需要从数据库查询
        // 首先检查是否在当前页缓存中
        int pageStart = m_currentPage * m_pageSize;
        int pageEnd = pageStart + m_currentPageRecords.size();
        
        if (index >= pageStart && index < pageEnd) {
            // 在当前页缓存中
            return m_currentPageRecords[index - pageStart];
        }
        
        // 不在当前页，需要从数据库查询
        DbDataRecord dbRecord = DatabaseManager::instance()->queryPacketByRowIndex(m_currentFileId, index);
        if (dbRecord.id > 0) {
            DataRecord record;
            record.rowIndex = dbRecord.rowIndex;
            record.msgIndex = dbRecord.msgIndex;
            record.dataIndex = dbRecord.dataIndex;
            record.messageType = dbRecord.messageType;
            record.timestampMs = dbRecord.timestampMs;
            
            // 从原始数据中获取完整的packetHeader和packetData
            if (dbRecord.msgIndex >= 0 && dbRecord.msgIndex < m_rawData.size()) {
                const SMbiMonPacketMsg& msg = m_rawData[dbRecord.msgIndex];
                record.packetHeader = msg.header;
                
                if (dbRecord.dataIndex >= 0 && dbRecord.dataIndex < msg.packetDatas.size()) {
                    record.packetData = msg.packetDatas[dbRecord.dataIndex];
                    // 从packetData.cmd1位运算解析并缓存，避免后续重复memcpy
                    record.terminalAddr = cmdTerminalAddr(record.packetData.cmd1);
                    record.subAddr = cmdSubAddr(record.packetData.cmd1);
                    record.t_r = cmdTR(record.packetData.cmd1);
                    record.dataCount = cmdDataCount(record.packetData.cmd1);
                }
            } else {
                rebuildPacketDataFromDb(record, dbRecord);
            }
            
            return record;
        }
        
        return DataRecord();
    }
    
    // 内存模式
    if (index >= 0 && index < m_filteredRecords.size()) {
        return m_filteredRecords[index];
    }
    return DataRecord();
}

/**
 * @brief 获取指定索引的消息
 * @param msgIndex 消息索引
 * @return 原始消息的常引用，索引无效时返回空消息
 */
const PacketParser::SMbiMonPacketMsg& DataStore::getMessage(int msgIndex) const
{
    static PacketParser::SMbiMonPacketMsg emptyMsg;
    
    // 内存模式：直接从m_rawData获取
    if (msgIndex >= 0 && msgIndex < m_rawData.size()) {
        return m_rawData[msgIndex];
    }
    
    // 数据库模式：m_rawData为空时，从数据库查询并重建完整消息
    if (m_useDatabase && m_currentFileId > 0 && msgIndex >= 0) {
        // 命中缓存则直接返回，避免重复查询数据库
        if (m_cachedMsgIndex == msgIndex) {
            return m_cachedMessage;
        }
        
        QVector<DbDataRecord> dbRecords = DatabaseManager::instance()->queryPacketsByMsgIndex(m_currentFileId, msgIndex);
        if (dbRecords.isEmpty()) {
            LOG_WARNING("DataStore", QString::fromUtf8(u8"[getMessage] 数据库查询无结果, fileId: %1, msgIndex: %2").arg(m_currentFileId).arg(msgIndex));
            return emptyMsg;
        }
        
        // 重建SMbiMonPacketMsg结构
        m_cachedMessage = PacketParser::SMbiMonPacketMsg();
        m_cachedMsgIndex = msgIndex;
        
        // 从第一条记录恢复包头信息（同一消息的所有数据包共享包头）
        const DbDataRecord& firstRecord = dbRecords[0];
        m_cachedMessage.header.header1 = 0xA5A5;
        m_cachedMessage.header.header2 = 0xA5;
        m_cachedMessage.header.mpuProduceId = static_cast<quint8>(firstRecord.mpuProduceId);
        m_cachedMessage.header.packetLen = static_cast<quint16>(firstRecord.packetLen);
        m_cachedMessage.header.year = static_cast<quint16>(firstRecord.headerYear);
        m_cachedMessage.header.month = static_cast<quint8>(firstRecord.headerMonth);
        m_cachedMessage.header.day = static_cast<quint8>(firstRecord.headerDay);
        m_cachedMessage.header.timestamp = firstRecord.headerTimestamp;
        
        // 逐条重建数据包，从数据库字段还原SMbiMonPacketData各成员
        for (const DbDataRecord& dbRecord : dbRecords) {
            SMbiMonPacketData pktData;
            pktData.header = 0xAABB;
            pktData.cmd1 = generateCmd1(dbRecord.terminalAddr, dbRecord.subAddr, dbRecord.trBit, dbRecord.wordCount);
            pktData.cmd2 = generateCmd2(dbRecord.terminalAddr2, dbRecord.subAddr2, dbRecord.trBit2, dbRecord.wordCount2);
            pktData.states1 = static_cast<quint16>(dbRecord.status1);
            pktData.states2 = static_cast<quint16>(dbRecord.status2);
            pktData.chstt = static_cast<quint16>(dbRecord.chstt);
            pktData.timestamp = dbRecord.packetTimestamp;
            pktData.datas = dbRecord.data;
            m_cachedMessage.packetDatas.append(pktData);
        }
        
        LOG_INFO("DataStore", QString::fromUtf8(u8"[getMessage] 从数据库重建消息, fileId: %1, msgIndex: %2, 数据包数: %3")
            .arg(m_currentFileId).arg(msgIndex).arg(m_cachedMessage.packetDatas.size()));
        
        return m_cachedMessage;
    }
    
    LOG_WARNING("DataStore", QString::fromUtf8(u8"[getMessage] 无法获取消息, msgIndex: %1, useDatabase: %2, fileId: %3, rawDataSize: %4")
        .arg(msgIndex).arg(m_useDatabase).arg(m_currentFileId).arg(m_rawData.size()));
    
    return emptyMsg;
}

/**
 * @brief 获取所有记录
 * @return 所有数据记录的数组（未筛选）
 */
QVector<DataRecord> DataStore::getAllRecords() const
{
    return m_allRecords;
}

QVector<DataRecord> DataStore::getAllRecordsForReport() const
{
    if (m_useDatabase && m_currentFileId > 0) {
        QVector<DataRecord> records;
        
        // 数据库模式：查询全量数据（不传任何筛选条件）
        QVector<DbDataRecord> dbRecords = DatabaseManager::instance()->queryPackets(
            m_currentFileId,
            QSet<int>(),       // 不筛选终端
            QSet<int>(),       // 不筛选子地址
            QSet<MessageType>(), // 不筛选消息类型
            0,                 // 起始时间
            LLONG_MAX,         // 结束时间
            0,                 // 不限制数量
            0,                 // 偏移
            -1,                // 不筛选chstt
            0,                 // 不筛选mpuId
            QSet<int>(),       // 不排除终端
            QSet<MessageType>(), // 不排除消息类型
            -1, -1,            // 不筛选包长度
            -1, -1, -1, -1, -1, -1, // 不筛选日期
            -1, -1, -1,        // 不筛选状态位
            -1, -1,            // 不筛选字计数
            -1                 // 不筛选错误标志
        );
        
        LOG_INFO("DataStore", QString::fromUtf8(u8"[报告数据] 数据库模式查询全量数据, 记录数: %1, m_rawData.size: %2")
                 .arg(dbRecords.size()).arg(m_rawData.size()));
        
        for (const DbDataRecord& dbRecord : dbRecords) {
            DataRecord record;
            record.rowIndex = dbRecord.rowIndex;
            record.msgIndex = dbRecord.msgIndex;
            record.dataIndex = dbRecord.dataIndex;
            record.messageType = dbRecord.messageType;
            record.timestampMs = dbRecord.timestampMs;
            
            // 优先从m_rawData获取完整packetData
            if (!m_rawData.isEmpty() && dbRecord.msgIndex >= 0 && dbRecord.msgIndex < m_rawData.size()) {
                const SMbiMonPacketMsg& msg = m_rawData[dbRecord.msgIndex];
                record.packetHeader = msg.header;
                
                if (dbRecord.dataIndex >= 0 && dbRecord.dataIndex < msg.packetDatas.size()) {
                    record.packetData = msg.packetDatas[dbRecord.dataIndex];
                    record.terminalAddr = cmdTerminalAddr(record.packetData.cmd1);
                    record.subAddr = cmdSubAddr(record.packetData.cmd1);
                    record.t_r = cmdTR(record.packetData.cmd1);
                    record.dataCount = cmdDataCount(record.packetData.cmd1);
                } else {
                    rebuildPacketDataFromDb(record, dbRecord);
                }
            } else {
                rebuildPacketDataFromDb(record, dbRecord);
            }
            
            records.append(record);
        }
        
        // 打印前10条记录的chstt值，用于诊断数据是否正确
        int sampleCount = qMin(10, records.size());
        QStringList chsttSamples;
        for (int i = 0; i < sampleCount; ++i) {
            chsttSamples << QString("%1").arg(records[i].packetData.chstt);
        }
        LOG_INFO("DataStore", QString::fromUtf8(u8"[报告数据] chstt采样(前%1条): [%2]")
                 .arg(sampleCount).arg(chsttSamples.join(", ")));
        
        return records;
    }
    
    // 内存模式：直接返回m_allRecords
    LOG_INFO("DataStore", QString::fromUtf8(u8"[报告数据] 内存模式返回全量数据, 记录数: %1").arg(m_allRecords.size()));
    return m_allRecords;
}

void DataStore::rebuildPacketDataFromDb(DataRecord& record, const DbDataRecord& dbRecord) const
{
    record.packetHeader = SMbiMonPacketHeader();
    record.packetHeader.mpuProduceId = static_cast<quint8>(dbRecord.mpuProduceId);
    record.packetHeader.packetLen = static_cast<quint16>(dbRecord.packetLen);
    record.packetHeader.year = static_cast<quint16>(dbRecord.headerYear);
    record.packetHeader.month = static_cast<quint8>(dbRecord.headerMonth);
    record.packetHeader.day = static_cast<quint8>(dbRecord.headerDay);
    record.packetHeader.timestamp = dbRecord.headerTimestamp;
    
    record.packetData.header = 0xAABB;
    record.packetData.cmd1 = generateCmd1(dbRecord.terminalAddr, dbRecord.subAddr, dbRecord.trBit, dbRecord.wordCount);
    record.packetData.cmd2 = generateCmd2(dbRecord.terminalAddr2, dbRecord.subAddr2, dbRecord.trBit2, dbRecord.wordCount2);
    record.packetData.states1 = static_cast<quint16>(dbRecord.status1);
    record.packetData.states2 = static_cast<quint16>(dbRecord.status2);
    record.packetData.chstt = static_cast<quint16>(dbRecord.chstt);
    record.packetData.timestamp = dbRecord.packetTimestamp;
    record.packetData.datas = dbRecord.data;
    
    // 从数据库字段直接填充缓存字段，避免后续重复解析cmd1
    record.terminalAddr = static_cast<quint8>(dbRecord.terminalAddr);
    record.subAddr = static_cast<quint8>(dbRecord.subAddr);
    record.t_r = static_cast<quint8>(dbRecord.trBit);
    record.dataCount = static_cast<quint8>(dbRecord.wordCount);
}

/**
 * @brief 获取筛选后的记录
 * @return 符合筛选条件的数据记录数组
 * 
 * 数据库模式下，如果存在列算式筛选，直接返回m_filteredRecords
 * （已在applyFilters中完成数据库查询+内存二次筛选）；
 * 否则从数据库查询并转换记录。
 */
QVector<DataRecord> DataStore::getFilteredRecords() const
{
    if (m_useDatabase && m_currentFileId > 0) {
        // 数据库模式+算式筛选时，数据已在m_filteredRecords中
        if (!m_columnExpressionFilters.isEmpty()) {
            return m_filteredRecords;
        }
        
        QVector<DataRecord> records;
        
        qint64 startTime = m_hasTimeRange ? m_startTime : 0;
        qint64 endTime = m_hasTimeRange ? m_endTime : LLONG_MAX;
        
        QVector<DbDataRecord> dbRecords = DatabaseManager::instance()->queryPackets(
            m_currentFileId,
            m_terminalFilter,
            m_subAddressFilter,
            m_messageTypeFilter,
            startTime,
            endTime,
            0,  // 不限制数量
            0,  // 偏移
            m_chsttFilter,
            m_mpuIdFilter,
            m_excludeTerminalFilter,
            m_excludeMessageTypeFilter,
            m_hasPacketLenFilter ? m_packetLenMin : -1,
            m_hasPacketLenFilter ? m_packetLenMax : -1,
            m_hasDateRangeFilter ? m_dateYearStart : -1,
            m_hasDateRangeFilter ? m_dateMonthStart : -1,
            m_hasDateRangeFilter ? m_dateDayStart : -1,
            m_hasDateRangeFilter ? m_dateYearEnd : -1,
            m_hasDateRangeFilter ? m_dateMonthEnd : -1,
            m_hasDateRangeFilter ? m_dateDayEnd : -1,
            m_hasStatusBitFilter ? m_statusBitField : -1,
            m_hasStatusBitFilter ? m_statusBitPosition : -1,
            m_hasStatusBitFilter ? (m_statusBitValue ? 1 : 0) : -1,
            m_hasWordCountFilter ? m_wordCountMin : -1,
            m_hasWordCountFilter ? m_wordCountMax : -1,
            m_errorFlagFilter
        );
        
        for (const DbDataRecord& dbRecord : dbRecords) {
            DataRecord record;
            record.rowIndex = dbRecord.rowIndex;
            record.msgIndex = dbRecord.msgIndex;
            record.dataIndex = dbRecord.dataIndex;
            record.messageType = dbRecord.messageType;
            record.timestampMs = dbRecord.timestampMs;
            
            // 优先从m_rawData获取完整packetData
            if (!m_rawData.isEmpty() && dbRecord.msgIndex >= 0 && dbRecord.msgIndex < m_rawData.size()) {
                const SMbiMonPacketMsg& msg = m_rawData[dbRecord.msgIndex];
                record.packetHeader = msg.header;
                
                if (dbRecord.dataIndex >= 0 && dbRecord.dataIndex < msg.packetDatas.size()) {
                    record.packetData = msg.packetDatas[dbRecord.dataIndex];
                    record.terminalAddr = cmdTerminalAddr(record.packetData.cmd1);
                    record.subAddr = cmdSubAddr(record.packetData.cmd1);
                    record.t_r = cmdTR(record.packetData.cmd1);
                    record.dataCount = cmdDataCount(record.packetData.cmd1);
                } else {
                    rebuildPacketDataFromDb(record, dbRecord);
                }
            } else {
                rebuildPacketDataFromDb(record, dbRecord);
            }
            
            records.append(record);
        }
        
        return records;
    }
    
    return m_filteredRecords;
}

/**
 * @brief 重建数据索引
 * 
 * 遍历所有原始数据，构建DataRecord数组。
 * 对每条数据执行以下操作：
 * 1. 分配行序号
 * 2. 记录消息索引和数据包索引
 * 3. 复制原始数据
 * 4. 检测消息类型
 * 5. 计算时间戳（毫秒）
 */
void DataStore::rebuildIndex()
{
    m_allRecords.clear();
    int rowIndex = 0;
    
    int estimatedRecords = 0;
    for (int msgIdx = 0; msgIdx < m_rawData.size(); ++msgIdx) {
        estimatedRecords += m_rawData[msgIdx].packetDatas.size();
    }
    m_allRecords.reserve(estimatedRecords);
    
    for (int msgIdx = 0; msgIdx < m_rawData.size(); ++msgIdx) {
        const SMbiMonPacketMsg& msg = m_rawData[msgIdx];
        
        // 遍历消息中的每个数据包
        for (int dataIdx = 0; dataIdx < msg.packetDatas.size(); ++dataIdx) {
            const SMbiMonPacketData& pkt = msg.packetDatas[dataIdx];
            
            // 创建数据记录
            DataRecord record;
            record.rowIndex = rowIndex++;
            record.msgIndex = msgIdx;
            record.dataIndex = dataIdx;
            record.packetData = pkt;
            record.packetHeader = msg.header;
            
            // 检测消息类型
            record.messageType = detectMessageType(pkt);
            
            // 使用位运算解析cmd1字段并缓存，避免后续重复memcpy解析
            record.terminalAddr = cmdTerminalAddr(pkt.cmd1);
            record.subAddr = cmdSubAddr(pkt.cmd1);
            record.t_r = cmdTR(pkt.cmd1);
            record.dataCount = cmdDataCount(pkt.cmd1);
            
            record.timestampMs = pkt.timestamp * 40.0 / 1000.0;
            
            m_allRecords.append(record);
        }
    }
}

DataStore::FilterSnapshot DataStore::captureFilterSnapshot() const
{
    FilterSnapshot snapshot;
    snapshot.terminalFilter = m_terminalFilter;
    snapshot.subAddressFilter = m_subAddressFilter;
    snapshot.messageTypeFilter = m_messageTypeFilter;
    snapshot.hasTimeRange = m_hasTimeRange;
    snapshot.startTime = m_startTime;
    snapshot.endTime = m_endTime;
    snapshot.chsttFilter = m_chsttFilter;
    snapshot.mpuIdFilter = m_mpuIdFilter;
    snapshot.hasPacketLenFilter = m_hasPacketLenFilter;
    snapshot.packetLenMin = m_packetLenMin;
    snapshot.packetLenMax = m_packetLenMax;
    snapshot.hasDateRangeFilter = m_hasDateRangeFilter;
    snapshot.dateYearStart = m_dateYearStart;
    snapshot.dateMonthStart = m_dateMonthStart;
    snapshot.dateDayStart = m_dateDayStart;
    snapshot.dateYearEnd = m_dateYearEnd;
    snapshot.dateMonthEnd = m_dateMonthEnd;
    snapshot.dateDayEnd = m_dateDayEnd;
    snapshot.hasStatusBitFilter = m_hasStatusBitFilter;
    snapshot.statusBitField = m_statusBitField;
    snapshot.statusBitPosition = m_statusBitPosition;
    snapshot.statusBitValue = m_statusBitValue;
    snapshot.excludeTerminalFilter = m_excludeTerminalFilter;
    snapshot.excludeMessageTypeFilter = m_excludeMessageTypeFilter;
    snapshot.hasWordCountFilter = m_hasWordCountFilter;
    snapshot.wordCountMin = m_wordCountMin;
    snapshot.wordCountMax = m_wordCountMax;
    snapshot.errorFlagFilter = m_errorFlagFilter;
    snapshot.columnExpressionFilters = m_columnExpressionFilters;
    return snapshot;
}

bool DataStore::matchesFilterSnapshot(const DataRecord& record, const FilterSnapshot& s) const
{
    if (s.hasTimeRange) {
        if (record.timestampMs < s.startTime || record.timestampMs > s.endTime)
            return false;
    }
    if (!s.terminalFilter.isEmpty() && !s.terminalFilter.contains(record.terminalAddr))
        return false;
    if (!s.subAddressFilter.isEmpty() && !s.subAddressFilter.contains(record.subAddr))
        return false;
    if (!s.messageTypeFilter.isEmpty() && !s.messageTypeFilter.contains(record.messageType))
        return false;
    if (s.chsttFilter >= 0 && record.packetData.chstt != s.chsttFilter)
        return false;
    if (s.mpuIdFilter > 0 && record.packetHeader.mpuProduceId != s.mpuIdFilter)
        return false;
    if (s.hasPacketLenFilter) {
        if (record.packetHeader.packetLen < s.packetLenMin || record.packetHeader.packetLen > s.packetLenMax)
            return false;
    }
    if (s.hasDateRangeFilter) {
        if (record.packetHeader.year < s.dateYearStart || record.packetHeader.year > s.dateYearEnd)
            return false;
        if (record.packetHeader.year == s.dateYearStart &&
            (record.packetHeader.month < s.dateMonthStart ||
             (record.packetHeader.month == s.dateMonthStart && record.packetHeader.day < s.dateDayStart)))
            return false;
        if (record.packetHeader.year == s.dateYearEnd &&
            (record.packetHeader.month > s.dateMonthEnd ||
             (record.packetHeader.month == s.dateMonthEnd && record.packetHeader.day > s.dateDayEnd)))
            return false;
    }
    if (s.hasStatusBitFilter) {
        quint16 statusWord = (s.statusBitField == 1) ? record.packetData.states1 : record.packetData.states2;
        bool bitSet = (statusWord >> s.statusBitPosition) & 1;
        if (bitSet != s.statusBitValue)
            return false;
    }
    if (!s.excludeTerminalFilter.isEmpty() && s.excludeTerminalFilter.contains(record.terminalAddr))
        return false;
    if (!s.excludeMessageTypeFilter.isEmpty() && s.excludeMessageTypeFilter.contains(record.messageType))
        return false;
    if (s.hasWordCountFilter) {
        if (record.dataCount < s.wordCountMin || record.dataCount > s.wordCountMax)
            return false;
    }
    if (s.errorFlagFilter >= 0) {
        bool hasError = (record.packetData.chstt == 0);
        if (hasError != (s.errorFlagFilter == 1))
            return false;
    }
    if (!s.columnExpressionFilters.isEmpty()) {
        for (auto it = s.columnExpressionFilters.begin(); it != s.columnExpressionFilters.end(); ++it) {
            int column = it.key();
            const FilterExpression& filter = it.value();
            QVariant columnValue;
            switch (column) {
            case 0: columnValue = record.rowIndex + 1; break;
            case 1: columnValue = record.packetHeader.mpuProduceId; break;
            case 2: columnValue = record.packetHeader.packetLen; break;
            case 3: continue;
            case 4: columnValue = static_cast<int>(record.packetHeader.timestamp); break;
            case 5: continue;
            case 6: columnValue = static_cast<int>(record.terminalAddr); break;
            case 7: columnValue = static_cast<int>(record.subAddr); break;
            case 8: continue;
            case 9: columnValue = static_cast<int>(record.dataCount); break;
            case 10: continue;
            case 11: continue;
            default: continue;
            }
            if (!filter.evaluate(columnValue))
                return false;
        }
    }
    return true;
}

/**
 * @brief 应用筛选条件
 * 
 * 根据当前所有筛选条件，更新筛选后的记录数组。
 * 如果设置了排序字段，还会对结果进行排序。
 * 
 * 数据库模式下，如果存在列算式筛选（SQL无法直接支持），
 * 则先从数据库查询所有符合其他条件的记录，
 * 再在内存中应用算式筛选进行二次过滤。
 */
void DataStore::applyFilters()
{
   if (m_batchFilterUpdate) return;
   
   m_cancelFilter = true;
   
   m_filteredRecords.clear();
   
   if (m_useDatabase && m_currentFileId > 0) {
       qint64 startTime = m_hasTimeRange ? m_startTime : 0;
       qint64 endTime = m_hasTimeRange ? m_endTime : LLONG_MAX;
       
       // 数据库模式下，列算式筛选无法通过SQL实现，
       // 需要先查询数据库记录再在内存中进行二次筛选
       if (!m_columnExpressionFilters.isEmpty()) {
           // 第一步：从数据库查询所有符合非算式筛选条件的记录
           QVector<DbDataRecord> dbRecords = DatabaseManager::instance()->queryPackets(
               m_currentFileId,
               m_terminalFilter,
               m_subAddressFilter,
               m_messageTypeFilter,
               startTime,
               endTime,
               0,  // 不限制数量，查询全部
               0,  // 偏移为0
               m_chsttFilter,
               m_mpuIdFilter,
               m_excludeTerminalFilter,
               m_excludeMessageTypeFilter,
               m_hasPacketLenFilter ? m_packetLenMin : -1,
               m_hasPacketLenFilter ? m_packetLenMax : -1,
               m_hasDateRangeFilter ? m_dateYearStart : -1,
               m_hasDateRangeFilter ? m_dateMonthStart : -1,
               m_hasDateRangeFilter ? m_dateDayStart : -1,
               m_hasDateRangeFilter ? m_dateYearEnd : -1,
               m_hasDateRangeFilter ? m_dateMonthEnd : -1,
               m_hasDateRangeFilter ? m_dateDayEnd : -1,
               m_hasStatusBitFilter ? m_statusBitField : -1,
               m_hasStatusBitFilter ? m_statusBitPosition : -1,
               m_hasStatusBitFilter ? (m_statusBitValue ? 1 : 0) : -1,
               m_hasWordCountFilter ? m_wordCountMin : -1,
               m_hasWordCountFilter ? m_wordCountMax : -1,
               m_errorFlagFilter
           );
           
           // 第二步：将数据库记录转换为DataRecord
           QVector<DataRecord> allDbRecords;
           allDbRecords.reserve(dbRecords.size());
           
           for (const DbDataRecord& dbRecord : dbRecords) {
               DataRecord record;
               record.rowIndex = dbRecord.rowIndex;
               record.msgIndex = dbRecord.msgIndex;
               record.dataIndex = dbRecord.dataIndex;
               record.messageType = dbRecord.messageType;
               record.timestampMs = dbRecord.timestampMs;
               
               // 从数据库字段重建packetData和缓存字段
               record.packetHeader = SMbiMonPacketHeader();
               record.packetHeader.mpuProduceId = static_cast<quint8>(dbRecord.mpuProduceId);
               record.packetHeader.packetLen = static_cast<quint16>(dbRecord.packetLen);
               record.packetHeader.year = static_cast<quint16>(dbRecord.headerYear);
               record.packetHeader.month = static_cast<quint8>(dbRecord.headerMonth);
               record.packetHeader.day = static_cast<quint8>(dbRecord.headerDay);
               record.packetHeader.timestamp = dbRecord.headerTimestamp;
               
               record.packetData.header = 0xAABB;
               record.packetData.cmd1 = generateCmd1(dbRecord.terminalAddr, dbRecord.subAddr, dbRecord.trBit, dbRecord.wordCount);
               record.packetData.cmd2 = generateCmd2(dbRecord.terminalAddr2, dbRecord.subAddr2, dbRecord.trBit2, dbRecord.wordCount2);
               record.packetData.states1 = static_cast<quint16>(dbRecord.status1);
               record.packetData.states2 = static_cast<quint16>(dbRecord.status2);
               record.packetData.chstt = static_cast<quint16>(dbRecord.chstt);
               record.packetData.timestamp = dbRecord.packetTimestamp;
               record.packetData.datas = dbRecord.data;
               
               record.terminalAddr = static_cast<quint8>(dbRecord.terminalAddr);
               record.subAddr = static_cast<quint8>(dbRecord.subAddr);
               record.t_r = static_cast<quint8>(dbRecord.trBit);
               record.dataCount = static_cast<quint8>(dbRecord.wordCount);
               
               allDbRecords.append(record);
           }
           
           // 第三步：在内存中应用算式筛选进行二次过滤
           for (const DataRecord& record : allDbRecords) {
               if (matchesFilter(record)) {
                   m_filteredRecords.append(record);
               }
           }
           
           // 第四步：排序
           if (!m_sortField.isEmpty()) {
               std::sort(m_filteredRecords.begin(), m_filteredRecords.end(),
                   [this](const DataRecord& a, const DataRecord& b) {
                       bool less = false;
                       if (m_sortField == "timestamp") {
                           less = a.timestampMs < b.timestampMs;
                       } else if (m_sortField == "terminal") {
                           less = a.terminalAddr < b.terminalAddr;
                       } else if (m_sortField == "type") {
                           less = static_cast<int>(a.messageType) < static_cast<int>(b.messageType);
                       }
                       return m_sortOrder == Qt::AscendingOrder ? less : !less;
                   });
           }
           
           // 第五步：使用内存分页模式（因为数据已在m_filteredRecords中）
           m_filteredCountForDb = m_filteredRecords.size();
           m_currentPage = 0;
           updateCurrentPageCacheForExpressionFilter();
           emit filterChanged();
           emit pageChanged(m_currentPage, totalPages(), m_filteredRecords.size());
       } else {
           // 无算式筛选时，使用纯数据库查询模式
           m_filteredCountForDb = DatabaseManager::instance()->queryPacketCount(
               m_currentFileId,
               m_terminalFilter,
               m_subAddressFilter,
               m_messageTypeFilter,
               startTime,
               endTime,
               m_chsttFilter,
               m_mpuIdFilter,
               m_excludeTerminalFilter,
               m_excludeMessageTypeFilter,
               m_hasPacketLenFilter ? m_packetLenMin : -1,
               m_hasPacketLenFilter ? m_packetLenMax : -1,
               m_hasDateRangeFilter ? m_dateYearStart : -1,
               m_hasDateRangeFilter ? m_dateMonthStart : -1,
               m_hasDateRangeFilter ? m_dateDayStart : -1,
               m_hasDateRangeFilter ? m_dateYearEnd : -1,
               m_hasDateRangeFilter ? m_dateMonthEnd : -1,
               m_hasDateRangeFilter ? m_dateDayEnd : -1,
               m_hasStatusBitFilter ? m_statusBitField : -1,
               m_hasStatusBitFilter ? m_statusBitPosition : -1,
               m_hasStatusBitFilter ? (m_statusBitValue ? 1 : 0) : -1,
               m_hasWordCountFilter ? m_wordCountMin : -1,
               m_hasWordCountFilter ? m_wordCountMax : -1,
               m_errorFlagFilter
           );
           
           m_currentPage = 0;
           updateCurrentPageCache();
           emit filterChanged();
           emit pageChanged(m_currentPage, totalPages(), static_cast<int>(m_filteredCountForDb));
       }
   } else {
       for (const DataRecord& record : m_allRecords) {
           if (matchesFilter(record)) {
               m_filteredRecords.append(record);
           }
       }
       
       if (!m_sortField.isEmpty()) {
           std::sort(m_filteredRecords.begin(), m_filteredRecords.end(),
               [this](const DataRecord& a, const DataRecord& b) {
                   bool less = false;
                   if (m_sortField == "timestamp") {
                       less = a.timestampMs < b.timestampMs;
                   } else if (m_sortField == "terminal") {
                       less = a.terminalAddr < b.terminalAddr;
                   } else if (m_sortField == "type") {
                       less = static_cast<int>(a.messageType) < static_cast<int>(b.messageType);
                   }
                   
                   return m_sortOrder == Qt::AscendingOrder ? less : !less;
               });
       }
       
       updateCurrentPageCache();
       emit filterChanged();
   }
}

/**
 * @brief 检查记录是否匹配筛选条件
 * @param record 要检查的数据记录
 * @return 匹配返回true，否则返回false
 * 
 * 检查以下筛选条件：
 * 1. 时间范围筛选
 * 2. 终端地址筛选
 * 3. 消息类型筛选
 */
bool DataStore::matchesFilter(const DataRecord& record) const
{
    if (m_hasTimeRange) {
        if (record.timestampMs < m_startTime || 
            record.timestampMs > m_endTime) {
            return false;
        }
    }
    
    if (!m_terminalFilter.isEmpty()) {
        if (!m_terminalFilter.contains(record.terminalAddr)) {
            return false;
        }
    }
    
    if (!m_subAddressFilter.isEmpty()) {
        if (!m_subAddressFilter.contains(record.subAddr)) {
            return false;
        }
    }
    
    if (!m_messageTypeFilter.isEmpty()) {
        if (!m_messageTypeFilter.contains(record.messageType)) {
            return false;
        }
    }
    
    if (m_chsttFilter >= 0) {
        if (record.packetData.chstt != m_chsttFilter) {
            return false;
        }
    }
    
    if (m_mpuIdFilter > 0) {
        if (record.packetHeader.mpuProduceId != m_mpuIdFilter) {
            return false;
        }
    }
    
    if (m_hasPacketLenFilter) {
        if (record.packetHeader.packetLen < m_packetLenMin || record.packetHeader.packetLen > m_packetLenMax) {
            return false;
        }
    }
    
    if (m_hasDateRangeFilter) {
        if (record.packetHeader.year < m_dateYearStart || record.packetHeader.year > m_dateYearEnd) {
            return false;
        }
        if (record.packetHeader.year == m_dateYearStart && 
            (record.packetHeader.month < m_dateMonthStart || 
             (record.packetHeader.month == m_dateMonthStart && record.packetHeader.day < m_dateDayStart))) {
            return false;
        }
        if (record.packetHeader.year == m_dateYearEnd && 
            (record.packetHeader.month > m_dateMonthEnd || 
             (record.packetHeader.month == m_dateMonthEnd && record.packetHeader.day > m_dateDayEnd))) {
            return false;
        }
    }
    
    if (m_hasStatusBitFilter) {
        quint16 statusWord = (m_statusBitField == 1) ? record.packetData.states1 : record.packetData.states2;
        bool bitSet = (statusWord >> m_statusBitPosition) & 1;
        if (bitSet != m_statusBitValue) {
            return false;
        }
    }
    
    if (!m_excludeTerminalFilter.isEmpty()) {
        if (m_excludeTerminalFilter.contains(record.terminalAddr)) {
            return false;
        }
    }
    
    if (!m_excludeMessageTypeFilter.isEmpty()) {
        if (m_excludeMessageTypeFilter.contains(record.messageType)) {
            return false;
        }
    }
    
    if (m_hasWordCountFilter) {
        if (record.dataCount < m_wordCountMin || record.dataCount > m_wordCountMax) {
            return false;
        }
    }
    
    if (m_errorFlagFilter >= 0) {
        bool hasError = (record.packetData.chstt == 0);
        if (hasError != (m_errorFlagFilter == 1)) {
            return false;
        }
    }
    
    if (!m_columnExpressionFilters.isEmpty()) {
        for (auto it = m_columnExpressionFilters.begin(); it != m_columnExpressionFilters.end(); ++it) {
            int column = it.key();
            const FilterExpression& filter = it.value();
            
            QVariant columnValue;
            switch (column) {
            case 0:
                columnValue = record.rowIndex + 1;
                break;
            case 1:
                columnValue = record.packetHeader.mpuProduceId;
                break;
            case 2:
                columnValue = record.packetHeader.packetLen;
                break;
            case 3:
                continue;
            case 4:
                columnValue = static_cast<int>(record.packetHeader.timestamp);
                break;
            case 5:
                continue;
            case 6:
                columnValue = static_cast<int>(record.terminalAddr);
                break;
            case 7:
                columnValue = static_cast<int>(record.subAddr);
                break;
            case 8:
                continue;
            case 9:
                columnValue = static_cast<int>(record.dataCount);
                break;
            case 10:
                continue;
            case 11:
                continue;
            default:
                continue;
            }
            
            if (!filter.evaluate(columnValue)) {
                return false;
            }
        }
    }
    
    return true;
}

/**
 * @brief 设置筛选条件
 * 
 * 设置单个字段的筛选条件，会覆盖之前的同名筛选条件
 * 
 * @param field 字段名称
 * @param value 筛选值
 */
void DataStore::setFilter(const QString& field, const QVariant& value)
{
    m_filters[field] = value;
    applyFilters();
}

/**
 * @brief 清除所有筛选条件
 * 
 * 清除所有自定义筛选、终端筛选、消息类型筛选和时间范围筛选
 */
void DataStore::beginBatchFilterUpdate()
{
    m_batchFilterUpdate = true;
}

void DataStore::endBatchFilterUpdate()
{
    m_batchFilterUpdate = false;
    applyFilters();
}

void DataStore::clearFilter()
{
    m_filters.clear();
    m_terminalFilter.clear();
    m_subAddressFilter.clear();
    m_messageTypeFilter.clear();
    m_chsttFilter = -1;
    m_mpuIdFilter = 0;
    m_hasTimeRange = false;
    m_hasPacketLenFilter = false;
    m_hasDateRangeFilter = false;
    m_hasStatusBitFilter = false;
    m_excludeTerminalFilter.clear();
    m_excludeMessageTypeFilter.clear();
    m_hasWordCountFilter = false;
    m_errorFlagFilter = -1;
    m_columnExpressionFilters.clear();  // 同时清除列算式筛选
    applyFilters();
}

/**
 * @brief 添加筛选条件
 * 
 * 添加一个筛选条件，不影响其他筛选条件
 * 
 * @param field 字段名称
 * @param value 筛选值
 */
void DataStore::addFilter(const QString& field, const QVariant& value)
{
    m_filters[field] = value;
    applyFilters();
}

/**
 * @brief 移除筛选条件
 * @param field 要移除的字段名称
 */
void DataStore::removeFilter(const QString& field)
{
    m_filters.remove(field);
    applyFilters();
}

/**
 * @brief 设置时间范围筛选
 * 
 * 只显示指定时间范围内的数据
 * 
 * @param startTime 起始时间戳
 * @param endTime 结束时间戳
 */
void DataStore::setTimeRange(quint32 startTime, quint32 endTime)
{
    m_hasTimeRange = true;
    m_startTime = startTime;
    m_endTime = endTime;
    applyFilters();
}

/**
 * @brief 清除时间范围筛选
 */
void DataStore::clearTimeRange()
{
    m_hasTimeRange = false;
    applyFilters();
}

/**
 * @brief 设置终端地址筛选
 * 
 * 只显示指定终端地址的数据
 * 
 * @param terminals 终端地址集合
 */
void DataStore::setTerminalFilter(const QSet<int>& terminals)
{
    m_terminalFilter = terminals;
    applyFilters();
}

/**
 * @brief 清除终端地址筛选
 */
void DataStore::clearTerminalFilter()
{
    m_terminalFilter.clear();
    applyFilters();
}

void DataStore::setSubAddressFilter(const QSet<int>& subAddresses)
{
    m_subAddressFilter = subAddresses;
    applyFilters();
}

void DataStore::clearSubAddressFilter()
{
    m_subAddressFilter.clear();
    applyFilters();
}

/**
 * @brief 设置消息类型筛选
 * 
 * 只显示指定消息类型的数据
 * 
 * @param types 消息类型集合
 */
void DataStore::setMessageTypeFilter(const QSet<MessageType>& types)
{
    m_messageTypeFilter = types;
    applyFilters();
}

/**
 * @brief 清除消息类型筛选
 */
void DataStore::clearMessageTypeFilter()
{
    m_messageTypeFilter.clear();
    applyFilters();
}

void DataStore::setChsttFilter(int chstt)
{
    m_chsttFilter = chstt;
    applyFilters();
}

void DataStore::clearChsttFilter()
{
    m_chsttFilter = -1;
    applyFilters();
}

void DataStore::setMpuIdFilter(int mpuId)
{
    m_mpuIdFilter = mpuId;
    applyFilters();
}

void DataStore::clearMpuIdFilter()
{
    m_mpuIdFilter = 0;
    applyFilters();
}

void DataStore::setPacketLenFilter(int minLen, int maxLen)
{
    m_hasPacketLenFilter = true;
    m_packetLenMin = minLen;
    m_packetLenMax = maxLen;
    applyFilters();
}

void DataStore::clearPacketLenFilter()
{
    m_hasPacketLenFilter = false;
    m_packetLenMin = 0;
    m_packetLenMax = 0;
    applyFilters();
}

void DataStore::setDateRangeFilter(int year, int month, int day)
{
    setDateRangeFilter(year, month, day, year, month, day);
}

void DataStore::setDateRangeFilter(int yearStart, int monthStart, int dayStart, int yearEnd, int monthEnd, int dayEnd)
{
    m_hasDateRangeFilter = true;
    m_dateYearStart = yearStart;
    m_dateMonthStart = monthStart;
    m_dateDayStart = dayStart;
    m_dateYearEnd = yearEnd;
    m_dateMonthEnd = monthEnd;
    m_dateDayEnd = dayEnd;
    applyFilters();
}

void DataStore::clearDateRangeFilter()
{
    m_hasDateRangeFilter = false;
    m_dateYearStart = 0;
    m_dateMonthStart = 0;
    m_dateDayStart = 0;
    m_dateYearEnd = 0;
    m_dateMonthEnd = 0;
    m_dateDayEnd = 0;
    applyFilters();
}

void DataStore::setStatusBitFilter(int statusField, int bitPosition, bool bitValue)
{
    m_hasStatusBitFilter = true;
    m_statusBitField = statusField;
    m_statusBitPosition = bitPosition;
    m_statusBitValue = bitValue;
    applyFilters();
}

void DataStore::clearStatusBitFilter()
{
    m_hasStatusBitFilter = false;
    m_statusBitField = 1;
    m_statusBitPosition = 0;
    m_statusBitValue = true;
    applyFilters();
}

void DataStore::setExcludeTerminalFilter(const QSet<int>& terminals)
{
    m_excludeTerminalFilter = terminals;
    applyFilters();
}

void DataStore::clearExcludeTerminalFilter()
{
    m_excludeTerminalFilter.clear();
    applyFilters();
}

void DataStore::setExcludeMessageTypeFilter(const QSet<MessageType>& types)
{
    m_excludeMessageTypeFilter = types;
    applyFilters();
}

void DataStore::clearExcludeMessageTypeFilter()
{
    m_excludeMessageTypeFilter.clear();
    applyFilters();
}

void DataStore::setWordCountFilter(int minCount, int maxCount)
{
    m_hasWordCountFilter = true;
    m_wordCountMin = minCount;
    m_wordCountMax = maxCount;
    applyFilters();
}

void DataStore::clearWordCountFilter()
{
    m_hasWordCountFilter = false;
    m_wordCountMin = 0;
    m_wordCountMax = 0;
    applyFilters();
}

void DataStore::setErrorFlagFilter(int errorFlag)
{
    m_errorFlagFilter = errorFlag;
    applyFilters();
}

void DataStore::clearErrorFlagFilter()
{
    m_errorFlagFilter = -1;
    applyFilters();
}

/**
 * @brief 获取所有终端地址
 * @return 数据中出现过的所有终端地址集合
 */
QSet<int> DataStore::getAllTerminals() const
{
    QSet<int> terminals;
    
    if (m_useDatabase && m_currentFileId > 0) {
        terminals = DatabaseManager::instance()->getAllTerminals(m_currentFileId);
    } else {
        for (const DataRecord& record : m_allRecords) {
            terminals.insert(record.terminalAddr);
        }
    }
    return terminals;
}

/**
 * @brief 获取所有子地址
 * @return 数据中出现过的所有子地址集合
 */
QSet<int> DataStore::getAllSubAddresses() const
{
    QSet<int> subAddrs;
    for (const DataRecord& record : m_allRecords) {
        subAddrs.insert(record.subAddr);
    }
    return subAddrs;
}

/**
 * @brief 获取所有消息类型
 * @return 数据中出现过的所有消息类型集合
 */
QSet<MessageType> DataStore::getAllMessageTypes() const
{
    QSet<MessageType> types;
    for (const DataRecord& record : m_allRecords) {
        types.insert(record.messageType);
    }
    return types;
}

/**
 * @brief 获取最小时间戳
 * @return 数据中的最小时间戳值
 */
quint32 DataStore::getMinTimestamp() const
{
    if (m_allRecords.isEmpty()) return 0;
    
    quint32 minTs = UINT32_MAX;
    for (const DataRecord& record : m_allRecords) {
        if (record.packetData.timestamp < minTs) {
            minTs = record.packetData.timestamp;
        }
    }
    return minTs;
}

qint64 DataStore::getMinTimestampMs() const
{
    if (m_useDatabase && m_currentFileId > 0) {
        return DatabaseManager::instance()->getMinTimestamp(m_currentFileId);
    }
    
    if (m_allRecords.isEmpty()) return 0;
    
    qint64 minTs = LLONG_MAX;
    for (const DataRecord& record : m_allRecords) {
        if (record.timestampMs < minTs) {
            minTs = record.timestampMs;
        }
    }
    return minTs;
}

/**
 * @brief 获取最大时间戳
 * @return 数据中的最大时间戳值
 */
quint32 DataStore::getMaxTimestamp() const
{
    if (m_allRecords.isEmpty()) return 0;
    
    // 遍历所有记录，找到最大时间戳
    quint32 maxTs = 0;
    for (const DataRecord& record : m_allRecords) {
        if (record.packetData.timestamp > maxTs) {
            maxTs = record.packetData.timestamp;
        }
    }
    return maxTs;
}

qint64 DataStore::getMaxTimestampMs() const
{
    if (m_useDatabase && m_currentFileId > 0) {
        return DatabaseManager::instance()->getMaxTimestamp(m_currentFileId);
    }
    
    if (m_allRecords.isEmpty()) return 0;
    
    qint64 maxTs = 0;
    for (const DataRecord& record : m_allRecords) {
        if (record.timestampMs > maxTs) {
            maxTs = record.timestampMs;
        }
    }
    return maxTs;
}

/**
 * @brief 获取终端统计信息
 * 
 * 统计筛选后数据中每个终端地址的数据量
 * 
 * @return 终端地址到数据量的映射
 */
QMap<int, int> DataStore::getTerminalStatistics() const
{
    QMap<int, int> stats;
    
    if (m_useDatabase && m_currentFileId > 0) {
        qint64 startTime = m_hasTimeRange ? m_startTime : 0;
        qint64 endTime = m_hasTimeRange ? m_endTime : LLONG_MAX;
        stats = DatabaseManager::instance()->getTerminalStatistics(
            m_currentFileId,
            m_terminalFilter,
            m_subAddressFilter,
            m_messageTypeFilter,
            startTime,
            endTime
        );
    } else {
        for (const DataRecord& record : m_filteredRecords) {
            int terminal = record.terminalAddr;
            stats[terminal]++;
        }
    }
    return stats;
}

QMap<MessageType, int> DataStore::getMessageTypeStatistics() const
{
    QMap<MessageType, int> stats;
    
    if (m_useDatabase && m_currentFileId > 0) {
        qint64 startTime = m_hasTimeRange ? m_startTime : 0;
        qint64 endTime = m_hasTimeRange ? m_endTime : LLONG_MAX;
        stats = DatabaseManager::instance()->getMessageTypeStatistics(
            m_currentFileId,
            m_terminalFilter,
            m_subAddressFilter,
            startTime,
            endTime
        );
    } else {
        for (const DataRecord& record : m_filteredRecords) {
            stats[record.messageType]++;
        }
    }
    return stats;
}

QMap<int, int> DataStore::getSubAddressStatistics() const
{
    QMap<int, int> stats;
    
    if (m_useDatabase && m_currentFileId > 0) {
        qint64 startTime = m_hasTimeRange ? m_startTime : 0;
        qint64 endTime = m_hasTimeRange ? m_endTime : LLONG_MAX;
        stats = DatabaseManager::instance()->getSubAddressStatistics(
            m_currentFileId,
            m_terminalFilter,
            m_messageTypeFilter,
            startTime,
            endTime
        );
    } else {
        for (const DataRecord& record : m_filteredRecords) {
            int subAddr = record.subAddr;
            stats[subAddr]++;
        }
    }
    return stats;
}

QVector<DataRecord> DataStore::getRecordsByScope(DataScope scope) const
{
    switch (scope) {
    case DataScope::AllData:
        if (m_useDatabase && m_allRecords.isEmpty()) {
            return getAllRecordsForReport();
        }
        return m_allRecords;
    case DataScope::FilteredData:
        return getFilteredRecords();
    case DataScope::CurrentPage:
        return getCurrentPageRecords();
    }
    return m_allRecords;
}

QMap<int, int> DataStore::getTerminalStatisticsByScope(DataScope scope) const
{
    QMap<int, int> stats;
    QVector<DataRecord> records = getRecordsByScope(scope);
    for (const DataRecord& record : records) {
        stats[record.terminalAddr]++;
    }
    return stats;
}

QMap<MessageType, int> DataStore::getMessageTypeStatisticsByScope(DataScope scope) const
{
    QMap<MessageType, int> stats;
    if (scope == DataScope::FilteredData) {
        return getMessageTypeStatistics();
    }
    if (scope == DataScope::AllData && !m_useDatabase) {
        for (const DataRecord& record : m_allRecords) {
            stats[record.messageType]++;
        }
        return stats;
    }
    QVector<DataRecord> records = getRecordsByScope(scope);
    for (const DataRecord& record : records) {
        stats[record.messageType]++;
    }
    return stats;
}

void DataStore::setDataScope(DataScope scope)
{
    if (m_dataScope == scope) return;
    m_dataScope = scope;
    emit dataScopeChanged();
}

/**
 * @brief 排序数据
 * 
 * 按指定字段对筛选后的数据进行排序
 * 
 * @param field 排序字段（timestamp/terminal/type）
 * @param order 排序顺序（升序/降序）
 */
void DataStore::sort(const QString& field, Qt::SortOrder order)
{
    m_sortField = field;
    m_sortOrder = order;
    applyFilters();
}

/**
 * @brief 构建数据索引（静态方法）
 * 
 * 从原始数据构建索引记录数组，用于后台线程构建索引
 * 
 * @param data 原始消息数据
 * @return 索引记录数组
 */
QVector<DataRecord> DataStore::buildIndex(
    const QVector<PacketParser::SMbiMonPacketMsg>& data,
    std::function<void(int current, int total)> progressCallback
)
{
    QVector<DataRecord> records;
    int rowIndex = 0;
    int totalMessages = data.size();
    
    // 预分配空间以提高性能
    int estimatedRecords = 0;
    for (int i = 0; i < qMin(100, totalMessages); ++i) {
        estimatedRecords += data[i].packetDatas.size();
    }
    if (totalMessages > 0) {
        estimatedRecords = estimatedRecords * totalMessages / qMin(100, totalMessages);
        records.reserve(estimatedRecords);
    }
    
    // 遍历每条消息
    for (int msgIdx = 0; msgIdx < data.size(); ++msgIdx) {
        const PacketParser::SMbiMonPacketMsg& msg = data[msgIdx];
        
        // 遍历消息中的每个数据包
        for (int dataIdx = 0; dataIdx < msg.packetDatas.size(); ++dataIdx) {
            const PacketParser::SMbiMonPacketData& pkt = msg.packetDatas[dataIdx];
            
            // 创建数据记录
            DataRecord record;
            record.rowIndex = rowIndex++;
            record.msgIndex = msgIdx;
            record.dataIndex = dataIdx;
            record.packetData = pkt;
            record.packetHeader = msg.header;
            
            // 检测消息类型
            record.messageType = detectMessageType(pkt);
            
            // 计算时间戳（单位：毫秒）
            record.timestampMs = pkt.timestamp * 40.0 / 1000.0;
            
            records.append(record);
        }
        
        // 每处理100条消息或最后一条消息时，报告进度
        if (progressCallback && (msgIdx % 100 == 0 || msgIdx == totalMessages - 1)) {
            progressCallback(msgIdx + 1, totalMessages);
        }
    }
    
    return records;
}

/**
 * @brief 设置已索引的数据
 * 
 * 直接设置原始数据和预构建的索引，
 * 用于后台线程构建索引后快速加载
 * 
 * @param rawData 原始消息数据
 * @param index 预构建的索引
 */
void DataStore::setIndexedData(const QVector<PacketParser::SMbiMonPacketMsg>& rawData, const QVector<DataRecord>& index)
{
    // 阻塞信号，避免多次触发UI更新
    blockSignals(true);
    
    m_rawData = rawData;
    m_allRecords = index;
    
    // 检查是否有筛选条件
    bool hasFilters = !m_filters.isEmpty() || 
                      !m_terminalFilter.isEmpty() || 
                      !m_subAddressFilter.isEmpty() || 
                      !m_messageTypeFilter.isEmpty() || 
                      m_hasTimeRange ||
                      !m_columnExpressionFilters.isEmpty();
    
    if (hasFilters) {
        // 有筛选条件，恢复信号并发送异步筛选
        blockSignals(false);
        applyFiltersAsync();
    } else {
        // 无筛选条件，直接赋值
        m_filteredRecords = m_allRecords;
        m_currentPage = 0;
        
        // 更新当前页数据缓存
        updateCurrentPageCache();
        
        // 恢复信号
        blockSignals(false);
        
        // 一次性发送所有信号
        emit dataChanged();
        emit pageChanged(m_currentPage, totalPages(), m_filteredRecords.size());
    }
}

/**
 * @brief 设置列算式筛选
 * 
 * 为指定列设置算式筛选条件
 * 
 * @param column 列索引
 * @param expression 筛选表达式
 * @return 表达式是否有效
 */
bool DataStore::setColumnExpressionFilter(int column, const QString& expression)
{
    FilterExpression filter(expression);
    if (!filter.isValid()) {
        return false;
    }
    
    m_columnExpressionFilters[column] = filter;
    applyFilters();
    return true;
}

/**
 * @brief 清除列算式筛选
 * @param column 列索引，-1表示清除所有列的筛选
 */
void DataStore::clearColumnExpressionFilter(int column)
{
    if (column == -1) {
        m_columnExpressionFilters.clear();
    } else {
        m_columnExpressionFilters.remove(column);
    }
    applyFilters();
}

/**
 * @brief 获取列算式筛选
 * @param column 列索引
 * @return 筛选表达式字符串
 */
QString DataStore::getColumnExpressionFilter(int column) const
{
    if (m_columnExpressionFilters.contains(column)) {
        return m_columnExpressionFilters[column].getExpression();
    }
    return QString();
}

/**
 * @brief 检查是否有列算式筛选
 * @return 是否有任意列设置了算式筛选
 */
bool DataStore::hasColumnExpressionFilters() const
{
    return !m_columnExpressionFilters.isEmpty();
}

// ========== 分页相关方法实现 ==========

/**
 * @brief 设置每页显示数量
 * @param size 每页条数
 * 
 * 更新每页显示数量，并重置到第一页
 */
void DataStore::setPageSize(int size)
{
    if (size > 0 && m_pageSize != size) {
        m_pageSize = size;
        m_currentPage = 0;
        updateCurrentPageCache();
        int filtered = m_useDatabase ? static_cast<int>(m_filteredCountForDb) : m_filteredRecords.size();
        emit pageChanged(m_currentPage, totalPages(), filtered);
    }
}

/**
 * @brief 跳转到指定页
 * @param page 页码（从0开始）
 * 
 * 跳转到指定页，如果页码无效则不执行操作
 */
void DataStore::setCurrentPage(int page)
{
    int maxPage = totalPages() - 1;
    if (page >= 0 && page <= maxPage && m_currentPage != page) {
        m_currentPage = page;
        // 数据库模式+算式筛选时，数据在m_filteredRecords中，使用内存分页
        if (m_useDatabase && !m_columnExpressionFilters.isEmpty()) {
            updateCurrentPageCacheForExpressionFilter();
        } else {
            updateCurrentPageCache();
        }
        int filtered = m_useDatabase ? static_cast<int>(m_filteredCountForDb) : m_filteredRecords.size();
        emit pageChanged(m_currentPage, totalPages(), filtered);
    }
}

/**
 * @brief 获取总页数
 * @return 总页数
 */
int DataStore::totalPages() const
{
    // 数据库模式+算式筛选时，数据在m_filteredRecords中，使用内存分页计算
    if (m_useDatabase && m_currentFileId > 0 && !m_columnExpressionFilters.isEmpty()) {
        if (m_pageSize <= 0 || m_filteredRecords.isEmpty()) {
            return 0;
        }
        return (m_filteredRecords.size() + m_pageSize - 1) / m_pageSize;
    }
    
    if (m_useDatabase && m_currentFileId > 0) {
        if (m_pageSize <= 0 || m_filteredCountForDb <= 0) {
            return 0;
        }
        return static_cast<int>((m_filteredCountForDb + m_pageSize - 1) / m_pageSize);
    }
    
    if (m_pageSize <= 0 || m_filteredRecords.isEmpty()) {
        return 0;
    }
    return (m_filteredRecords.size() + m_pageSize - 1) / m_pageSize;
}

/**
 * @brief 获取当前页数据
 * @return 当前页的数据记录的常引用
 */
const QVector<DataRecord>& DataStore::getCurrentPageRecords() const
{
    return m_currentPageRecords;
}

/**
 * @brief 异步筛选（在后台线程执行）
 * 
 * 在后台线程中遍历所有数据应用筛选条件，
 * 每处理1000条记录发送一次进度信号，
 * 完成后发送filterChanged信号
 */
void DataStore::applyFiltersAsync()
{
    m_cancelFilter = false;
    
    int total = m_allRecords.size();
    
    if (total == 0) {
        m_filteredRecords.clear();
        m_currentPage = 0;
        emit filterChanged();
        emit pageChanged(m_currentPage, totalPages(), 0);
        return;
    }
    
    QVector<DataRecord> allRecordsCopy = m_allRecords;
    QString sortField = m_sortField;
    Qt::SortOrder sortOrder = m_sortOrder;
    FilterSnapshot filterSnapshot = captureFilterSnapshot();
    
    QtConcurrent::run([this, total, allRecordsCopy, sortField, sortOrder, filterSnapshot]() {
        QVector<DataRecord> filtered;
        filtered.reserve(total);
        
        int processed = 0;
        int lastPercent = 0;
        
        for (const DataRecord& record : allRecordsCopy) {
            if (m_cancelFilter) {
                emit filterProgress(0, 0, total);
                return;
            }
            
            if (matchesFilterSnapshot(record, filterSnapshot)) {
                filtered.append(record);
            }
            
            processed++;
            int percent = static_cast<int>(processed * 100.0 / total);
            
            if (processed % 1000 == 0 || percent != lastPercent) {
                emit filterProgress(percent, processed, total);
                lastPercent = percent;
            }
        }
        
        if (!sortField.isEmpty() && !m_cancelFilter) {
            std::sort(filtered.begin(), filtered.end(),
                [sortField, sortOrder](const DataRecord& a, const DataRecord& b) {
                    bool less = false;
                    if (sortField == "timestamp") {
                        less = a.timestampMs < b.timestampMs;
                    } else if (sortField == "terminal") {
                        less = a.terminalAddr < b.terminalAddr;
                    } else if (sortField == "type") {
                        less = static_cast<int>(a.messageType) < static_cast<int>(b.messageType);
                    }
                    
                    return sortOrder == Qt::AscendingOrder ? less : !less;
                });
        }
        
        // 返回UI线程更新结果
        qt5InvokeMethod(this, [this, filtered]() {
            m_filteredRecords = filtered;
            m_currentPage = 0;
            
            updateCurrentPageCache();
            
            emit filterProgress(100, m_allRecords.size(), m_allRecords.size());
            emit filterChanged();
            emit pageChanged(m_currentPage, totalPages(), m_filteredRecords.size());
        });
    });
}

/**
 * @brief 取消异步筛选
 */
void DataStore::cancelAsyncFilter()
{
    m_cancelFilter = true;
}

/**
 * @brief 更新当前页数据缓存
 * 
 * 根据当前页码和每页条数，从筛选后的数据中提取当前页数据
 */
void DataStore::updateCurrentPageCache()
{
    if (m_useDatabase && m_currentFileId > 0) {
        m_currentPageRecords.clear();
        
        if (m_filteredCountForDb <= 0 || m_pageSize <= 0) {
            return;
        }
        
        int offset = m_currentPage * m_pageSize;
        qint64 startTime = m_hasTimeRange ? m_startTime : 0;
        qint64 endTime = m_hasTimeRange ? m_endTime : LLONG_MAX;
        
        QVector<DbDataRecord> dbRecords = DatabaseManager::instance()->queryPackets(
            m_currentFileId,
            m_terminalFilter,
            m_subAddressFilter,
            m_messageTypeFilter,
            startTime,
            endTime,
            m_pageSize,
            offset,
            m_chsttFilter,
            m_mpuIdFilter,
            m_excludeTerminalFilter,
            m_excludeMessageTypeFilter,
            m_hasPacketLenFilter ? m_packetLenMin : -1,
            m_hasPacketLenFilter ? m_packetLenMax : -1,
            m_hasDateRangeFilter ? m_dateYearStart : -1,
            m_hasDateRangeFilter ? m_dateMonthStart : -1,
            m_hasDateRangeFilter ? m_dateDayStart : -1,
            m_hasDateRangeFilter ? m_dateYearEnd : -1,
            m_hasDateRangeFilter ? m_dateMonthEnd : -1,
            m_hasDateRangeFilter ? m_dateDayEnd : -1,
            m_hasStatusBitFilter ? m_statusBitField : -1,
            m_hasStatusBitFilter ? m_statusBitPosition : -1,
            m_hasStatusBitFilter ? (m_statusBitValue ? 1 : 0) : -1,
            m_hasWordCountFilter ? m_wordCountMin : -1,
            m_hasWordCountFilter ? m_wordCountMax : -1,
            m_errorFlagFilter
        );
        
        LOG_INFO("DataStore", QString::fromUtf8(u8"[数据存储] 查询返回记录数: %1, m_rawData.size: %2, 当前页: %3, 每页大小: %4, 偏移: %5")
                 .arg(dbRecords.size()).arg(m_rawData.size()).arg(m_currentPage).arg(m_pageSize).arg(offset));
        
        for (const DbDataRecord& dbRecord : dbRecords) {
            DataRecord record;
            record.rowIndex = dbRecord.rowIndex;
            record.msgIndex = dbRecord.msgIndex;
            record.dataIndex = dbRecord.dataIndex;
            record.messageType = dbRecord.messageType;
            record.timestampMs = dbRecord.timestampMs;
            
            if (!m_rawData.isEmpty() && dbRecord.msgIndex >= 0 && dbRecord.msgIndex < m_rawData.size()) {
                const SMbiMonPacketMsg& msg = m_rawData[dbRecord.msgIndex];
                record.packetHeader = msg.header;
                
                if (dbRecord.dataIndex >= 0 && dbRecord.dataIndex < msg.packetDatas.size()) {
                    record.packetData = msg.packetDatas[dbRecord.dataIndex];
                    // 从packetData.cmd1位运算解析并缓存
                    record.terminalAddr = cmdTerminalAddr(record.packetData.cmd1);
                    record.subAddr = cmdSubAddr(record.packetData.cmd1);
                    record.t_r = cmdTR(record.packetData.cmd1);
                    record.dataCount = cmdDataCount(record.packetData.cmd1);
                } else {
                    LOG_ERROR("DataStore", QString::fromUtf8(u8"[数据存储] dataIndex越界 - msgIndex: %1, dataIndex: %2, packetDatas.size: %3")
                             .arg(dbRecord.msgIndex).arg(dbRecord.dataIndex).arg(msg.packetDatas.size()));
                }
            } else {
                // m_rawData为空（数据库模式），从数据库字段重建packetData和缓存字段
                record.packetHeader = SMbiMonPacketHeader();
                record.packetHeader.mpuProduceId = static_cast<quint8>(dbRecord.mpuProduceId);
                record.packetHeader.packetLen = static_cast<quint16>(dbRecord.packetLen);
                record.packetHeader.year = static_cast<quint16>(dbRecord.headerYear);
                record.packetHeader.month = static_cast<quint8>(dbRecord.headerMonth);
                record.packetHeader.day = static_cast<quint8>(dbRecord.headerDay);
                record.packetHeader.timestamp = dbRecord.headerTimestamp;
                
                record.packetData.header = 0xAABB;
                record.packetData.cmd1 = generateCmd1(dbRecord.terminalAddr, dbRecord.subAddr, dbRecord.trBit, dbRecord.wordCount);
                record.packetData.cmd2 = generateCmd2(dbRecord.terminalAddr2, dbRecord.subAddr2, dbRecord.trBit2, dbRecord.wordCount2);
                record.packetData.states1 = static_cast<quint16>(dbRecord.status1);
                record.packetData.states2 = static_cast<quint16>(dbRecord.status2);
                record.packetData.chstt = static_cast<quint16>(dbRecord.chstt);
                record.packetData.timestamp = dbRecord.packetTimestamp;
                record.packetData.datas = dbRecord.data;
                
                record.terminalAddr = static_cast<quint8>(dbRecord.terminalAddr);
                record.subAddr = static_cast<quint8>(dbRecord.subAddr);
                record.t_r = static_cast<quint8>(dbRecord.trBit);
                record.dataCount = static_cast<quint8>(dbRecord.wordCount);
            }
            
            m_currentPageRecords.append(record);
        }
    } else {
        if (m_filteredRecords.isEmpty() || m_pageSize <= 0) {
            m_currentPageRecords.clear();
            return;
        }
        
        int start = m_currentPage * m_pageSize;
        int end = qMin(start + m_pageSize, m_filteredRecords.size());
        
        m_currentPageRecords = m_filteredRecords.mid(start, end - start);
    }
}

/**
 * @brief 数据库模式+算式筛选时更新当前页数据缓存
 * 
 * 当数据库模式下存在算式筛选时，数据已加载到m_filteredRecords中，
 * 需要从m_filteredRecords中分页（类似内存模式），而非从数据库查询。
 * 这是因为SQL无法直接支持复杂的算式筛选表达式（如">1&&<10"），
 * 所以需要将数据库查询结果加载到内存后进行二次筛选。
 */
void DataStore::updateCurrentPageCacheForExpressionFilter()
{
    // 数据已在m_filteredRecords中，直接从中分页
    if (m_filteredRecords.isEmpty() || m_pageSize <= 0) {
        m_currentPageRecords.clear();
        return;
    }
    
    int start = m_currentPage * m_pageSize;
    int end = qMin(start + m_pageSize, m_filteredRecords.size());
    
    m_currentPageRecords = m_filteredRecords.mid(start, end - start);
}

quint16 DataStore::generateCmd1(int terminalAddr, int subAddr, int trBit, int wordCount) const
{
    CMD cmd;
    cmd.zhongduandizhi = terminalAddr & 0x1F;
    cmd.T_R = trBit & 0x1;
    cmd.zidizhi = subAddr & 0x1F;
    cmd.sjzjs_fsdm = wordCount & 0x1F;
    quint16 result;
    memcpy(&result, &cmd, sizeof(quint16));
    return result;
}

quint16 DataStore::generateCmd2(int terminalAddr, int subAddr, int trBit, int wordCount) const
{
    CMD cmd;
    cmd.zhongduandizhi = terminalAddr & 0x1F;
    cmd.T_R = trBit & 0x1;
    cmd.zidizhi = subAddr & 0x1F;
    cmd.sjzjs_fsdm = wordCount & 0x1F;
    quint16 result;
    memcpy(&result, &cmd, sizeof(quint16));
    return result;
}

