/**
 * @file ModelManager.cpp
 * @brief AI模型管理器类实现
 * 
 * 本文件实现了ModelManager类，提供AI模型的统一管理功能。
 * 主要职责包括：
 * - 管理多个AI模型提供商的适配器实例
 * - 处理模型选择和切换
 * - 协调查询请求的发送和响应处理
 * - 提供单例访问接口
 * 
 * 支持的模型提供商：
 * - 千问（Qwen）：阿里云提供的大语言模型
 * - DeepSeek：深度求索提供的大语言模型
 * - Kimi：月之暗面提供的大语言模型
 * - Ollama：本地部署的开源大语言模型
 * 
 * @author 1553BTools
 * @date 2024
 */

#include "ModelManager.h"
#include "providers/QwenProvider.h"
#include "providers/DeepSeekProvider.h"
#include "providers/KimiProvider.h"
#include "providers/OllamaProvider.h"
#include "core/config/ConfigManager.h"
#include <QDebug>

/**
 * @brief 静态单例实例指针
 * 
 * 初始化为nullptr，首次调用instance()时创建实例
 */
ModelManager* ModelManager::m_instance = nullptr;
QMutex ModelManager::m_instanceMutex;

/**
 * @brief 构造函数
 * 
 * 私有构造函数，实现单例模式。
 * 初始化查询状态为false，表示当前没有正在进行的查询。
 * 
 * @param parent 父对象指针，通常为nullptr（单例对象）
 */
ModelManager::ModelManager(QObject *parent)
    : QObject(parent)
    , m_querying(false)
{
}

/**
 * @brief 析构函数
 * 
 * 清理所有提供商适配器实例，释放内存资源。
 * 使用qDeleteAll安全删除QMap中的所有指针值。
 */
ModelManager::~ModelManager()
{
    qDeleteAll(m_providers);
    m_providers.clear();
}

/**
 * @brief 获取单例实例
 * 
 * 实现懒汉式单例模式，首次调用时创建实例。
 * 该方法不是线程安全的，但在Qt应用程序中通常在主线程调用。
 * 
 * @return ModelManager单例指针，全局唯一
 * 
 * @note 如果需要在多线程环境中使用，应考虑添加线程安全机制
 */
ModelManager* ModelManager::instance()
{
    if (!m_instance) {
        QMutexLocker locker(&m_instanceMutex);
        if (!m_instance) {
            m_instance = new ModelManager();
        }
    }
    return m_instance;
}

/**
 * @brief 初始化模型管理器
 * 
 * 执行以下初始化操作：
 * 1. 从配置加载所有模型提供商
 * 2. 为每个提供商创建对应的适配器实例
 * 3. 设置默认模型（从配置文件读取）
 * 
 * 该方法应在应用程序启动时调用，且应在ConfigManager初始化之后调用。
 * 
 * @see loadProviders()
 * @see ConfigManager::getDefaultModel()
 */
void ModelManager::initialize()
{
    loadProviders();
    m_currentModel = ConfigManager::instance()->getDefaultModel();
}

/**
 * @brief 加载模型提供商
 * 
 * 从配置管理器读取提供商列表，为每个提供商创建适配器实例。
 * 创建适配器后，建立信号连接：
 * - responseReceived -> onAdapterResponseReceived：处理响应
 * - errorOccurred -> onAdapterErrorOccurred：处理错误
 * 
 * 该方法会先清理已有的提供商实例，确保不会内存泄漏。
 * 
 * @see createProvider()
 * @see ConfigManager::getModelProviders()
 */
void ModelManager::loadProviders()
{
    qDeleteAll(m_providers);
    m_providers.clear();
    
    auto providers = ConfigManager::instance()->getModelProviders();
    for (const auto& provider : providers) {
        ModelAdapter* adapter = createProvider(provider.id);
        if (adapter) {
            m_providers[provider.id] = adapter;
            
            connect(adapter, &ModelAdapter::responseReceived, 
                    this, &ModelManager::onAdapterResponseReceived);
            connect(adapter, &ModelAdapter::errorOccurred,
                    this, &ModelManager::onAdapterErrorOccurred);
        }
    }
}

