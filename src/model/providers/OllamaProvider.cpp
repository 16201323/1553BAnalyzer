#include "OllamaProvider.h"
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QUrl>
#include <QEventLoop>
#include <QTimer>

OllamaProvider::OllamaProvider(QObject *parent)
    : ModelAdapter(parent)
{
    m_apiUrl = "http://localhost:11434/api/chat";
    m_modelName = "qwen2.5:7b";
    m_timeout = 120;
    m_ready = true;
}

OllamaProvider::~OllamaProvider()
{
}

void OllamaProvider::setApiKey(const QString& key)
{
    Q_UNUSED(key)
}

void OllamaProvider::setApiUrl(const QString& url)
{
    m_apiUrl = url;
}

void OllamaProvider::setModelName(const QString& name)
{
    m_modelName = name;
}

void OllamaProvider::setSystemPrompt(const QString& prompt)
{
    m_systemPrompt = prompt;
}

void OllamaProvider::setTimeout(int seconds)
{
    m_timeout = seconds;
}

bool OllamaProvider::isReady() const
{
    return m_ready;
}

void OllamaProvider::sendRequest(const QString& query, const QJsonObject& context)
{
    QJsonObject requestBody = buildRequestBody(query, context);
    QJsonDocument doc(requestBody);
    
    QNetworkRequest req((QUrl(m_apiUrl)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QNetworkReply* reply = m_networkManager->post(req, doc.toJson());
    reply->setProperty("startTime", QDateTime::currentMSecsSinceEpoch());
    
    connect(reply, &QNetworkReply::finished, this, &OllamaProvider::onReplyFinished);
    
    emit progressUpdate(tr("正在发送请求到Ollama..."));
}

void OllamaProvider::cancelRequest()
{
    cancelOpenAIRequest();
}

QJsonObject OllamaProvider::buildRequestBody(const QString& query, const QJsonObject& context)
{
    QJsonObject body;
    body["model"] = m_modelName;
    body["stream"] = false;
    
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

void OllamaProvider::onReplyFinished()
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
    
    if (result.contains("error")) {
        response.success = false;
        response.error = result["error"].toString();
        emit responseReceived(response);
        reply->deleteLater();
        return;
    }
    
    if (result.contains("message")) {
        QJsonObject message = result["message"].toObject();
        response.content = message["content"].toString();
        response.success = true;
    }
    
    if (result.contains("eval_count")) {
        response.tokensUsed = result["eval_count"].toInt() + 
                              result["prompt_eval_count"].toInt();
    }
    
    emit responseReceived(response);
    reply->deleteLater();
}

ModelResponse OllamaProvider::chat(const QList<ChatMessage>& messages, int timeoutSeconds)
{
    ModelResponse response;
    response.success = false;
    
    QJsonObject body;
    body["model"] = m_modelName;
    body["stream"] = false;
    
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
    
    QNetworkRequest req((QUrl(m_apiUrl)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
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
    
    if (result.contains("error")) {
        response.error = result["error"].toString();
        reply->deleteLater();
        return response;
    }
    
    if (result.contains("message")) {
        QJsonObject message = result["message"].toObject();
        response.content = message["content"].toString();
        response.success = true;
    }
    
    if (result.contains("eval_count")) {
        response.tokensUsed = result["eval_count"].toInt() + 
                              result["prompt_eval_count"].toInt();
    }
    
    reply->deleteLater();
    return response;
}
