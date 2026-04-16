/**
 * @file ConfigManager.cpp
 * @brief 配置管理器实现
 * 
 * 该文件实现了应用程序配置的管理功能，包括：
 * - 配置文件的加载和保存
 * - 默认配置的初始化
 * - 配置项的读取和设置
 * 
 * 配置文件加载策略：
 * 1. 首先检查程序目录/config/config.xml是否存在
 * 2. 如果不存在，从资源文件(":/config/config.xml")复制到程序目录
 * 3. 然后从程序目录读取配置
 * 4. 保存时保存到程序目录
 * 
 * @author 1553BTools
 * @date 2024
 */

#include "ConfigManager.h"
#include "XmlConfigParser.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include "utils/Logger.h"

ConfigManager* ConfigManager::m_instance = nullptr;

/**
 * @brief 构造函数，初始化默认配置
 * @param parent 父对象指针
 */
ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
    , m_configLoaded(false)
    , m_apiTimeout(60)
    , m_retryCount(3)
{
    LOG_DEBUG("ConfigManager", "ConfigManager实例创建");
    loadDefaults();
}

/**
 * @brief 析构函数
 */
ConfigManager::~ConfigManager()
{
    LOG_DEBUG("ConfigManager", "ConfigManager实例销毁");
}

/**
 * @brief 获取单例实例
 * @return ConfigManager单例指针
 */
ConfigManager* ConfigManager::instance()
{
    if (!m_instance) {
        m_instance = new ConfigManager();
    }
    return m_instance;
}

/**
 * @brief 加载默认配置值
 * 
 * 当配置文件不存在或加载失败时使用默认配置
 */
void ConfigManager::loadDefaults()
{
    LOG_DEBUG("ConfigManager", "加载默认配置");
    
    m_parserConfig.byteOrder = "little";
    m_parserConfig.header1 = 0xA5A5;
    m_parserConfig.header2 = 0xA5;
    m_parserConfig.dataHeader = 0xAABB;
    m_parserConfig.timestampUnit = 40;
    m_parserConfig.maxErrorTolerance = 0;
    
    m_ganttConfig.colorBC2RT = QColor("#3498DB");
    m_ganttConfig.colorRT2BC = QColor("#2ECC71");
    m_ganttConfig.colorRT2RT = QColor("#F39C12");
    m_ganttConfig.colorBroadcast = QColor("#9B59B6");
    m_ganttConfig.colorError = QColor("#E74C3C");
    
    m_databaseConfig.recordThreshold = 50000;
    
    m_defaultModel = "qwen.qwen-plus";
    m_apiTimeout = 60;
    m_retryCount = 3;
    m_reportFormat = "html";
    
    LOG_DEBUG("ConfigManager", "默认配置加载完成");
}

/**
 * @brief 从资源文件复制配置到程序目录
 * @param targetPath 目标文件路径
 * @return 复制成功返回true，失败返回false
 */
bool ConfigManager::copyConfigFromResource(const QString& targetPath)
{
    LOG_INFO("ConfigManager", QString("从资源文件复制配置到: %1").arg(targetPath));
    
    QFile resourceFile(":/config/config.xml");
    if (!resourceFile.exists()) {
        LOG_ERROR("ConfigManager", "资源文件中不存在config.xml");
        m_lastError = tr("资源文件中不存在默认配置");
        return false;
    }
    
    if (!resourceFile.open(QIODevice::ReadOnly)) {
        LOG_ERROR("ConfigManager", "无法打开资源文件中的config.xml");
        m_lastError = tr("无法打开资源文件中的默认配置");
        return false;
    }
    
    QByteArray content = resourceFile.readAll();
    resourceFile.close();
    
    QFileInfo fileInfo(targetPath);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            LOG_ERROR("ConfigManager", QString("无法创建配置目录: %1").arg(dir.absolutePath()));
            m_lastError = tr("无法创建配置目录: %1").arg(dir.absolutePath());
            return false;
        }
        LOG_INFO("ConfigManager", QString("创建配置目录: %1").arg(dir.absolutePath()));
    }
    
    QFile targetFile(targetPath);
    if (!targetFile.open(QIODevice::WriteOnly)) {
        LOG_ERROR("ConfigManager", QString("无法创建配置文件: %1").arg(targetPath));
        m_lastError = tr("无法创建配置文件: %1").arg(targetPath);
        return false;
    }
    
    targetFile.write(content);
    targetFile.close();
    
    LOG_INFO("ConfigManager", QString("配置文件复制成功: %1").arg(targetPath));
    return true;
}

