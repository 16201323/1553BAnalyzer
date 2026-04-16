/**
 * @file DataModel.cpp
 * @brief 数据表格模型类实现
 * 
 * 本文件实现了DataModel类的所有方法，包括：
 * - 数据获取和格式化
 * - 表头显示
 * - 排序功能
 * - 数据变化响应
 * 
 * @author 1553BTools
 * @date 2024
 */

#include "DataModel.h"
#include "core/parser/PacketStruct.h"
#include <QDateTime>
#include <QColor>
#include <cstring>

/**
 * @brief 构造函数，初始化成员变量
 * @param parent 父对象指针
 */
DataModel::DataModel(QObject *parent)
    : QAbstractTableModel(parent)
    , m_dataStore(nullptr)
    , m_currentSortOrder(Qt::AscendingOrder)
    , m_currentSortColumn(-1)
{
}

/**
 * @brief 析构函数
 */
DataModel::~DataModel()
{
}

/**
 * @brief 设置数据存储对象
 * @param store DataStore对象指针
 * 
 * 断开旧的数据存储连接，建立新的连接：
 * - dataChanged信号 -> onDataChanged槽
 * - filterChanged信号 -> onFilterChanged槽
 */
void DataModel::setDataStore(DataStore* store)
{
    beginResetModel();
    
    if (m_dataStore) {
        disconnect(m_dataStore, nullptr, this, nullptr);
    }
    
    m_dataStore = store;
    
    if (m_dataStore) {
        connect(m_dataStore, &DataStore::dataChanged, this, &DataModel::onDataChanged);
        connect(m_dataStore, &DataStore::filterChanged, this, &DataModel::onFilterChanged);
    }
    
    endResetModel();
}

/**
 * @brief 获取表格行数
 * @param parent 父索引（表格模型中不使用）
 * @return 当前页的数据行数，无数据存储时返回0
 */
int DataModel::rowCount(const QModelIndex& parent) const
{
    // 表格模型不支持层级结构，父索引有效时返回0
    if (parent.isValid() || !m_dataStore) {
        return 0;
    }
    // 直接返回当前页数据量，避免调用getCurrentPageRecords()
    const QVector<DataRecord>& pageData = m_dataStore->getCurrentPageRecords();
    return pageData.size();
}

/**
 * @brief 获取表格列数
 * @param parent 父索引（表格模型中不使用）
 * @return 列总数（ColCount）
 */
int DataModel::columnCount(const QModelIndex& parent) const
{
    // 表格模型不支持层级结构，父索引有效时返回0
    if (parent.isValid()) {
        return 0;
    }
    return ColCount;
}

/**
 * @brief 获取单元格数据
 * @param index 单元格索引
 * @param role 数据角色
 * @return 根据角色返回相应的数据
 * 
 * 支持的数据角色：
 * - DisplayRole: 显示文本，根据列类型格式化数据
 * - BackgroundRole: 背景色，状态列和消息类型列有着色
 * - TextAlignmentRole: 文本居中对齐
 * - ToolTipRole: 工具提示，数据列显示完整十六进制数据
 */
QVariant DataModel::data(const QModelIndex& index, int role) const
{
    // 验证索引有效性
    if (!index.isValid() || !m_dataStore) {
        return QVariant();
    }
    
    int row = index.row();
    int col = index.column();
    
    // 获取当前页数据（直接访问缓存）
    const QVector<DataRecord>& currentPageData = m_dataStore->getCurrentPageRecords();
    
    // 验证行号范围
    if (row < 0 || row >= currentPageData.size()) {
        return QVariant();
    }
    
    // 从当前页数据中获取记录
    const DataRecord& record = currentPageData[row];
    
    // 使用DataRecord中缓存的cmd1字段，避免每次显示都执行memcpy解析
    if (role == Qt::DisplayRole) {
        switch (col) {
        case ColRowIndex:
            return row + 1;
            
        case ColMpuId:
            return QString("MPU%1").arg(record.packetHeader.mpuProduceId);
            
        case ColPacketLen:
            return record.packetHeader.packetLen;
            
        case ColDate:
            return QString("%1-%2-%3")
                .arg(record.packetHeader.year)
                .arg(record.packetHeader.month, 2, 10, QChar('0'))
                .arg(record.packetHeader.day, 2, 10, QChar('0'));
            
        case ColTimestamp:
            {
                quint32 ts = record.packetHeader.timestamp;
                double totalMs = static_cast<double>(ts) * 40.0 / 1000.0;
                
                int hours = static_cast<int>(totalMs) / 3600000;
                int minutes = (static_cast<int>(totalMs) % 3600000) / 60000;
                int seconds = (static_cast<int>(totalMs) % 60000) / 1000;
                int milliseconds = static_cast<int>(totalMs) % 1000;
                
                return QString("%1:%2:%3.%4")
                    .arg(hours, 2, 10, QChar('0'))
                    .arg(minutes, 2, 10, QChar('0'))
                    .arg(seconds, 2, 10, QChar('0'))
                    .arg(milliseconds, 3, 10, QChar('0'));
            }
            
        case ColMessageType:
            return messageTypeToString(record.messageType);
            
        case ColTerminalAddr:
            return QString::number(record.terminalAddr);
            
        case ColSubAddr:
            return QString::number(record.subAddr);
            
        case ColTR:
            return record.t_r ? "RT→BC" : "BC→RT";
            
        case ColDataCount:
            return QString::number(record.dataCount);
            
        case ColStatus:
            // 传输状态：chstt非0为成功，0为失败
            return record.packetData.chstt ? tr("成功") : tr("失败");
            
        case ColDataHex:
            {
                QString hexData = record.packetData.datas.toHex(' ').toUpper();
                const int maxDisplayLen = 50;
                if (hexData.length() <= maxDisplayLen) {
                    return hexData;
                } else {
                    return hexData.left(maxDisplayLen) + "...";
                }
            }
            
        default:
            return QVariant();
        }
    }
    
    // 处理背景色角色
    if (role == Qt::BackgroundRole) {
        // 状态列：成功为浅绿色，失败为浅红色
        if (col == ColStatus) {
            return record.packetData.chstt ? QColor(200, 255, 200) : QColor(255, 200, 200);
        }
        
        // 消息类型列：不同类型使用不同颜色
        if (col == ColMessageType) {
            switch (record.messageType) {
            case MessageType::BC_TO_RT:
                return QColor(52, 152, 219, 100);   // 蓝色
            case MessageType::RT_TO_BC:
                return QColor(46, 204, 113, 100);   // 绿色
            case MessageType::RT_TO_RT:
                return QColor(243, 156, 18, 100);   // 橙色
            case MessageType::Broadcast:
                return QColor(155, 89, 182, 100);   // 紫色
            default:
                return QVariant();
            }
        }
    }
    
    // 处理文本对齐角色：所有列居中对齐
    if (role == Qt::TextAlignmentRole) {
        return Qt::AlignCenter;
    }
    
    // 处理工具提示角色：数据列显示完整十六进制数据
    if (role == Qt::ToolTipRole) {
        if (col == ColDataHex) {
            return record.packetData.datas.toHex(' ');
        }
    }
    
    return QVariant();
}

