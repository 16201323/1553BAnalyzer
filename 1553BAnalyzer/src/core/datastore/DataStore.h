/**
 * @file DataStore.h
 * @brief 数据存储管理类定义
 * 
 * DataStore类负责管理所有解析后的1553B数据，
 * 提供数据存储、查询、筛选和统计功能。
 * 
 * 主要功能：
 * - 数据存储和索引管理
 * - 多条件筛选（终端地址、消息类型、时间范围等）
 * - 统计信息计算
 * - 数据变化通知
 * - 数据库后端支持（可选）
 */

#ifndef DATASTORE_H
#define DATASTORE_H

#include <QObject>
#include <QVector>
#include <QHash>
#include <QSet>
#include <QVariant>
#include <atomic>
#include <functional>
#include "core/parser/PacketStruct.h"
#include "FilterExpression.h"

struct DbDataRecord;

/**
 * @brief 存储模式枚举
 */
enum class StorageMode {
    Memory,     // 内存模式（默认，适合小数据量）
    Database    // 数据库模式（适合大数据量）
};

enum class DataScope {
    AllData,        // 文件所有数据
    FilteredData,   // 当前筛选数据
    CurrentPage     // 当前页数据
};

/**
 * @brief 数据记录结构
 * 
 * 存储单条1553B数据的完整信息，包括原始数据、
 * 解析后的字段和计算后的派生数据。
 * 
 * 每条记录对应表格中的一行数据。
 */
struct DataRecord {
    int rowIndex;                      // 行序号（从0开始，用于表格显示）
    int msgIndex;                      // 消息索引（在原始数据数组中的位置）
    int dataIndex;                     // 数据包索引（在消息中的位置）
    SMbiMonPacketData packetData;      // 原始数据包内容
    SMbiMonPacketHeader packetHeader;  // 包头信息
    MessageType messageType;           // 消息类型（BC→RT、RT→BC等）
    double timestampMs;                // 时间戳（毫秒，由原始时间戳转换）
    quint8 terminalAddr;               // 终端地址（缓存自cmd1，避免重复memcpy）
    quint8 subAddr;                    // 子地址（缓存自cmd1）
    quint8 t_r;                        // 收发状态（缓存自cmd1，0=接收，1=发送）
    quint8 dataCount;                  // 数据计数/发送码（缓存自cmd1）
};

/**
 * @brief 数据存储管理类
 * 
 * 管理所有解析后的1553B数据，提供以下核心功能：
 * 
 * 1. 数据存储
 *    - 存储原始解析数据
 *    - 构建数据索引
 *    - 管理数据生命周期
 * 
 * 2. 数据筛选
 *    - 按终端地址筛选
 *    - 按消息类型筛选
 *    - 按时间范围筛选
 *    - 按自定义字段筛选
 * 
 * 3. 统计功能
 *    - 终端数据量统计
 *    - 消息类型统计
 *    - 时间范围计算
 * 
 * 4. 信号通知
 *    - 数据变化通知
 *    - 筛选条件变化通知
 * 
 * 使用示例：
 * @code
 * DataStore store;
 * store.loadData(rawData);
 * 
 * // 设置筛选条件
 * store.setTerminalFilter({1, 2, 3});
 * store.setMessageTypeFilter({MessageType::BCtoRT});
 * 
 * // 获取筛选结果
 * QVector<DataRecord> records = store.getFilteredRecords();
 * 
 * // 获取统计信息
 * QMap<int, int> stats = store.getTerminalStatistics();
 * @endcode
 */
