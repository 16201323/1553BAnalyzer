/**
 * @file DatabaseManager.cpp
 * @brief 数据库管理器实现
 * 
 * 实现了高性能的SQLite数据库管理功能
 * 
 * @author 1553BTools
 * @date 2024
 */

#include "DatabaseManager.h"
#include "utils/Logger.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QSqlField>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QCryptographicHash>
#include <QThread>
#include <QtConcurrent>
#include <QMutexLocker>
#include <QAtomicInt>
#include <QDebug>
#include <QCoreApplication>
#include <climits>

// 数据库版本号
constexpr int DATABASE_VERSION = 4;

// 批量插入大小
constexpr int BATCH_INSERT_SIZE = 1000;

// 读连接数量
constexpr int READ_CONNECTION_COUNT = 4;

DatabaseManager* DatabaseManager::s_instance = nullptr;

DatabaseManager* DatabaseManager::instance()
{
    if (!s_instance) {
        s_instance = new DatabaseManager();
    }
    return s_instance;
}

DatabaseManager::DatabaseManager(QObject* parent)
    : QObject(parent)
    , m_initialized(false)
    , m_currentReadDb(0)
    , m_readConnectionCount(READ_CONNECTION_COUNT)
    , m_currentFileId(-1)
    , m_importCanceled(false)
{
}

bool DatabaseManager::initialize(const QString& dbPath)
{
    if (m_initialized) {
        LOG_INFO("DatabaseManager", "[数据库] 已初始化");
        return true;
    }
    
    if (dbPath.isEmpty()) {
        QString dataPath = QCoreApplication::applicationDirPath() + "/data";
        QDir().mkpath(dataPath);
        m_dbPath = dataPath + "/1553b_data.db";
    } else {
        m_dbPath = dbPath;
    }
    
    LOG_INFO("DatabaseManager", QString("[数据库] 数据库路径: %1").arg(m_dbPath));
    LOG_INFO("DatabaseManager", QString("[数据库] 应用目录: %1").arg(QCoreApplication::applicationDirPath()));
    
    bool dbExists = QFile::exists(m_dbPath);
    LOG_INFO("DatabaseManager", QString("[数据库] 数据库文件是否存在: %1").arg(dbExists));
    
    m_writeDb = QSqlDatabase::addDatabase("QSQLITE", "write_conn");
    m_writeDb.setDatabaseName(m_dbPath);
    m_writeDb.setConnectOptions("QSQLITE_BUSY_TIMEOUT=5000");
    
    if (!m_writeDb.open()) {
        logError(tr("无法打开数据库: %1").arg(m_writeDb.lastError().text()));
        return false;
    }
    LOG_INFO("DatabaseManager", "[数据库] 写连接已打开");
    
    QSqlQuery query(m_writeDb);
    // SQLite性能优化PRAGMA设置
    // WAL模式：允许并发读写，读操作不阻塞写操作
    query.exec("PRAGMA journal_mode = WAL");
    // NORMAL同步模式：在关键点同步，平衡安全性和性能
    query.exec("PRAGMA synchronous = NORMAL");
    // 64MB缓存：负号表示KB单位（-64000 = 64MB）
    query.exec("PRAGMA cache_size = -64000");
    // 临时表存储在内存中，避免磁盘IO
    query.exec("PRAGMA temp_store = MEMORY");
    // 正常锁定模式：允许多进程访问
    query.exec("PRAGMA locking_mode = NORMAL");
    
    // 创建只读连接池：多个只读连接支持并发查询
    int readCreated = 0;
    for (int i = 0; i < m_readConnectionCount; ++i) {
        QString connName = QString("read_conn_%1").arg(i);
        QSqlDatabase readDb = QSqlDatabase::addDatabase("QSQLITE", connName);
        readDb.setDatabaseName(m_dbPath);
        // Qt6的QSQLITE驱动不支持QSQLITE_OPEN_READONLY选项
        // 通过query_mode=readonly实现只读（Qt6.2+支持）
        readDb.setConnectOptions("QSQLITE_BUSY_TIMEOUT=5000");
        
        if (!readDb.open()) {
            logError(tr("无法打开读数据库连接: %1").arg(readDb.lastError().text()));
            continue;
        }
        
        m_readDbs.append(readDb);
        readCreated++;
    }
    LOG_INFO("DatabaseManager", QString("[数据库] 读连接数: %1").arg(readCreated));
    
    if (!dbExists) {
        LOG_INFO("DatabaseManager", "[数据库] 创建新数据库表...");
        if (!createTables()) {
            logError(tr("创建数据库表失败"));
            return false;
        }
        LOG_INFO("DatabaseManager", "[数据库] 数据表已创建");
        
        if (!createColumnarTables()) {
            logError(tr("创建列式存储表失败"));
            return false;
        }
        LOG_INFO("DatabaseManager", "[数据库] 列式存储表已创建");
        
        if (!createIndexes()) {
            logError(tr("创建索引失败"));
            return false;
        }
        LOG_INFO("DatabaseManager", "[数据库] 索引已创建");
        
        if (!createViews()) {
            logError(tr("创建视图失败"));
            return false;
        }
        LOG_INFO("DatabaseManager", "[数据库] 视图已创建");
        
        if (!initDatabaseVersion()) {
            logError(tr("初始化数据库版本失败"));
            return false;
        }
        LOG_INFO("DatabaseManager", "[数据库] 版本已初始化");
    } else {
        int currentVersion = getDatabaseVersion();
        LOG_INFO("DatabaseManager", QString("[数据库] 现有数据库版本: %1").arg(currentVersion));
        if (currentVersion < DATABASE_VERSION) {
            if (!upgradeDatabase(DATABASE_VERSION)) {
                logError(tr("升级数据库失败"));
                return false;
            }
        }
    }
    
    m_initialized = true;
    
    {
        QDir dbDir(QFileInfo(m_dbPath).absolutePath());
        QStringList tempFiles = dbDir.entryList(QStringList() << "temp_*.db" << "temp_*.db-wal" << "temp_*.db-shm", QDir::Files);
        for (const QString& tempFile : tempFiles) {
            QString fullPath = dbDir.absoluteFilePath(tempFile);
            if (QFile::remove(fullPath)) {
                LOG_INFO("DatabaseManager", QString("[数据库] 清理残留临时文件: %1").arg(tempFile));
            }
        }
    }
    
    LOG_INFO("DatabaseManager", "数据库初始化成功");
    return true;
}

int DatabaseManager::getDatabaseVersion()
{
    QSqlQuery query(getReadConnection());
    query.exec("SELECT version FROM db_version WHERE id = 1");
    
    if (query.next()) {
        return query.value(0).toInt();
    }
    
    return 0;
}

bool DatabaseManager::upgradeDatabase(int targetVersion)
{
    QSqlDatabase& db = getWriteConnection();
    
    // 开始事务
    if (!db.transaction()) {
        logError(tr("开始事务失败: %1").arg(db.lastError().text()));
        return false;
    }
    
    // 获取当前版本
    int currentVersion = 0;
    QSqlQuery versionQuery(db);
    if (versionQuery.exec("SELECT version FROM db_version WHERE id = 1") && versionQuery.next()) {
        currentVersion = versionQuery.value(0).toInt();
    }
    
    LOG_INFO("DatabaseManager", QString("[数据库] 升级数据库 - 当前版本: %1, 目标版本: %2").arg(currentVersion).arg(targetVersion));
    
    // 版本1到版本2：添加msg_index列
    if (currentVersion < 2) {
        LOG_INFO("DatabaseManager", "[数据库] 执行升级脚本: 版本1 -> 版本2 (添加msg_index列)");
        
        QSqlQuery query(db);
        
        // 检查msg_index列是否已存在
        bool columnExists = false;
        if (query.exec("PRAGMA table_info(packets)")) {
            while (query.next()) {
                if (query.value(1).toString() == "msg_index") {
                    columnExists = true;
                    break;
                }
            }
        }
        
        if (!columnExists) {
            if (!query.exec("ALTER TABLE packets ADD COLUMN msg_index INTEGER NOT NULL DEFAULT 0")) {
                LOG_ERROR("DatabaseManager", QString("[数据库] 添加msg_index列失败: %1").arg(query.lastError().text()));
                db.rollback();
                return false;
            }
            LOG_INFO("DatabaseManager", "[数据库] msg_index列添加成功");
        } else {
            LOG_INFO("DatabaseManager", "[数据库] msg_index列已存在，跳过添加");
        }
    }
    
    if (currentVersion < 3) {
        LOG_INFO("DatabaseManager", "[数据库] 执行升级脚本: 版本2 -> 版本3 (添加packetHeader字段)");
        
        QSqlQuery query(db);
        
        QStringList columnsToAdd = {
            "ALTER TABLE packets ADD COLUMN mpu_produce_id INTEGER DEFAULT 0",
            "ALTER TABLE packets ADD COLUMN packet_len INTEGER DEFAULT 0",
            "ALTER TABLE packets ADD COLUMN header_year INTEGER DEFAULT 0",
            "ALTER TABLE packets ADD COLUMN header_month INTEGER DEFAULT 0",
            "ALTER TABLE packets ADD COLUMN header_day INTEGER DEFAULT 0"
        };
        
        for (const QString& sql : columnsToAdd) {
            QString colName = sql.split("ADD COLUMN ")[1].split(" ")[0];
            bool colExists = false;
            QSqlQuery checkQuery(db);
            if (checkQuery.exec("PRAGMA table_info(packets)")) {
                while (checkQuery.next()) {
                    if (checkQuery.value(1).toString() == colName) {
                        colExists = true;
                        break;
                    }
                }
            }
            if (!colExists) {
                if (!query.exec(sql)) {
                    LOG_ERROR("DatabaseManager", QString("[数据库] 添加%1列失败: %2").arg(colName).arg(query.lastError().text()));
                    db.rollback();
                    return false;
                }
                LOG_INFO("DatabaseManager", QString("[数据库] %1列添加成功").arg(colName));
            }
        }
    }
    
    if (currentVersion < 4) {
        LOG_INFO("DatabaseManager", "[数据库] 执行升级脚本: 版本3 -> 版本4 (添加header_timestamp列)");
        
        QSqlQuery query(db);
        
        bool colExists = false;
        QSqlQuery checkQuery(db);
        if (checkQuery.exec("PRAGMA table_info(packets)")) {
            while (checkQuery.next()) {
                if (checkQuery.value(1).toString() == "header_timestamp") {
                    colExists = true;
                    break;
                }
            }
        }
        
        if (!colExists) {
            if (!query.exec("ALTER TABLE packets ADD COLUMN header_timestamp INTEGER DEFAULT 0")) {
                LOG_ERROR("DatabaseManager", QString("[数据库] 添加header_timestamp列失败: %1").arg(query.lastError().text()));
                db.rollback();
                return false;
            }
            
            query.exec("UPDATE packets SET header_timestamp = packet_timestamp WHERE header_timestamp = 0");
            LOG_INFO("DatabaseManager", "[数据库] header_timestamp列添加成功");
        } else {
            LOG_INFO("DatabaseManager", "[数据库] header_timestamp列已存在，跳过添加");
        }
    }
    
    // 更新版本号
    QSqlQuery query(db);
    query.prepare("UPDATE db_version SET version = ?, update_time = ?");
    query.addBindValue(targetVersion);
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    
    if (!query.exec()) {
        db.rollback();
        logError(tr("更新版本号失败: %1").arg(query.lastError().text()));
        return false;
    }
    
    // 提交事务
    if (!db.commit()) {
        db.rollback();
        logError(tr("提交事务失败: %1").arg(db.lastError().text()));
        return false;
    }
    
    LOG_INFO("DatabaseManager", QString("[数据库] 数据库升级完成，版本: %1").arg(targetVersion));
    return true;
}

bool DatabaseManager::createTables()
{
    QSqlDatabase& db = getWriteConnection();
    
    // 创建文件管理表
    if (!executeSql(db, 
        "CREATE TABLE IF NOT EXISTS files ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "file_path TEXT NOT NULL UNIQUE, "
        "file_name TEXT NOT NULL, "
        "file_size INTEGER, "
        "file_hash TEXT, "
        "import_time DATETIME DEFAULT CURRENT_TIMESTAMP, "
        "message_count INTEGER DEFAULT 0, "
        "packet_count INTEGER DEFAULT 0, "
        "parse_duration INTEGER, "
        "is_valid BOOLEAN DEFAULT 1, "
        "is_current BOOLEAN DEFAULT 0, "
        "notes TEXT"
        ")")) {
        return false;
    }
    
    // 创建数据包主表
    if (!executeSql(db,
        "CREATE TABLE IF NOT EXISTS packets ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "file_id INTEGER NOT NULL, "
        "message_id INTEGER, "
        "msg_index INTEGER NOT NULL, "
        "data_index INTEGER NOT NULL, "
        "row_index INTEGER NOT NULL, "
        "terminal_addr INTEGER NOT NULL, "
        "sub_addr INTEGER NOT NULL, "
        "tr_bit INTEGER NOT NULL, "
        "mode_code INTEGER, "
        "word_count INTEGER, "
        "terminal_addr2 INTEGER, "
        "sub_addr2 INTEGER, "
        "tr_bit2 INTEGER, "
        "mode_code2 INTEGER, "
        "word_count2 INTEGER, "
        "status1 INTEGER, "
        "status2 INTEGER, "
        "chstt INTEGER, "
        "packet_timestamp INTEGER, "
        "timestamp_ms REAL, "
        "data BLOB, "
        "data_size INTEGER, "
        "is_compressed BOOLEAN DEFAULT 0, "
        "message_type INTEGER NOT NULL, "
        "error_flag BOOLEAN DEFAULT 0, "
        "mpu_produce_id INTEGER DEFAULT 0, "
        "packet_len INTEGER DEFAULT 0, "
        "header_year INTEGER DEFAULT 0, "
        "header_month INTEGER DEFAULT 0, "
        "header_day INTEGER DEFAULT 0, "
        "header_timestamp INTEGER DEFAULT 0, "
        "FOREIGN KEY (file_id) REFERENCES files(id) ON DELETE CASCADE"
        ")")) {
        return false;
    }
    
    // 创建数据库版本表
    if (!executeSql(db,
        "CREATE TABLE IF NOT EXISTS db_version ("
        "id INTEGER PRIMARY KEY CHECK (id = 1), "
        "version INTEGER NOT NULL, "
        "create_time DATETIME DEFAULT CURRENT_TIMESTAMP, "
        "update_time DATETIME DEFAULT CURRENT_TIMESTAMP"
        ")")) {
        return false;
    }
    
    // 创建统计缓存表
    if (!executeSql(db,
        "CREATE TABLE IF NOT EXISTS statistics_cache ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "file_id INTEGER NOT NULL, "
        "cache_type TEXT NOT NULL, "
        "cache_data TEXT NOT NULL, "
        "update_time DATETIME DEFAULT CURRENT_TIMESTAMP, "
        "ttl_seconds INTEGER DEFAULT 3600, "
        "FOREIGN KEY (file_id) REFERENCES files(id) ON DELETE CASCADE"
        ")")) {
        return false;
    }
    
    return true;
}

