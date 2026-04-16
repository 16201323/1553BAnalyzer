/**
 * @file Logger.cpp
 * @brief 日志管理器实现
 * 
 * 本文件实现了应用程序的日志记录功能，包括：
 * - 日志文件的创建和管理
 * - 多级别日志记录（Debug/Info/Warning/Error）
 * - 日志文件轮转（防止日志文件过大）
 * - 线程安全的日志写入
 * 
 * 日志格式：
 * [时间戳] [级别] [模块名] 日志消息
 * 
 * 使用示例：
 * @code
 * Logger::instance()->setLogFile("app.log");
 * LOG_INFO("ModuleName", "应用程序启动");
 * LOG_ERROR("ModuleName", "发生错误: %1").arg(errorMsg);
 * @endcode
 * 
 * @author 1553BTools
 * @date 2024
 */

#include "Logger.h"
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#endif

/**
 * @brief 静态单例实例指针
 */
Logger* Logger::m_instance = nullptr;

/**
 * @brief 构造函数
 * 
 * 初始化默认配置：
 * - 日志级别：Debug（最低级别，记录所有日志）
 * - 最大文件大小：10MB
 * - 最大备份数量：5个
 */
Logger::Logger()
    : m_logLevel(LogLevel::Debug)
    , m_maxFileSize(10 * 1024 * 1024)
    , m_maxBackupCount(5)
    , m_initialized(false)
{
}

/**
 * @brief 析构函数
 * 
 * 关闭日志文件，确保所有日志已写入磁盘
 */
Logger::~Logger()
{
    if (m_logFile.isOpen()) {
        m_logStream.flush();
        m_logFile.close();
    }
}

/**
 * @brief 获取单例实例
 * @return Logger单例指针
 */
Logger* Logger::instance()
{
    if (!m_instance) {
        m_instance = new Logger();
    }
    return m_instance;
}

/**
 * @brief 设置日志文件路径
 * @param filePath 日志文件路径
 * 
 * 打开或创建日志文件，设置UTF-8编码。
 * 如果文件不存在，会自动创建所在目录。
 * 文件打开后写入应用程序启动标记。
 */
void Logger::setLogFile(const QString& filePath)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_logFile.isOpen()) {
        m_logStream.flush();
        m_logFile.close();
    }
    
    m_logFilePath = filePath;
    
    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    m_logFile.setFileName(filePath);
    if (m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        m_logStream.setDevice(&m_logFile);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        m_logStream.setEncoding(QStringConverter::Utf8);
#else
        m_logStream.setCodec("UTF-8");
#endif
        m_initialized = true;
        m_logStream << "\n========================================\n";
        m_logStream << QString("[%1] 应用程序启动\n")
                       .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
        m_logStream << "========================================\n";
        m_logStream.flush();
    } else {
        m_lastError = QString("无法打开日志文件: %1").arg(filePath);
        m_initialized = false;
    }
}

/**
 * @brief 设置日志级别
 * @param level 日志级别
 * 
 * 只有大于等于此级别的日志才会被记录
 */
void Logger::setLogLevel(LogLevel level)
{
    m_logLevel = level;
}

/**
 * @brief 设置最大文件大小
 * @param maxSizeMB 最大大小（MB）
 * 
 * 当日志文件超过此大小时，会触发日志轮转
 */
void Logger::setMaxFileSize(qint64 maxSizeMB)
{
    m_maxFileSize = maxSizeMB * 1024 * 1024;
}

/**
 * @brief 设置最大备份数量
 * @param count 备份文件数量
 * 
 * 日志轮转时保留的历史日志文件数量
 */
void Logger::setMaxBackupCount(int count)
{
    m_maxBackupCount = count;
}

/**
 * @brief 记录Debug级别日志
 * @param module 模块名称
 * @param message 日志消息
 */
void Logger::debug(const QString& module, const QString& message)
{
    log(LogLevel::Debug, module, message);
}

/**
 * @brief 记录Info级别日志
 * @param module 模块名称
 * @param message 日志消息
 */
void Logger::info(const QString& module, const QString& message)
{
    log(LogLevel::Info, module, message);
}

/**
 * @brief 记录Warning级别日志
 * @param module 模块名称
 * @param message 日志消息
 */