/**
 * @brief 加载配置文件
 * @param filePath 配置文件路径，为空时使用默认路径
 * @return 加载成功返回true，失败返回false
 * 
 * 加载流程：
 * 1. 确定配置文件路径
 * 2. 如果配置文件不存在，从资源文件复制
 * 3. 解析配置文件
 */
bool ConfigManager::loadConfig(const QString& filePath)
{
    QString configPath = filePath;
    if (configPath.isEmpty()) {
        configPath = getConfigFilePath();
    }
    
    m_configFilePath = configPath;
    
    LOG_INFO("ConfigManager", QString("开始加载配置文件: %1").arg(configPath));
    
    QFile file(configPath);
    if (!file.exists()) {
        LOG_WARNING("ConfigManager", QString("配置文件不存在: %1").arg(configPath));
        
        if (!copyConfigFromResource(configPath)) {
            LOG_WARNING("ConfigManager", "从资源文件复制失败，使用默认配置");
            loadDefaults();
            emit configLoadFailed(m_lastError);
            return false;
        }
    }
    
    XmlConfigParser parser;
    if (!parser.parse(configPath)) {
        m_lastError = parser.lastError();
        LOG_ERROR("ConfigManager", QString("配置文件解析失败: %1").arg(m_lastError));
        loadDefaults();
        emit configLoadFailed(m_lastError);
        return false;
    }
    
    m_parserConfig = parser.parserConfig();
    m_ganttConfig = parser.ganttConfig();
    m_databaseConfig = parser.databaseConfig();
    m_modelProviders = parser.modelProviders();
    m_defaultModel = parser.defaultModel();
    m_apiTimeout = parser.apiTimeout();
    m_retryCount = parser.retryCount();
    m_reportFormat = parser.reportFormat();
    
    m_configLoaded = true;
    
    LOG_INFO("ConfigManager", QString("配置文件加载成功，共加载 %1 个模型提供商")
              .arg(m_modelProviders.size()));
    
    emit configLoaded();
    return true;
}

/**
 * @brief 保存配置到文件
 * @param filePath 保存路径，为空时使用当前配置文件路径
 * @return 保存成功返回true，失败返回false
 */
bool ConfigManager::saveConfig(const QString& filePath)
{
    QString configPath = filePath;
    if (configPath.isEmpty()) {
        configPath = m_configFilePath;
    }
    if (configPath.isEmpty()) {
        configPath = getConfigFilePath();
    }
    
    LOG_INFO("ConfigManager", QString("保存配置到文件: %1").arg(configPath));
    
    QFileInfo fileInfo(configPath);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            LOG_ERROR("ConfigManager", QString("无法创建配置目录: %1").arg(dir.absolutePath()));
            m_lastError = tr("无法创建配置目录: %1").arg(dir.absolutePath());
            return false;
        }
    }
    
    XmlConfigParser parser;
    bool result = parser.save(configPath, m_parserConfig, m_ganttConfig, m_databaseConfig,
                       m_modelProviders, m_defaultModel, m_apiTimeout, m_retryCount, m_reportFormat);
    
    if (result) {
        LOG_INFO("ConfigManager", "配置保存成功");
        // 通知所有模块配置已变更，即时生效
        emit configChanged();
    } else {
        LOG_ERROR("ConfigManager", QString("配置保存失败: %1").arg(parser.lastError()));
        m_lastError = parser.lastError();
    }
    
    return result;
}

/**
 * @brief 获取解析器配置
 * @return 解析器配置结构体
 */
ParserConfig ConfigManager::getParserConfig() const
{
    return m_parserConfig;
}

/**
 * @brief 获取甘特图配置
 * @return 甘特图配置结构体
 */
GanttConfig ConfigManager::getGanttConfig() const
{
    return m_ganttConfig;
}

/**
 * @brief 获取数据库配置
 * @return 数据库配置结构体
 */
DatabaseConfig ConfigManager::getDatabaseConfig() const
{
    return m_databaseConfig;
}

/**
 * @brief 获取模型提供商列表
 * @return 模型提供商列表
 */
QVector<ModelProvider> ConfigManager::getModelProviders() const
{
    return m_modelProviders;
}

/**
 * @brief 获取默认模型ID
 * @return 默认模型ID，格式为"providerId.instanceName"
 */