bool DatabaseManager::createColumnarTables()
{
    QSqlDatabase& db = getWriteConnection();
    
    // 创建终端地址列式存储表
    if (!executeSql(db,
        "CREATE TABLE IF NOT EXISTS packets_col_terminal ("
        "row_index INTEGER PRIMARY KEY, "
        "file_id INTEGER NOT NULL, "
        "terminal_addr INTEGER NOT NULL, "
        "sub_addr INTEGER NOT NULL, "
        "tr_bit INTEGER NOT NULL"
        ")")) {
        return false;
    }
    
    // 创建时间戳列式存储表
    if (!executeSql(db,
        "CREATE TABLE IF NOT EXISTS packets_col_timestamp ("
        "row_index INTEGER PRIMARY KEY, "
        "file_id INTEGER NOT NULL, "
        "timestamp_ms REAL NOT NULL, "
        "packet_timestamp INTEGER NOT NULL"
        ")")) {
        return false;
    }
    
    // 创建消息类型列式存储表
    if (!executeSql(db,
        "CREATE TABLE IF NOT EXISTS packets_col_type ("
        "row_index INTEGER PRIMARY KEY, "
        "file_id INTEGER NOT NULL, "
        "message_type INTEGER NOT NULL, "
        "error_flag BOOLEAN DEFAULT 0"
        ")")) {
        return false;
    }
    
    // 创建状态字列式存储表
    if (!executeSql(db,
        "CREATE TABLE IF NOT EXISTS packets_col_status ("
        "row_index INTEGER PRIMARY KEY, "
        "file_id INTEGER NOT NULL, "
        "status1 INTEGER, "
        "status2 INTEGER, "
        "chstt INTEGER"
        ")")) {
        return false;
    }
    
    return true;
}

bool DatabaseManager::createIndexes()
{
    QSqlDatabase& db = getWriteConnection();
    
    // 文件表索引
    executeSql(db, "CREATE INDEX IF NOT EXISTS idx_files_import_time ON files(import_time)");
    executeSql(db, "CREATE INDEX IF NOT EXISTS idx_files_is_current ON files(is_current)");
    
    // 数据包主表索引
    executeSql(db, "CREATE INDEX IF NOT EXISTS idx_packets_file_id ON packets(file_id)");
    executeSql(db, "CREATE INDEX IF NOT EXISTS idx_packets_row_index ON packets(row_index)");
    executeSql(db, "CREATE INDEX IF NOT EXISTS idx_packets_terminal_addr ON packets(terminal_addr)");
    executeSql(db, "CREATE INDEX IF NOT EXISTS idx_packets_sub_addr ON packets(sub_addr)");
    executeSql(db, "CREATE INDEX IF NOT EXISTS idx_packets_message_type ON packets(message_type)");
    executeSql(db, "CREATE INDEX IF NOT EXISTS idx_packets_timestamp ON packets(timestamp_ms)");
    executeSql(db, "CREATE INDEX IF NOT EXISTS idx_packets_mpu_produce_id ON packets(mpu_produce_id)");
    executeSql(db, "CREATE INDEX IF NOT EXISTS idx_packets_chstt ON packets(chstt)");
    
    executeSql(db,
        "CREATE INDEX IF NOT EXISTS idx_packets_filter ON packets("
        "file_id, terminal_addr, sub_addr, message_type, timestamp_ms"
        ")");
    
    executeSql(db,
        "CREATE INDEX IF NOT EXISTS idx_packets_filter_ext ON packets("
        "file_id, terminal_addr, sub_addr, message_type, chstt, mpu_produce_id, timestamp_ms"
        ")");
    
    executeSql(db,
        "CREATE INDEX IF NOT EXISTS idx_packets_file_row ON packets("
        "file_id, row_index"
        ")");
    
    // 列式存储表索引
    executeSql(db, "CREATE INDEX IF NOT EXISTS idx_col_terminal_file ON packets_col_terminal(file_id)");
    executeSql(db, "CREATE INDEX IF NOT EXISTS idx_col_terminal_addr ON packets_col_terminal(terminal_addr)");
    executeSql(db, "CREATE INDEX IF NOT EXISTS idx_col_timestamp_file ON packets_col_timestamp(file_id)");
    executeSql(db, "CREATE INDEX IF NOT EXISTS idx_col_timestamp_time ON packets_col_timestamp(timestamp_ms)");
    executeSql(db, "CREATE INDEX IF NOT EXISTS idx_col_type_file ON packets_col_type(file_id)");
    executeSql(db, "CREATE INDEX IF NOT EXISTS idx_col_type_type ON packets_col_type(message_type)");
    executeSql(db, "CREATE INDEX IF NOT EXISTS idx_col_status_file ON packets_col_status(file_id)");
    
    // 统计缓存表索引
    executeSql(db, "CREATE INDEX IF NOT EXISTS idx_stats_cache ON statistics_cache(file_id, cache_type)");
    
    return true;
}

bool DatabaseManager::createViews()
{
    // 创建视图（如果需要）
    return true;
}

bool DatabaseManager::initDatabaseVersion()
{
    QSqlQuery query(getWriteConnection());
    query.prepare("INSERT INTO db_version (id, version) VALUES (1, ?)");
    query.addBindValue(DATABASE_VERSION);
    
    if (!query.exec()) {
        logError(tr("初始化数据库版本失败: %1").arg(query.lastError().text()));
        return false;
    }
    
    return true;
}

// ========== 数据导入（并行+批量） ==========

void DatabaseManager::cancelImport()
{
    m_importCanceled.store(true);
    LOG_INFO("DatabaseManager", "[数据库] 导入取消已请求");
}

int DatabaseManager::importDataParallel(
    const QVector<SMbiMonPacketMsg>& data,
    const QString& filePath,
    std::function<void(int current, int total, const QString& status)> progressCallback)
{
    LOG_INFO("DatabaseManager", QString("[数据库] 开始并行导入, 数据量: %1, 文件路径: %2, 已初始化: %3")
             .arg(data.size()).arg(filePath).arg(m_initialized));
    
    m_importCanceled.store(false);
    
    if (!m_initialized) {
        logError(tr("数据库未初始化"));
        return -1;
    }
    
    int existingFileId = getFileIdByPath(filePath);
    if (existingFileId > 0) {
        qint64 packetCount = getTotalPacketCount(existingFileId);
        qint64 messageCount = getTotalMessageCount(existingFileId);
        LOG_INFO("DatabaseManager", QString("[数据库] 文件已存在, 文件ID: %1, 消息数: %2, 数据包数: %3")
                 .arg(existingFileId).arg(messageCount).arg(packetCount));
        
        if (messageCount == data.size() && packetCount > 0) {
            LOG_INFO("DatabaseManager", QString("[数据库] 数据完整, 返回现有文件ID: %1").arg(existingFileId));
            return existingFileId;
        }
        
        LOG_INFO("DatabaseManager", QString("[数据库] 数据不完整(预期消息: %1, 实际消息: %2), 删除旧数据重新导入")
                 .arg(data.size()).arg(messageCount));
        deleteFileData(existingFileId);
    }
    
    QElapsedTimer timer;
    timer.start();
    
    QFileInfo fileInfo(filePath);
    QString fileHash = calculateFileHash(filePath);
    LOG_INFO("DatabaseManager", QString("[数据库] 文件信息 - 名称: %1, 大小: %2, 哈希: %3")
             .arg(fileInfo.fileName()).arg(fileInfo.size()).arg(fileHash));
    
    QSqlDatabase& db = getWriteConnection();
    if (!db.transaction()) {
        logError(tr("开始事务失败: %1").arg(db.lastError().text()));
        return -1;
    }
    LOG_INFO("DatabaseManager", "[数据库] 事务已开始");
    
    QSqlQuery query(db);
    query.prepare(
        "INSERT INTO files (file_path, file_name, file_size, file_hash, is_current) "
        "VALUES (?, ?, ?, ?, 1)"
    );
    query.addBindValue(filePath);
    query.addBindValue(fileInfo.fileName());
    query.addBindValue(fileInfo.size());
    query.addBindValue(fileHash);
    
    if (!query.exec()) {
        db.rollback();
        logError(tr("插入文件记录失败: %1").arg(query.lastError().text()));
        return -1;
    }
    
    int fileId = query.lastInsertId().toInt();
    LOG_INFO("DatabaseManager", QString("[数据库] 文件记录已插入, 文件ID: %1").arg(fileId));
    
    {
        QSqlQuery updateQuery(db);
        updateQuery.prepare("UPDATE files SET is_current = 0 WHERE id != ?");
        updateQuery.addBindValue(fileId);
        if (!updateQuery.exec()) {
            LOG_ERROR("DatabaseManager", QString("[数据库] 更新is_current失败: %1").arg(updateQuery.lastError().text()));
        }
    }
    
    // ===== 并行导入策略 =====
    // 将数据按CPU核心数分块，每个线程写入独立的临时SQLite数据库
    // 导入完成后，将临时数据库合并到主数据库
    // 这种方式避免了多线程写入同一数据库的锁竞争问题
    
    int threadCount = QThread::idealThreadCount();
    int totalMessages = data.size();
    int chunkSize = totalMessages / threadCount;
    LOG_INFO("DatabaseManager", QString("[数据库] 导入参数 - 线程数: %1, 消息总数: %2, 分块大小: %3")
             .arg(threadCount).arg(totalMessages).arg(chunkSize));
    
    if (progressCallback) {
        progressCallback(0, totalMessages, tr("准备并行导入..."));
    }
    
    // 创建每个线程的导入上下文：分配数据范围和临时数据库路径
    QStringList tempDbs;
    QVector<ImportContext> contexts;
    
    for (int i = 0; i < threadCount; ++i) {
        ImportContext ctx;
        ctx.tempDbPath = QString("%1/temp_%2_%3.db")
                            .arg(QFileInfo(m_dbPath).absolutePath())
                            .arg(fileId)
                            .arg(i);
        ctx.startIdx = i * chunkSize;
        ctx.endIdx = (i == threadCount - 1) ? totalMessages : (i + 1) * chunkSize;
        ctx.fileId = fileId;
        ctx.totalMessages = totalMessages;
        
        for (int j = ctx.startIdx; j < ctx.endIdx; ++j) {
            ctx.data.append(data[j]);
        }
        
        LOG_INFO("DatabaseManager", QString("[数据库] 上下文 %1 - 起始索引: %2, 结束索引: %3, 数据量: %4, 临时库: %5")
                 .arg(i).arg(ctx.startIdx).arg(ctx.endIdx).arg(ctx.data.size()).arg(ctx.tempDbPath));
        
        contexts.append(ctx);
        tempDbs.append(ctx.tempDbPath);
    }
    
    // 使用原子计数器跟踪各线程的导入进度
    QAtomicInt processedCount(0);
    
    // 启动并行导入：每个线程处理一个数据块，写入各自的临时数据库
    QVector<QFuture<void>> futures;
    for (int i = 0; i < threadCount; ++i) {
        futures.append(QtConcurrent::run([this, &contexts, i, &processedCount, progressCallback, totalMessages]() {
            importToTempDb(contexts[i], processedCount, progressCallback, totalMessages);
        }));
    }
    
    // 等待所有导入线程完成
    for (auto& future : futures) {
        future.waitForFinished();
    }
    LOG_INFO("DatabaseManager", "[数据库] 所有导入线程已完成");
    
    // 导入取消处理：清理临时数据库文件并回滚事务
    if (m_importCanceled.load()) {
        for (const QString& tempDb : tempDbs) {
            for (int retry = 0; retry < 5; ++retry) {
                if (QFile::remove(tempDb) || !QFile::exists(tempDb)) break;
                QThread::msleep(100);
            }
        }
        db.rollback();
        m_importCanceled.store(false);
        LOG_INFO("DatabaseManager", "[数据库] 导入已取消");
        return -1;
    }
    
    if (progressCallback) {
        // 并行导入已完成，进度达到80%，开始合并阶段
        progressCallback(totalMessages * 8 / 10, totalMessages, tr("合并数据..."));
    }
    
    if (!db.commit()) {
        db.rollback();
        logError(tr("提交事务失败: %1").arg(db.lastError().text()));
        return -1;
    }
    LOG_INFO("DatabaseManager", "[数据库] 事务已提交，开始合并临时数据库...");
    
    // 合并临时数据库到主数据库
    mergeTempDatabases(tempDbs, fileId, progressCallback, totalMessages);
    LOG_INFO("DatabaseManager", "[数据库] 合并完成");
    
    // 清理临时数据库文件（带重试机制，避免文件被锁时删除失败）
    for (const QString& tempDb : tempDbs) {
        if (QFile::exists(tempDb)) {
            bool removed = false;
            for (int retry = 0; retry < 5; ++retry) {
                if (QFile::remove(tempDb)) {
                    removed = true;
                    break;
                }
                QThread::msleep(100);
            }
            if (!removed) {
                LOG_WARNING("DatabaseManager", QString("[数据库] 无法删除临时数据库文件: %1").arg(tempDb));
            }
        }
    }
    
    qint64 duration = timer.elapsed();
    
    clearCache();
    
    qint64 messageCount = getTotalMessageCount(fileId);
    qint64 packetCount = getTotalPacketCount(fileId);
    LOG_INFO("DatabaseManager", QString("[数据库] 合并后 - 消息数: %1, 数据包数: %2, 耗时: %3 毫秒")
             .arg(messageCount).arg(packetCount).arg(duration));
    
    QSqlQuery updateQuery(db);
    updateQuery.prepare(
        "UPDATE files SET message_count = ?, packet_count = ?, parse_duration = ? "
        "WHERE id = ?"
    );
    updateQuery.addBindValue(messageCount);
    updateQuery.addBindValue(packetCount);
    updateQuery.addBindValue(duration);
    updateQuery.addBindValue(fileId);
    if (!updateQuery.exec()) {
        LOG_ERROR("DatabaseManager", QString("[数据库] 更新文件统计失败: %1").arg(updateQuery.lastError().text()));
    }
    
    // 更新当前文件ID
    {
        QMutexLocker locker(&m_fileMutex);
        m_currentFileId = fileId;
    }
    
    // 清理缓存
    clearCache();
    
    // 优化数据库
    optimizeDatabase();
    
    LOG_INFO("DatabaseManager", QString("导入完成: %1 个数据包, 耗时 %2 毫秒").arg(packetCount).arg(duration));
    
    emit importFinished(fileId, true);
    
    return fileId;
}

