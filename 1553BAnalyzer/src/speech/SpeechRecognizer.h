/**
 * @file SpeechRecognizer.h
 * @brief 语音识别控制器类定义
 *
 * SpeechRecognizer是语音识别模块的核心控制器，
 * 整合AudioCapture（音频采集）和VoskEngine（识别引擎），
 * 提供统一的语音识别接口。
 *
 * 主要功能：
 * - 语音识别引擎初始化
 * - 录音控制（开始/停止/取消）
 * - 实时识别结果输出
 * - 麦克风设备检测
 * - 音频电平监控
 *
 * 该类运行在独立的线程中，通过信号槽与UI线程通信。
 *
 * @author 1553BTools
 * @date 2026
 */

#ifndef SPEECHRECOGNIZER_H
#define SPEECHRECOGNIZER_H

#include <QObject>
#include <QThread>
#include <QTimer>

#include "core/config/ConfigManager.h"
#include "VoskEngine.h"

Q_DECLARE_METATYPE(SpeechConfig)
#include "AudioCapture.h"

/**
 * @brief 语音识别控制器状态枚举
 */
enum class SpeechState
{
    Idle,       // 空闲状态，未开始录音
    Recording,  // 录音中，正在识别
    Processing  // 处理中，正在处理最后一段音频
};

/* 注册SpeechState为Qt元类型，使其可以跨线程传递 */
Q_DECLARE_METATYPE(SpeechState)

/**
 * @brief 语音识别控制器类
 *
 * 整合音频采集和识别引擎，提供完整的语音识别功能。
 * 该类应在独立线程中运行，避免阻塞UI线程。
 *
 * 使用流程：
 * 1. 调用initialize()初始化引擎
 * 2. 调用startRecording()开始录音
 * 3. 通过partialResult信号接收实时识别结果
 * 4. 调用stopRecording()停止录音
 * 5. 通过finalResult信号接收最终识别结果
 *
 * 信号说明：
 * - partialResult: 实时识别结果（用户正在说话时持续更新）
 * - finalResult: 最终识别结果（一段完整语音的识别结果）
 * - stateChanged: 状态变化通知
 * - audioLevel: 音频电平（用于UI波形显示）
 * - error: 错误通知
 */
class SpeechRecognizer : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象指针
     */
    explicit SpeechRecognizer(QObject *parent = nullptr);

    /**
     * @brief 析构函数，停止录音并释放资源
     */
    ~SpeechRecognizer();

    /**
     * @brief 初始化语音识别引擎
     * @param config 语音识别配置
     * @return 初始化成功返回true，失败返回false
     *
     * 初始化流程：
     * 1. 保存配置
     * 2. 检测麦克风设备
     * 3. 加载Vosk模型
     * 4. 创建识别器
     *
     * 模型加载是耗时操作，建议在后台线程中调用。
     */
    bool initialize(const SpeechConfig& config = SpeechConfig());

    /**
     * @brief 获取当前状态
     * @return 当前语音识别状态
     */
    SpeechState state() const;

    /**
     * @brief 检查引擎是否已初始化
     * @return 已初始化返回true
     */
    bool isInitialized() const;

    /**
     * @brief 检测系统中是否有可用的麦克风设备
     * @return 有麦克风返回true
     */
    static bool hasMicrophone();

signals:
    /**
     * @brief 实时识别结果信号
     * @param text 当前识别到的文字（部分结果）
     *
     * 用户正在说话时持续发出此信号，
     * 文字会随着语音输入不断更新。
     */
    void partialResult(const QString& text);

    /**
     * @brief 最终识别结果信号
     * @param text 一段完整语音的识别结果
     *
     * 当检测到语音端点或用户停止录音时发出。
     */
    void finalResult(const QString& text);

    /**
     * @brief 状态变化信号
     * @param state 新的语音识别状态
     */
    void stateChanged(SpeechState state);

    /**
     * @brief 音频电平信号
     * @param level 当前音频电平（0.0-1.0）
     *
     * 用于UI显示录音状态（波形动画、音量指示等）。
     */
    void audioLevel(qreal level);

    /**
     * @brief 错误信号
     * @param message 错误描述信息
     */
    void error(const QString& message);

    /**
     * @brief 初始化完成信号
     * @param success 初始化是否成功
     *
     * 在后台线程中完成初始化后发出此信号，
     * 通知UI线程更新界面状态。
     * 使用此信号替代QMetaObject::invokeMethod的lambda形式，
     * 以兼容Qt5.9.9（lambda形式从Qt5.10才开始支持）。
     */
    void initializationDone(bool success);

public slots:
    /**
     * @brief 开始录音
     * @return 启动成功返回true，失败返回false
     *
     * 启动流程：
     * 1. 检查引擎是否已初始化
     * 2. 启动音频采集
     * 3. 更新状态为Recording
     */
    bool startRecording();

    /**
     * @brief 停止录音并获取最终结果
     *
     * 停止流程：
     * 1. 停止音频采集
     * 2. 获取识别器的最终结果
     * 3. 更新状态为Idle
     */
    void stopRecording();

    /**
     * @brief 取消录音（不获取结果）
     *
     * 取消流程：
     * 1. 停止音频采集
     * 2. 重置识别器
     * 3. 更新状态为Idle
     */
    void cancelRecording();

    /**
     * @brief 执行初始化槽（供跨线程信号槽调用）
     *
     * 使用默认配置调用initialize()完成引擎初始化，
     * 并通过initializationDone信号通知UI线程初始化结果。
     * 此槽设计为响应信号触发，无参数版本简化了跨线程调用，
     * 避免使用Qt5.10+才支持的invokeMethod lambda语法，
     * 同时避免需要将SpeechConfig注册为Qt元类型。
     */
    void doInitialize();

    /**
     * @brief 使用指定配置执行初始化槽（供跨线程信号槽调用）
     *
     * 使用ConfigManager提供的配置调用initialize()完成引擎初始化，
     * 并通过initializationDone信号通知UI线程初始化结果。
     */
    void doInitializeWithConfig(const SpeechConfig& config);

private slots:
    /**
     * @brief 音频数据就绪槽
     * @param data 音频数据
     *
     * 将音频数据送入Vosk引擎处理，
     * 根据返回状态获取部分结果或最终结果。
     */
    void onAudioDataReady(const QByteArray& data);

    /**
     * @brief 音频电平变化槽
     * @param level 音频电平
     *
     * 转发音频电平信号给UI层。
     */
    void onAudioLevelChanged(qreal level);

    /**
     * @brief 音频采集错误槽
     * @param errorMessage 错误信息
     *
     * 停止录音并转发错误信号。
     */
    void onAudioCaptureError(const QString& errorMessage);

private:
    /**
     * @brief 设置语音识别状态
     * @param state 新状态
     */
    void setState(SpeechState state);

    VoskEngine m_voskEngine;        // Vosk识别引擎
    AudioCapture m_audioCapture;    // 音频采集模块
    SpeechConfig m_config;          // 语音识别配置
    SpeechState m_state;            // 当前状态
    QString m_currentPartial;       // 当前部分识别结果
    QString m_accumulatedText;      // 累积的识别文本（多次最终结果拼接）
};

#endif // SPEECHRECOGNIZER_H
