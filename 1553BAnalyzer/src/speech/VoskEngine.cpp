/**
 * @file VoskEngine.cpp
 * @brief Vosk语音识别引擎封装类实现
 *
 * 本文件实现了VoskEngine类的所有方法，包括：
 * - 模型加载与释放
 * - 识别器创建与销毁
 * - 音频数据处理
 * - 识别结果解析
 *
 * 使用Vosk C API（vosk_api.h）实现底层调用。
 * 通过void*指针持有Vosk模型和识别器实例，
 * 避免在头文件中暴露Vosk的内部结构。
 *
 * @author 1553BTools
 * @date 2026
 */

#include "VoskEngine.h"
#include "core/config/ConfigManager.h"
#include "vosk_api.h"
#include "utils/Logger.h"
#include <QCoreApplication>

/**
 * @brief 构造函数，初始化成员变量
 */
VoskEngine::VoskEngine()
    : m_model(nullptr)
    , m_recognizer(nullptr)
    , m_modelLoaded(false)
    , m_recognizerReady(false)
{
}

/**
 * @brief 析构函数，确保资源被正确释放
 */
VoskEngine::~VoskEngine()
{
    cleanup();
}

/**
 * @brief 加载Vosk语音识别模型
 *
 * 模型加载过程：
 * 1. 检查是否已有模型，有则先释放
 * 2. 将相对路径转换为绝对路径
 * 3. 调用vosk_model_new()加载模型
 * 4. 检查加载结果
 *
 * @param modelPath 模型目录路径
 * @return 加载成功返回true，失败返回false
 */
bool VoskEngine::loadModel(const QString& modelPath)
{
    /* 如果已有模型，先释放 */
    if (m_model) {
        vosk_model_free(static_cast<VoskModel*>(m_model));
        m_model = nullptr;
        m_modelLoaded = false;
    }

    /* 处理相对路径：相对于应用程序目录 */
    QString absolutePath = modelPath;
    if (!absolutePath.contains(":") && !absolutePath.startsWith("/")) {
        absolutePath = QCoreApplication::applicationDirPath() + "/" + modelPath;
    }

    LOG_INFO("Speech", QString::fromUtf8(u8"正在加载Vosk模型: %1").arg(absolutePath));

    /* 调用Vosk C API加载模型 */
    VoskModel* model = vosk_model_new(absolutePath.toUtf8().constData());
    if (!model) {
        LOG_ERROR("Speech", QString::fromUtf8(u8"Vosk模型加载失败: %1").arg(absolutePath));
        return false;
    }

    m_model = model;
    m_modelLoaded = true;
    LOG_INFO("Speech", QString::fromUtf8(u8"Vosk模型加载成功"));
    return true;
}

/**
 * @brief 创建语音识别器
 *
 * 识别器创建过程：
 * 1. 检查模型是否已加载
 * 2. 如果已有识别器，先释放
 * 3. 调用vosk_recognizer_new()创建识别器
 * 4. 检查创建结果
 *
 * @param sampleRate 音频采样率
 * @return 创建成功返回true，失败返回false
 */
bool VoskEngine::createRecognizer(int sampleRate)
{
    /* 模型必须先加载 */
    if (!m_modelLoaded || !m_model) {
        LOG_ERROR("Speech", QString::fromUtf8(u8"无法创建识别器：模型未加载"));
        return false;
    }

    /* 如果已有识别器，先释放 */
    if (m_recognizer) {
        vosk_recognizer_free(static_cast<VoskRecognizer*>(m_recognizer));
        m_recognizer = nullptr;
        m_recognizerReady = false;
    }

    /* 调用Vosk C API创建识别器 */
    VoskRecognizer* recognizer = vosk_recognizer_new(
        static_cast<VoskModel*>(m_model),
        static_cast<float>(sampleRate)
    );

    if (!recognizer) {
        LOG_ERROR("Speech", QString::fromUtf8(u8"Vosk识别器创建失败"));
        return false;
    }

    m_recognizer = recognizer;
    m_recognizerReady = true;
    LOG_INFO("Speech", QString::fromUtf8(u8"Vosk识别器创建成功，采样率: %1Hz").arg(sampleRate));
    return true;
}

/**
 * @brief 处理音频数据
 *
 * 将音频数据送入Vosk识别器进行处理。
 * 返回值指示是否检测到语音端点：
 * - 返回0：正在识别中，需要继续送入数据
 * - 返回1：检测到语音端点，可调用getFinalResult()获取完整结果
 *
 * @param audioData 16位有符号整数PCM格式音频数据
 * @return Vosk状态码（0=需要更多数据，1=检测到端点）
 */
int VoskEngine::processAudio(const QByteArray& audioData)
{
    if (!m_recognizerReady || !m_recognizer) {
        return 0;
    }

    /* 调用Vosk C API处理音频数据 */
    return vosk_recognizer_accept_waveform(
        static_cast<VoskRecognizer*>(m_recognizer),
        audioData.constData(),
        audioData.size()
    );
}

/**
 * @brief 获取当前的部分识别结果（实时结果）
 *
 * 调用Vosk C API获取部分识别结果，并从JSON中提取文本内容。
 * 部分结果会随着音频输入不断更新，适合实时显示。
 *
 * @return 部分识别结果文本
 */