void DatabaseManager::importToTempDb(ImportContext& ctx, QAtomicInt& processedCount, 
                                      std::function<void(int, int, const QString&)> progressCallback,
                                      int totalMessages)
{
    LOG_INFO("DatabaseManager", QString("[数据库] 开始导入临时库 - 文件ID: %1, 起始索引: %2, 结束索引: %3, 数据量: %4")
             .arg(ctx.fileId).arg(ctx.startIdx).arg(ctx.endIdx).arg(ctx.data.size()));
    
    QString connectionName = QString("temp_conn_%1_%2").arg(ctx.fileId).arg(ctx.startIdx);
    
    {
        QSqlDatabase tempDb = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        tempDb.setDatabaseName(ctx.tempDbPath);
        
        if (!tempDb.open()) {
            logError(tr("无法打开临时数据库: %1").arg(tempDb.lastError().text()));
            LOG_ERROR("DatabaseManager", QString("[数据库] 打开临时库失败: %1, 错误: %2")
                     .arg(ctx.tempDbPath).arg(tempDb.lastError().text()));
            return;
        }
        LOG_INFO("DatabaseManager", QString("[数据库] 临时库已打开: %1").arg(ctx.tempDbPath));
        
        {
            QSqlQuery query(tempDb);
            query.exec("PRAGMA journal_mode = OFF");
            query.exec("PRAGMA synchronous = OFF");
            query.exec("PRAGMA locking_mode = EXCLUSIVE");
            query.exec("PRAGMA cache_size = -64000");
            
            query.exec(
                "CREATE TABLE packets ("
                "id INTEGER, "
                "file_id INTEGER NOT NULL, "
                "message_id INTEGER, "
                "msg_index INTEGER NOT NULL, "
                "data_index INTEGER NOT NULL, "
                "row_index INTEGER NOT NULL, "
                "terminal_addr INTEGER NOT NULL, "
                "sub_addr INTEGER NOT NULL, "
                "tr_bit INTEGER NOT NULL, "
                "mode_code INTEGER, "
                "word_count INTEGER, "
                "terminal_addr2 INTEGER, "
                "sub_addr2 INTEGER, "
                "tr_bit2 INTEGER, "
                "mode_code2 INTEGER, "
                "word_count2 INTEGER, "
                "status1 INTEGER, "
                "status2 INTEGER, "
                "chstt INTEGER, "
                "packet_timestamp INTEGER, "
                "timestamp_ms REAL, "
                "data BLOB, "
                "data_size INTEGER, "
                "is_compressed BOOLEAN DEFAULT 0, "
                "message_type INTEGER NOT NULL, "
                "error_flag BOOLEAN DEFAULT 0, "
                "mpu_produce_id INTEGER DEFAULT 0, "
                "packet_len INTEGER DEFAULT 0, "
                "header_year INTEGER DEFAULT 0, "
                "header_month INTEGER DEFAULT 0, "
                "header_day INTEGER DEFAULT 0, "
                "header_timestamp INTEGER DEFAULT 0"
                ")"
            );
            
            query.exec(
                "CREATE TABLE packets_col_terminal ("
                "row_index INTEGER NOT NULL, "
                "file_id INTEGER NOT NULL, "
                "terminal_addr INTEGER NOT NULL, "
                "sub_addr INTEGER NOT NULL, "
                "tr_bit INTEGER NOT NULL"
                ")"
            );
            
            query.exec(
                "CREATE TABLE packets_col_timestamp ("
                "row_index INTEGER NOT NULL, "
                "file_id INTEGER NOT NULL, "
                "timestamp_ms REAL NOT NULL, "
                "packet_timestamp INTEGER NOT NULL"
                ")"
            );
            
            query.exec(
                "CREATE TABLE packets_col_type ("
                "row_index INTEGER NOT NULL, "
                "file_id INTEGER NOT NULL, "
                "message_type INTEGER NOT NULL, "
                "error_flag BOOLEAN DEFAULT 0"
                ")"
            );
        }
        
        if (!tempDb.transaction()) {
            LOG_ERROR("DatabaseManager", QString("[数据库] 开始临时库事务失败: %1").arg(tempDb.lastError().text()));
            tempDb.close();
            return;
        }
        
        int globalIndex = 0;
        QVector<DbDataRecord> records;
        records.reserve(BATCH_INSERT_SIZE);
        int totalRecordsInserted = 0;
        
        for (int i = 0; i < ctx.data.size(); ++i) {
            if (m_importCanceled.load()) {
                LOG_INFO("DatabaseManager", QString("[数据库] 临时库导入被取消 - 文件ID: %1, 已插入: %2")
                         .arg(ctx.fileId).arg(totalRecordsInserted));
                tempDb.rollback();
                tempDb.close();
                QSqlDatabase::removeDatabase(connectionName);
                return;
            }
            
            const SMbiMonPacketMsg& msg = ctx.data[i];
            int actualMsgIndex = ctx.startIdx + i;  // 计算在原始数据数组中的实际索引
            QVector<DbDataRecord> msgRecords = convertToRecords(msg, ctx.fileId, actualMsgIndex, globalIndex);
            
            LOG_DEBUG("DatabaseManager", QString("[数据库] 消息 %1 - actualMsgIndex: %2, 数据包数: %3")
                     .arg(i).arg(actualMsgIndex).arg(msgRecords.size()));
            
            for (const DbDataRecord& record : msgRecords) {
                records.append(record);
                
                if (records.size() >= BATCH_INSERT_SIZE) {
                    batchInsertPackets(tempDb, ctx.fileId, records);
                    insertColumnarData(tempDb, ctx.fileId, records);
                    totalRecordsInserted += records.size();
                    records.clear();
                }
            }
            
            if (progressCallback && (i % 100 == 0)) {
                int current = processedCount.fetchAndAddRelaxed(100) + 100;
                // 并行导入阶段进度上限为80%，避免合并阶段进度倒退
                int cappedCurrent = qMin(current, totalMessages * 8 / 10);
                progressCallback(cappedCurrent, totalMessages, tr("正在导入数据..."));
            } else {
                processedCount.fetchAndAddRelaxed(1);
            }
        }
        
        if (!records.isEmpty()) {
            batchInsertPackets(tempDb, ctx.fileId, records);
            insertColumnarData(tempDb, ctx.fileId, records);
            totalRecordsInserted += records.size();
        }
        
        if (!tempDb.commit()) {
            LOG_ERROR("DatabaseManager", QString("[数据库] 提交临时库失败: %1").arg(tempDb.lastError().text()));
        }
        
        LOG_INFO("DatabaseManager", QString("[数据库] 临时库导入完成 - 文件ID: %1, 插入记录数: %2")
                 .arg(ctx.fileId).arg(totalRecordsInserted));
        
        tempDb.close();
    }
    
    QSqlDatabase::removeDatabase(connectionName);
}

