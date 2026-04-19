/**
 * @file ModelManager.h
 * @brief AI模型管理器类定义
 * 
 * ModelManager类是AI模型的管理中心，负责：
 * - 管理多个AI模型提供商（千问、DeepSeek、Kimi、Ollama）
 * - 处理模型选择和切换
 * - 发送查询请求和接收响应
 * - 提供单例访问接口
 * 
 * 模型ID格式：提供商ID.模型名称
 * 例如：qwen.qwen-turbo、deepseek.deepseek-chat
 * 
 * 使用示例：
 * @code
 * ModelManager::instance()->initialize();
 * ModelManager::instance()->setCurrentModel("qwen.qwen-turbo");
 * ModelManager::instance()->sendQuery("你好", context);
 * @endcode
 * 
 * @author 1553BTools
 * @date 2024
 */

#ifndef MODELMANAGER_H
#define MODELMANAGER_H

#include <QObject>
#include <QMap>
#include <QMutex>
#include "ModelAdapter.h"

/**
 * @brief AI模型管理器类
 * 
 * 该类采用单例模式，统一管理所有AI模型提供商。
 * 提供模型选择、查询发送、响应处理等功能。
 */
class ModelManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     * @return ModelManager单例指针
     */
    static ModelManager* instance();
    
    /**
     * @brief 初始化模型管理器
     * 
     * 加载配置中的模型提供商，设置默认模型
     */
    void initialize();
    
    /**
     * @brief 设置当前使用的模型
     * @param modelId 模型ID（格式：提供商.模型名）
     */
    void setCurrentModel(const QString& modelId);
    
    /**
     * @brief 获取当前使用的模型ID
     * @return 当前模型ID
     */
    QString currentModel() const;
    
    /**
     * @brief 发送查询请求
     * @param query 查询文本
     * @param context 上下文信息（JSON格式）
     * 
     * 根据当前选择的模型发送请求，响应通过信号返回
     */
    void sendQuery(const QString& query, const QJsonObject& context);
    
    /**
     * @brief 取消当前查询
     */
    void cancelQuery();
    
    /**
     * @brief 获取所有可用模型列表
     * @return 模型ID列表
     */
    QStringList availableModels() const;
    
    /**
     * @brief 获取模型的显示名称
     * @param modelId 模型ID
     * @return 显示名称（如"千问 - qwen-turbo"）
     */
    QString modelDisplayName(const QString& modelId) const;
    
    /**
     * @brief 检查是否正在查询
     * @return 正在查询返回true，否则返回false
     */
    bool isQuerying() const;
    
    /**
     * @brief 获取模型提供商适配器
     * @param modelId 模型ID（格式：提供商.模型名）
     * @return 适配器指针，未找到返回nullptr
     */
    ModelAdapter* getProvider(const QString& modelId);

signals:
    /**
     * @brief 查询开始信号
     */
    void queryStarted();
    
    /**
     * @brief 查询完成信号
     * @param response 模型响应
     */
    void queryFinished(const ModelResponse& response);
    
    /**
     * @brief 查询错误信号
     * @param error 错误信息
     */
    void queryError(const QString& error);
    
    /**
     * @brief 模型切换信号
     * @param modelId 新的模型ID
     */
    void modelChanged(const QString& modelId);

private slots:
    /**
     * @brief 适配器响应接收槽
     * @param response 模型响应
     */
    void onAdapterResponseReceived(const ModelResponse& response);
    
    /**
     * @brief 适配器错误发生槽
     * @param error 错误信息
     */
    void onAdapterErrorOccurred(const QString& error);

private:
    /**
     * @brief 私有构造函数（单例模式）
     * @param parent 父对象指针
     */
    explicit ModelManager(QObject *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~ModelManager();
    
    /**
     * @brief 加载模型提供商
     * 
     * 从配置中读取提供商信息并创建对应的适配器
     */
    void loadProviders();
    
    /**
     * @brief 创建模型提供商适配器
     * @param providerId 提供商ID
     * @return 适配器指针，失败返回nullptr
     */
    ModelAdapter* createProvider(const QString& providerId);
    
    static ModelManager* m_instance;               // 单例实例
    static QMutex m_instanceMutex;                 // 单例互斥锁
    
    QMap<QString, ModelAdapter*> m_providers;      // 提供商映射表
    QString m_currentModel;                        // 当前模型ID
    bool m_querying;                               // 是否正在查询
};

#endif
