/**
 * @file ConfigManager.h
 * @brief 配置管理器类定义
 * 
 * ConfigManager类负责管理应用程序的所有配置信息，包括：
 * - 解析器配置：字节序、包头标识、数据头标识等
 * - 甘特图配置：各种消息类型的颜色
 * - AI模型配置：模型提供商、API密钥、URL等
 * 
 * 配置文件格式为XML，支持从资源文件复制默认配置。
 * 采用单例模式，全局唯一实例。
 * 
 * 使用示例：
 * @code
 * ConfigManager::instance()->loadConfig();
 * ParserConfig parserCfg = ConfigManager::instance()->getParserConfig();
 * QVector<ModelProvider> providers = ConfigManager::instance()->getModelProviders();
 * @endcode
 * 
 * @author 1553BTools
 * @date 2024
 */

#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QVector>
#include <QVariant>
#include <QColor>

/**
 * @brief 模型实例配置结构
 * 
 * 存储单个AI模型实例的配置信息
 */
struct ModelInstance {
    QString name;           // 模型名称（如qwen-turbo、deepseek-chat）
    QString apiKey;         // API密钥
    QString apiUrl;         // API基础URL
    QString systemPrompt;   // 系统提示词
    bool enabled;           // 是否启用
};

/**
 * @brief 模型提供商配置结构
 * 
 * 存储一个AI模型提供商的所有配置信息
 */
struct ModelProvider {
    QString id;                     // 提供商ID（如qwen、deepseek、kimi、ollama）
    QString name;                   // 提供商显示名称（如千问、DeepSeek）
    QVector<ModelInstance> instances; // 该提供商下的模型实例列表
};

/**
 * @brief 解析器配置结构
 * 
 * 存储1553B二进制文件解析器的配置参数
 */
struct ParserConfig {
    QString byteOrder;      // 字节序（"little"小端/"big"大端）
    quint16 header1;        // 包头1标识值
    quint16 header2;        // 包头2标识值
    quint16 dataHeader;     // 数据头标识值
    int timestampUnit;      // 时间戳单位（微秒）
    int maxErrorTolerance;  // 最大错误容差次数
};

/**
 * @brief 甘特图配置结构
 * 
 * 存储甘特图显示的颜色配置
 */
struct GanttConfig {
    QColor colorBC2RT;      // BC→RT消息颜色
    QColor colorRT2BC;      // RT→BC消息颜色
    QColor colorRT2RT;      // RT→RT消息颜色
    QColor colorBroadcast;  // 广播消息颜色
    QColor colorError;      // 错误状态颜色
};

/**
 * @brief 数据库配置结构
 * 
 * 存储数据库存储模式的配置参数
 */
struct DatabaseConfig {
    int recordThreshold;    // 使用数据库模式的记录数阈值
};

/**
 * @brief 语音识别引擎配置
 *
 * 包含Vosk引擎运行所需的全部参数。
 * 模型路径指向包含am、conf、graph等子目录的模型文件夹。
 */
struct SpeechEngineConfig {
    QString modelPath;      // Vosk模型目录路径
    int sampleRate;         // 音频采样率（Hz），Vosk要求16000Hz
    bool enabled;           // 是否启用语音识别功能

    SpeechEngineConfig()
        : modelPath("models/vosk-model-cn-0.22")
        , sampleRate(16000)
        , enabled(true)
    {}
};

/**
 * @brief 音频采集配置
 *
 * 包含麦克风音频采集所需的参数。
 */
struct AudioCaptureConfig {
    int bufferDurationMs;   // 音频缓冲区大小（毫秒）
    int channelCount;       // 音频通道数，Vosk仅支持单声道
    int sampleSize;         // 音频采样位深，Vosk要求16位

    AudioCaptureConfig()
        : bufferDurationMs(100)
        , channelCount(1)
        , sampleSize(16)
    {}
};

/**
 * @brief 语音识别运行时配置
 *
 * 控制语音识别器的运行时行为。
 */
struct SpeechRuntimeConfig {
    bool autoStop;          // 是否启用自动停止
    int silenceTimeoutMs;   // 静音超时时间（毫秒）
    qreal silenceThreshold; // 静音阈值（0.0-1.0）

    SpeechRuntimeConfig()
        : autoStop(false)
        , silenceTimeoutMs(2000)
        , silenceThreshold(0.01)
    {}
};

/**
 * @brief 语音识别完整配置
 *
 * 聚合所有语音识别相关的配置项。
 */
struct SpeechConfig {
    SpeechEngineConfig engine;      // 引擎配置
    AudioCaptureConfig capture;     // 音频采集配置
    SpeechRuntimeConfig runtime;    // 运行时配置
};

/**
 * @brief 配置管理器类
 * 
 * 该类采用单例模式，统一管理应用程序的所有配置。
 * 配置从XML文件加载，支持保存修改。
 */
class ConfigManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     * @return ConfigManager单例指针
     */
    static ConfigManager* instance();
    
    /**
     * @brief 加载配置文件
     * @param filePath 配置文件路径，默认使用标准路径
     * @return 加载成功返回true，失败返回false
     * 
     * 如果配置文件不存在，会从资源文件复制默认配置
     */
    bool loadConfig(const QString& filePath = QString());
    
    // 获取当前配置文件路径
    QString configFilePath() const;
    
    /**
     * @brief 保存配置文件
     * @param filePath 配置文件路径，默认使用当前路径
     * @return 保存成功返回true，失败返回false
     */
    bool saveConfig(const QString& filePath = QString());
    
    /**
     * @brief 加载默认配置
     * 
     * 重置所有配置为默认值
     */
    void loadDefaults();
    
    /**
     * @brief 获取解析器配置
     * @return 解析器配置结构
     */
    ParserConfig getParserConfig() const;
    
    /**
     * @brief 获取甘特图配置
     * @return 甘特图配置结构
     */
    GanttConfig getGanttConfig() const;
    
    /**
     * @brief 获取数据库配置
     * @return 数据库配置结构
     */
    DatabaseConfig getDatabaseConfig() const;
    
    /**
     * @brief 获取模型提供商列表
     * @return 模型提供商数组
     */
    QVector<ModelProvider> getModelProviders() const;
    
    /**
     * @brief 获取默认模型ID
     * @return 默认模型ID（格式：提供商.模型名）
     */
    QString getDefaultModel() const;
    
    /**
     * @brief 获取API超时时间
     * @return 超时时间（毫秒）
     */
    int getApiTimeout() const;
    
    /**
     * @brief 获取重试次数
     * @return 失败重试次数
     */
    int getRetryCount() const;
    
    /**
     * @brief 设置解析器配置
     * @param config 解析器配置结构
     */
    void setParserConfig(const ParserConfig& config);
    
    /**
     * @brief 设置甘特图配置
     * @param config 甘特图配置结构
     */
    void setGanttConfig(const GanttConfig& config);
    
    /**
     * @brief 设置数据库配置
     * @param config 数据库配置结构
     */
    void setDatabaseConfig(const DatabaseConfig& config);
    
    /**
     * @brief 设置模型提供商列表
     * @param providers 模型提供商数组
     */
    void setModelProviders(const QVector<ModelProvider>& providers);
    
    /**
     * @brief 设置默认模型
     * @param model 模型ID（格式：提供商.模型名）
     */
    void setDefaultModel(const QString& model);
    
    /**
     * @brief 设置API超时时间
     * @param timeout 超时时间（毫秒）
     */
    void setApiTimeout(int timeout);
    
    /**
     * @brief 设置重试次数
     * @param count 失败重试次数
     */
    void setRetryCount(int count);
    
    QString getReportFormat() const;
    void setReportFormat(const QString& format);
    
    /**
     * @brief 获取语音识别配置
     * @return 语音识别配置结构
     */
    SpeechConfig getSpeechConfig() const;
    
    /**
     * @brief 设置语音识别配置
     * @param config 语音识别配置结构
     */
    void setSpeechConfig(const SpeechConfig& config);
    
    /**
     * @brief 获取配置文件路径
     * @return 配置文件的完整路径
     */
    QString getConfigFilePath() const;
    
    /**
     * @brief 检查配置是否已加载
     * @return 已加载返回true，否则返回false
     */
    bool isConfigLoaded() const;
    
    /**
     * @brief 获取最后的错误信息
     * @return 错误信息字符串，无错误返回空字符串
     */
    QString getLastError() const;

signals:
    void configLoaded();
    
    void configLoadFailed(const QString& error);
    
    // 配置变更信号，设置面板保存后发出，通知各模块即时生效
    void configChanged();

private:
    /**
     * @brief 私有构造函数（单例模式）
     * @param parent 父对象指针
     */
    explicit ConfigManager(QObject *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~ConfigManager();
    
    /**
     * @brief 从资源文件复制配置
     * @param targetPath 目标文件路径
     * @return 复制成功返回true，失败返回false
     * 
     * 当配置文件不存在时，从Qt资源系统复制默认配置
     */
    bool copyConfigFromResource(const QString& targetPath);
    
    static ConfigManager* m_instance;       // 单例实例指针
    
    ParserConfig m_parserConfig;            // 解析器配置
    GanttConfig m_ganttConfig;              // 甘特图配置
    DatabaseConfig m_databaseConfig;        // 数据库配置
    QVector<ModelProvider> m_modelProviders; // 模型提供商列表
    QString m_defaultModel;                 // 默认模型ID
    int m_apiTimeout;                       // API超时时间（毫秒）
    int m_retryCount;                       // 失败重试次数
    QString m_reportFormat;                 // 智能分析报告默认格式（html/pdf）
    SpeechConfig m_speechConfig;            // 语音识别配置
    
    bool m_configLoaded;                    // 配置是否已加载
    QString m_lastError;                    // 最后的错误信息
    QString m_configFilePath;               // 配置文件路径
};

#endif