void DatabaseManager::mergeTempDatabases(const QStringList& tempDbs, int fileId,
                                          std::function<void(int, int, const QString&)> progressCallback,
                                          int totalMessages)
{
    QSqlDatabase& db = getWriteConnection();
    
    qint64 currentMaxRowIndex = 0;
    {
        QSqlQuery maxQuery(db);
        if (maxQuery.exec("SELECT COALESCE(MAX(row_index), -1) + 1 FROM packets WHERE file_id = ?")) {
            maxQuery.addBindValue(fileId);
            if (maxQuery.next()) {
                currentMaxRowIndex = maxQuery.value(0).toLongLong();
            }
        }
        qint64 colMax = 0;
        if (maxQuery.exec("SELECT COALESCE(MAX(row_index), -1) + 1 FROM packets_col_terminal")) {
            if (maxQuery.next()) {
                colMax = maxQuery.value(0).toLongLong();
            }
        }
        if (colMax > currentMaxRowIndex) {
            currentMaxRowIndex = colMax;
        }
        if (maxQuery.exec("SELECT COALESCE(MAX(row_index), -1) + 1 FROM packets_col_timestamp")) {
            if (maxQuery.next()) {
                colMax = maxQuery.value(0).toLongLong();
            }
        }
        if (colMax > currentMaxRowIndex) {
            currentMaxRowIndex = colMax;
        }
        if (maxQuery.exec("SELECT COALESCE(MAX(row_index), -1) + 1 FROM packets_col_type")) {
            if (maxQuery.next()) {
                colMax = maxQuery.value(0).toLongLong();
            }
        }
        if (colMax > currentMaxRowIndex) {
            currentMaxRowIndex = colMax;
        }
        if (maxQuery.exec("SELECT COALESCE(MAX(row_index), -1) + 1 FROM packets_col_status")) {
            if (maxQuery.next()) {
                colMax = maxQuery.value(0).toLongLong();
            }
        }
        if (colMax > currentMaxRowIndex) {
            currentMaxRowIndex = colMax;
        }
    }
    LOG_INFO("DatabaseManager", QString("[数据库] 新数据起始row_index: %1").arg(currentMaxRowIndex));
    
    for (int i = 0; i < tempDbs.size(); ++i) {
        const QString& tempDb = tempDbs[i];
        QString alias = QString("temp_%1").arg(i);
        
        LOG_INFO("DatabaseManager", QString("[数据库] 合并临时库: %1, 别名: %2").arg(tempDb).arg(alias));
        
        if (!QFile::exists(tempDb)) {
            LOG_ERROR("DatabaseManager", QString("[数据库] 临时库文件不存在: %1").arg(tempDb));
            continue;
        }
        
        qint64 tempRowCount = 0;
        bool mergeSuccess = false;
        
        {
            QSqlQuery query(db);
            if (!query.exec(QString("ATTACH DATABASE '%1' AS %2").arg(tempDb).arg(alias))) {
                LOG_ERROR("DatabaseManager", QString("[数据库] ATTACH临时库失败: %1").arg(query.lastError().text()));
                continue;
            }
            LOG_INFO("DatabaseManager", "[数据库] ATTACH成功");
            
            if (query.exec(QString("SELECT COUNT(*) FROM %1.packets").arg(alias)) && query.next()) {
                tempRowCount = query.value(0).toLongLong();
            }
            LOG_INFO("DatabaseManager", QString("[数据库] 临时库记录数: %1").arg(tempRowCount));
            
            if (tempRowCount == 0) {
                LOG_WARNING("DatabaseManager", QString("[数据库] 临时库无数据，跳过: %1").arg(tempDb));
                query.exec(QString("DETACH DATABASE %1").arg(alias));
                continue;
            }
            
            if (!db.transaction()) {
                LOG_ERROR("DatabaseManager", QString("[数据库] 开始合并事务失败: %1").arg(db.lastError().text()));
                query.exec(QString("DETACH DATABASE %1").arg(alias));
                continue;
            }
            
            bool hasError = false;
            
            QString insertPacketsSql = QString(
                "INSERT INTO main.packets ("
                "file_id, message_id, msg_index, data_index, row_index, "
                "terminal_addr, sub_addr, tr_bit, mode_code, word_count, "
                "terminal_addr2, sub_addr2, tr_bit2, mode_code2, word_count2, "
                "status1, status2, chstt, packet_timestamp, timestamp_ms, "
                "data, data_size, is_compressed, message_type, error_flag, "
                "mpu_produce_id, packet_len, header_year, header_month, header_day, header_timestamp"
                ") SELECT "
                "file_id, message_id, msg_index, data_index, row_index + %1, "
                "terminal_addr, sub_addr, tr_bit, mode_code, word_count, "
                "terminal_addr2, sub_addr2, tr_bit2, mode_code2, word_count2, "
                "status1, status2, chstt, packet_timestamp, timestamp_ms, "
                "data, data_size, is_compressed, message_type, error_flag, "
                "mpu_produce_id, packet_len, header_year, header_month, header_day, header_timestamp "
                "FROM %2.packets ORDER BY row_index"
            ).arg(currentMaxRowIndex).arg(alias);
            
            if (!query.exec(insertPacketsSql)) {
                LOG_ERROR("DatabaseManager", QString("[数据库] 合并packets失败: %1").arg(query.lastError().text()));
                hasError = true;
            } else {
                LOG_INFO("DatabaseManager", QString("[数据库] 合并packets成功, 影响行数: %1").arg(query.numRowsAffected()));
            }
            
            if (!hasError) {
                QString insertColTerminalSql = QString(
                    "INSERT INTO main.packets_col_terminal ("
                    "row_index, file_id, terminal_addr, sub_addr, tr_bit"
                    ") SELECT "
                    "row_index + %1, file_id, terminal_addr, sub_addr, tr_bit "
                    "FROM %2.packets_col_terminal ORDER BY row_index"
                ).arg(currentMaxRowIndex).arg(alias);
                
                if (!query.exec(insertColTerminalSql)) {
                    LOG_ERROR("DatabaseManager", QString("[数据库] 合并col_terminal失败: %1").arg(query.lastError().text()));
                    hasError = true;
                } else {
                    LOG_INFO("DatabaseManager", QString("[数据库] 合并col_terminal成功, 影响行数: %1").arg(query.numRowsAffected()));
                }
            }
            
            if (!hasError) {
                QString insertColTimestampSql = QString(
                    "INSERT INTO main.packets_col_timestamp ("
                    "row_index, file_id, timestamp_ms, packet_timestamp"
                    ") SELECT "
                    "row_index + %1, file_id, timestamp_ms, packet_timestamp "
                    "FROM %2.packets_col_timestamp ORDER BY row_index"
                ).arg(currentMaxRowIndex).arg(alias);
                
                if (!query.exec(insertColTimestampSql)) {
                    LOG_ERROR("DatabaseManager", QString("[数据库] 合并col_timestamp失败: %1").arg(query.lastError().text()));
                    hasError = true;
                } else {
                    LOG_INFO("DatabaseManager", QString("[数据库] 合并col_timestamp成功, 影响行数: %1").arg(query.numRowsAffected()));
                }
            }
            
            if (!hasError) {
                QString insertColTypeSql = QString(
                    "INSERT INTO main.packets_col_type ("
                    "row_index, file_id, message_type, error_flag"
                    ") SELECT "
                    "row_index + %1, file_id, message_type, error_flag "
                    "FROM %2.packets_col_type ORDER BY row_index"
                ).arg(currentMaxRowIndex).arg(alias);
                
                if (!query.exec(insertColTypeSql)) {
                    LOG_ERROR("DatabaseManager", QString("[数据库] 合并col_type失败: %1").arg(query.lastError().text()));
                    hasError = true;
                } else {
                    LOG_INFO("DatabaseManager", QString("[数据库] 合并col_type成功, 影响行数: %1").arg(query.numRowsAffected()));
                }
            }
            
            if (!hasError) {
                QString insertColStatusSql = QString(
                    "INSERT INTO main.packets_col_status ("
                    "row_index, file_id, status1, status2, chstt"
                    ") SELECT "
                    "row_index + %1, file_id, status1, status2, chstt "
                    "FROM %2.packets ORDER BY row_index"
                ).arg(currentMaxRowIndex).arg(alias);
                
                if (!query.exec(insertColStatusSql)) {
                    LOG_ERROR("DatabaseManager", QString("[数据库] 合并col_status失败: %1").arg(query.lastError().text()));
                    hasError = true;
                } else {
                    LOG_INFO("DatabaseManager", QString("[数据库] 合并col_status成功, 影响行数: %1").arg(query.numRowsAffected()));
                }
            }
            
            query.finish();
            query.clear();
            
            if (hasError) {
                LOG_ERROR("DatabaseManager", QString("[数据库] 合并过程中出现错误，回滚事务 (临时库 %1)").arg(i));
                db.rollback();
            } else {
                if (!db.commit()) {
                    LOG_ERROR("DatabaseManager", QString("[数据库] 合并事务提交失败: %1").arg(db.lastError().text()));
                    db.rollback();
                } else {
                    LOG_INFO("DatabaseManager", QString("[数据库] 合并事务已提交 (临时库 %1)").arg(i));
                    mergeSuccess = true;
                }
            }
        }
        
        {
            QSqlQuery detachQuery(db);
            if (!detachQuery.exec(QString("DETACH DATABASE %1").arg(alias))) {
                LOG_ERROR("DatabaseManager", QString("[数据库] DETACH临时库失败: %1").arg(detachQuery.lastError().text()));
            } else {
                LOG_INFO("DatabaseManager", QString("[数据库] DETACH成功: %1").arg(alias));
            }
        }
        
        if (!mergeSuccess) {
            continue;
        }
        
        currentMaxRowIndex += tempRowCount;
        
        if (progressCallback && totalMessages > 0) {
            int baseProgress = totalMessages * 8 / 10;
            int mergeProgress = (i + 1) * totalMessages / 5 / tempDbs.size();
            progressCallback(baseProgress + mergeProgress, totalMessages, tr("合并数据 (%1/%2)...").arg(i + 1).arg(tempDbs.size()));
        }
    }
}

/**
 * @brief 批量插入数据包记录
 *
 * 使用单条INSERT语句插入多行数据（bulk insert），比逐条插入快10-100倍。
 * 每条记录31个字段，使用占位符(?)绑定参数值。
 *
 * @param db 目标数据库连接
 * @param fileId 文件ID
 * @param records 待插入的数据包记录列表
 */
void DatabaseManager::batchInsertPackets(QSqlDatabase& db, int fileId, const QVector<DbDataRecord>& records)
{
    if (records.isEmpty()) {
        return;
    }
    
    QString sql = "INSERT INTO packets ("
                  "file_id, message_id, msg_index, data_index, row_index, "
                  "terminal_addr, sub_addr, tr_bit, mode_code, word_count, "
                  "terminal_addr2, sub_addr2, tr_bit2, mode_code2, word_count2, "
                  "status1, status2, chstt, packet_timestamp, timestamp_ms, "
                  "data, data_size, is_compressed, message_type, error_flag, "
                  "mpu_produce_id, packet_len, header_year, header_month, header_day, header_timestamp"
                  ") VALUES ";
    
    QStringList placeholders;
    for (int i = 0; i < records.size(); ++i) {
        placeholders << "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
    }
    
    sql += placeholders.join(", ");
    
    QSqlQuery query(db);
    query.prepare(sql);
    
    for (const DbDataRecord& record : records) {
        query.addBindValue(record.fileId);
        query.addBindValue(record.messageId);
        query.addBindValue(record.msgIndex);
        query.addBindValue(record.dataIndex);
        query.addBindValue(record.rowIndex);
        
        query.addBindValue(record.terminalAddr);
        query.addBindValue(record.subAddr);
        query.addBindValue(record.trBit);
        query.addBindValue(record.modeCode);
        query.addBindValue(record.wordCount);
        
        query.addBindValue(record.terminalAddr2);
        query.addBindValue(record.subAddr2);
        query.addBindValue(record.trBit2);
        query.addBindValue(record.modeCode2);
        query.addBindValue(record.wordCount2);
        
        query.addBindValue(record.status1);
        query.addBindValue(record.status2);
        query.addBindValue(record.chstt);
        query.addBindValue(record.packetTimestamp);
        query.addBindValue(record.timestampMs);
        
        query.addBindValue(record.data);
        query.addBindValue(record.dataSize);
        query.addBindValue(record.isCompressed);
        query.addBindValue(static_cast<int>(record.messageType));
        query.addBindValue(record.errorFlag);
        
        query.addBindValue(record.mpuProduceId);
        query.addBindValue(record.packetLen);
        query.addBindValue(record.headerYear);
        query.addBindValue(record.headerMonth);
        query.addBindValue(record.headerDay);
        query.addBindValue(record.headerTimestamp);
    }
    
    if (!query.exec()) {
        logError(tr("批量插入数据失败: %1").arg(query.lastError().text()));
        LOG_ERROR("DatabaseManager", QString("[数据库] 批量插入失败 - 记录数: %1, 错误: %2, 连接: %3, 是否打开: %4")
                 .arg(records.size()).arg(query.lastError().text()).arg(db.connectionName()).arg(db.isOpen()));
    }
}

/**
 * @brief 插入列式存储数据
 *
 * 列式存储将高频查询字段（终端地址、时间戳、消息类型）拆分到独立表中，
 * 查询时只需扫描相关列，减少IO量，提升统计查询性能。
 *
 * 三张列式表：
 * - packets_col_terminal: 终端地址、子地址、TR位（用于终端统计筛选）
 * - packets_col_timestamp: 时间戳（用于时间范围筛选）
 * - packets_col_type: 消息类型、错误标志（用于消息类型筛选）
 *
 * @param db 目标数据库连接
 * @param fileId 文件ID
 * @param records 待插入的数据包记录列表
 */
void DatabaseManager::insertColumnarData(QSqlDatabase& db, int fileId, const QVector<DbDataRecord>& records)
{
    if (records.isEmpty()) {
        return;
    }
    
    // 插入终端地址列式数据
    QString sqlTerminal = "INSERT INTO packets_col_terminal "
                          "(row_index, file_id, terminal_addr, sub_addr, tr_bit) VALUES ";
    
    // 插入时间戳列式数据
    QString sqlTimestamp = "INSERT INTO packets_col_timestamp "
                           "(row_index, file_id, timestamp_ms, packet_timestamp) VALUES ";
    
    // 插入消息类型列式数据
    QString sqlType = "INSERT INTO packets_col_type "
                      "(row_index, file_id, message_type, error_flag) VALUES ";
    
    QStringList placeholdersTerminal;
    QStringList placeholdersTimestamp;
    QStringList placeholdersType;
    
    for (int i = 0; i < records.size(); ++i) {
        placeholdersTerminal << "(?, ?, ?, ?, ?)";
        placeholdersTimestamp << "(?, ?, ?, ?)";
        placeholdersType << "(?, ?, ?, ?)";
    }
    
    sqlTerminal += placeholdersTerminal.join(", ");
    sqlTimestamp += placeholdersTimestamp.join(", ");
    sqlType += placeholdersType.join(", ");
    
    // 插入终端地址数据
    {
        QSqlQuery query(db);
        query.prepare(sqlTerminal);
        
        for (const DbDataRecord& record : records) {
            query.addBindValue(record.rowIndex);
            query.addBindValue(record.fileId);
            query.addBindValue(record.terminalAddr);
            query.addBindValue(record.subAddr);
            query.addBindValue(record.trBit);
        }
        
        if (!query.exec()) {
            logError(tr("插入终端地址列式数据失败: %1").arg(query.lastError().text()));
        }
    }
    
    // 插入时间戳数据
    {
        QSqlQuery query(db);
        query.prepare(sqlTimestamp);
        
        for (const DbDataRecord& record : records) {
            query.addBindValue(record.rowIndex);
            query.addBindValue(record.fileId);
            query.addBindValue(record.timestampMs);
            query.addBindValue(record.packetTimestamp);
        }
        
        if (!query.exec()) {
            logError(tr("插入时间戳列式数据失败: %1").arg(query.lastError().text()));
        }
    }
    
    // 插入消息类型数据
    {
        QSqlQuery query(db);
        query.prepare(sqlType);
        
        for (const DbDataRecord& record : records) {
            query.addBindValue(record.rowIndex);
            query.addBindValue(record.fileId);
            query.addBindValue(static_cast<int>(record.messageType));
            query.addBindValue(record.errorFlag);
        }
        
        if (!query.exec()) {
            logError(tr("插入消息类型列式数据失败: %1").arg(query.lastError().text()));
        }
    }
}

// ========== 数据查询（读写分离） ==========