void Logger::warning(const QString& module, const QString& message)
{
    log(LogLevel::Warning, module, message);
}

/**
 * @brief 记录Error级别日志
 * @param module 模块名称
 * @param message 日志消息
 */
void Logger::error(const QString& module, const QString& message)
{
    log(LogLevel::Error, module, message);
}

/**
 * @brief 记录日志
 * @param level 日志级别
 * @param module 模块名称
 * @param message 日志消息
 * 
 * 根据当前日志级别过滤，只记录符合条件的日志
 */
void Logger::log(LogLevel level, const QString& module, const QString& message)
{
    if (level < m_logLevel) {
        return;
    }
    
    writeLog(level, module, message);
}

/**
 * @brief 写入日志
 * @param level 日志级别
 * @param module 模块名称
 * @param message 日志消息
 * 
 * 格式化日志消息并写入文件。
 * Warning及以上级别的日志同时输出到控制台。
 */
void Logger::writeLog(LogLevel level, const QString& module, const QString& message)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_initialized) {
        return;
    }
    
    checkFileSize();
    
    QString formattedMessage = formatMessage(level, module, message);
    m_logStream << formattedMessage;
    m_logStream.flush();
    
    if (level >= LogLevel::Warning) {
        qDebug().noquote() << formattedMessage;
    }
}

/**
 * @brief 检查文件大小
 * 
 * 如果日志文件超过最大大小，触发日志轮转
 */
void Logger::checkFileSize()
{
    if (m_logFile.size() > m_maxFileSize) {
        rotateLog();
    }
}

/**
 * @brief 日志轮转
 * 
 * 执行日志文件轮转操作：
 * 1. 将现有备份文件重命名（.1 -> .2, .2 -> .3, ...）
 * 2. 将当前日志文件重命名为 .1
 * 3. 创建新的日志文件
 * 
 * 轮转后保留 m_maxBackupCount 个历史文件
 */
void Logger::rotateLog()
{
    m_logStream.flush();
    m_logFile.close();
    
    for (int i = m_maxBackupCount - 1; i >= 1; --i) {
        QString oldFile = QString("%1.%2").arg(m_logFilePath).arg(i);
        QString newFile = QString("%1.%2").arg(m_logFilePath).arg(i + 1);
        
        if (QFile::exists(oldFile)) {
            if (QFile::exists(newFile)) {
                QFile::remove(newFile);
            }
            QFile::rename(oldFile, newFile);
        }
    }
    
    QString backupFile = QString("%1.1").arg(m_logFilePath);
    if (QFile::exists(backupFile)) {
        QFile::remove(backupFile);
    }
    QFile::rename(m_logFilePath, backupFile);
    
    m_logFile.setFileName(m_logFilePath);
    m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    m_logStream.setDevice(&m_logFile);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    m_logStream.setEncoding(QStringConverter::Utf8);
#else
    m_logStream.setCodec("UTF-8");
#endif
}

/**
 * @brief 日志级别转字符串
 * @param level 日志级别
 * @return 级别字符串（5字符固定宽度）
 */
QString Logger::levelToString(LogLevel level)
{
    switch (level) {
    case LogLevel::Debug:   return "DEBUG";
    case LogLevel::Info:    return "INFO ";
    case LogLevel::Warning: return "WARN ";
    case LogLevel::Error:   return "ERROR";
    default:                return "UNKN ";
    }
}

/**
 * @brief 格式化日志消息
 * @param level 日志级别
 * @param module 模块名称
 * @param message 日志消息
 * @return 格式化后的日志字符串
 * 
 * 格式：[时间戳] [级别] [模块名] 消息
 * 模块名左对齐填充到20字符宽度
 */
QString Logger::formatMessage(LogLevel level, const QString& module, const QString& message)
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString levelStr = levelToString(level);
    return QString("[%1] [%2] [%3] %4\n")
           .arg(timestamp)
           .arg(levelStr)
           .arg(module.leftJustified(20, ' '))
           .arg(message);
}

/**
 * @brief 获取日志文件路径
 * @return 日志文件路径
 */
QString Logger::logFilePath() const
{
    return m_logFilePath;
}

/**
 * @brief 获取最后的错误信息
 * @return 错误信息字符串
 */
QString Logger::lastError() const
{
    return m_lastError;
}
