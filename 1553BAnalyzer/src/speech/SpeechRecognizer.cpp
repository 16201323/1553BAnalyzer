/**
 * @file SpeechRecognizer.cpp
 * @brief 语音识别控制器类实现
 *
 * 本文件实现了SpeechRecognizer类的所有方法，包括：
 * - 引擎初始化（模型加载、识别器创建）
 * - 录音控制（开始/停止/取消）
 * - 音频数据处理和识别结果分发
 * - 状态管理和错误处理
 *
 * 语音识别处理流程：
 * 1. AudioCapture采集音频数据 → dataReady信号
 * 2. SpeechRecognizer接收音频数据 → 送入VoskEngine
 * 3. VoskEngine返回识别状态（0=继续，1=端点检测）
 * 4. 根据状态获取部分结果或最终结果
 * 5. 通过信号将结果发送给UI层
 *
 * @author 1553BTools
 * @date 2026
 */

#include "SpeechRecognizer.h"
#include "utils/Logger.h"

/**
 * @brief 构造函数，初始化成员变量并连接信号槽
 * @param parent 父对象指针
 */
SpeechRecognizer::SpeechRecognizer(QObject *parent)
    : QObject(parent)
    , m_state(SpeechState::Idle)
{
    /* 连接音频采集模块的信号 */
    connect(&m_audioCapture, &AudioCapture::dataReady,
            this, &SpeechRecognizer::onAudioDataReady);
    connect(&m_audioCapture, &AudioCapture::levelChanged,
            this, &SpeechRecognizer::onAudioLevelChanged);
    connect(&m_audioCapture, &AudioCapture::error,
            this, &SpeechRecognizer::onAudioCaptureError);
}

/**
 * @brief 析构函数，停止录音
 */
SpeechRecognizer::~SpeechRecognizer()
{
    if (m_state == SpeechState::Recording) {
        cancelRecording();
    }
}

/**
 * @brief 初始化语音识别引擎
 *
 * 初始化流程：
 * 1. 保存配置参数
 * 2. 检查功能是否启用
 * 3. 检测麦克风设备
 * 4. 加载Vosk模型
 * 5. 创建识别器
 *
 * 模型加载可能需要数秒时间，建议在后台线程中调用。
 *
 * @param config 语音识别配置
 * @return 初始化成功返回true，失败返回false
 */
bool SpeechRecognizer::initialize(const SpeechConfig& config)
{
    m_config = config;

    /* 检查功能是否启用 */
    if (!m_config.engine.enabled) {
        LOG_INFO("Speech", QString::fromUtf8(u8"语音识别功能已禁用"));
        return false;
    }

    /* 检测麦克风设备 */
    if (!AudioCapture::hasMicrophone()) {
        QString msg = tr(u8"未检测到麦克风设备，语音识别功能不可用");
        LOG_ERROR("Speech", msg);
        emit error(msg);
        return false;
    }

    /* 加载Vosk模型 */
    LOG_INFO("Speech", QString::fromUtf8(u8"正在初始化语音识别引擎..."));
    if (!m_voskEngine.loadModel(m_config.engine.modelPath)) {
        QString msg = tr(u8"语音模型加载失败，请检查模型文件路径: %1").arg(m_config.engine.modelPath);
        LOG_ERROR("Speech", msg);
        emit error(msg);
        return false;
    }

    /* 创建识别器 */
    if (!m_voskEngine.createRecognizer(m_config.engine.sampleRate)) {
        QString msg = tr(u8"语音识别器创建失败");
        LOG_ERROR("Speech", msg);
        emit error(msg);
        return false;
    }

    LOG_INFO("Speech", QString::fromUtf8(u8"语音识别引擎初始化成功"));
    return true;
}

/**
 * @brief 执行初始化槽（供跨线程信号槽调用）
 *
 * 使用默认配置调用initialize()完成引擎初始化，
 * 并通过initializationDone信号通知UI线程初始化结果。
 * 无参数版本简化了跨线程调用，
 * 避免使用Qt5.10+才支持的QMetaObject::invokeMethod lambda语法，
 * 同时避免需要将SpeechConfig注册为Qt元类型。
 */
void SpeechRecognizer::doInitialize()
{
    bool ok = initialize(SpeechConfig());
    emit initializationDone(ok);
}

void SpeechRecognizer::doInitializeWithConfig(const SpeechConfig& config)
{
    bool ok = initialize(config);
    emit initializationDone(ok);
}

/**
 * @brief 开始录音
 *
 * 启动流程：
 * 1. 检查引擎是否已初始化
 * 2. 检查是否已在录音中
 * 3. 重置识别器状态
 * 4. 清空累积文本
 * 5. 启动音频采集
 * 6. 更新状态为Recording
 *
 * @return 启动成功返回true，失败返回false
 */
bool SpeechRecognizer::startRecording()
{
    LOG_INFO("Speech", QString::fromUtf8(u8"startRecording 被调用"));

    /* 检查引擎是否已初始化 */
    if (!m_voskEngine.isInitialized()) {
        QString msg = tr(u8"语音识别引擎未初始化");
        LOG_ERROR("Speech", msg);
        emit error(msg);
        return false;
    }

    /* 防止重复启动 */
    if (m_state == SpeechState::Recording) {
        LOG_INFO("Speech", QString::fromUtf8(u8"已在录音中，忽略重复启动请求"));
        return true;
    }

    /* 重置识别器状态和累积文本 */
    m_voskEngine.reset();
    m_currentPartial.clear();
    m_accumulatedText.clear();

    /* 启动音频采集 */
    LOG_INFO("Speech", QString::fromUtf8(u8"正在启动音频采集..."));
    if (!m_audioCapture.start()) {
        QString msg = tr(u8"音频采集启动失败");
        LOG_ERROR("Speech", msg);
        emit error(msg);
        return false;
    }

    /* 更新状态 */
    setState(SpeechState::Recording);
    LOG_INFO("Speech", QString::fromUtf8(u8"开始录音"));
    return true;
}