QVector<DbDataRecord> DatabaseManager::queryPackets(
    int fileId,
    const QSet<int>& terminalFilter,
    const QSet<int>& subAddrFilter,
    const QSet<MessageType>& typeFilter,
    qint64 startTime,
    qint64 endTime,
    int limit,
    int offset,
    int chsttFilter,
    int mpuIdFilter,
    const QSet<int>& excludeTerminalFilter,
    const QSet<MessageType>& excludeTypeFilter,
    int packetLenMin,
    int packetLenMax,
    int dateYearStart,
    int dateMonthStart,
    int dateDayStart,
    int dateYearEnd,
    int dateMonthEnd,
    int dateDayEnd,
    int statusBitField,
    int statusBitPosition,
    int statusBitValue,
    int wordCountMin,
    int wordCountMax,
    int errorFlagFilter)
{
    QVector<DbDataRecord> records;
    
    QSqlDatabase& db = getReadConnection();
    
    QString sql = "SELECT * FROM packets WHERE file_id = ?";
    
    if (!terminalFilter.isEmpty()) {
        sql += " AND terminal_addr IN (";
        QStringList placeholders;
        for (int i = 0; i < terminalFilter.size(); ++i) {
            placeholders << "?";
        }
        sql += placeholders.join(", ") + ")";
    }
    
    if (!subAddrFilter.isEmpty()) {
        sql += " AND sub_addr IN (";
        QStringList placeholders;
        for (int i = 0; i < subAddrFilter.size(); ++i) {
            placeholders << "?";
        }
        sql += placeholders.join(", ") + ")";
    }
    
    if (!typeFilter.isEmpty()) {
        sql += " AND message_type IN (";
        QStringList placeholders;
        for (int i = 0; i < typeFilter.size(); ++i) {
            placeholders << "?";
        }
        sql += placeholders.join(", ") + ")";
    }
    
    if (chsttFilter >= 0) {
        sql += " AND chstt = ?";
    }
    
    if (mpuIdFilter > 0) {
        sql += " AND mpu_produce_id = ?";
    }
    
    if (startTime > 0) {
        sql += " AND timestamp_ms >= ?";
    }
    
    if (endTime < LLONG_MAX) {
        sql += " AND timestamp_ms <= ?";
    }
    
    if (!excludeTerminalFilter.isEmpty()) {
        sql += " AND terminal_addr NOT IN (";
        QStringList placeholders;
        for (int i = 0; i < excludeTerminalFilter.size(); ++i) {
            placeholders << "?";
        }
        sql += placeholders.join(", ") + ")";
    }
    
    if (!excludeTypeFilter.isEmpty()) {
        sql += " AND message_type NOT IN (";
        QStringList placeholders;
        for (int i = 0; i < excludeTypeFilter.size(); ++i) {
            placeholders << "?";
        }
        sql += placeholders.join(", ") + ")";
    }
    
    if (packetLenMin >= 0) {
        sql += " AND packet_len >= ?";
    }
    if (packetLenMax >= 0) {
        sql += " AND packet_len <= ?";
    }
    
    if (dateYearStart >= 0) {
        sql += " AND (header_year > ? OR (header_year = ? AND header_month > ?) OR (header_year = ? AND header_month = ? AND header_day >= ?))";
    }
    if (dateYearEnd >= 0) {
        sql += " AND (header_year < ? OR (header_year = ? AND header_month < ?) OR (header_year = ? AND header_month = ? AND header_day <= ?))";
    }
    
    if (statusBitField >= 1 && statusBitPosition >= 0 && statusBitPosition <= 15 && statusBitValue >= 0) {
        if (statusBitField == 1) {
            sql += QString(" AND (status1 & %1) %2 0").arg(1 << statusBitPosition).arg(statusBitValue ? "!=" : "=");
        } else {
            sql += QString(" AND (status2 & %1) %2 0").arg(1 << statusBitPosition).arg(statusBitValue ? "!=" : "=");
        }
    }
    
    if (wordCountMin >= 0) {
        sql += " AND word_count >= ?";
    }
    if (wordCountMax >= 0) {
        sql += " AND word_count <= ?";
    }
    
    if (errorFlagFilter >= 0) {
        sql += QString(" AND error_flag = %1").arg(errorFlagFilter);
    }
    
    sql += " ORDER BY row_index";
    
    if (limit > 0) {
        sql += QString(" LIMIT %1 OFFSET %2").arg(limit).arg(offset);
    }
    
    QSqlQuery query(db);
    query.prepare(sql);
    
    query.addBindValue(fileId);
    
    for (int terminal : terminalFilter) {
        query.addBindValue(terminal);
    }
    
    for (int subAddr : subAddrFilter) {
        query.addBindValue(subAddr);
    }
    
    for (MessageType type : typeFilter) {
        query.addBindValue(static_cast<int>(type));
    }
    
    if (chsttFilter >= 0) {
        query.addBindValue(chsttFilter);
    }
    
    if (mpuIdFilter > 0) {
        query.addBindValue(mpuIdFilter);
    }
    
    if (startTime > 0) {
        query.addBindValue(startTime);
    }
    
    if (endTime < LLONG_MAX) {
        query.addBindValue(endTime);
    }
    
    for (int terminal : excludeTerminalFilter) {
        query.addBindValue(terminal);
    }
    
    for (MessageType type : excludeTypeFilter) {
        query.addBindValue(static_cast<int>(type));
    }
    
    if (packetLenMin >= 0) {
        query.addBindValue(packetLenMin);
    }
    if (packetLenMax >= 0) {
        query.addBindValue(packetLenMax);
    }
    
    if (dateYearStart >= 0) {
        query.addBindValue(dateYearStart);
        query.addBindValue(dateYearStart);
        query.addBindValue(dateMonthStart);
        query.addBindValue(dateYearStart);
        query.addBindValue(dateMonthStart);
        query.addBindValue(dateDayStart);
    }
    if (dateYearEnd >= 0) {
        query.addBindValue(dateYearEnd);
        query.addBindValue(dateYearEnd);
        query.addBindValue(dateMonthEnd);
        query.addBindValue(dateYearEnd);
        query.addBindValue(dateMonthEnd);
        query.addBindValue(dateDayEnd);
    }
    
    if (wordCountMin >= 0) {
        query.addBindValue(wordCountMin);
    }
    if (wordCountMax >= 0) {
        query.addBindValue(wordCountMax);
    }
    
    // 执行查询
    if (!query.exec()) {
        logError(tr("查询数据包失败: %1").arg(query.lastError().text()));
        return records;
    }
    
    // 处理结果
    while (query.next()) {
        DbDataRecord record;
        record.id = query.value("id").toLongLong();
        record.fileId = query.value("file_id").toInt();
        record.messageId = query.value("message_id").toLongLong();
        record.msgIndex = query.value("msg_index").toInt();
        record.dataIndex = query.value("data_index").toInt();
        record.rowIndex = query.value("row_index").toInt();
        
        record.terminalAddr = query.value("terminal_addr").toInt();
        record.subAddr = query.value("sub_addr").toInt();
        record.trBit = query.value("tr_bit").toInt();
        record.modeCode = query.value("mode_code").toInt();
        record.wordCount = query.value("word_count").toInt();
        
        record.terminalAddr2 = query.value("terminal_addr2").toInt();
        record.subAddr2 = query.value("sub_addr2").toInt();
        record.trBit2 = query.value("tr_bit2").toInt();
        record.modeCode2 = query.value("mode_code2").toInt();
        record.wordCount2 = query.value("word_count2").toInt();
        
        record.status1 = query.value("status1").toInt();
        record.status2 = query.value("status2").toInt();
        record.chstt = query.value("chstt").toInt();
        
        record.packetTimestamp = query.value("packet_timestamp").toUInt();
        record.timestampMs = query.value("timestamp_ms").toDouble();
        
        record.data = query.value("data").toByteArray();
        record.dataSize = query.value("data_size").toInt();
        record.isCompressed = query.value("is_compressed").toBool();
        
        record.messageType = static_cast<MessageType>(query.value("message_type").toInt());
        record.errorFlag = query.value("error_flag").toBool();
        
        record.mpuProduceId = query.value("mpu_produce_id").toInt();
        record.packetLen = query.value("packet_len").toInt();
        record.headerYear = query.value("header_year").toInt();
        record.headerMonth = query.value("header_month").toInt();
        record.headerDay = query.value("header_day").toInt();
        record.headerTimestamp = query.value("header_timestamp").toUInt();
        
        // 解压数据
        if (record.isCompressed && !record.data.isEmpty()) {
            record.data = decompressData(record.data);
        }
        
        records.append(record);
    }
    
    return records;
}

DbDataRecord DatabaseManager::queryPacketByRowIndex(int fileId, int rowIndex)
{
    QSqlDatabase& db = getReadConnection();
    
    QSqlQuery query(db);
    query.prepare("SELECT * FROM packets WHERE file_id = ? AND row_index = ?");
    query.addBindValue(fileId);
    query.addBindValue(rowIndex);
    
    if (query.exec() && query.next()) {
        DbDataRecord record;
        record.id = query.value("id").toLongLong();
        record.fileId = query.value("file_id").toInt();
        record.messageId = query.value("message_id").toLongLong();
        record.msgIndex = query.value("msg_index").toInt();
        record.dataIndex = query.value("data_index").toInt();
        record.rowIndex = query.value("row_index").toInt();
        
        record.terminalAddr = query.value("terminal_addr").toInt();
        record.subAddr = query.value("sub_addr").toInt();
        record.trBit = query.value("tr_bit").toInt();
        record.modeCode = query.value("mode_code").toInt();
        record.wordCount = query.value("word_count").toInt();
        
        record.terminalAddr2 = query.value("terminal_addr2").toInt();
        record.subAddr2 = query.value("sub_addr2").toInt();
        record.trBit2 = query.value("tr_bit2").toInt();
        record.modeCode2 = query.value("mode_code2").toInt();
        record.wordCount2 = query.value("word_count2").toInt();
        
        record.status1 = query.value("status1").toInt();
        record.status2 = query.value("status2").toInt();
        record.chstt = query.value("chstt").toInt();
        
        record.packetTimestamp = query.value("packet_timestamp").toUInt();
        record.timestampMs = query.value("timestamp_ms").toDouble();
        
        record.data = query.value("data").toByteArray();
        record.dataSize = query.value("data_size").toInt();
        record.isCompressed = query.value("is_compressed").toBool();
        
        record.messageType = static_cast<MessageType>(query.value("message_type").toInt());
        record.errorFlag = query.value("error_flag").toBool();
        
        record.mpuProduceId = query.value("mpu_produce_id").toInt();
        record.packetLen = query.value("packet_len").toInt();
        record.headerYear = query.value("header_year").toInt();
        record.headerMonth = query.value("header_month").toInt();
        record.headerDay = query.value("header_day").toInt();
        record.headerTimestamp = query.value("header_timestamp").toUInt();
        
        if (record.isCompressed && !record.data.isEmpty()) {
            record.data = decompressData(record.data);
        }
        
        return record;
    }
    
    return DbDataRecord();
}

QVector<DbDataRecord> DatabaseManager::queryPacketsByMsgIndex(int fileId, int msgIndex)
{
    QVector<DbDataRecord> records;
    
    QSqlDatabase& db = getReadConnection();
    
    // 按msg_index查询同一消息的所有数据包，按data_index排序保证数据包顺序正确
    QSqlQuery query(db);
    query.prepare("SELECT * FROM packets WHERE file_id = ? AND msg_index = ? ORDER BY data_index");
    query.addBindValue(fileId);
    query.addBindValue(msgIndex);
    
    if (!query.exec()) {
        logError(tr("按msgIndex查询数据包失败: %1").arg(query.lastError().text()));
        LOG_ERROR("DatabaseManager", QString("[queryPacketsByMsgIndex] 查询失败, fileId: %1, msgIndex: %2, 错误: %3")
            .arg(fileId).arg(msgIndex).arg(query.lastError().text()));
        return records;
    }
    
    // 逐条解析查询结果，填充DbDataRecord各字段
    while (query.next()) {
        DbDataRecord record;
        record.id = query.value("id").toLongLong();
        record.fileId = query.value("file_id").toInt();
        record.messageId = query.value("message_id").toLongLong();
        record.msgIndex = query.value("msg_index").toInt();
        record.dataIndex = query.value("data_index").toInt();
        record.rowIndex = query.value("row_index").toInt();
        
        // 命令字1相关字段
        record.terminalAddr = query.value("terminal_addr").toInt();
        record.subAddr = query.value("sub_addr").toInt();
        record.trBit = query.value("tr_bit").toInt();
        record.modeCode = query.value("mode_code").toInt();
        record.wordCount = query.value("word_count").toInt();
        
        // 命令字2相关字段（RT→RT消息使用）
        record.terminalAddr2 = query.value("terminal_addr2").toInt();
        record.subAddr2 = query.value("sub_addr2").toInt();
        record.trBit2 = query.value("tr_bit2").toInt();
        record.modeCode2 = query.value("mode_code2").toInt();
        record.wordCount2 = query.value("word_count2").toInt();
        
        // 状态字与通道状态
        record.status1 = query.value("status1").toInt();
        record.status2 = query.value("status2").toInt();
        record.chstt = query.value("chstt").toInt();
        
        // 时间戳
        record.packetTimestamp = query.value("packet_timestamp").toUInt();
        record.timestampMs = query.value("timestamp_ms").toDouble();
        
        // 数据内容（可能压缩存储）
        record.data = query.value("data").toByteArray();
        record.dataSize = query.value("data_size").toInt();
        record.isCompressed = query.value("is_compressed").toBool();
        
        // 消息类型与错误标志
        record.messageType = static_cast<MessageType>(query.value("message_type").toInt());
        record.errorFlag = query.value("error_flag").toBool();
        
        // 包头信息（同一消息的所有数据包共享）
        record.mpuProduceId = query.value("mpu_produce_id").toInt();
        record.packetLen = query.value("packet_len").toInt();
        record.headerYear = query.value("header_year").toInt();
        record.headerMonth = query.value("header_month").toInt();
        record.headerDay = query.value("header_day").toInt();
        record.headerTimestamp = query.value("header_timestamp").toUInt();
        
        // 解压数据内容
        if (record.isCompressed && !record.data.isEmpty()) {
            record.data = decompressData(record.data);
        }
        
        records.append(record);
    }
    
    LOG_INFO("DatabaseManager", QString("[queryPacketsByMsgIndex] 查询完成, fileId: %1, msgIndex: %2, 返回记录数: %3")
        .arg(fileId).arg(msgIndex).arg(records.size()));
    
    return records;
}

