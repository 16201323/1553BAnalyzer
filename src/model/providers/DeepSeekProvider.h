/**
 * @file DeepSeekProvider.h
 * @brief DeepSeek AI模型提供商类定义
 * 
 * DeepSeekProvider类继承自ModelAdapter，实现了与DeepSeek大语言模型的API对接。
 * 
 * 主要功能：
 * - 发送HTTP请求到DeepSeek API
 * - 处理API响应
 * - 支持上下文对话
 * - 支持自定义系统提示词
 * 
 * API文档：https://platform.deepseek.com/api-docs/
 * 
 * 支持的模型：
 * - deepseek-chat：通用对话模型
 * - deepseek-coder：代码专用模型
 * 
 * @author 1553BTools
 * @date 2024
 */

#ifndef DEEPSEEKPROVIDER_H
#define DEEPSEEKPROVIDER_H

#include "model/ModelAdapter.h"
#include "ui/widgets/AIQueryPanel.h"

/**
 * @brief DeepSeek模型提供商类
 * 
 * 该类实现了与DeepSeek API的通信，
 * 使用QNetworkAccessManager发送HTTP请求。
 * DeepSeek API兼容OpenAI格式。
 */
class DeepSeekProvider : public ModelAdapter
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象指针
     */
    explicit DeepSeekProvider(QObject *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~DeepSeekProvider();
    
    /**
     * @brief 获取提供商ID
     * @return "deepseek"
     */
    QString providerId() const override { return "deepseek"; }
    
    /**
     * @brief 获取提供商显示名称
     * @return "DeepSeek"
     */
    QString providerName() const override { return "DeepSeek"; }
    
    void setApiKey(const QString& key) override;
    void setApiUrl(const QString& url) override;
    void setModelName(const QString& name) override;
    void setSystemPrompt(const QString& prompt) override;
    void setTimeout(int seconds) override;
    
    void sendRequest(const QString& query, const QJsonObject& context) override;
    void cancelRequest() override;
    
    ModelResponse chat(const QList<ChatMessage>& messages, int timeoutSeconds = 60) override;
    
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
