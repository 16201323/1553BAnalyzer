/**
 * @file DatabaseManager.h
 * @brief 数据库管理器 - SQLite数据库优化版本
 * 
 * 本类实现了高性能的SQLite数据库管理，支持：
 * - 并行导入：多线程并行处理数据导入
 * - 批量插入：使用批量VALUES语法优化导入速度
 * - 列式存储：针对分析型查询优化存储结构
 * - 读写分离：主数据库写入，多个只读副本查询
 * - 连接池：管理数据库连接，提升并发性能
 * - 查询缓存：缓存常用查询结果
 * 
 * 性能目标：
 * - 20万条数据导入：10-30秒（原18分钟）
 * - 筛选查询：<100ms
 * - 统计图表生成：<200ms
 * - 内存占用：50-100MB（原2-3GB）
 * 
 * @author 1553BTools
 * @date 2024
 */

#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QHash>
#include <QVector>
#include <QSet>
#include <QMutex>
#include <QWaitCondition>
#include <atomic>
#include <functional>
#include "../parser/PacketStruct.h"
#include "DataStore.h"

/**
 * @brief 数据库记录结构（用于数据库查询结果）
 */
struct DbDataRecord {
    qint64 id;                  ///< 数据库主键ID
    int fileId;                 ///< 文件ID
    qint64 messageId;           ///< 消息ID
    int msgIndex;               ///< 消息索引（在原始数据数组中的位置）
    int dataIndex;              ///< 数据包索引
    int rowIndex;               ///< 全局行索引
    
    int terminalAddr;           ///< 终端地址
    int subAddr;                ///< 子地址
    int trBit;                  ///< 收发状态
    int modeCode;               ///< 模式码
    int wordCount;              ///< 数据字计数
    
    int terminalAddr2;          ///< 终端地址2
    int subAddr2;               ///< 子地址2
    int trBit2;                 ///< 收发状态2
    int modeCode2;              ///< 模式码2
    int wordCount2;             ///< 数据字计数2
    
    int status1;                ///< 状态字1
    int status2;                ///< 状态字2
    int chstt;                  ///< 获取状态
    
    quint32 packetTimestamp;    ///< 数据包时间戳（单位40us）
    double timestampMs;         ///< 时间戳（毫秒）
    
    QByteArray data;            ///< 数据内容
    int dataSize;               ///< 数据大小
    bool isCompressed;          ///< 是否压缩
    
    MessageType messageType;    ///< 消息类型
    bool errorFlag;             ///< 错误标志
    
    int mpuProduceId;           ///< 任务机标识
    int packetLen;              ///< 包总长度
    int headerYear;             ///< 年份
    int headerMonth;            ///< 月份
    int headerDay;              ///< 日期
    quint32 headerTimestamp;    ///< 包头时间戳（单位40us）
    
    DbDataRecord() : id(0), fileId(0), messageId(0), msgIndex(0), dataIndex(0), rowIndex(0),
                   terminalAddr(0), subAddr(0), trBit(0), modeCode(0), wordCount(0),
                   terminalAddr2(0), subAddr2(0), trBit2(0), modeCode2(0), wordCount2(0),
                   status1(0), status2(0), chstt(0), packetTimestamp(0), timestampMs(0.0),
                   dataSize(0), isCompressed(false), messageType(MessageType::Unknown),
                   errorFlag(false), mpuProduceId(0), packetLen(0),
                   headerYear(0), headerMonth(0), headerDay(0), headerTimestamp(0) {}
};

/**
 * @brief 甘特图数据结构
 */
struct GanttData {
    int terminalAddr;           ///< 终端地址
    qint64 startTime;           ///< 开始时间（毫秒）
    qint64 endTime;             ///< 结束时间（毫秒）
    int count;                  ///< 数据包数量
    MessageType messageType;    ///< 消息类型
    
    GanttData() : terminalAddr(0), startTime(0), endTime(0), count(0),
                  messageType(MessageType::Unknown) {}
};

/**
 * @brief 数据库管理器类
 * 
 * 单例模式，提供高性能的数据库访问接口
 */
class DatabaseManager : public QObject {
    Q_OBJECT
    
public:
    /**
     * @brief 获取单例实例
     */
    static DatabaseManager* instance();
    
