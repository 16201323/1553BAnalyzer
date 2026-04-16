/**
 * @file QwenProvider.h
 * @brief 千问（Qwen）AI模型提供商类定义
 * 
 * QwenProvider类继承自ModelAdapter，实现了与阿里云千问大语言模型的API对接。
 * 
 * 主要功能：
 * - 发送HTTP请求到千问API
 * - 处理API响应
 * - 支持上下文对话
 * - 支持自定义系统提示词
 * 
 * API文档：https://help.aliyun.com/document_detail/2712195.html
 * 
 * 支持的模型：
 * - qwen-turbo：快速响应模型
 * - qwen-plus：均衡模型
 * - qwen-max：高级模型
 * 
 * @author 1553BTools
 * @date 2024
 */

#ifndef QWENPROVIDER_H
#define QWENPROVIDER_H

#include "model/ModelAdapter.h"
#include "ui/widgets/AIQueryPanel.h"

/**
 * @brief 千问模型提供商类
 * 
 * 该类实现了与阿里云千问API的通信，
 * 使用QNetworkAccessManager发送HTTP请求。
 */
class QwenProvider : public ModelAdapter
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象指针
     */
    explicit QwenProvider(QObject *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~QwenProvider();
    
    /**
     * @brief 获取提供商ID
     * @return "qwen"
     */
    QString providerId() const override { return "qwen"; }
    
    /**
     * @brief 获取提供商显示名称
     * @return "千问"
     */
    QString providerName() const override { return "千问"; }
    
    /**
     * @brief 设置API密钥
     * @param key API密钥字符串
     */
    void setApiKey(const QString& key) override;
    
    /**
     * @brief 设置API URL
     * @param url API端点URL
     */
    void setApiUrl(const QString& url) override;
    
    /**
     * @brief 设置模型名称
     * @param name 模型名称（如qwen-plus）
     */
    void setModelName(const QString& name) override;
    
    /**
     * @brief 设置系统提示词
     * @param prompt 系统提示词内容
     */
    void setSystemPrompt(const QString& prompt) override;
    
    /**
     * @brief 设置请求超时时间
     * @param seconds 超时秒数
     */
    void setTimeout(int seconds) override;
    
    /**
     * @brief 发送请求到千问API
     * @param query 用户查询文本
     * @param context 上下文数据（JSON格式）
     */
    void sendRequest(const QString& query, const QJsonObject& context) override;
    
    /**
     * @brief 同步聊天方法
     * @param messages 消息列表
     * @param timeoutSeconds 超时时间（秒）
     * @return 模型响应
     */
    ModelResponse chat(const QList<ChatMessage>& messages, int timeoutSeconds = 60) override;
    
    /**
     * @brief 取消当前请求
     */
    void cancelRequest() override;
    
    /**
     * @brief 检查是否准备好发送请求
     * @return API Key已配置返回true
     */
    bool isReady() const override;

private slots:
    /**
     * @brief 网络响应完成槽函数
     * 
     * 处理API响应，解析JSON数据，发出responseReceived信号
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