DbDataRecord DatabaseManager::queryPacketById(qint64 packetId)
{
    QSqlDatabase& db = getReadConnection();
    
    QSqlQuery query(db);
    query.prepare("SELECT * FROM packets WHERE id = ?");
    query.addBindValue(packetId);
    
    if (query.exec() && query.next()) {
        DbDataRecord record;
        record.id = query.value("id").toLongLong();
        record.fileId = query.value("file_id").toInt();
        record.messageId = query.value("message_id").toLongLong();
        record.msgIndex = query.value("msg_index").toInt();
        record.dataIndex = query.value("data_index").toInt();
        record.rowIndex = query.value("row_index").toInt();
        
        record.terminalAddr = query.value("terminal_addr").toInt();
        record.subAddr = query.value("sub_addr").toInt();
        record.trBit = query.value("tr_bit").toInt();
        record.modeCode = query.value("mode_code").toInt();
        record.wordCount = query.value("word_count").toInt();
        
        record.terminalAddr2 = query.value("terminal_addr2").toInt();
        record.subAddr2 = query.value("sub_addr2").toInt();
        record.trBit2 = query.value("tr_bit2").toInt();
        record.modeCode2 = query.value("mode_code2").toInt();
        record.wordCount2 = query.value("word_count2").toInt();
        
        record.status1 = query.value("status1").toInt();
        record.status2 = query.value("status2").toInt();
        record.chstt = query.value("chstt").toInt();
        
        record.packetTimestamp = query.value("packet_timestamp").toUInt();
        record.timestampMs = query.value("timestamp_ms").toDouble();
        
        record.data = query.value("data").toByteArray();
        record.dataSize = query.value("data_size").toInt();
        record.isCompressed = query.value("is_compressed").toBool();
        
        record.messageType = static_cast<MessageType>(query.value("message_type").toInt());
        record.errorFlag = query.value("error_flag").toBool();
        
        record.mpuProduceId = query.value("mpu_produce_id").toInt();
        record.packetLen = query.value("packet_len").toInt();
        record.headerYear = query.value("header_year").toInt();
        record.headerMonth = query.value("header_month").toInt();
        record.headerDay = query.value("header_day").toInt();
        record.headerTimestamp = query.value("header_timestamp").toUInt();
        
        if (record.isCompressed && !record.data.isEmpty()) {
            record.data = decompressData(record.data);
        }
        
        return record;
    }
    
    return DbDataRecord();
}

qint64 DatabaseManager::queryPacketCount(
    int fileId,
    const QSet<int>& terminalFilter,
    const QSet<int>& subAddrFilter,
    const QSet<MessageType>& typeFilter,
    qint64 startTime,
    qint64 endTime,
    int chsttFilter,
    int mpuIdFilter,
    const QSet<int>& excludeTerminalFilter,
    const QSet<MessageType>& excludeTypeFilter,
    int packetLenMin,
    int packetLenMax,
    int dateYearStart,
    int dateMonthStart,
    int dateDayStart,
    int dateYearEnd,
    int dateMonthEnd,
    int dateDayEnd,
    int statusBitField,
    int statusBitPosition,
    int statusBitValue,
    int wordCountMin,
    int wordCountMax,
    int errorFlagFilter)
{
    QSqlDatabase& db = getReadConnection();
    
    QString sql = "SELECT COUNT(*) FROM packets WHERE file_id = ?";
    
    if (!terminalFilter.isEmpty()) {
        sql += " AND terminal_addr IN (";
        QStringList placeholders;
        for (int i = 0; i < terminalFilter.size(); ++i) {
            placeholders << "?";
        }
        sql += placeholders.join(", ") + ")";
    }
    
    if (!subAddrFilter.isEmpty()) {
        sql += " AND sub_addr IN (";
        QStringList placeholders;
        for (int i = 0; i < subAddrFilter.size(); ++i) {
            placeholders << "?";
        }
        sql += placeholders.join(", ") + ")";
    }
    
    if (!typeFilter.isEmpty()) {
        sql += " AND message_type IN (";
        QStringList placeholders;
        for (int i = 0; i < typeFilter.size(); ++i) {
            placeholders << "?";
        }
        sql += placeholders.join(", ") + ")";
    }
    
    if (chsttFilter >= 0) {
        sql += " AND chstt = ?";
    }
    
    if (mpuIdFilter > 0) {
        sql += " AND mpu_produce_id = ?";
    }
    
    if (startTime > 0) {
        sql += " AND timestamp_ms >= ?";
    }
    
    if (endTime < LLONG_MAX) {
        sql += " AND timestamp_ms <= ?";
    }
    
    if (!excludeTerminalFilter.isEmpty()) {
        sql += " AND terminal_addr NOT IN (";
        QStringList placeholders;
        for (int i = 0; i < excludeTerminalFilter.size(); ++i) {
            placeholders << "?";
        }
        sql += placeholders.join(", ") + ")";
    }
    
    if (!excludeTypeFilter.isEmpty()) {
        sql += " AND message_type NOT IN (";
        QStringList placeholders;
        for (int i = 0; i < excludeTypeFilter.size(); ++i) {
            placeholders << "?";
        }
        sql += placeholders.join(", ") + ")";
    }
    
    if (packetLenMin >= 0) {
        sql += " AND packet_len >= ?";
    }
    if (packetLenMax >= 0) {
        sql += " AND packet_len <= ?";
    }
    
    if (dateYearStart >= 0) {
        sql += " AND (header_year > ? OR (header_year = ? AND header_month > ?) OR (header_year = ? AND header_month = ? AND header_day >= ?))";
    }
    if (dateYearEnd >= 0) {
        sql += " AND (header_year < ? OR (header_year = ? AND header_month < ?) OR (header_year = ? AND header_month = ? AND header_day <= ?))";
    }
    
    if (statusBitField >= 1 && statusBitPosition >= 0 && statusBitPosition <= 15 && statusBitValue >= 0) {
        if (statusBitField == 1) {
            sql += QString(" AND (status1 & %1) %2 0").arg(1 << statusBitPosition).arg(statusBitValue ? "!=" : "=");
        } else {
            sql += QString(" AND (status2 & %1) %2 0").arg(1 << statusBitPosition).arg(statusBitValue ? "!=" : "=");
        }
    }
    
    if (wordCountMin >= 0) {
        sql += " AND word_count >= ?";
    }
    if (wordCountMax >= 0) {
        sql += " AND word_count <= ?";
    }
    
    if (errorFlagFilter >= 0) {
        sql += QString(" AND error_flag = %1").arg(errorFlagFilter);
    }
    
    QSqlQuery query(db);
    query.prepare(sql);
    
    query.addBindValue(fileId);
    
    for (int terminal : terminalFilter) {
        query.addBindValue(terminal);
    }
    
    for (int subAddr : subAddrFilter) {
        query.addBindValue(subAddr);
    }
    
    for (MessageType type : typeFilter) {
        query.addBindValue(static_cast<int>(type));
    }
    
    if (chsttFilter >= 0) {
        query.addBindValue(chsttFilter);
    }
    
    if (mpuIdFilter > 0) {
        query.addBindValue(mpuIdFilter);
    }
    
    if (startTime > 0) {
        query.addBindValue(startTime);
    }
    
    if (endTime < LLONG_MAX) {
        query.addBindValue(endTime);
    }
    
    for (int terminal : excludeTerminalFilter) {
        query.addBindValue(terminal);
    }
    
    for (MessageType type : excludeTypeFilter) {
        query.addBindValue(static_cast<int>(type));
    }
    
    if (packetLenMin >= 0) {
        query.addBindValue(packetLenMin);
    }
    if (packetLenMax >= 0) {
        query.addBindValue(packetLenMax);
    }
    
    if (dateYearStart >= 0) {
        query.addBindValue(dateYearStart);
        query.addBindValue(dateYearStart);
        query.addBindValue(dateMonthStart);
        query.addBindValue(dateYearStart);
        query.addBindValue(dateMonthStart);
        query.addBindValue(dateDayStart);
    }
    if (dateYearEnd >= 0) {
        query.addBindValue(dateYearEnd);
        query.addBindValue(dateYearEnd);
        query.addBindValue(dateMonthEnd);
        query.addBindValue(dateYearEnd);
        query.addBindValue(dateMonthEnd);
        query.addBindValue(dateDayEnd);
    }
    
    if (wordCountMin >= 0) {
        query.addBindValue(wordCountMin);
    }
    if (wordCountMax >= 0) {
        query.addBindValue(wordCountMax);
    }
    
    if (query.exec() && query.next()) {
        return query.value(0).toLongLong();
    }
    
    return 0;
}

// ========== 列式查询（优化分析） ==========

QHash<int, qint64> DatabaseManager::queryTerminalDistribution(int fileId)
{
    QString cacheKey = QString("terminal_dist_%1").arg(fileId);
    QVariant cached = getCachedStatistics(fileId, cacheKey);
    
    if (cached.isValid()) {
        return cached.value<QHash<int, qint64>>();
    }
    
    QSqlDatabase& db = getReadConnection();
    
    QSqlQuery query(db);
    query.prepare(
        "SELECT terminal_addr, COUNT(*) as count "
        "FROM packets_col_terminal "
        "WHERE file_id = ? "
        "GROUP BY terminal_addr"
    );
    query.addBindValue(fileId);
    
    QHash<int, qint64> distribution;
    
    if (query.exec()) {
        while (query.next()) {
            int terminal = query.value(0).toInt();
            qint64 count = query.value(1).toLongLong();
            distribution[terminal] = count;
        }
    }
    
    setCachedStatistics(fileId, cacheKey, QVariant::fromValue(distribution));
    
    return distribution;
}

QHash<MessageType, qint64> DatabaseManager::queryMessageTypeDistribution(int fileId)
{
    QString cacheKey = QString("type_dist_%1").arg(fileId);
    QVariant cached = getCachedStatistics(fileId, cacheKey);
    
    if (cached.isValid()) {
        return cached.value<QHash<MessageType, qint64>>();
    }
    
    QSqlDatabase& db = getReadConnection();
    
    QSqlQuery query(db);
    query.prepare(
        "SELECT message_type, COUNT(*) as count "
        "FROM packets_col_type "
        "WHERE file_id = ? "
        "GROUP BY message_type"
    );
    query.addBindValue(fileId);
    
    QHash<MessageType, qint64> distribution;
    
    if (query.exec()) {
        while (query.next()) {
            MessageType type = static_cast<MessageType>(query.value(0).toInt());
            qint64 count = query.value(1).toLongLong();
            distribution[type] = count;
        }
    }
    
    setCachedStatistics(fileId, cacheKey, QVariant::fromValue(distribution));
    
    return distribution;
}

QPair<qint64, qint64> DatabaseManager::queryTimeRange(int fileId)
{
    QString cacheKey = QString("time_range_%1").arg(fileId);
    QVariant cached = getCachedStatistics(fileId, cacheKey);
    
    if (cached.isValid()) {
        return cached.value<QPair<qint64, qint64>>();
    }
    
    QSqlDatabase& db = getReadConnection();
    
    QSqlQuery query(db);
    query.prepare(
        "SELECT MIN(timestamp_ms), MAX(timestamp_ms) "
        "FROM packets_col_timestamp "
        "WHERE file_id = ?"
    );
    query.addBindValue(fileId);
    
    QPair<qint64, qint64> range(0, 0);
    
    if (query.exec() && query.next()) {
        range.first = query.value(0).toLongLong();
        range.second = query.value(1).toLongLong();
    }
    
    setCachedStatistics(fileId, cacheKey, QVariant::fromValue(range));
    
    return range;
}

QHash<int, qint64> DatabaseManager::querySubAddressDistribution(int fileId)
{
    QString cacheKey = QString("subaddr_dist_%1").arg(fileId);
    QVariant cached = getCachedStatistics(fileId, cacheKey);
    
    if (cached.isValid()) {
        return cached.value<QHash<int, qint64>>();
    }
    
    QSqlDatabase& db = getReadConnection();
    
    QSqlQuery query(db);
    query.prepare(
        "SELECT sub_addr, COUNT(*) as count "
        "FROM packets_col_terminal "
        "WHERE file_id = ? "
        "GROUP BY sub_addr"
    );
    query.addBindValue(fileId);
    
    QHash<int, qint64> distribution;
    
    if (query.exec()) {
        while (query.next()) {
            int subAddr = query.value(0).toInt();
            qint64 count = query.value(1).toLongLong();
            distribution[subAddr] = count;
        }
    }
    
    setCachedStatistics(fileId, cacheKey, QVariant::fromValue(distribution));
    
    return distribution;
}

QVector<QPair<qint64, qint64>> DatabaseManager::queryTimeDistribution(int fileId, int hours)
{
    QSqlDatabase& db = getReadConnection();
    
    qint64 startTime = queryTimeRange(fileId).first;
    qint64 interval = hours * 3600000LL;  // 小时转毫秒
    
    QSqlQuery query(db);
    query.prepare(
        "SELECT "
        "  CAST((timestamp_ms - ?) / ? AS INTEGER) as interval_id, "
        "  COUNT(*) as count "
        "FROM packets_col_timestamp "
        "WHERE file_id = ? "
        "GROUP BY interval_id "
        "ORDER BY interval_id"
    );
    query.addBindValue(startTime);
    query.addBindValue(interval);
    query.addBindValue(fileId);
    
    QVector<QPair<qint64, qint64>> distribution;
    
    if (query.exec()) {
        while (query.next()) {
            qint64 intervalId = query.value(0).toLongLong();
            qint64 count = query.value(1).toLongLong();
            distribution.append(qMakePair(startTime + intervalId * interval, count));
        }
    }
    
    return distribution;
}

