#include "DeepSeekProvider.h"
#include "utils/Logger.h"

DeepSeekProvider::DeepSeekProvider(QObject *parent)
    : ModelAdapter(parent)
{
    m_apiUrl = "https://api.deepseek.com/v1/chat/completions";
    m_modelName = "deepseek-chat";
    m_timeout = 60;
    m_ready = false;
}

DeepSeekProvider::~DeepSeekProvider()
{
}

void DeepSeekProvider::setApiKey(const QString& key)
{
    m_apiKey = key;
    m_ready = !key.isEmpty();
}

void DeepSeekProvider::setApiUrl(const QString& url)
{
    m_apiUrl = url;
}

void DeepSeekProvider::setModelName(const QString& name)
{
    m_modelName = name;
}

void DeepSeekProvider::setSystemPrompt(const QString& prompt)
{
    m_systemPrompt = prompt;
}

void DeepSeekProvider::setTimeout(int seconds)
{
    m_timeout = seconds;
}

bool DeepSeekProvider::isReady() const
{
    return m_ready;
}

void DeepSeekProvider::sendRequest(const QString& query, const QJsonObject& context)
{
    if (!m_ready) {
        ModelResponse response;
        response.success = false;
        response.error = tr(u8"API Key未配置");
        emit responseReceived(response);
        return;
    }
    
    QJsonObject requestBody = buildOpenAIRequestBody(query, context);
    QJsonDocument doc(requestBody);
    
    QNetworkRequest req = createOpenAIRequest();
    QNetworkReply* reply = m_networkManager->post(req, doc.toJson());
    reply->setProperty("startTime", QDateTime::currentMSecsSinceEpoch());
    
    connect(reply, &QNetworkReply::finished, this, &DeepSeekProvider::onReplyFinished);
    
    emit progressUpdate(tr(u8"正在发送请求到DeepSeek..."));
}

void DeepSeekProvider::cancelRequest()
{
    cancelOpenAIRequest();
}

QJsonObject DeepSeekProvider::buildRequestBody(const QString& query, const QJsonObject& context)
{
    return buildOpenAIRequestBody(query, context);
}

void DeepSeekProvider::onReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    ModelResponse response = parseOpenAIResponse(reply);
    
    /* 如果解析失败且没有错误信息，添加默认错误描述 */
    if (!response.success && response.error.isEmpty()) {
        response.error = tr(u8"DeepSeek API返回了无法解析的响应");
        LOG_ERROR("DeepSeekProvider", QString::fromUtf8(u8"响应解析失败，无错误信息"));
    }
    
    if (response.success) {
        LOG_INFO("DeepSeekProvider", QString::fromUtf8(u8"解析成功，内容长度: %1").arg(response.content.length()));
    } else {
        LOG_ERROR("DeepSeekProvider", QString::fromUtf8(u8"响应错误: %1").arg(response.error));
    }
    
    emit responseReceived(response);
    reply->deleteLater();
}

ModelResponse DeepSeekProvider::chat(const QList<ChatMessage>& messages, int timeoutSeconds)
{
    return openAIChat(messages, timeoutSeconds);
}