class DataStore : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象指针
     * 
     * 初始化数据存储对象，设置默认参数
     */
    explicit DataStore(QObject *parent = nullptr);
    
    /**
     * @brief 析构函数
     * 
     * 清理数据存储资源
     */
    ~DataStore();
    
    // ========== 存储模式管理 ==========
    
    /**
     * @brief 设置存储模式
     * @param mode 存储模式（Memory或Database）
     */
    void setStorageMode(StorageMode mode);
    
    /**
     * @brief 获取当前存储模式
     * @return 当前存储模式
     */
    StorageMode storageMode() const { return m_storageMode; }
    
    /**
     * @brief 检查是否使用数据库模式
     * @return 是否使用数据库模式
     */
    bool isDatabaseMode() const { return m_storageMode == StorageMode::Database; }
    
    /**
     * @brief 设置数据量阈值
     * 
     * 当数据量超过阈值时，自动切换到数据库模式
     * 
     * @param threshold 数据量阈值（默认50000）
     */
    void setAutoSwitchThreshold(int threshold) { m_autoSwitchThreshold = threshold; }
    
    /**
     * @brief 从文件导入数据（数据库模式）
     * 
     * 使用并行导入、批量插入优化大数据量导入
     * 
     * @param data 原始消息数据数组
     * @param filePath 文件路径（用于生成唯一标识）
     * @param progressCallback 进度回调函数
     * @return 导入的文件ID，失败返回-1
     */
    int importFromData(
        const QVector<PacketParser::SMbiMonPacketMsg>& data,
        const QString& filePath,
        std::function<void(int current, int total, const QString& status)> progressCallback = nullptr
    );
    
    /**
     * @brief 切换到指定文件
     * @param fileId 文件ID
     * @return 是否成功
     */
    bool switchToFile(int fileId);
    
    /**
     * @brief 获取当前文件ID
     * @return 当前文件ID
     */
    int currentFileId() const { return m_currentFileId; }
    
    QSet<int> terminalFilter() const { return m_terminalFilter; }
    QSet<int> subAddressFilter() const { return m_subAddressFilter; }
    QSet<MessageType> messageTypeFilter() const { return m_messageTypeFilter; }
    bool hasTimeRange() const { return m_hasTimeRange; }
    quint32 startTime() const { return m_startTime; }
    quint32 endTime() const { return m_endTime; }
    
    /**
     * @brief 设置数据
     * 
     * 将解析后的原始数据加载到存储中，
     * 自动重建索引并发送数据变化信号
     * 
     * @param data 原始消息数据数组
     */
    void setData(const QVector<PacketParser::SMbiMonPacketMsg>& data);
    
    /**
     * @brief 清空数据
     * 
     * 清空所有存储的数据和索引，
     * 重置筛选条件
     */
    void clear();
    
    /**
     * @brief 获取总记录数
     * @return 所有记录的总数（未筛选）
     */
    int totalCount() const;
    
    /**
     * @brief 获取消息数
     * @return 原始消息的数量
     */
    int messageCount() const;
    
    /**
     * @brief 获取指定索引的记录
     * @param index 记录索引
     * @return 数据记录，索引无效时返回空记录
     */
    DataRecord getRecord(int index) const;
    
    /**
     * @brief 获取指定索引的消息
     * @param msgIndex 消息索引
     * @return 原始消息的常引用
     */
    const PacketParser::SMbiMonPacketMsg& getMessage(int msgIndex) const;
    
    /**
     * @brief 获取所有记录
     * @return 所有数据记录的数组
     */
    QVector<DataRecord> getAllRecords() const;
    
    /**
     * @brief 获取全量数据用于报告生成（不受筛选条件影响）
     * 
     * 数据库模式下从数据库查询全量数据并正确填充packetData；
     * 内存模式下返回m_allRecords。
     * 报告生成必须使用此方法，确保分析结果基于完整数据。
     * 
     * @return 全量数据记录数组
     */
    QVector<DataRecord> getAllRecordsForReport() const;
    
    /**
     * @brief 获取筛选后的记录
     * @return 符合筛选条件的数据记录数组
     */
    QVector<DataRecord> getFilteredRecords() const;
    
    /**
     * @brief 根据数据范围获取记录
     * @param scope 数据范围
     * @return 对应范围的数据记录数组
     */
    QVector<DataRecord> getRecordsByScope(DataScope scope) const;
    
    /**
     * @brief 根据数据范围获取统计信息
     * @param scope 数据范围
     * @return 终端地址到数据量的映射
     */
    QMap<int, int> getTerminalStatisticsByScope(DataScope scope) const;
    
    /**
     * @brief 根据数据范围获取消息类型统计
     * @param scope 数据范围
     * @return 消息类型到数据量的映射
     */
    QMap<MessageType, int> getMessageTypeStatisticsByScope(DataScope scope) const;
    
    /**
     * @brief 设置数据范围
     * @param scope 数据范围
     */
    void setDataScope(DataScope scope);
    
    /**
     * @brief 获取数据范围
     * @return 当前数据范围
     */
    DataScope dataScope() const { return m_dataScope; }
    
    /**
     * @brief 设置筛选条件
     * 
     * 设置单个字段的筛选条件，会覆盖之前的同名筛选条件
     * 
     * @param field 字段名称
     * @param value 筛选值
     */
    void setFilter(const QString& field, const QVariant& value);
    
    /**
     * @brief 清除所有筛选条件
     * 
     * 清除所有自定义筛选、终端筛选、消息类型筛选和时间范围筛选
     */
    void clearFilter();
    
    /**
     * @brief 添加筛选条件
     * 
     * 添加一个筛选条件，不影响其他筛选条件
     * 
     * @param field 字段名称
     * @param value 筛选值
     */
    void addFilter(const QString& field, const QVariant& value);
    
    /**
     * @brief 移除筛选条件
     * @param field 要移除的字段名称
     */
    void removeFilter(const QString& field);
    
    /**
     * @brief 设置时间范围筛选
     * 
     * 只显示指定时间范围内的数据
     * 
     * @param startTime 起始时间戳
     * @param endTime 结束时间戳
     */
    void setTimeRange(quint32 startTime, quint32 endTime);
    
    /**
     * @brief 清除时间范围筛选
     */
    void clearTimeRange();
    
    /**
     * @brief 设置终端地址筛选
     * 
     * 只显示指定终端地址的数据
     * 
     * @param terminals 终端地址集合
     */
    void setTerminalFilter(const QSet<int>& terminals);
    
    /**
     * @brief 清除终端地址筛选
     */
    void clearTerminalFilter();
    
    /**
     * @brief 设置子地址筛选
     * 
     * 只显示指定子地址的数据
     * 
     * @param subAddresses 子地址集合
     */
    void setSubAddressFilter(const QSet<int>& subAddresses);
    
    /**
     * @brief 清除子地址筛选
     */
    void clearSubAddressFilter();
    
    /**
     * @brief 设置消息类型筛选
     * 
     * 只显示指定消息类型的数据
     * 
     * @param types 消息类型集合
     */
    void setMessageTypeFilter(const QSet<MessageType>& types);
    
    void clearMessageTypeFilter();
    
    void setChsttFilter(int chstt);
    
    void clearChsttFilter();
    
    int chsttFilter() const { return m_chsttFilter; }
    
    void setMpuIdFilter(int mpuId);
    
    void clearMpuIdFilter();
    
    int mpuIdFilter() const { return m_mpuIdFilter; }
    
    void setPacketLenFilter(int minLen, int maxLen);
    void clearPacketLenFilter();
    bool hasPacketLenFilter() const { return m_hasPacketLenFilter; }
    int packetLenMin() const { return m_packetLenMin; }
    int packetLenMax() const { return m_packetLenMax; }
    
    void setDateRangeFilter(int year, int month, int day);
    void setDateRangeFilter(int yearStart, int monthStart, int dayStart, int yearEnd, int monthEnd, int dayEnd);
    void clearDateRangeFilter();
    bool hasDateRangeFilter() const { return m_hasDateRangeFilter; }
    int dateYearStart() const { return m_dateYearStart; }
    int dateMonthStart() const { return m_dateMonthStart; }
    int dateDayStart() const { return m_dateDayStart; }
    int dateYearEnd() const { return m_dateYearEnd; }
    int dateMonthEnd() const { return m_dateMonthEnd; }
    int dateDayEnd() const { return m_dateDayEnd; }
    
    void setStatusBitFilter(int statusField, int bitPosition, bool bitValue);
    void clearStatusBitFilter();
    bool hasStatusBitFilter() const { return m_hasStatusBitFilter; }
    int statusBitField() const { return m_statusBitField; }
    int statusBitPosition() const { return m_statusBitPosition; }
    bool statusBitValue() const { return m_statusBitValue; }
    
    void setExcludeTerminalFilter(const QSet<int>& terminals);
    void clearExcludeTerminalFilter();
    QSet<int> excludeTerminalFilter() const { return m_excludeTerminalFilter; }
    
    void setExcludeMessageTypeFilter(const QSet<MessageType>& types);
    void clearExcludeMessageTypeFilter();
    QSet<MessageType> excludeMessageTypeFilter() const { return m_excludeMessageTypeFilter; }
    
    void setWordCountFilter(int minCount, int maxCount);
    void clearWordCountFilter();
    bool hasWordCountFilter() const { return m_hasWordCountFilter; }
    int wordCountMin() const { return m_wordCountMin; }
    int wordCountMax() const { return m_wordCountMax; }
    
    void setErrorFlagFilter(int errorFlag);
    void clearErrorFlagFilter();
    int errorFlagFilter() const { return m_errorFlagFilter; }
    
    /**
     * @brief 设置列算式筛选
     * 
     * 为指定列设置算式筛选条件
     * 
     * @param column 列索引
     * @param expression 筛选表达式（如">1;<3&&>10"）
     * @return 表达式是否有效
     */
    bool setColumnExpressionFilter(int column, const QString& expression);
    
    /**
     * @brief 清除列算式筛选
     * @param column 列索引，-1表示清除所有列的筛选
     */
    void clearColumnExpressionFilter(int column = -1);
    
    void beginBatchFilterUpdate();
    void endBatchFilterUpdate();
    
    /**
     * @brief 获取列算式筛选
     * @param column 列索引
     * @return 筛选表达式字符串
     */
    QString getColumnExpressionFilter(int column) const;
    
    /**
     * @brief 检查是否有列算式筛选
     * @return 是否有任意列设置了算式筛选
     */
    bool hasColumnExpressionFilters() const;
    
    /**
     * @brief 获取所有终端地址
     * @return 数据中出现过的所有终端地址集合
     */
    QSet<int> getAllTerminals() const;
    
    /**
     * @brief 获取所有子地址
     * @return 数据中出现过的所有子地址集合
     */
    QSet<int> getAllSubAddresses() const;
    
    /**
     * @brief 获取所有消息类型
     * @return 数据中出现过的所有消息类型集合
     */
    QSet<MessageType> getAllMessageTypes() const;
    
    /**
     * @brief 获取最小时间戳
     * @return 数据中的最小时间戳值
     */
    quint32 getMinTimestamp() const;
    
    /**
     * @brief 获取最小时间戳（毫秒）
     * @return 数据中的最小时间戳值（毫秒）
     */
    qint64 getMinTimestampMs() const;
    
    /**
     * @brief 获取最大时间戳
     * @return 数据中的最大时间戳值
     */
    quint32 getMaxTimestamp() const;
    
    /**
     * @brief 获取最大时间戳（毫秒）
     * @return 数据中的最大时间戳值（毫秒）
     */
    qint64 getMaxTimestampMs() const;
    
    /**
     * @brief 获取终端统计信息
     * 
     * 统计每个终端地址的数据量
     * 
     * @return 终端地址到数据量的映射
     */
    QMap<int, int> getTerminalStatistics() const;
    
    /**
     * @brief 获取消息类型统计信息
     * 
     * 统计每种消息类型的数据量
     * 
     * @return 消息类型到数据量的映射
     */
    QMap<MessageType, int> getMessageTypeStatistics() const;
    
    QMap<int, int> getSubAddressStatistics() const;
    
    /**
     * @brief 排序数据
     * 
     * 按指定字段对数据进行排序
     * 
     * @param field 排序字段
     * @param order 排序顺序（升序/降序）
     */
    void sort(const QString& field, Qt::SortOrder order);
    
    /**
     * @brief 构建数据索引（静态方法）
     * 
     * 从原始数据构建索引记录数组
     * 
     * @param data 原始消息数据
     * @param progressCallback 进度回调函数，参数为当前进度和总数量
     * @return 索引记录数组
     */
    static QVector<DataRecord> buildIndex(
        const QVector<PacketParser::SMbiMonPacketMsg>& data,
        std::function<void(int current, int total)> progressCallback = nullptr
    );
    
    /**
     * @brief 设置已索引的数据
     * 
     * 直接设置原始数据和预构建的索引，
     * 用于后台线程构建索引后快速加载
     * 
     * @param rawData 原始消息数据
     * @param index 预构建的索引
     */
    void setIndexedData(const QVector<PacketParser::SMbiMonPacketMsg>& rawData, const QVector<DataRecord>& index);
    
    // ========== 分页相关方法 ==========
    
    /**
     * @brief 设置每页显示数量
     * @param size 每页条数（默认100）
     */
    void setPageSize(int size);
    
    /**
     * @brief 获取每页显示数量
     * @return 每页条数
     */
    int pageSize() const { return m_pageSize; }
    
    /**
     * @brief 跳转到指定页
     * @param page 页码（从0开始）
     */
    void setCurrentPage(int page);
    
    /**
     * @brief 获取当前页码
     * @return 当前页码（从0开始）
     */
    int currentPage() const { return m_currentPage; }
    
    /**
     * @brief 获取总页数
     * @return 总页数
     */
    int totalPages() const;
    
    /**
     * @brief 获取当前页数据
     * @return 当前页的数据记录的常引用
     */
    const QVector<DataRecord>& getCurrentPageRecords() const;
    
    /**
     * @brief 获取筛选后的总记录数
     * @return 筛选后的记录总数
     */
    int filteredCount() const;
    
    /**
     * @brief 异步筛选（在后台线程执行）
     * 
     * 在后台线程中遍历所有数据应用筛选条件，
     * 完成后发送filterChanged信号
     */
    void applyFiltersAsync();
    
    /**
     * @brief 取消异步筛选
     */
    void cancelAsyncFilter();
    