    /**
     * @brief 初始化数据库
     * @param dbPath 数据库路径（空则使用默认路径）
     * @return 成功返回true
     * 
     * 自动检测数据库是否存在，不存在则创建
     * 自动创建所有表、索引、视图
     * 自动进行版本管理和迁移
     */
    bool initialize(const QString& dbPath = QString());
    
    /**
     * @brief 检查数据库是否已初始化
     */
    bool isInitialized() const { return m_initialized; }
    
    /**
     * @brief 获取数据库版本
     */
    int getDatabaseVersion();
    
    /**
     * @brief 升级数据库版本
     */
    bool upgradeDatabase(int targetVersion);
    
    // ========== 数据导入（并行+批量） ==========
    
    /**
     * @brief 并行导入数据
     * @param data 解析后的数据
     * @param filePath 源文件路径
     * @param progressCallback 进度回调
     * @return 成功返回文件ID，失败返回-1
     * 
     * 使用多线程并行处理
     * 使用批量VALUES插入
     * 自动禁用索引，导入后重建
     */
    int importDataParallel(
        const QVector<SMbiMonPacketMsg>& data,
        const QString& filePath,
        std::function<void(int current, int total, const QString& status)> progressCallback = nullptr
    );
    
    /**
     * @brief 取消正在进行的导入操作
     */
    void cancelImport();
    
    // ========== 数据查询（读写分离） ==========
    
    /**
     * @brief 查询数据包（带筛选）
     */
    QVector<DbDataRecord> queryPackets(
        int fileId,
        const QSet<int>& terminalFilter = QSet<int>(),
        const QSet<int>& subAddrFilter = QSet<int>(),
        const QSet<MessageType>& typeFilter = QSet<MessageType>(),
        qint64 startTime = 0,
        qint64 endTime = LLONG_MAX,
        int limit = -1,
        int offset = 0,
        int chsttFilter = -1,
        int mpuIdFilter = 0,
        const QSet<int>& excludeTerminalFilter = QSet<int>(),
        const QSet<MessageType>& excludeTypeFilter = QSet<MessageType>(),
        int packetLenMin = -1,
        int packetLenMax = -1,
        int dateYearStart = -1,
        int dateMonthStart = -1,
        int dateDayStart = -1,
        int dateYearEnd = -1,
        int dateMonthEnd = -1,
        int dateDayEnd = -1,
        int statusBitField = -1,
        int statusBitPosition = -1,
        int statusBitValue = -1,
        int wordCountMin = -1,
        int wordCountMax = -1,
        int errorFlagFilter = -1
    );
    
    /**
     * @brief 查询指定行的数据包
     */
    DbDataRecord queryPacketByRowIndex(int fileId, int rowIndex);
    
    QVector<DbDataRecord> queryPacketsByMsgIndex(int fileId, int msgIndex);
    
    /**
     * @brief 查询指定ID的数据包
     */
    DbDataRecord queryPacketById(qint64 packetId);
    
    /**
     * @brief 查询数据包总数（带筛选）
     */
    qint64 queryPacketCount(
        int fileId,
        const QSet<int>& terminalFilter = QSet<int>(),
        const QSet<int>& subAddrFilter = QSet<int>(),
        const QSet<MessageType>& typeFilter = QSet<MessageType>(),
        qint64 startTime = 0,
        qint64 endTime = LLONG_MAX,
        int chsttFilter = -1,
        int mpuIdFilter = 0,
        const QSet<int>& excludeTerminalFilter = QSet<int>(),
        const QSet<MessageType>& excludeTypeFilter = QSet<MessageType>(),
        int packetLenMin = -1,
        int packetLenMax = -1,
        int dateYearStart = -1,
        int dateMonthStart = -1,
        int dateDayStart = -1,
        int dateYearEnd = -1,
        int dateMonthEnd = -1,
        int dateDayEnd = -1,
        int statusBitField = -1,
        int statusBitPosition = -1,
        int statusBitValue = -1,
        int wordCountMin = -1,
        int wordCountMax = -1,
        int errorFlagFilter = -1
    );
    
    // ========== 列式查询（优化分析） ==========
    
    /**
     * @brief 查询终端地址分布（使用列式存储）
     */
    QHash<int, qint64> queryTerminalDistribution(int fileId);
    
    /**
     * @brief 查询消息类型分布（使用列式存储）
     */
    QHash<MessageType, qint64> queryMessageTypeDistribution(int fileId);
    
    /**
     * @brief 查询时间范围（使用列式存储）
     */
    QPair<qint64, qint64> queryTimeRange(int fileId);
    
