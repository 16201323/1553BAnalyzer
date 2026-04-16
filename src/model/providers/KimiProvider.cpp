#include "KimiProvider.h"

KimiProvider::KimiProvider(QObject *parent)
    : ModelAdapter(parent)
{
    m_apiUrl = "https://api.moonshot.cn/v1/chat/completions";
    m_modelName = "moonshot-v1-8k";
    m_timeout = 60;
    m_ready = false;
}

KimiProvider::~KimiProvider()
{
}

void KimiProvider::setApiKey(const QString& key)
{
    m_apiKey = key;
    m_ready = !key.isEmpty();
}

void KimiProvider::setApiUrl(const QString& url)
{
    m_apiUrl = url;
}

void KimiProvider::setModelName(const QString& name)
{
    m_modelName = name;
}

void KimiProvider::setSystemPrompt(const QString& prompt)
{
    m_systemPrompt = prompt;
}

void KimiProvider::setTimeout(int seconds)
{
    m_timeout = seconds;
}

bool KimiProvider::isReady() const
{
    return m_ready;
}

void KimiProvider::sendRequest(const QString& query, const QJsonObject& context)
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
    
    connect(reply, &QNetworkReply::finished, this, &KimiProvider::onReplyFinished);
    
    emit progressUpdate(tr("正在发送请求到Kimi..."));
}

void KimiProvider::cancelRequest()
{
    cancelOpenAIRequest();
}

QJsonObject KimiProvider::buildRequestBody(const QString& query, const QJsonObject& context)
{
    return buildOpenAIRequestBody(query, context);
}

void KimiProvider::onReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    ModelResponse response = parseOpenAIResponse(reply);
    emit responseReceived(response);
    reply->deleteLater();
}

ModelResponse KimiProvider::chat(const QList<ChatMessage>& messages, int timeoutSeconds)
{
    return openAIChat(messages, timeoutSeconds);
}