signals:
    /**
     * @brief 数据变化信号
     * 
     * 当数据被重新加载或清空时发出
     */
    void dataChanged();
    
    /**
     * @brief 筛选条件变化信号
     * 
     * 当筛选条件被修改时发出
     */
    void filterChanged();
    
    /**
     * @brief 数据范围变化信号
     * 
     * 当数据范围（所有数据/筛选数据/当前页）切换时发出
     */
    void dataScopeChanged();
    
    /**
     * @brief 数据清空信号
     * 
     * 当数据被清空时发出
     */
    void dataCleared();
    
    /**
     * @brief 筛选进度信号
     * @param percent 进度百分比（0-100）
     * @param processed 已处理记录数
     * @param total 总记录数
     */
    void filterProgress(int percent, int processed, int total);
    
    /**
     * @brief 分页变化信号
     * @param currentPage 当前页码（从0开始）
     * @param totalPages 总页数
     * @param filteredCount 筛选后的记录总数
     */
    void pageChanged(int currentPage, int totalPages, int filteredCount);

private:
    /**
     * @brief 重建数据索引
     * 
     * 遍历所有原始数据，构建DataRecord数组，
     * 计算派生字段（如消息类型、时间戳等）
     */
    void rebuildIndex();
    
    struct FilterSnapshot {
        QSet<int> terminalFilter;
        QSet<int> subAddressFilter;
        QSet<MessageType> messageTypeFilter;
        bool hasTimeRange;
        quint32 startTime;
        quint32 endTime;
        int chsttFilter;
        int mpuIdFilter;
        bool hasPacketLenFilter;
        int packetLenMin;
        int packetLenMax;
        bool hasDateRangeFilter;
        int dateYearStart, dateMonthStart, dateDayStart;
        int dateYearEnd, dateMonthEnd, dateDayEnd;
        bool hasStatusBitFilter;
        int statusBitField;
        int statusBitPosition;
        bool statusBitValue;
        QSet<int> excludeTerminalFilter;
        QSet<MessageType> excludeMessageTypeFilter;
        bool hasWordCountFilter;
        int wordCountMin;
        int wordCountMax;
        int errorFlagFilter;
        QHash<int, FilterExpression> columnExpressionFilters;
    };
    
    FilterSnapshot captureFilterSnapshot() const;
    bool matchesFilterSnapshot(const DataRecord& record, const FilterSnapshot& snapshot) const;
    
    void applyFilters();
    
    bool matchesFilter(const DataRecord& record) const;
    
    /**
     * @brief 更新当前页数据缓存
     * 
     * 根据当前页码和每页条数，从筛选后的数据中提取当前页数据
     */
    void updateCurrentPageCache();
    
    /**
     * @brief 数据库模式+算式筛选时更新当前页数据缓存
     * 
     * 当数据库模式下存在算式筛选时，数据已加载到m_filteredRecords中，
     * 需要从m_filteredRecords中分页（类似内存模式），而非从数据库查询
     */
    void updateCurrentPageCacheForExpressionFilter();
    
    QVector<PacketParser::SMbiMonPacketMsg> m_rawData;      // 原始消息数据数组
    QVector<DataRecord> m_allRecords;         // 所有数据记录索引
    QVector<DataRecord> m_filteredRecords;    // 筛选后的数据记录
    
    QHash<QString, QVariant> m_filters;       // 自定义筛选条件
    QSet<int> m_terminalFilter;               // 终端地址筛选集合
    QSet<int> m_subAddressFilter;             // 子地址筛选集合
    QSet<MessageType> m_messageTypeFilter;    // 消息类型筛选集合
    int m_chsttFilter;                        // 收发状态筛选（-1=不过滤，0=失败，1=成功）
    int m_mpuIdFilter;                        // 任务机ID筛选（0=不过滤）
    bool m_hasTimeRange;                      // 是否有时间范围筛选
    quint32 m_startTime;                      // 时间范围起始值
    quint32 m_endTime;                        // 时间范围结束值
    
    bool m_hasPacketLenFilter;                // 是否有包长度筛选
    int m_packetLenMin;                       // 包长度最小值
    int m_packetLenMax;                       // 包长度最大值
    
    bool m_hasDateRangeFilter;                // 是否有日期范围筛选
    int m_dateYearStart;                      // 起始年份
    int m_dateMonthStart;                     // 起始月份
    int m_dateDayStart;                       // 起始日期
    int m_dateYearEnd;                        // 结束年份
    int m_dateMonthEnd;                       // 结束月份
    int m_dateDayEnd;                         // 结束日期
    
    bool m_hasStatusBitFilter;                // 是否有状态字位筛选
    int m_statusBitField;                     // 状态字字段（1或2）
    int m_statusBitPosition;                  // 位位置（0-15）
    bool m_statusBitValue;                    // 位值（true=1，false=0）
    
    QSet<int> m_excludeTerminalFilter;        // 排除终端地址集合
    QSet<MessageType> m_excludeMessageTypeFilter; // 排除消息类型集合
    
    bool m_hasWordCountFilter;                // 是否有数据计数筛选
    int m_wordCountMin;                       // 数据计数最小值
    int m_wordCountMax;                       // 数据计数最大值
    
    int m_errorFlagFilter;                    // 错误标志筛选（-1=不过滤，0=无错误，1=有错误）
    
    QString m_sortField;                      // 当前排序字段
    Qt::SortOrder m_sortOrder;                // 当前排序顺序
    
    QHash<int, FilterExpression> m_columnExpressionFilters;  // 列算式筛选
    
    // 分页相关
    int m_pageSize;                           // 每页条数
    int m_currentPage;                        // 当前页码（从0开始）
    QVector<DataRecord> m_currentPageRecords; // 当前页数据缓存
    
    // 异步筛选相关
    std::atomic<bool> m_cancelFilter;         // 取消筛选标志
    
    // 数据库模式相关
    StorageMode m_storageMode;                // 存储模式
    int m_autoSwitchThreshold;                // 自动切换阈值
    int m_currentFileId;                      // 当前文件ID（数据库模式）
    bool m_useDatabase;                       // 是否使用数据库后端
    qint64 m_filteredCountForDb;              // 数据库模式下的筛选后记录总数
    bool m_batchFilterUpdate;                 // 批量筛选更新标志
    DataScope m_dataScope;                    // 数据范围（所有数据/筛选数据/当前页）
    mutable PacketParser::SMbiMonPacketMsg m_cachedMessage; // 数据库模式下缓存的消息（用于getMessage）
    mutable int m_cachedMsgIndex;             // 缓存消息对应的msgIndex
    
    quint16 generateCmd1(int terminalAddr, int subAddr, int trBit, int wordCount) const;
    quint16 generateCmd2(int terminalAddr, int subAddr, int trBit, int wordCount) const;
    
    /**
     * @brief 从数据库记录重建packetData（数据库模式下m_rawData为空时使用）
     */
    void rebuildPacketDataFromDb(DataRecord& record, const DbDataRecord& dbRecord) const;
};

#endif