/**
 * @brief 创建模型提供商适配器
 * 
 * 根据提供商ID创建对应的适配器实例。
 * 使用工厂模式，根据ID字符串创建不同的提供商对象。
 * 
 * 支持的提供商ID：
 * - "qwen"：创建QwenProvider实例
 * - "deepseek"：创建DeepSeekProvider实例
 * - "kimi"：创建KimiProvider实例
 * - "ollama"：创建OllamaProvider实例
 * 
 * @param providerId 提供商ID字符串
 * @return 适配器指针，未知提供商返回nullptr
 * 
 * @note 返回的指针所有权归ModelManager，由析构函数负责清理
 */
ModelAdapter* ModelManager::createProvider(const QString& providerId)
{
    if (providerId == "qwen") {
        return new QwenProvider(this);
    } else if (providerId == "deepseek") {
        return new DeepSeekProvider(this);
    } else if (providerId == "kimi") {
        return new KimiProvider(this);
    } else if (providerId == "ollama") {
        return new OllamaProvider(this);
    }
    return nullptr;
}

/**
 * @brief 设置当前使用的模型
 * 
 * 切换当前使用的AI模型，并持久化到配置文件。
 * 模型ID格式为"提供商.模型名"，例如"qwen.qwen-turbo"。
 * 
 * 切换模型后会发出modelChanged信号，通知UI更新。
 * 
 * @param modelId 模型ID，格式：提供商ID.模型名称
 * 
 * @note 该方法不会验证模型ID的有效性，调用者应确保ID正确
 * @see ConfigManager::setDefaultModel()
 */
void ModelManager::setCurrentModel(const QString& modelId)
{
    m_currentModel = modelId;
    ConfigManager::instance()->setDefaultModel(modelId);
    emit modelChanged(modelId);
}

/**
 * @brief 获取当前使用的模型ID
 * 
 * @return 当前模型ID，格式：提供商ID.模型名称
 */
QString ModelManager::currentModel() const
{
    return m_currentModel;
}

/**
 * @brief 发送查询请求
 * 
 * 向当前选定的AI模型发送查询请求。该方法执行以下步骤：
 * 
 * 1. 检查是否有正在进行的查询，如有则返回错误
 * 2. 解析模型ID，提取提供商ID和模型名称
 * 3. 验证提供商是否存在
 * 4. 从配置中获取模型的API密钥、URL等参数
 * 5. 配置适配器参数
 * 6. 设置查询状态并发送请求
 * 
 * 响应通过queryFinished信号异步返回，错误通过queryError信号返回。
 * 
 * @param query 查询文本，用户输入的自然语言问题
 * @param context 上下文信息，JSON格式，包含数据摘要等信息
 * 
 * @note 该方法是异步的，不会阻塞等待响应
 * @note 如果发生错误，会通过queryError信号通知
 * 
 * @see cancelQuery()
 * @see onAdapterResponseReceived()
 * @see onAdapterErrorOccurred()
 */
void ModelManager::sendQuery(const QString& query, const QJsonObject& context)
{
    if (m_querying) {
        emit queryError(tr("上一个查询正在进行中"));
        return;
    }
    
    QStringList parts = m_currentModel.split(".");
    if (parts.size() != 2) {
        emit queryError(tr("无效的模型ID: %1").arg(m_currentModel));
        return;
    }
    
    QString providerId = parts[0];
    QString modelName = parts[1];
    
    if (!m_providers.contains(providerId)) {
        emit queryError(tr("未找到模型提供商: %1").arg(providerId));
        return;
    }
    
    ModelAdapter* adapter = m_providers[providerId];
    
    auto providers = ConfigManager::instance()->getModelProviders();
    for (const auto& provider : providers) {
        if (provider.id == providerId) {
            for (const auto& instance : provider.instances) {
                if (instance.name == modelName) {
                    adapter->setApiKey(instance.apiKey);
                    adapter->setApiUrl(instance.apiUrl);
                    adapter->setModelName(instance.name);
                    adapter->setSystemPrompt(instance.systemPrompt);
                    adapter->setTimeout(ConfigManager::instance()->getApiTimeout());
                    break;
                }
            }
            break;
        }
    }
    
    m_querying = true;
    emit queryStarted();
    adapter->sendRequest(query, context);
}

/**
 * @brief 取消当前查询
 * 
 * 取消正在进行的查询请求。该方法执行以下操作：
 * 1. 解析当前模型ID获取提供商ID
 * 2. 调用对应适配器的cancelRequest方法
 * 3. 重置查询状态
 * 
 * 取消后不会收到响应或错误信号。
 * 
 * @note 如果没有正在进行的查询，该方法不会有任何效果
 * @see sendQuery()
 */
