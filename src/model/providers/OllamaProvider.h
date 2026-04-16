/** 
 * @file OllamaProvider.h
 * @brief Ollama本地模型提供商类定义
 * 
 * OllamaProvider类继承自ModelAdapter，实现了与本地Ollama服务的API对接。
 * 
 * 主要功能：
 * - 发送HTTP请求到本地Ollama服务
 * - 处理API响应
 * - 支持上下文对话
 * - 支持自定义系统提示词
 * 
 * 特点：
 * - 无需API Key（本地部署）
 * - 支持多种开源模型（Llama、Mistral、Qwen等）
 * - 数据不出本地，安全性高
 * 
 * API文档：https://github.com/ollama/ollama/blob/main/docs/api.md
 * 
 * 常用模型：
 * - llama2：Meta Llama 2模型
 * - mistral：Mistral AI模型
 * - qwen：阿里千问开源版
 * - deepseek-coder：DeepSeek代码模型
 * 
 * @author 1553BTools
 * @date 2024
 */

#ifndef OLLAMAPROVIDER_H
#define OLLAMAPROVIDER_H

#include "model/ModelAdapter.h"
#include "ui/widgets/AIQueryPanel.h"

/**
 * @brief Ollama本地模型提供商类
 * 
 * 该类实现了与本地Ollama服务的通信，
 * 使用QNetworkAccessManager发送HTTP请求。
 * Ollama默认运行在 http://localhost:11434
 */
class OllamaProvider : public ModelAdapter
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象指针
     */
    explicit OllamaProvider(QObject *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~OllamaProvider();
    
    /**
     * @brief 获取提供商ID
     * @return "ollama"
     */
    QString providerId() const override { return "ollama"; }
    
    /**
     * @brief 获取提供商显示名称
     * @return "Ollama"
     */
    QString providerName() const override { return "Ollama"; }
    
    /**
     * @brief 设置API密钥（Ollama不需要，空实现）
     * @param key API密钥（忽略）
     */
    void setApiKey(const QString& key) override;
    
    void setApiUrl(const QString& url) override;
    void setModelName(const QString& name) override;
    void setSystemPrompt(const QString& prompt) override;
    void setTimeout(int seconds) override;
    
    void sendRequest(const QString& query, const QJsonObject& context) override;
    void cancelRequest() override;
    
    ModelResponse chat(const QList<ChatMessage>& messages, int timeoutSeconds = 60) override;
    
    /**
     * @brief 检查是否准备好发送请求
     * @return Ollama始终准备好（无需API Key）
     */
    bool isReady() const override;

private slots:
    /**
     * @brief 网络响应完成槽函数
     */
    void onReplyFinished();

private:
    /**
     * @brief 构建请求体
     * @param query 用户查询文本
     * @param context 上下文数据
     * @return JSON请求体对象
     */
    QJsonObject buildRequestBody(const QString& query, const QJsonObject& context);
};

#endif