QString VoskEngine::getPartialResult()
{
    if (!m_recognizerReady || !m_recognizer) {
        return QString();
    }

    /* 调用Vosk C API获取部分结果 */
    const char* result = vosk_recognizer_partial_result(
        static_cast<VoskRecognizer*>(m_recognizer)
    );

    QString text = extractTextFromJson(result);

    /* Vosk 0.3.45版本中，partial_result返回的是内部缓冲区指针，无需手动释放 */
    /* 较新版本Vosk API才提供vosk_recognizer_free_result函数 */

    return text;
}

/**
 * @brief 获取最终的识别结果
 *
 * 当processAudio()返回1时调用，获取一段完整语音的识别结果。
 * 调用后识别器会自动重置，准备处理下一段语音。
 *
 * @return 最终识别结果文本
 */
QString VoskEngine::getFinalResult()
{
    if (!m_recognizerReady || !m_recognizer) {
        return QString();
    }

    /* 调用Vosk C API获取最终结果 */
    const char* result = vosk_recognizer_final_result(
        static_cast<VoskRecognizer*>(m_recognizer)
    );

    QString text = extractTextFromJson(result);

    /* Vosk 0.3.45版本中，final_result返回的是内部缓冲区指针，无需手动释放 */
    /* 较新版本Vosk API才提供vosk_recognizer_free_result函数 */

    return text;
}

/**
 * @brief 重置识别器状态
 *
 * 清除识别器内部缓冲区，开始新的识别会话。
 * 不影响已加载的模型。
 */
void VoskEngine::reset()
{
    if (m_recognizerReady && m_recognizer) {
        vosk_recognizer_reset(static_cast<VoskRecognizer*>(m_recognizer));
    }
}

/**
 * @brief 检查引擎是否已初始化
 * @return 模型和识别器都已就绪返回true
 */
bool VoskEngine::isInitialized() const
{
    return m_modelLoaded && m_recognizerReady;
}

/**
 * @brief 检查模型是否已加载
 * @return 模型已加载返回true
 */
bool VoskEngine::isModelLoaded() const
{
    return m_modelLoaded;
}

/**
 * @brief 释放所有资源
 *
 * 按顺序释放识别器和模型：
 * 1. 先释放识别器（依赖模型）
 * 2. 再释放模型
 * 3. 重置所有状态标志
 */
void VoskEngine::cleanup()
{
    /* 先释放识别器 */
    if (m_recognizer) {
        vosk_recognizer_free(static_cast<VoskRecognizer*>(m_recognizer));
        m_recognizer = nullptr;
        m_recognizerReady = false;
    }

    /* 再释放模型 */
    if (m_model) {
        vosk_model_free(static_cast<VoskModel*>(m_model));
        m_model = nullptr;
        m_modelLoaded = false;
    }
}

/**
 * @brief 从Vosk返回的JSON字符串中提取text字段
 *
 * Vosk返回的JSON格式：
 * - 部分结果：{"partial" : "你好世界"}
 * - 最终结果：{"text" : "你好世界"}
 *
 * 解析流程：
 * 1. 检查输入是否为空
 * 2. 解析JSON文档
 * 3. 优先提取"text"字段（最终结果）
 * 4. 如果没有"text"，提取"partial"字段（部分结果）
 * 5. 去除中文字符之间的多余空格（Vosk中文模型会在每个字之间加空格）
 *
 * @param jsonResult Vosk返回的JSON格式结果字符串
 * @return 提取的文本内容
 */
QString VoskEngine::extractTextFromJson(const char* jsonResult)
{
    if (!jsonResult || jsonResult[0] == '\0') {
        return QString();
    }

    QJsonDocument doc = QJsonDocument::fromJson(QByteArray(jsonResult));
    if (doc.isNull() || !doc.isObject()) {
        return QString();
    }

    QJsonObject obj = doc.object();

    /* 优先提取最终结果的text字段 */
    QString text;
    if (obj.contains("text")) {
        text = obj["text"].toString();
    } else if (obj.contains("partial")) {
        text = obj["partial"].toString();
    } else {
        return QString();
    }

    /* 去除中文字符之间的多余空格
     * Vosk中文模型会在每个汉字之间加空格，如"你 好 世 界"
     * 这里将连续的"汉字 汉字"之间的空格去除，但保留英文单词之间的空格
     */
    QString cleaned;
    for (int i = 0; i < text.length(); ++i) {
        QChar ch = text[i];
        if (ch == ' ') {
            /* 检查空格前后是否都是中文字符 */
            bool prevIsChinese = (i > 0) && isChineseChar(text[i - 1]);
            bool nextIsChinese = (i < text.length() - 1) && isChineseChar(text[i + 1]);
            /* 如果前后都是中文，跳过这个空格 */
            if (prevIsChinese && nextIsChinese) {
                continue;
            }
        }
        cleaned.append(ch);
    }

    return cleaned;
}

/**
 * @brief 判断字符是否为中文字符（包括中文标点）
 * @param ch 待判断的字符
 * @return 是中文字符返回true
 */
bool VoskEngine::isChineseChar(QChar ch)
{
    ushort unicode = ch.unicode();
    /* CJK统一汉字范围 */
    if (unicode >= 0x4E00 && unicode <= 0x9FFF) return true;
    /* CJK统一汉字扩展A */
    if (unicode >= 0x3400 && unicode <= 0x4DBF) return true;
    /* CJK统一汉字扩展B-I */
    if (unicode >= 0x20000 && unicode <= 0x2EBEF) return true;
    /* 中文标点符号范围 */
    if (unicode >= 0x3000 && unicode <= 0x303F) return true;
    if (unicode >= 0xFF00 && unicode <= 0xFFEF) return true;
    return false;
}
