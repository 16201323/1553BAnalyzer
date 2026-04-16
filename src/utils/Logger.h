/**
 * @file Logger.h
 * @brief 日志管理器类定义
 * 
 * Logger类提供应用程序的日志记录功能，支持：
 * - 多级别日志（Debug、Info、Warning、Error）
 * - 日志文件自动轮转
 * - 线程安全
 * - 文件大小限制
 * 
 * 使用宏定义简化日志调用：
 * - LOG_DEBUG：调试信息
 * - LOG_INFO：一般信息
 * - LOG_WARNING：警告信息
 * - LOG_ERROR：错误信息
 * 
 * 使用示例：
 * @code
 * // 初始化日志
 * Logger::instance()->setLogFile("app.log");
 * Logger::instance()->setLogLevel(LogLevel::Info);
 * 
 * // 记录日志
 * LOG_INFO("MainWindow", "应用程序启动");
 * LOG_DEBUG("Parser", QString("解析文件: %1").arg(filename));
 * LOG_ERROR("Network", "连接失败");
 * @endcode
 * 
 * 日志格式：
 * [时间戳] [级别] [模块] 消息内容
 * 例如：[2024-01-15 10:30:45.123] [INFO] [MainWindow] 应用程序启动
 * 
 * @author 1553BTools
 * @date 2024
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <QString>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QDateTime>
#include <QDebug>

/**
 * @brief 日志级别枚举
 * 
 * 定义四个日志级别，从低到高依次为：
 * - Debug：调试信息，仅开发时使用
 * - Info：一般信息，记录正常操作
 * - Warning：警告信息，记录潜在问题
 * - Error：错误信息，记录错误和异常
 */
enum class LogLevel {
    Debug = 0,      ///< 调试级别：详细的调试信息
    Info = 1,       ///< 信息级别：正常的操作信息
    Warning = 2,    ///< 警告级别：潜在问题警告
    Error = 3       ///< 错误级别：错误和异常信息
};

/**
 * @brief 日志管理器类
 * 
 * 单例模式的日志管理器，提供线程安全的日志记录功能。
 * 支持日志文件自动轮转，当文件超过指定大小时自动备份。
 */
class Logger
{
public:
    /**
     * @brief 获取单例实例
     * @return Logger单例指针
     */
    static Logger* instance();
    
    /**
     * @brief 设置日志文件路径
     * @param filePath 日志文件路径
     * 
     * 设置日志文件的保存位置，如果文件不存在会自动创建
     */
    void setLogFile(const QString& filePath);
    
    /**
     * @brief 设置日志级别
     * @param level 最低日志级别
     * 
     * 只有大于等于此级别的日志才会被记录
     */
    void setLogLevel(LogLevel level);
    
    /**
     * @brief 设置最大文件大小
     * @param maxSizeMB 最大文件大小（MB）
     * 
     * 当日志文件超过此大小时，会触发日志轮转
     */
    void setMaxFileSize(qint64 maxSizeMB);
    
    /**
     * @brief 设置最大备份数量
     * @param count 最大备份文件数量
     * 
     * 日志轮转时保留的备份文件数量
     */
    void setMaxBackupCount(int count);
    
    /**
     * @brief 记录调试日志
     * @param module 模块名称
     * @param message 日志消息
     */
    void debug(const QString& module, const QString& message);
    
    /**
     * @brief 记录信息日志
     * @param module 模块名称
     * @param message 日志消息
     */
    void info(const QString& module, const QString& message);
    
    /**
     * @brief 记录警告日志
     * @param module 模块名称
     * @param message 日志消息
     */
    void warning(const QString& module, const QString& message);
    
    /**
     * @brief 记录错误日志
     * @param module 模块名称
     * @param message 日志消息
     */
    void error(const QString& module, const QString& message);
    
    /**
     * @brief 通用日志记录方法
     * @param level 日志级别
     * @param module 模块名称
     * @param message 日志消息
     */
    void log(LogLevel level, const QString& module, const QString& message);
    
    /**
     * @brief 获取日志文件路径
     * @return 当前日志文件的完整路径
     */
    QString logFilePath() const;
    
    /**
     * @brief 获取最后的错误信息
     * @return 最后的错误描述，无错误返回空字符串
     */
    QString lastError() const;

private:
    /**
     * @brief 私有构造函数（单例模式）
     */
    Logger();
    
    /**
     * @brief 析构函数
     * 
     * 关闭日志文件并释放资源
     */
    ~Logger();
    
    /**
     * @brief 写入日志
     * @param level 日志级别
     * @param module 模块名称
     * @param message 日志消息
     * 
     * 内部方法，实际执行日志写入操作
     */
    void writeLog(LogLevel level, const QString& module, const QString& message);
    
    /**
     * @brief 检查文件大小
     * 
     * 检查日志文件是否超过最大限制，如果是则触发轮转
     */
    void checkFileSize();
    
    /**
     * @brief 执行日志轮转
     * 
     * 将当前日志文件重命名为备份文件，
     * 并创建新的日志文件
     */
    void rotateLog();
    
    /**
     * @brief 日志级别转字符串
     * @param level 日志级别
     * @return 级别字符串（如"DEBUG"、"INFO"）
     */
    QString levelToString(LogLevel level);
    
    /**
     * @brief 格式化日志消息
     * @param level 日志级别
     * @param module 模块名称
     * @param message 日志消息
     * @return 格式化后的完整日志行
     * 
     * 格式：[时间戳] [级别] [模块] 消息
     */
    QString formatMessage(LogLevel level, const QString& module, const QString& message);
    
    static Logger* m_instance;      ///< 单例实例指针
    
    QFile m_logFile;                ///< 日志文件对象
    QTextStream m_logStream;        ///< 文本流，用于写入日志
    QMutex m_mutex;                 ///< 互斥锁，保证线程安全
    LogLevel m_logLevel;            ///< 当前日志级别
    qint64 m_maxFileSize;           ///< 最大文件大小（字节）
    int m_maxBackupCount;           ///< 最大备份文件数量
    QString m_logFilePath;          ///< 日志文件路径
    QString m_lastError;            ///< 最后的错误信息
    bool m_initialized;             ///< 是否已初始化
};

/**
 * @brief 调试日志宏
 * @param module 模块名称
 * @param message 日志消息
 */
#define LOG_DEBUG(module, message) Logger::instance()->debug(module, message)

/**
 * @brief 信息日志宏
 * @param module 模块名称
 * @param message 日志消息
 */
#define LOG_INFO(module, message) Logger::instance()->info(module, message)

/**
 * @brief 警告日志宏
 * @param module 模块名称
 * @param message 日志消息
 */
#define LOG_WARNING(module, message) Logger::instance()->warning(module, message)

/**
 * @brief 错误日志宏
 * @param module 模块名称
 * @param message 日志消息
 */
#define LOG_ERROR(module, message) Logger::instance()->error(module, message)

#endif