/**
 * @brief 获取表头数据
 * @param section 节（列号）
 * @param orientation 方向（仅处理水平方向）
 * @param role 数据角色（仅处理显示角色）
 * @return 表头文本
 */
QVariant DataModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    // 仅处理水平表头的显示角色
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QVariant();
    }
    
    // 根据列号返回对应的表头文本
    switch (section) {
    case ColRowIndex:
        return tr("序号");
    case ColMpuId:
        return tr("任务机");
    case ColPacketLen:
        return tr("包长度");
    case ColDate:
        return tr("日期");
    case ColTimestamp:
        return tr("时间戳");
    case ColMessageType:
        return tr("消息类型");
    case ColTerminalAddr:
        return tr("终端地址");
    case ColSubAddr:
        return tr("子地址");
    case ColTR:
        return tr("收发");
    case ColDataCount:
        return tr("数据计数");
    case ColStatus:
        return tr("状态");
    case ColDataHex:
        return tr("数据(HEX)");
    default:
        return QVariant();
    }
}

/**
 * @brief 排序数据
 * @param column 排序列
 * @param order 排序顺序
 * 
 * 支持的排序列：
 * - ColTimestamp: 按时间戳排序
 * - ColTerminalAddr: 按终端地址排序
 * - ColMessageType: 按消息类型排序
 */
void DataModel::sort(int column, Qt::SortOrder order)
{
    // 记录当前排序状态
    m_currentSortColumn = column;
    m_currentSortOrder = order;
    
    // 将列号转换为排序字段名
    QString field;
    switch (column) {
    case ColTimestamp:
        field = "timestamp";
        break;
    case ColTerminalAddr:
        field = "terminal";
        break;
    case ColMessageType:
        field = "type";
        break;
    default:
        // 不支持的排序列，直接返回
        return;
    }
    
    // 调用DataStore进行排序
    if (m_dataStore) {
        m_dataStore->sort(field, order);
    }
}

/**
 * @brief 获取指定行的数据记录
 * @param row 行号（当前页内的行号）
 * @return 数据记录，无效行返回空记录
 */
DataRecord DataModel::getRecord(int row) const
{
    if (m_dataStore) {
        QVector<DataRecord> currentPageData = m_dataStore->getCurrentPageRecords();
        if (row >= 0 && row < currentPageData.size()) {
            return currentPageData[row];
        }
    }
    return DataRecord();
}

/**
 * @brief 从模型索引获取行号
 * @param index 模型索引
 * @return 行号，无效索引返回-1
 */
int DataModel::getRowIndex(const QModelIndex& index) const
{
    return index.isValid() ? index.row() : -1;
}

/**
 * @brief 刷新模型
 * 
 * 触发模型重置，通知所有连接的视图更新显示
 */
void DataModel::refresh()
{
    beginResetModel();
    endResetModel();
}

/**
 * @brief 数据变化槽函数
 * 
 * 响应DataStore::dataChanged信号，
 * 重置模型并发出dataLoaded信号
 */
void DataModel::onDataChanged()
{
    beginResetModel();
    endResetModel();
    emit dataLoaded(m_dataStore ? m_dataStore->totalCount() : 0);
}

/**
 * @brief 筛选条件变化槽函数
 * 
 * 响应DataStore::filterChanged信号，
 * 重置模型以更新显示
 */
void DataModel::onFilterChanged()
{
    beginResetModel();
    endResetModel();
}
