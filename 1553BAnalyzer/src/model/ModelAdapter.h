/**
 * @file ModelAdapter.h
 * @brief AI模型适配器基类定义
 * 
 * ModelAdapter是一个抽象基类，定义了所有AI模型提供商适配器的统一接口。
 * 各个具体的模型提供商（千问、DeepSeek、Kimi、Ollama）需要继承此类
 * 并实现所有纯虚函数。
 * 
 * 适配器模式说明：
 * - 统一不同AI模型的API调用方式
 * - 屏蔽各提供商API的差异
 * - 提供一致的响应格式
 * 
 * 使用示例：
 * @code
 * class QwenProvider : public ModelAdapter {
 *     // 实现所有纯虚函数...
 * };
 * 
 * ModelAdapter* adapter = new QwenProvider(this);
 * adapter->setApiKey("your-api-key");
 * adapter->setModelName("qwen-turbo");
 * adapter->sendRequest("你好", context);
 * @endcode
 * 
 * @author 1553BTools
 * @date 2024
 */

#ifndef MODELADAPTER_H
#define MODELADAPTER_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QUrl>
#include <QDateTime>
#include <QEventLoop>
#include <QTimer>

struct ChatMessage;

/**
 * @brief 模型响应结构
 * 
 * 存储AI模型返回的响应信息，包括内容、错误和统计信息
 */
struct ModelResponse {
    bool success = false;      // 请求是否成功（默认false，防止未初始化）
    QString content;           // 响应内容（AI生成的文本）
    QString error;             // 错误信息（失败时）
    int tokensUsed = 0;        // 使用的token数量
    double duration = 0.0;     // 请求耗时（秒）
};

/**
 * @brief AI模型适配器抽象基类
 * 
 * 定义了所有AI模型提供商必须实现的接口。
 * 使用Qt的信号槽机制进行异步通信。
 */
class ModelAdapter : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象指针
     */
    explicit ModelAdapter(QObject *parent = nullptr);
    
    /**
     * @brief 虚析构函数
     */
    virtual ~ModelAdapter();
    
    /**
     * @brief 获取提供商ID
     * @return 提供商唯一标识符（如"qwen"、"deepseek"）
     */
    virtual QString providerId() const = 0;
    
    /**
     * @brief 获取提供商显示名称
     * @return 提供商显示名称（如"千问"、"DeepSeek"）
     */
    virtual QString providerName() const = 0;
    
    /**
     * @brief 设置API密钥
     * @param key API密钥字符串
     * 
     * 各提供商的密钥格式可能不同
     */
    virtual void setApiKey(const QString& key) = 0;
    
    /**
     * @brief 设置API基础URL
     * @param url API基础URL
     * 
     * 用于支持私有部署或代理服务器
     */
    virtual void setApiUrl(const QString& url) = 0;
    
    /**
     * @brief 设置模型名称
     * @param name 模型名称（如"qwen-turbo"、"deepseek-chat"）
     */
    virtual void setModelName(const QString& name) = 0;
    
    /**
     * @brief 设置系统提示词
     * @param prompt 系统提示词内容
     * 
     * 系统提示词用于设定AI的角色和行为
     */
    virtual void setSystemPrompt(const QString& prompt) = 0;
    
    /**
     * @brief 设置请求超时时间
     * @param seconds 超时时间（秒）
     */
    virtual void setTimeout(int seconds) = 0;
    
    /**
     * @brief 发送请求到AI模型
     * @param query 用户查询文本
     * @param context 上下文信息（JSON格式）
     * 
     * 异步方法，响应通过responseReceived信号返回
     * 
     * @see responseReceived
     */
    virtual void sendRequest(const QString& query, const QJsonObject& context) = 0;
    
    /**
     * @brief 同步聊天方法
     * @param messages 消息列表
     * @param timeout 超时时间（秒）
     * @return 模型响应
     * 
     * 同步方法，会阻塞等待响应。用于报告生成等需要同步调用的场景。
     */
    virtual ModelResponse chat(const QList<ChatMessage>& messages, int timeoutSeconds = 60) = 0;
    
    /**
     * @brief 取消当前请求
     * 
     * 取消正在进行的请求，中止网络连接
     */
    virtual void cancelRequest() = 0;
    
    /**
     * @brief 检查适配器是否就绪
     * @return 就绪返回true，否则返回false
     * 
     * 检查必要的配置（如API密钥）是否已设置
     */
    virtual bool isReady() const = 0;
    
    QNetworkAccessManager* networkManager() const { return m_networkManager; }
    
    static QNetworkAccessManager* sharedNetworkManager();
    
protected:
    QNetworkAccessManager* m_networkManager;
    QString m_apiKey;
    QString m_apiUrl;
    QString m_modelName;
    QString m_systemPrompt;
    int m_timeout;
    bool m_ready;
    
    QJsonObject buildOpenAIRequestBody(const QString& query, const QJsonObject& context);
    void sendOpenAIRequest(const QString& query, const QJsonObject& context);
    ModelResponse openAIChat(const QList<ChatMessage>& messages, int timeoutSeconds);
    void cancelOpenAIRequest();
    ModelResponse parseOpenAIResponse(QNetworkReply* reply);
    QNetworkRequest createOpenAIRequest() const;
    
signals:
    /**
     * @brief 响应接收信号
     * @param response 模型响应结构
     * 
     * 当AI模型返回响应时发出此信号
     */
    void responseReceived(const ModelResponse& response);
    
    /**
     * @brief 错误发生信号
     * @param error 错误描述信息
     * 
     * 当请求过程中发生错误时发出此信号
     */
    void errorOccurred(const QString& error);
    
    /**
     * @brief 进度更新信号
     * @param status 当前状态描述
     * 
     * 用于报告请求进度（如"正在连接"、"正在处理"）
     */
    void progressUpdate(const QString& status);
};

#endif