/**
 * @brief 停止录音并获取最终结果
 *
 * 停止流程：
 * 1. 停止音频采集
 * 2. 获取识别器的最终结果
 * 3. 拼接累积文本和最终结果
 * 4. 发出finalResult信号
 * 5. 更新状态为Idle
 */
void SpeechRecognizer::stopRecording()
{
    if (m_state != SpeechState::Recording) {
        return;
    }

    /* 停止音频采集 */
    m_audioCapture.stop();

    /* 获取最终识别结果 */
    QString finalText = m_voskEngine.getFinalResult();
    if (!finalText.isEmpty()) {
        if (!m_accumulatedText.isEmpty()) {
            m_accumulatedText += finalText;
        } else {
            m_accumulatedText = finalText;
        }
    }

    /* 发出最终结果信号 */
    if (!m_accumulatedText.isEmpty()) {
        emit finalResult(m_accumulatedText);
    }

    /* 更新状态 */
    setState(SpeechState::Idle);
    LOG_INFO("Speech", QString::fromUtf8(u8"录音结束，识别结果: %1").arg(m_accumulatedText));
}

/**
 * @brief 取消录音（不获取结果）
 *
 * 取消流程：
 * 1. 停止音频采集
 * 2. 重置识别器
 * 3. 清空累积文本
 * 4. 更新状态为Idle
 */
void SpeechRecognizer::cancelRecording()
{
    if (m_state != SpeechState::Recording) {
        return;
    }

    /* 停止音频采集 */
    m_audioCapture.stop();

    /* 重置识别器 */
    m_voskEngine.reset();

    /* 清空累积文本 */
    m_currentPartial.clear();
    m_accumulatedText.clear();

    /* 更新状态 */
    setState(SpeechState::Idle);
    LOG_INFO("Speech", QString::fromUtf8(u8"录音已取消"));
}

/**
 * @brief 获取当前状态
 * @return 当前语音识别状态
 */
SpeechState SpeechRecognizer::state() const
{
    return m_state;
}

/**
 * @brief 检查引擎是否已初始化
 * @return 已初始化返回true
 */
bool SpeechRecognizer::isInitialized() const
{
    return m_voskEngine.isInitialized();
}

/**
 * @brief 检测系统中是否有可用的麦克风设备
 * @return 有麦克风返回true
 */
bool SpeechRecognizer::hasMicrophone()
{
    return AudioCapture::hasMicrophone();
}

/**
 * @brief 音频数据就绪槽
 *
 * 处理流程：
 * 1. 将音频数据送入Vosk引擎
 * 2. 检查Vosk返回的状态码
 * 3. 如果状态码为1（端点检测），获取最终结果并拼接到累积文本
 * 4. 获取当前部分结果并发出partialResult信号
 *
 * @param data 音频数据（16位有符号整数PCM格式）
 */
void SpeechRecognizer::onAudioDataReady(const QByteArray& data)
{
    if (m_state != SpeechState::Recording) {
        return;
    }

    /* 将音频数据送入Vosk引擎处理 */
    int status = m_voskEngine.processAudio(data);

    if (status == 1) {
        /* 检测到语音端点，获取最终结果 */
        QString finalText = m_voskEngine.getFinalResult();
        if (!finalText.isEmpty()) {
            m_accumulatedText += finalText;
        }
    }

    /* 获取当前部分识别结果 */
    QString partial = m_voskEngine.getPartialResult();
    if (partial != m_currentPartial) {
        m_currentPartial = partial;
        /* 拼接累积文本和当前部分结果，作为实时显示内容 */
        QString displayText = m_accumulatedText + m_currentPartial;
        if (!displayText.isEmpty()) {
            emit partialResult(displayText);
        }
    }
}

/**
 * @brief 音频电平变化槽
 * @param level 音频电平
 *
 * 转发音频电平信号给UI层，用于波形显示。
 */
void SpeechRecognizer::onAudioLevelChanged(qreal level)
{
    emit audioLevel(level);
}

/**
 * @brief 音频采集错误槽
 * @param errorMessage 错误信息
 *
 * 停止录音并转发错误信号给UI层。
 */
void SpeechRecognizer::onAudioCaptureError(const QString& errorMessage)
{
    /* 停止录音 */
    if (m_state == SpeechState::Recording) {
        m_audioCapture.stop();
        setState(SpeechState::Idle);
    }

    /* 转发错误信号 */
    emit error(errorMessage);
    LOG_ERROR("Speech", QString::fromUtf8(u8"音频采集错误: %1").arg(errorMessage));
}

/**
 * @brief 设置语音识别状态
 * @param state 新状态
 *
 * 更新内部状态并发出stateChanged信号。
 */
void SpeechRecognizer::setState(SpeechState state)
{
    if (m_state != state) {
        m_state = state;
        emit stateChanged(state);
    }
}