void ModelManager::cancelQuery()
{
    QStringList parts = m_currentModel.split(".");
    if (parts.size() == 2) {
        QString providerId = parts[0];
        if (m_providers.contains(providerId)) {
            m_providers[providerId]->cancelRequest();
        }
    }
    m_querying = false;
}

/**
 * @brief 获取所有可用模型列表
 * 
 * 遍历配置中的所有提供商和模型实例，收集已启用的模型ID。
 * 模型ID格式为"提供商ID.模型名称"。
 * 
 * @return 可用模型ID列表，只包含enabled为true的模型
 * 
 * @see ConfigManager::getModelProviders()
 */
QStringList ModelManager::availableModels() const
{
    QStringList models;
    auto providers = ConfigManager::instance()->getModelProviders();
    for (const auto& provider : providers) {
        for (const auto& instance : provider.instances) {
            if (instance.enabled) {
                models.append(QString("%1.%2").arg(provider.id, instance.name));
            }
        }
    }
    return models;
}

/**
 * @brief 获取模型的显示名称
 * 
 * 将模型ID转换为用户友好的显示名称。
 * 格式为"提供商名称 - 模型名称"，例如"千问 - qwen-turbo"。
 * 
 * @param modelId 模型ID，格式：提供商ID.模型名称
 * @return 显示名称，如果找不到提供商则返回原始modelId
 * 
 * @see ConfigManager::getModelProviders()
 */
QString ModelManager::modelDisplayName(const QString& modelId) const
{
    QStringList parts = modelId.split(".");
    if (parts.size() != 2) return modelId;
    
    auto providers = ConfigManager::instance()->getModelProviders();
    for (const auto& provider : providers) {
        if (provider.id == parts[0]) {
            return QString("%1 - %2").arg(provider.name, parts[1]);
        }
    }
    return modelId;
}

/**
 * @brief 检查是否正在查询
 * 
 * @return 正在查询返回true，否则返回false
 */
bool ModelManager::isQuerying() const
{
    return m_querying;
}

ModelAdapter* ModelManager::getProvider(const QString& modelId)
{
    QStringList parts = modelId.split(".");
    if (parts.size() != 2) {
        return nullptr;
    }
    
    QString providerId = parts[0];
    
    if (!m_providers.contains(providerId)) {
        return nullptr;
    }
    
    ModelAdapter* adapter = m_providers[providerId];
    
    QString modelName = parts[1];
    auto providers = ConfigManager::instance()->getModelProviders();
    for (const auto& provider : providers) {
        if (provider.id == providerId) {
            for (const auto& instance : provider.instances) {
                if (instance.name == modelName) {
                    adapter->setApiKey(instance.apiKey);
                    adapter->setApiUrl(instance.apiUrl);
                    adapter->setModelName(instance.name);
                    adapter->setSystemPrompt(instance.systemPrompt);
                    adapter->setTimeout(ConfigManager::instance()->getApiTimeout());
                    return adapter;
                }
            }
        }
    }
    
    return nullptr;
}

/**
 * @brief 适配器响应接收槽
 * 
 * 处理适配器发出的响应信号。该方法执行以下操作：
 * 1. 重置查询状态标志
 * 2. 发出queryFinished信号，通知调用方响应已就绪
 * 
 * @param response 模型响应，包含回复内容和可能的工具调用
 * 
 * @note 该槽函数在适配器的工作线程中被调用，但信号连接使用Qt::AutoConnection，
 *       所以实际执行会在ModelManager所在的线程
 */
void ModelManager::onAdapterResponseReceived(const ModelResponse& response)
{
    m_querying = false;
    emit queryFinished(response);
}

/**
 * @brief 适配器错误发生槽
 * 
 * 处理适配器发出的错误信号。该方法执行以下操作：
 * 1. 重置查询状态标志
 * 2. 发出queryError信号，通知调用方发生错误
 * 
 * @param error 错误描述信息
 * 
 * @note 错误可能包括网络错误、API错误、超时等
 */
void ModelManager::onAdapterErrorOccurred(const QString& error)
{
    m_querying = false;
    emit queryError(error);
}
