/**
 * @file ModelAdapter.cpp
 * @brief AI模型适配器基类实现
 * 
 * 本文件实现了AI模型适配器的基类，定义了所有模型适配器的通用接口。
 * 
 * ModelAdapter是抽象基类，具体的模型适配器需要继承此类并实现：
 * - sendRequest：发送请求到AI模型
 * - cancelRequest：取消正在进行的请求
 * - 设置API密钥、URL、模型名称等参数
 * 
 * 派生类：
 * - QwenProvider：千问模型适配器
 * - DeepSeekProvider：DeepSeek模型适配器
 * - KimiProvider：Kimi模型适配器
 * - OllamaProvider：Ollama本地模型适配器
 * 
 * @author 1553BTools
 * @date 2024
 */

#include "ModelAdapter.h"
#include "ui/widgets/AIQueryPanel.h"
#include <QApplication>

/**
 * @brief 获取全局共享的QNetworkAccessManager
 * 
 * 所有AI Provider共享同一个网络管理器实例，避免每个Provider
 * 各自创建独立实例导致的资源浪费（每个实例会创建内部线程和连接池）。
 * 使用静态局部变量实现线程安全的延迟初始化，应用退出时自动释放。
 */
QNetworkAccessManager* ModelAdapter::sharedNetworkManager()
{
    static QNetworkAccessManager* s_sharedManager = []() {
        QNetworkAccessManager* mgr = new QNetworkAccessManager();
        QObject::connect(qApp, &QApplication::aboutToQuit, mgr, &QNetworkAccessManager::deleteLater);
        return mgr;
    }();
    return s_sharedManager;
}

ModelAdapter::ModelAdapter(QObject *parent)
    : QObject(parent)
    , m_networkManager(sharedNetworkManager())
    , m_timeout(60)
    , m_ready(false)
{
}

ModelAdapter::~ModelAdapter()
{
}

QNetworkRequest ModelAdapter::createOpenAIRequest() const
{
    QNetworkRequest req((QUrl(m_apiUrl)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!m_apiKey.isEmpty()) {
        req.setRawHeader("Authorization", QString("Bearer %1").arg(m_apiKey).toUtf8());
    }
    return req;
}

QJsonObject ModelAdapter::buildOpenAIRequestBody(const QString& query, const QJsonObject& context)
{
    QJsonObject body;
    body["model"] = m_modelName;
    
    QJsonArray messages;
    
    QString systemPrompt = m_systemPrompt;
    if (context.contains("system_prompt") && context["system_prompt"].isString()) {
        systemPrompt = context["system_prompt"].toString();
    }
    
    if (!systemPrompt.isEmpty()) {
        QJsonObject systemMsg;
        systemMsg["role"] = "system";
        systemMsg["content"] = systemPrompt;
        messages.append(systemMsg);
    }
    
    QString contextStr;
    if (context.contains("data_count")) {
        QJsonObject dataContext;
        dataContext["data_count"] = context["data_count"];
        contextStr = QJsonDocument(dataContext).toJson(QJsonDocument::Compact);
    }
    
    QJsonObject userMsg;
    userMsg["role"] = "user";
    if (contextStr.isEmpty()) {
        userMsg["content"] = query;
    } else {
        userMsg["content"] = QString("当前数据量：%1\n\n用户问题：%2").arg(contextStr, query);
    }
    messages.append(userMsg);
    
    body["messages"] = messages;
    return body;
}

void ModelAdapter::sendOpenAIRequest(const QString& query, const QJsonObject& context)
{
    if (!m_ready) {
        ModelResponse response;
        response.success = false;
        response.error = tr("API Key未配置");
        emit responseReceived(response);
        return;
    }
    
    QJsonObject requestBody = buildOpenAIRequestBody(query, context);
    QJsonDocument doc(requestBody);
    
    QNetworkRequest req = createOpenAIRequest();
    QNetworkReply* reply = m_networkManager->post(req, doc.toJson());
    reply->setProperty("startTime", QDateTime::currentMSecsSinceEpoch());
    
    emit progressUpdate(tr("正在发送请求..."));
}

ModelResponse ModelAdapter::openAIChat(const QList<ChatMessage>& messages, int timeoutSeconds)
{
    ModelResponse response;
    response.success = false;
    
    if (!m_ready) {
        response.error = tr("API Key未配置");
        return response;
    }
    
    QJsonObject body;
    body["model"] = m_modelName;
    
    QJsonArray msgArray;
    
    if (!m_systemPrompt.isEmpty()) {
        QJsonObject systemMsg;
        systemMsg["role"] = "system";
        systemMsg["content"] = m_systemPrompt;
        msgArray.append(systemMsg);
    }
    
    for (const ChatMessage& msg : messages) {
        QJsonObject jsonMsg;
        switch (msg.role) {
            case ChatMessage::User:
                jsonMsg["role"] = "user";
                break;
            case ChatMessage::Assistant:
                jsonMsg["role"] = "assistant";
                break;
            case ChatMessage::System:
                jsonMsg["role"] = "system";
                break;
        }
        jsonMsg["content"] = msg.content;
        msgArray.append(jsonMsg);
    }
    
    body["messages"] = msgArray;
    
    QJsonDocument doc(body);
    QNetworkRequest req = createOpenAIRequest();
    
    QNetworkAccessManager localManager;
    QNetworkReply* reply = localManager.post(req, doc.toJson());
    
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeoutTimer.start(timeoutSeconds * 1000);
    
    loop.exec();
    
    if (!timeoutTimer.isActive()) {
        reply->abort();
        response.error = tr("请求超时(%1秒)").arg(timeoutSeconds);
        reply->deleteLater();
        return response;
    }
    timeoutTimer.stop();
    
    if (reply->error() != QNetworkReply::NoError) {
        response.error = reply->errorString();
        reply->deleteLater();
        return response;
    }
    
    response = parseOpenAIResponse(reply);
    reply->deleteLater();
    return response;
}

void ModelAdapter::cancelOpenAIRequest()
{
    if (m_networkManager) {
        for (QNetworkReply* reply : m_networkManager->findChildren<QNetworkReply*>()) {
            reply->abort();
        }
    }
}

ModelResponse ModelAdapter::parseOpenAIResponse(QNetworkReply* reply)
{
    ModelResponse response;
    response.success = false;
    response.duration = (QDateTime::currentMSecsSinceEpoch() - 
                         reply->property("startTime").toLongLong()) / 1000.0;
    
    if (reply->error() != QNetworkReply::NoError) {
        response.error = reply->errorString();
        return response;
    }
    
    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject result = doc.object();
    
    if (result.contains("error")) {
        response.error = result["error"].toObject()["message"].toString();
        return response;
    }
    
    if (result.contains("choices")) {
        QJsonArray choices = result["choices"].toArray();
        if (!choices.isEmpty()) {
            QJsonObject choice = choices[0].toObject();
            QJsonObject message = choice["message"].toObject();
            response.content = message["content"].toString();
            response.success = true;
        }
    }
    
    if (result.contains("usage")) {
        QJsonObject usage = result["usage"].toObject();
        response.tokensUsed = usage["total_tokens"].toInt();
    }
    
    return response;
}