QString ConfigManager::getDefaultModel() const
{
    return m_defaultModel;
}

/**
 * @brief 获取API超时时间
 * @return 超时时间（秒）
 */
int ConfigManager::getApiTimeout() const
{
    return m_apiTimeout;
}

/**
 * @brief 获取API重试次数
 * @return 重试次数
 */
int ConfigManager::getRetryCount() const
{
    return m_retryCount;
}

/**
 * @brief 设置解析器配置
 * @param config 解析器配置结构体
 */
void ConfigManager::setParserConfig(const ParserConfig& config)
{
    m_parserConfig = config;
    LOG_DEBUG("ConfigManager", "解析器配置已更新");
}

/**
 * @brief 设置甘特图配置
 * @param config 甘特图配置结构体
 */
void ConfigManager::setGanttConfig(const GanttConfig& config)
{
    m_ganttConfig = config;
    LOG_DEBUG("ConfigManager", "甘特图配置已更新");
}

/**
 * @brief 设置数据库配置
 * @param config 数据库配置结构体
 */
void ConfigManager::setDatabaseConfig(const DatabaseConfig& config)
{
    m_databaseConfig = config;
    LOG_DEBUG("ConfigManager", QString("数据库配置已更新, 阈值: %1").arg(config.recordThreshold));
}

/**
 * @brief 设置模型提供商列表
 * @param providers 模型提供商列表
 */
void ConfigManager::setModelProviders(const QVector<ModelProvider>& providers)
{
    m_modelProviders = providers;
    LOG_DEBUG("ConfigManager", QString("模型提供商列表已更新，共 %1 个").arg(providers.size()));
}

/**
 * @brief 设置默认模型
 * @param model 默认模型ID
 */
void ConfigManager::setDefaultModel(const QString& model)
{
    m_defaultModel = model;
    LOG_DEBUG("ConfigManager", QString("默认模型已设置为: %1").arg(model));
}

void ConfigManager::setApiTimeout(int timeout)
{
    m_apiTimeout = timeout;
    LOG_DEBUG("ConfigManager", QString("API超时已设置为: %1秒").arg(timeout));
}

void ConfigManager::setRetryCount(int count)
{
    m_retryCount = count;
    LOG_DEBUG("ConfigManager", QString("重试次数已设置为: %1").arg(count));
}

QString ConfigManager::getReportFormat() const
{
    return m_reportFormat;
}

void ConfigManager::setReportFormat(const QString& format)
{
    m_reportFormat = format;
    LOG_DEBUG("ConfigManager", QString("报告格式已设置为: %1").arg(format));
}

/**
 * @brief 获取配置文件路径
 * @return 配置文件完整路径（程序目录/config/config.xml）
 */
QString ConfigManager::getConfigFilePath() const
{
    // 优先使用exe所在目录的config/config.xml
    QString appDir = QCoreApplication::applicationDirPath();
    QString appConfigPath = appDir + "/config/config.xml";
    
    if (QFile::exists(appConfigPath)) {
        LOG_INFO("ConfigManager", QString("使用exe目录配置文件: %1").arg(appConfigPath));
        return appConfigPath;
    }
    
    // exe目录不存在时，查找源码目录的resources/config/config.xml（开发环境）
    QString sourceConfigPath = QCoreApplication::applicationDirPath() + "/../../resources/config/config.xml";
    QString canonicalPath = QFileInfo(sourceConfigPath).canonicalFilePath();
    if (!canonicalPath.isEmpty() && QFile::exists(canonicalPath)) {
        LOG_INFO("ConfigManager", QString("使用源码目录配置文件: %1").arg(canonicalPath));
        return canonicalPath;
    }
    
    // 都不存在时返回默认路径（后续会从资源文件复制）
    LOG_INFO("ConfigManager", QString("配置文件未找到，将使用默认路径: %1").arg(appConfigPath));
    return appConfigPath;
}

/**
 * @brief 检查配置是否已加载
 * @return 已加载返回true，否则返回false
 */
bool ConfigManager::isConfigLoaded() const
{
    return m_configLoaded;
}

QString ConfigManager::configFilePath() const
{
    if (!m_configFilePath.isEmpty()) {
        return m_configFilePath;
    }
    return getConfigFilePath();
}

/**
 * @brief 获取最后的错误信息
 * @return 错误信息字符串
 */
QString ConfigManager::getLastError() const
{
    return m_lastError;
}