    /**
     * @brief 查询子地址分布（使用列式存储）
     */
    QHash<int, qint64> querySubAddressDistribution(int fileId);
    
    /**
     * @brief 查询时间分布（使用列式存储）
     */
    QVector<QPair<qint64, qint64>> queryTimeDistribution(int fileId, int hours);
    
    // ========== 文件管理 ==========
    
    /**
     * @brief 获取所有文件列表
     */
    QVector<QVariantMap> getFileList();
    
    /**
     * @brief 切换当前活动文件
     */
    bool switchCurrentFile(int fileId);
    
    /**
     * @brief 获取当前活动文件ID
     */
    int getCurrentFileId();
    
    /**
     * @brief 删除文件及其数据
     */
    bool deleteFile(int fileId);
    
    /**
     * @brief 获取文件信息
     */
    QVariantMap getFileInfo(int fileId);
    
    /**
     * @brief 检查文件是否已导入
     */
    bool isFileImported(const QString& filePath);
    
    /**
     * @brief 根据文件路径获取文件ID
     * @return 文件ID，不存在返回-1
     */
    int getFileIdByPath(const QString& filePath);
    
    int getFileIdByFileName(const QString& fileName);
    
    /**
     * @brief 删除文件及其关联数据
     */
    bool deleteFileData(int fileId);
    
    // ========== 统计分析 ==========
    
    /**
     * @brief 获取数据包总数
     */
    qint64 getTotalPacketCount(int fileId = -1);
    
    /**
     * @brief 获取消息总数
     */
    qint64 getTotalMessageCount(int fileId = -1);
    
    /**
     * @brief 获取甘特图数据
     */
    QVector<GanttData> getGanttData(int fileId, qint64 startTime, qint64 endTime);
    
    /**
     * @brief 获取终端统计信息
     */
    QMap<int, int> getTerminalStatistics(int fileId, 
        const QSet<int>& terminalFilter,
        const QSet<int>& subAddrFilter,
        const QSet<MessageType>& typeFilter,
        qint64 startTime, qint64 endTime);
    
    /**
     * @brief 获取消息类型统计信息
     */
    QMap<MessageType, int> getMessageTypeStatistics(int fileId,
        const QSet<int>& terminalFilter,
        const QSet<int>& subAddrFilter,
        qint64 startTime, qint64 endTime);
    
    QMap<int, int> getSubAddressStatistics(int fileId,
        const QSet<int>& terminalFilter,
        const QSet<MessageType>& typeFilter,
        qint64 startTime, qint64 endTime);
    
    /**
     * @brief 获取所有终端地址
     */
    QSet<int> getAllTerminals(int fileId);
    
    /**
     * @brief 获取最小时间戳
     */
    qint64 getMinTimestamp(int fileId);
    
    /**
     * @brief 获取最大时间戳
     */
    qint64 getMaxTimestamp(int fileId);
    
    // ========== 数据导出 ==========
    
    /**
     * @brief 导出为CSV
     */
    bool exportToCSV(int fileId, const QString& filePath, const QString& whereClause = QString());
    
    /**
     * @brief 导出为Excel
     */
    bool exportToExcel(int fileId, const QString& filePath, const QString& whereClause = QString());
    
    /**
     * @brief 导出为JSON
     */
    bool exportToJSON(int fileId, const QString& filePath, const QString& whereClause = QString());
    
    // ========== 性能优化 ==========
    
    /**
     * @brief 优化数据库
     */
    void optimizeDatabase();
    
    /**
     * @brief 清理缓存
     */
    void clearCache();
    
    bool clearPacketsForFile(int fileId);
    
    /**
     * @brief 获取数据库统计信息
     */
    QVariantMap getDatabaseStats();
    
    /**
     * @brief 获取最后的错误信息
     */
    QString lastError() const { return m_lastError; }
    
signals:
    /**
     * @brief 导入进度信号
     */
    void importProgress(int current, int total, const QString& status);
    
    /**
     * @brief 导入完成信号
     */
    void importFinished(int fileId, bool success);
    
    /**
     * @brief 数据库错误信号
     */
    void databaseError(const QString& error);
    
    /**
     * @brief 文件切换信号
     */
    void fileSwitched(int fileId);
    
private:
    /**
     * @brief 私有构造函数（单例）
     */
    DatabaseManager(QObject* parent = nullptr);
    