// ========== 文件管理 ==========

QVector<QVariantMap> DatabaseManager::getFileList()
{
    QVector<QVariantMap> files;
    
    QSqlDatabase& db = getReadConnection();
    
    QSqlQuery query(db);
    query.exec(
        "SELECT id, file_path, file_name, file_size, file_hash, import_time, "
        "message_count, packet_count, parse_duration, is_valid, is_current, notes "
        "FROM files ORDER BY import_time DESC"
    );
    
    while (query.next()) {
        QVariantMap file;
        file["id"] = query.value(0);
        file["file_path"] = query.value(1);
        file["file_name"] = query.value(2);
        file["file_size"] = query.value(3);
        file["file_hash"] = query.value(4);
        file["import_time"] = query.value(5);
        file["message_count"] = query.value(6);
        file["packet_count"] = query.value(7);
        file["parse_duration"] = query.value(8);
        file["is_valid"] = query.value(9);
        file["is_current"] = query.value(10);
        file["notes"] = query.value(11);
        
        files.append(file);
    }
    
    return files;
}

bool DatabaseManager::switchCurrentFile(int fileId)
{
    QSqlDatabase& db = getWriteConnection();
    
    if (!db.transaction()) {
        logError(tr("开始事务失败: %1").arg(db.lastError().text()));
        return false;
    }
    
    QSqlQuery query(db);
    
    // 清除所有文件的当前标志
    query.exec("UPDATE files SET is_current = 0");
    
    // 设置指定文件为当前文件
    query.prepare("UPDATE files SET is_current = 1 WHERE id = ?");
    query.addBindValue(fileId);
    
    if (!query.exec()) {
        db.rollback();
        logError(tr("切换文件失败: %1").arg(query.lastError().text()));
        return false;
    }
    
    if (!db.commit()) {
        db.rollback();
        logError(tr("提交事务失败: %1").arg(db.lastError().text()));
        return false;
    }
    
    // 更新当前文件ID
    {
        QMutexLocker locker(&m_fileMutex);
        m_currentFileId = fileId;
    }
    
    // 清理缓存
    clearCache();
    
    emit fileSwitched(fileId);
    
    return true;
}

int DatabaseManager::getCurrentFileId()
{
    QMutexLocker locker(&m_fileMutex);
    
    if (m_currentFileId >= 0) {
        return m_currentFileId;
    }
    
    // 从数据库查询
    QSqlDatabase& db = getReadConnection();
    
    QSqlQuery query(db);
    query.exec("SELECT id FROM files WHERE is_current = 1 LIMIT 1");
    
    if (query.next()) {
        m_currentFileId = query.value(0).toInt();
    }
    
    return m_currentFileId;
}

bool DatabaseManager::deleteFile(int fileId)
{
    QSqlDatabase& db = getWriteConnection();
    
    if (!db.transaction()) {
        logError(tr("开始事务失败: %1").arg(db.lastError().text()));
        return false;
    }
    
    QSqlQuery query(db);
    
    // 删除文件记录（级联删除相关数据）
    query.prepare("DELETE FROM files WHERE id = ?");
    query.addBindValue(fileId);
    
    if (!query.exec()) {
        db.rollback();
        logError(tr("删除文件失败: %1").arg(query.lastError().text()));
        return false;
    }
    
    if (!db.commit()) {
        db.rollback();
        logError(tr("提交事务失败: %1").arg(db.lastError().text()));
        return false;
    }
    
    // 如果删除的是当前文件，清除当前文件ID
    {
        QMutexLocker locker(&m_fileMutex);
        if (m_currentFileId == fileId) {
            m_currentFileId = -1;
        }
    }
    
    // 清理缓存
    clearCache();
    
    // 优化数据库
    optimizeDatabase();
    
    return true;
}

QVariantMap DatabaseManager::getFileInfo(int fileId)
{
    QVariantMap fileInfo;
    
    QSqlDatabase& db = getReadConnection();
    
    QSqlQuery query(db);
    query.prepare(
        "SELECT id, file_path, file_name, file_size, file_hash, import_time, "
        "message_count, packet_count, parse_duration, is_valid, is_current, notes "
        "FROM files WHERE id = ?"
    );
    query.addBindValue(fileId);
    
    if (query.exec() && query.next()) {
        fileInfo["id"] = query.value(0);
        fileInfo["file_path"] = query.value(1);
        fileInfo["file_name"] = query.value(2);
        fileInfo["file_size"] = query.value(3);
        fileInfo["file_hash"] = query.value(4);
        fileInfo["import_time"] = query.value(5);
        fileInfo["message_count"] = query.value(6);
        fileInfo["packet_count"] = query.value(7);
        fileInfo["parse_duration"] = query.value(8);
        fileInfo["is_valid"] = query.value(9);
        fileInfo["is_current"] = query.value(10);
        fileInfo["notes"] = query.value(11);
    }
    
    return fileInfo;
}

bool DatabaseManager::isFileImported(const QString& filePath)
{
    QSqlDatabase& db = getReadConnection();
    
    QSqlQuery query(db);
    query.prepare("SELECT id FROM files WHERE file_path = ?");
    query.addBindValue(filePath);
    
    return query.exec() && query.next();
}

int DatabaseManager::getFileIdByPath(const QString& filePath)
{
    QSqlDatabase& db = getReadConnection();
    
    QSqlQuery query(db);
    query.prepare("SELECT id FROM files WHERE file_path = ?");
    query.addBindValue(filePath);
    
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    
    return -1;
}

int DatabaseManager::getFileIdByFileName(const QString& fileName)
{
    QSqlDatabase& db = getReadConnection();
    
    QSqlQuery query(db);
    query.prepare("SELECT id FROM files WHERE file_name = ?");
    query.addBindValue(fileName);
    
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    
    return -1;
}

bool DatabaseManager::deleteFileData(int fileId)
{
    LOG_INFO("DatabaseManager", QString("[数据库] 删除文件数据, 文件ID: %1").arg(fileId));
    
    QSqlDatabase& db = getWriteConnection();
    
    if (!db.transaction()) {
        LOG_ERROR("DatabaseManager", QString("[数据库] 开始删除事务失败: %1").arg(db.lastError().text()));
        return false;
    }
    
    QSqlQuery query(db);
    
    query.prepare("DELETE FROM packets WHERE file_id = ?");
    query.addBindValue(fileId);
    if (!query.exec()) {
        LOG_ERROR("DatabaseManager", QString("[数据库] 删除packets失败: %1").arg(query.lastError().text()));
    }
    
    query.prepare("DELETE FROM packets_col_terminal WHERE file_id = ?");
    query.addBindValue(fileId);
    if (!query.exec()) {
        LOG_ERROR("DatabaseManager", QString("[数据库] 删除col_terminal失败: %1").arg(query.lastError().text()));
    }
    
    query.prepare("DELETE FROM packets_col_timestamp WHERE file_id = ?");
    query.addBindValue(fileId);
    if (!query.exec()) {
        LOG_ERROR("DatabaseManager", QString("[数据库] 删除col_timestamp失败: %1").arg(query.lastError().text()));
    }
    
    query.prepare("DELETE FROM packets_col_type WHERE file_id = ?");
    query.addBindValue(fileId);
    if (!query.exec()) {
        LOG_ERROR("DatabaseManager", QString("[数据库] 删除col_type失败: %1").arg(query.lastError().text()));
    }
    
    query.prepare("DELETE FROM packets_col_status WHERE file_id = ?");
    query.addBindValue(fileId);
    if (!query.exec()) {
        LOG_ERROR("DatabaseManager", QString("[数据库] 删除col_status失败: %1").arg(query.lastError().text()));
    }
    
    query.prepare("DELETE FROM files WHERE id = ?");
    query.addBindValue(fileId);
    if (!query.exec()) {
        LOG_ERROR("DatabaseManager", QString("[数据库] 删除文件记录失败: %1").arg(query.lastError().text()));
    }
    
    if (!db.commit()) {
        db.rollback();
        LOG_ERROR("DatabaseManager", QString("[数据库] 提交删除事务失败: %1").arg(db.lastError().text()));
        return false;
    }
    
    LOG_INFO("DatabaseManager", QString("[数据库] 成功删除文件数据, 文件ID: %1").arg(fileId));
    return true;
}

// ========== 统计分析 ==========

qint64 DatabaseManager::getTotalPacketCount(int fileId)
{
    QString cacheKey = QString("total_packets_%1").arg(fileId);
    QVariant cached = getCachedStatistics(fileId, cacheKey);
    
    if (cached.isValid()) {
        return cached.toLongLong();
    }
    
    QSqlDatabase& db = getReadConnection();
    
    QSqlQuery query(db);
    
    if (fileId < 0) {
        query.exec("SELECT COUNT(*) FROM packets");
    } else {
        query.prepare("SELECT COUNT(*) FROM packets WHERE file_id = ?");
        query.addBindValue(fileId);
        query.exec();
    }
    
    qint64 count = 0;
    if (query.next()) {
        count = query.value(0).toLongLong();
    }
    
    setCachedStatistics(fileId, cacheKey, count);
    
    return count;
}

qint64 DatabaseManager::getTotalMessageCount(int fileId)
{
    QString cacheKey = QString("total_messages_%1").arg(fileId);
    QVariant cached = getCachedStatistics(fileId, cacheKey);
    
    if (cached.isValid()) {
        return cached.toLongLong();
    }
    
    QSqlDatabase& db = getReadConnection();
    
    QSqlQuery query(db);
    
    // 从packets表计算不同msg_index的数量作为消息数
    if (fileId < 0) {
        query.exec("SELECT COUNT(DISTINCT msg_index) FROM packets");
    } else {
        query.prepare("SELECT COUNT(DISTINCT msg_index) FROM packets WHERE file_id = ?");
        query.addBindValue(fileId);
        query.exec();
    }
    
    qint64 count = 0;
    if (query.next()) {
        count = query.value(0).toLongLong();
    }
    
    setCachedStatistics(fileId, cacheKey, count);
    
    return count;
}

QVector<GanttData> DatabaseManager::getGanttData(int fileId, qint64 startTime, qint64 endTime)
{
    QVector<GanttData> ganttData;
    
    QSqlDatabase& db = getReadConnection();
    
    QSqlQuery query(db);
    query.prepare(
        "SELECT terminal_addr, MIN(timestamp_ms), MAX(timestamp_ms), COUNT(*), message_type "
        "FROM packets "
        "WHERE file_id = ? AND timestamp_ms >= ? AND timestamp_ms <= ? "
        "GROUP BY terminal_addr, message_type "
        "ORDER BY terminal_addr, MIN(timestamp_ms)"
    );
    query.addBindValue(fileId);
    query.addBindValue(startTime);
    query.addBindValue(endTime);
    
    if (query.exec()) {
        while (query.next()) {
            GanttData data;
            data.terminalAddr = query.value(0).toInt();
            data.startTime = query.value(1).toLongLong();
            data.endTime = query.value(2).toLongLong();
            data.count = query.value(3).toInt();
            data.messageType = static_cast<MessageType>(query.value(4).toInt());
            
            ganttData.append(data);
        }
    }
    
    return ganttData;
}

QMap<int, int> DatabaseManager::getTerminalStatistics(int fileId, 
    const QSet<int>& terminalFilter,
    const QSet<int>& subAddrFilter,
    const QSet<MessageType>& typeFilter,
    qint64 startTime, qint64 endTime)
{
    QMap<int, int> stats;
    
    QSqlDatabase& db = getReadConnection();
    
    QString sql = "SELECT terminal_addr, COUNT(*) FROM packets WHERE file_id = ?";
    
    if (!terminalFilter.isEmpty()) {
        sql += " AND terminal_addr IN (";
        QStringList placeholders;
        for (int i = 0; i < terminalFilter.size(); ++i) {
            placeholders << "?";
        }
        sql += placeholders.join(", ") + ")";
    }
    
    if (!subAddrFilter.isEmpty()) {
        sql += " AND sub_addr IN (";
        QStringList placeholders;
        for (int i = 0; i < subAddrFilter.size(); ++i) {
            placeholders << "?";
        }
        sql += placeholders.join(", ") + ")";
    }
    
    if (!typeFilter.isEmpty()) {
        sql += " AND message_type IN (";
        QStringList placeholders;
        for (int i = 0; i < typeFilter.size(); ++i) {
            placeholders << "?";
        }
        sql += placeholders.join(", ") + ")";
    }
    
    if (startTime > 0) {
        sql += " AND timestamp_ms >= ?";
    }
    
    if (endTime < LLONG_MAX) {
        sql += " AND timestamp_ms <= ?";
    }
    
    sql += " GROUP BY terminal_addr";
    
    QSqlQuery query(db);
    query.prepare(sql);
    
    query.addBindValue(fileId);
    
    for (int terminal : terminalFilter) {
        query.addBindValue(terminal);
    }
    
    for (int subAddr : subAddrFilter) {
        query.addBindValue(subAddr);
    }
    
    for (MessageType type : typeFilter) {
        query.addBindValue(static_cast<int>(type));
    }
    
    if (startTime > 0) {
        query.addBindValue(startTime);
    }
    
    if (endTime < LLONG_MAX) {
        query.addBindValue(endTime);
    }
    
    if (query.exec()) {
        while (query.next()) {
            int terminal = query.value(0).toInt();
            int count = query.value(1).toInt();
            stats[terminal] = count;
        }
    }
    
    return stats;
}

