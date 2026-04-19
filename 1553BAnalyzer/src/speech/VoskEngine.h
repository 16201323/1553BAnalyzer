/**
 * @file VoskEngine.h
 * @brief Vosk语音识别引擎封装类定义
 *
 * VoskEngine类对Vosk C API进行面向对象封装，提供：
 * - 模型加载与管理
 * - 识别器创建与销毁
 * - 音频数据处理
 * - 识别结果获取
 *
 * 该类不是线程安全的，所有调用应在同一线程中执行。
 * 语音识别处理通常在独立的工作线程中运行，
 * 通过信号槽机制与UI线程通信。
 *
 * @author 1553BTools
 * @date 2026
 */

#ifndef VOSKENGINE_H
#define VOSKENGINE_H

#include <QString>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>

/**
 * @brief Vosk语音识别引擎封装类
 *
 * 封装Vosk C API，提供简洁的C++接口。
 * 使用前必须调用loadModel()加载模型，
 * 然后调用createRecognizer()创建识别器。
 *
 * 典型使用流程：
 * @code
 * VoskEngine engine;
 * engine.loadModel("models/vosk-model-cn-0.22");
 * engine.createRecognizer(16000);
 * // 送入音频数据
 * engine.processAudio(audioData);
 * // 获取识别结果
 * QString partial = engine.getPartialResult();
 * QString final_ = engine.getFinalResult();
 * @endcode
 */
class VoskEngine
{
public:
    /**
     * @brief 构造函数
     */
    VoskEngine();

    /**
     * @brief 析构函数，释放所有资源
     */
    ~VoskEngine();

    /**
     * @brief 加载Vosk语音识别模型
     * @param modelPath 模型目录路径（必须包含am、conf、graph等子目录）
     * @return 加载成功返回true，失败返回false
     *
     * 模型加载是耗时操作，建议在后台线程中调用。
     * 加载失败时会输出错误日志。
     */
    bool loadModel(const QString& modelPath);

    /**
     * @brief 创建语音识别器
     * @param sampleRate 音频采样率（通常为16000Hz）
     * @return 创建成功返回true，失败返回false
     *
     * 必须先成功加载模型后才能创建识别器。
     * 如果已有识别器，会先销毁旧的再创建新的。
     */
    bool createRecognizer(int sampleRate = 16000);

    /**
     * @brief 处理音频数据
     * @param audioData 音频数据（16位有符号整数PCM格式）
     * @return 返回Vosk的状态码：
     *         - 0: 需要更多数据（正在识别中）
     *         - 1: 检测到语音端点（可获取最终结果）
     *
     * 音频数据必须是16位有符号整数PCM格式，单声道，
     * 采样率与createRecognizer()中指定的一致。
     */
    int processAudio(const QByteArray& audioData);

    /**
     * @brief 获取当前的部分识别结果（实时结果）
     * @return 部分识别结果文本，可能为空字符串
     *
     * 返回的是当前正在识别的文字，会随着音频输入不断更新。
     * 适用于实时显示用户正在说的内容。
     */
    QString getPartialResult();

    /**
     * @brief 获取最终的识别结果
     * @return 最终识别结果文本，可能为空字符串
     *
     * 当processAudio()返回1（检测到语音端点）时调用，
     * 返回一段完整语音的识别结果。
     */
    QString getFinalResult();

    /**
     * @brief 重置识别器状态
     *
     * 清除识别器的内部缓冲区，开始新的识别会话。
     * 通常在切换说话人或开始新的录音时调用。
     */
    void reset();

    /**
     * @brief 检查引擎是否已初始化（模型和识别器都已就绪）
     * @return 已初始化返回true，否则返回false
     */
    bool isInitialized() const;

    /**
     * @brief 检查模型是否已加载
     * @return 模型已加载返回true，否则返回false
     */
    bool isModelLoaded() const;

    /**
     * @brief 释放所有资源
     *
     * 释放识别器和模型，恢复到未初始化状态。
     * 析构函数会自动调用此方法。
     */
    void cleanup();

private:
    /**
     * @brief 从Vosk返回的JSON字符串中提取text字段
     * @param jsonResult Vosk返回的JSON格式结果字符串
     * @return 提取的text字段内容，解析失败返回空字符串
     *
     * Vosk返回的JSON格式示例：
     * 部分结果：{"partial" : "你好世界"}
     * 最终结果：{"text" : "你好世界"}
     *
     * 会自动去除中文字符之间的多余空格。
     */
    QString extractTextFromJson(const char* jsonResult);

    /**
     * @brief 判断字符是否为中文字符（包括中文标点）
     * @param ch 待判断的字符
     * @return 是中文字符返回true
     */
    bool isChineseChar(QChar ch);

    void* m_model;          // VoskModel指针（使用void*避免头文件依赖）
    void* m_recognizer;     // VoskRecognizer指针
    bool m_modelLoaded;     // 模型是否已加载
    bool m_recognizerReady; // 识别器是否已就绪
};

#endif // VOSKENGINE_H