    /**
     * @brief 禁止拷贝
     */
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;
    
    // ========== 数据库初始化 ==========
    
    /**
     * @brief 创建数据库表
     */
    bool createTables();
    
    /**
     * @brief 创建索引
     */
    bool createIndexes();
    
    /**
     * @brief 创建列式存储表
     */
    bool createColumnarTables();
    
    /**
     * @brief 创建视图
     */
    bool createViews();
    
    /**
     * @brief 初始化数据库版本
     */
    bool initDatabaseVersion();
    
    // ========== 并行导入相关 ==========
    
    /**
     * @brief 导入上下文结构
     */
    struct ImportContext {
        QString tempDbPath;                     ///< 临时数据库路径
        int startIdx;                           ///< 起始索引
        int endIdx;                             ///< 结束索引
        QVector<SMbiMonPacketMsg> data;         ///< 数据
        int fileId;                             ///< 文件ID
        int totalMessages;                      ///< 总消息数
    };
    
    /**
     * @brief 导入到临时数据库
     */
    void importToTempDb(ImportContext& ctx, QAtomicInt& processedCount, 
                        std::function<void(int, int, const QString&)> progressCallback,
                        int totalMessages);
    
    /**
     * @brief 合并临时数据库
     */
    void mergeTempDatabases(const QStringList& tempDbs, int fileId,
                            std::function<void(int, int, const QString&)> progressCallback = nullptr,
                            int totalMessages = 0);
    
    /**
     * @brief 批量插入数据包
     */
    void batchInsertPackets(QSqlDatabase& db, int fileId, const QVector<DbDataRecord>& records);
    
    /**
     * @brief 插入列式数据
     */
    void insertColumnarData(QSqlDatabase& db, int fileId, const QVector<DbDataRecord>& records);
    
    // ========== 读写分离 ==========
    
    /**
     * @brief 获取写连接
     */
    QSqlDatabase& getWriteConnection();
    
    /**
     * @brief 获取读连接
     */
    QSqlDatabase& getReadConnection();
    
    // ========== 缓存管理 ==========
    
    /**
     * @brief 获取缓存的统计数据
     */
    QVariant getCachedStatistics(int fileId, const QString& cacheType);
    
    /**
     * @brief 设置缓存的统计数据
     */
    void setCachedStatistics(int fileId, const QString& cacheType, const QVariant& data);
    
    // ========== 辅助方法 ==========
    
    /**
     * @brief 将SMbiMonPacketMsg转换为DbDataRecord列表
     */
    QVector<DbDataRecord> convertToRecords(const SMbiMonPacketMsg& msg, int fileId, int msgIndex, int& globalIndex);
    
    /**
     * @brief 计算文件哈希
     */
    QString calculateFileHash(const QString& filePath);
    
    /**
     * @brief 压缩数据
     */
    QByteArray compressData(const QByteArray& data);
    
    /**
     * @brief 解压数据
     */
    QByteArray decompressData(const QByteArray& compressedData);
    
    /**
     * @brief 执行SQL语句
     */
    bool executeSql(QSqlDatabase& db, const QString& sql);
    
    /**
     * @brief 记录错误
     */
    void logError(const QString& error);
    
private:
    static DatabaseManager* s_instance;         ///< 单例实例
    
    QString m_dbPath;                           ///< 数据库路径
    bool m_initialized;                         ///< 是否已初始化
    QString m_lastError;                        ///< 最后的错误信息
    
    // 读写分离
    QSqlDatabase m_writeDb;                     ///< 写数据库连接
    QVector<QSqlDatabase> m_readDbs;            ///< 读数据库连接池
    std::atomic<int> m_currentReadDb;           ///< 当前读数据库索引（原子操作）
    int m_readConnectionCount;                  ///< 读连接数量
    
    // 预编译语句缓存
    QHash<QString, QSqlQuery> m_statementCache; ///< 预编译语句缓存
    
    // 统计缓存
    QHash<QString, QVariant> m_statsCache;      ///< 统计数据缓存
    QMutex m_statsMutex;                        ///< 统计缓存互斥锁
    
    // 文件管理
    int m_currentFileId;                        ///< 当前文件ID
    QMutex m_fileMutex;                         ///< 文件管理互斥锁
    
    // 导入控制
    std::atomic<bool> m_importCanceled;         ///< 导入是否取消
    QMutex m_importMutex;                       ///< 导入互斥锁
};

#endif
