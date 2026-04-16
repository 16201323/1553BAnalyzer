#include "QwenProvider.h"
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QUrl>
#include <QEventLoop>
#include <QTimer>

QwenProvider::QwenProvider(QObject *parent)
    : ModelAdapter(parent)
{
    m_apiUrl = "https://dashscope.aliyuncs.com/api/v1/services/aigc/text-generation/generation";
    m_modelName = "qwen-plus";
    m_timeout = 60;
    m_ready = false;
}

QwenProvider::~QwenProvider()
{
}

void QwenProvider::setApiKey(const QString& key)
{
    m_apiKey = key;
    m_ready = !key.isEmpty();
}

void QwenProvider::setApiUrl(const QString& url)
{
    m_apiUrl = url;
}

void QwenProvider::setModelName(const QString& name)
{
    m_modelName = name;
}

void QwenProvider::setSystemPrompt(const QString& prompt)
{
    m_systemPrompt = prompt;
}

void QwenProvider::setTimeout(int seconds)
{
    m_timeout = seconds;
}

bool QwenProvider::isReady() const
{
    return m_ready;
}

void QwenProvider::sendRequest(const QString& query, const QJsonObject& context)
{
    if (!m_ready) {
        ModelResponse response;
        response.success = false;
        response.error = tr("API Key未配置");
        emit responseReceived(response);
        return;
    }
    
    QJsonObject requestBody = buildRequestBody(query, context);
    QJsonDocument doc(requestBody);
    
    QNetworkRequest req = createOpenAIRequest();
    QNetworkReply* reply = m_networkManager->post(req, doc.toJson());
    reply->setProperty("startTime", QDateTime::currentMSecsSinceEpoch());
    
    connect(reply, &QNetworkReply::finished, this, &QwenProvider::onReplyFinished);
    
    emit progressUpdate(tr("正在发送请求到千问..."));
}

void QwenProvider::cancelRequest()
{
    cancelOpenAIRequest();
}

QJsonObject QwenProvider::buildRequestBody(const QString& query, const QJsonObject& context)
{
    QJsonObject body;
    body["model"] = m_modelName;
    
    QJsonObject input;
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
    
    input["messages"] = messages;
    body["input"] = input;
    
    QJsonObject parameters;
    parameters["result_format"] = "message";
    body["parameters"] = parameters;
    
    return body;
}

void QwenProvider::onReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    ModelResponse response;
    response.duration = (QDateTime::currentMSecsSinceEpoch() - 
                         reply->property("startTime").toLongLong()) / 1000.0;
    
    if (reply->error() != QNetworkReply::NoError) {
        response.success = false;
        response.error = reply->errorString();
        emit responseReceived(response);
        reply->deleteLater();
        return;
    }
    
    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject result = doc.object();
    
    if (result.contains("code")) {
        response.success = false;
        response.error = result["message"].toString();
        emit responseReceived(response);
        reply->deleteLater();
        return;
    }
    
    if (result.contains("output")) {
        QJsonObject output = result["output"].toObject();
        if (output.contains("choices")) {
            QJsonArray choices = output["choices"].toArray();
            if (!choices.isEmpty()) {
                QJsonObject choice = choices[0].toObject();
                QJsonObject message = choice["message"].toObject();
                response.content = message["content"].toString();
                response.success = true;
            }
        }
    }
    
    if (result.contains("usage")) {
        QJsonObject usage = result["usage"].toObject();
        response.tokensUsed = usage["total_tokens"].toInt();
    }
    
    emit responseReceived(response);
    reply->deleteLater();
}

ModelResponse QwenProvider::chat(const QList<ChatMessage>& messages, int timeoutSeconds)
{
    ModelResponse response;
    response.success = false;
    
    if (!m_ready) {
        response.error = tr("API Key未配置");
        return response;
    }
    
    QJsonObject body;
    body["model"] = m_modelName;
    
    QJsonObject input;
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
    
    input["messages"] = msgArray;
    body["input"] = input;
    
    QJsonObject parameters;
    parameters["result_format"] = "message";
    body["parameters"] = parameters;
    
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
    
    QByteArray data = reply->readAll();
    QJsonDocument respDoc = QJsonDocument::fromJson(data);
    QJsonObject result = respDoc.object();
    
    if (result.contains("code")) {
        response.error = result["message"].toString();
        reply->deleteLater();
        return response;
    }
    
    if (result.contains("output")) {
        QJsonObject output = result["output"].toObject();
        if (output.contains("choices")) {
            QJsonArray choices = output["choices"].toArray();
            if (!choices.isEmpty()) {
                QJsonObject choice = choices[0].toObject();
                QJsonObject message = choice["message"].toObject();
                response.content = message["content"].toString();
                response.success = true;
            }
        }
    }
    
    if (result.contains("usage")) {
        QJsonObject usage = result["usage"].toObject();
        response.tokensUsed = usage["total_tokens"].toInt();
    }
    
    reply->deleteLater();
    return response;
}