QMap<MessageType, int> DatabaseManager::getMessageTypeStatistics(int fileId,
    const QSet<int>& terminalFilter,
    const QSet<int>& subAddrFilter,
    qint64 startTime, qint64 endTime)
{
    QMap<MessageType, int> stats;
    
    QSqlDatabase& db = getReadConnection();
    
    QString sql = "SELECT message_type, COUNT(*) FROM packets WHERE file_id = ?";
    
    if (!terminalFilter.isEmpty()) {
        sql += " AND terminal_addr IN (";
        QStringList placeholders;
        for (int i = 0; i < terminalFilter.size(); ++i) {
            placeholders << "?";
        }
        sql += placeholders.join(", ") + ")";
    }
    
    if (!subAddrFilter.isEmpty()) {
        sql += " AND sub_addr IN (";
        QStringList placeholders;
        for (int i = 0; i < subAddrFilter.size(); ++i) {
            placeholders << "?";
        }
        sql += placeholders.join(", ") + ")";
    }
    
    if (startTime > 0) {
        sql += " AND timestamp_ms >= ?";
    }
    
    if (endTime < LLONG_MAX) {
        sql += " AND timestamp_ms <= ?";
    }
    
    sql += " GROUP BY message_type";
    
    QSqlQuery query(db);
    query.prepare(sql);
    
    query.addBindValue(fileId);
    
    for (int terminal : terminalFilter) {
        query.addBindValue(terminal);
    }
    
    for (int subAddr : subAddrFilter) {
        query.addBindValue(subAddr);
    }
    
    if (startTime > 0) {
        query.addBindValue(startTime);
    }
    
    if (endTime < LLONG_MAX) {
        query.addBindValue(endTime);
    }
    
    if (query.exec()) {
        while (query.next()) {
            MessageType type = static_cast<MessageType>(query.value(0).toInt());
            int count = query.value(1).toInt();
            stats[type] = count;
        }
    }
    
    return stats;
}

QMap<int, int> DatabaseManager::getSubAddressStatistics(int fileId,
    const QSet<int>& terminalFilter,
    const QSet<MessageType>& typeFilter,
    qint64 startTime, qint64 endTime)
{
    QMap<int, int> stats;
    
    QSqlDatabase& db = getReadConnection();
    
    QString sql = "SELECT sub_addr, COUNT(*) FROM packets WHERE file_id = ?";
    
    if (!terminalFilter.isEmpty()) {
        sql += " AND terminal_addr IN (";
        QStringList placeholders;
        for (int i = 0; i < terminalFilter.size(); ++i) {
            placeholders << "?";
        }
        sql += placeholders.join(", ") + ")";
    }
    
    if (!typeFilter.isEmpty()) {
        sql += " AND message_type IN (";
        QStringList placeholders;
        for (int i = 0; i < typeFilter.size(); ++i) {
            placeholders << "?";
        }
        sql += placeholders.join(", ") + ")";
    }
    
    if (startTime > 0) {
        sql += " AND timestamp_ms >= ?";
    }
    
    if (endTime < LLONG_MAX) {
        sql += " AND timestamp_ms <= ?";
    }
    
    sql += " GROUP BY sub_addr";
    
    QSqlQuery query(db);
    query.prepare(sql);
    
    query.addBindValue(fileId);
    
    for (int terminal : terminalFilter) {
        query.addBindValue(terminal);
    }
    
    for (MessageType type : typeFilter) {
        query.addBindValue(static_cast<int>(type));
    }
    
    if (startTime > 0) {
        query.addBindValue(startTime);
    }
    
    if (endTime < LLONG_MAX) {
        query.addBindValue(endTime);
    }
    
    if (query.exec()) {
        while (query.next()) {
            int subAddr = query.value(0).toInt();
            int count = query.value(1).toInt();
            stats[subAddr] = count;
        }
    }
    
    return stats;
}

QSet<int> DatabaseManager::getAllTerminals(int fileId)
{
    QSet<int> terminals;
    
    QSqlDatabase& db = getReadConnection();
    
    QSqlQuery query(db);
    query.prepare("SELECT DISTINCT terminal_addr FROM packets WHERE file_id = ?");
    query.addBindValue(fileId);
    
    if (query.exec()) {
        while (query.next()) {
            terminals.insert(query.value(0).toInt());
        }
    }
    
    return terminals;
}

qint64 DatabaseManager::getMinTimestamp(int fileId)
{
    QSqlDatabase& db = getReadConnection();
    
    QSqlQuery query(db);
    query.prepare("SELECT MIN(timestamp_ms) FROM packets WHERE file_id = ?");
    query.addBindValue(fileId);
    
    if (query.exec() && query.next()) {
        return query.value(0).toLongLong();
    }
    
    return 0;
}

qint64 DatabaseManager::getMaxTimestamp(int fileId)
{
    QSqlDatabase& db = getReadConnection();
    
    QSqlQuery query(db);
    query.prepare("SELECT MAX(timestamp_ms) FROM packets WHERE file_id = ?");
    query.addBindValue(fileId);
    
    if (query.exec() && query.next()) {
        return query.value(0).toLongLong();
    }
    
    return 0;
}

// ========== 性能优化 ==========

void DatabaseManager::optimizeDatabase()
{
    QSqlDatabase& db = getWriteConnection();
    
    QSqlQuery query(db);
    query.exec("VACUUM");
    query.exec("ANALYZE");
    
    LOG_INFO("DatabaseManager", "Database optimized");
}

void DatabaseManager::clearCache()
{
    QMutexLocker locker(&m_statsMutex);
    m_statsCache.clear();
    
    LOG_INFO("DatabaseManager", "Cache cleared");
}

bool DatabaseManager::clearPacketsForFile(int fileId)
{
    LOG_INFO("DatabaseManager", QString("[数据库] 清除文件数据包, 文件ID: %1").arg(fileId));
    
    QSqlDatabase& db = getWriteConnection();
    
    QSqlQuery query(db);
    
    query.prepare("DELETE FROM packets WHERE file_id = ?");
    query.addBindValue(fileId);
    if (!query.exec()) {
        LOG_ERROR("DatabaseManager", QString("[数据库] 删除packets失败: %1").arg(query.lastError().text()));
        return false;
    }
    
    query.prepare("DELETE FROM packets_col_terminal WHERE file_id = ?");
    query.addBindValue(fileId);
    query.exec();
    
    query.prepare("DELETE FROM packets_col_timestamp WHERE file_id = ?");
    query.addBindValue(fileId);
    query.exec();
    
    query.prepare("DELETE FROM packets_col_type WHERE file_id = ?");
    query.addBindValue(fileId);
    query.exec();
    
    query.prepare("DELETE FROM packets_col_status WHERE file_id = ?");
    query.addBindValue(fileId);
    query.exec();
    
    query.prepare("UPDATE files SET message_count = 0, packet_count = 0 WHERE id = ?");
    query.addBindValue(fileId);
    query.exec();
    
    clearCache();
    
    LOG_INFO("DatabaseManager", QString("[数据库] 已清除文件数据包, 文件ID: %1").arg(fileId));
    return true;
}

QVariantMap DatabaseManager::getDatabaseStats()
{
    QVariantMap stats;
    
    QSqlDatabase& db = getReadConnection();
    
    // 数据库大小
    QFileInfo dbFile(m_dbPath);
    stats["db_size"] = dbFile.size();
    
    // 文件数量
    QSqlQuery query(db);
    query.exec("SELECT COUNT(*) FROM files");
    if (query.next()) {
        stats["file_count"] = query.value(0).toLongLong();
    }
    
    // 数据包数量
    query.exec("SELECT COUNT(*) FROM packets");
    if (query.next()) {
        stats["packet_count"] = query.value(0).toLongLong();
    }
    
    // 消息数量（从packets表计算不同msg_index的数量）
    query.exec("SELECT COUNT(DISTINCT msg_index) FROM packets");
    if (query.next()) {
        stats["message_count"] = query.value(0).toLongLong();
    }
    
    // 数据库版本
    stats["db_version"] = getDatabaseVersion();
    
    return stats;
}

// ========== 读写分离 ==========

QSqlDatabase& DatabaseManager::getWriteConnection()
{
    return m_writeDb;
}

/**
 * @brief 获取只读数据库连接（轮询方式）
 *
 * 使用原子操作fetch_add实现无锁轮询，从连接池中选取一个只读连接。
 * 这种方式比互斥锁更高效，适合高频并发读取场景。
 *
 * @return 只读数据库连接引用，如果没有只读连接则返回写连接
 */
QSqlDatabase& DatabaseManager::getReadConnection()
{
    if (m_readDbs.isEmpty()) {
        return m_writeDb;
    }
    
    // 原子递增取模：线程安全的轮询选择读连接
    int idx = m_currentReadDb.fetch_add(1) % m_readDbs.size();
    return m_readDbs[idx];
}

// ========== 缓存管理 ==========

QVariant DatabaseManager::getCachedStatistics(int fileId, const QString& cacheType)
{
    QMutexLocker locker(&m_statsMutex);
    
    QString key = QString("%1_%2").arg(fileId).arg(cacheType);
    
    if (m_statsCache.contains(key)) {
        return m_statsCache[key];
    }
    
    return QVariant();
}

void DatabaseManager::setCachedStatistics(int fileId, const QString& cacheType, const QVariant& data)
{
    QMutexLocker locker(&m_statsMutex);
    
    QString key = QString("%1_%2").arg(fileId).arg(cacheType);
    m_statsCache[key] = data;
}

// ========== 辅助方法 ==========

QVector<DbDataRecord> DatabaseManager::convertToRecords(const SMbiMonPacketMsg& msg, int fileId, int msgIndex, int& globalIndex)
{
    QVector<DbDataRecord> records;
    
    // 解析命令字
    auto parseCmd = [](quint16 cmdRaw, int& terminalAddr, int& subAddr, int& trBit, int& modeCode, int& wordCount) {
        CMD cmd;
        memcpy(&cmd, &cmdRaw, sizeof(CMD));
        
        terminalAddr = cmd.zhongduandizhi;
        subAddr = cmd.zidizhi;
        trBit = cmd.T_R;
        modeCode = (subAddr == 0 || subAddr == 31) ? cmd.sjzjs_fsdm : 0;
        wordCount = (subAddr == 0 || subAddr == 31) ? 0 : cmd.sjzjs_fsdm;
    };
    
    // 计算时间戳（毫秒）
    auto calcTimestampMs = [](quint32 timestamp) -> double {
        return timestamp * 40.0 / 1000.0;  // 40us转ms
    };
    
    // 处理每个数据包
    for (int i = 0; i < msg.packetDatas.size(); ++i) {
        const SMbiMonPacketData& packetData = msg.packetDatas[i];
        
        DbDataRecord record;
        record.fileId = fileId;
        record.msgIndex = msgIndex;
        record.dataIndex = i;
        record.rowIndex = globalIndex++;
        
        // 解析命令字1
        parseCmd(packetData.cmd1, record.terminalAddr, record.subAddr, 
                 record.trBit, record.modeCode, record.wordCount);
        
        // 解析命令字2
        parseCmd(packetData.cmd2, record.terminalAddr2, record.subAddr2,
                 record.trBit2, record.modeCode2, record.wordCount2);
        
        // 状态字
        record.status1 = packetData.states1;
        record.status2 = packetData.states2;
        record.chstt = packetData.chstt;
        
        // 时间戳
        record.packetTimestamp = packetData.timestamp;
        record.timestampMs = calcTimestampMs(packetData.timestamp);
        
        // 数据内容
        record.data = packetData.datas;
        record.dataSize = packetData.datas.size();
        record.isCompressed = false;
        
        // 消息类型
        record.messageType = detectMessageType(packetData);
        
        // 错误标志
        record.errorFlag = (packetData.chstt == 0);
        
        record.mpuProduceId = msg.header.mpuProduceId;
        record.packetLen = msg.header.packetLen;
        record.headerYear = msg.header.year;
        record.headerMonth = msg.header.month;
        record.headerDay = msg.header.day;
        record.headerTimestamp = msg.header.timestamp;
        
        records.append(record);
    }
    
    return records;
}

QString DatabaseManager::calculateFileHash(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    
    QCryptographicHash hash(QCryptographicHash::Md5);
    
    // 只计算前1MB的哈希，提高性能
    const qint64 maxSize = 1024 * 1024;
    qint64 bytesRead = 0;
    
    while (!file.atEnd() && bytesRead < maxSize) {
        QByteArray chunk = file.read(8192);
        hash.addData(chunk);
        bytesRead += chunk.size();
    }
    
    file.close();
    
    return hash.result().toHex();
}

QByteArray DatabaseManager::compressData(const QByteArray& data)
{
    return qCompress(data, 9);  // 最大压缩级别
}

QByteArray DatabaseManager::decompressData(const QByteArray& compressedData)
{
    return qUncompress(compressedData);
}

bool DatabaseManager::executeSql(QSqlDatabase& db, const QString& sql)
{
    QSqlQuery query(db);
    
    if (!query.exec(sql)) {
        logError(tr("执行SQL失败: %1\nSQL: %2").arg(query.lastError().text()).arg(sql));
        return false;
    }
    
    return true;
}

void DatabaseManager::logError(const QString& error)
{
    m_lastError = error;
    LOG_ERROR("DatabaseManager", QString("DatabaseManager Error: %1").arg(error));
    emit databaseError(error);
}

