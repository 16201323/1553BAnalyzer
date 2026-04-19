/**
 * @file AudioCapture.h
 * @brief 音频采集模块类定义
 *
 * AudioCapture类封装了Qt Multimedia的QAudioInput，
 * 提供麦克风音频采集功能，包括：
 * - 麦克风设备检测
 * - 音频采集启动/停止
 * - 音频数据实时输出
 * - 音频电平计算（用于波形显示）
 *
 * 采集的音频格式为16位有符号整数PCM，单声道，16kHz采样率，
 * 与Vosk引擎要求的输入格式一致。
 *
 * @author 1553BTools
 * @date 2026
 */

#ifndef AUDIOCAPTURE_H
#define AUDIOCAPTURE_H

#include <QObject>
#include <QAudioInput>
#include <QAudioFormat>
#include <QIODevice>
#include <QByteArray>

/**
 * @brief 音频采集模块类
 *
 * 该类负责从麦克风采集音频数据，并通过信号发送给
 * 语音识别引擎进行处理。
 *
 * 使用流程：
 * 1. 调用hasMicrophone()检测麦克风设备
 * 2. 调用start()开始采集
 * 3. 通过dataReady信号接收音频数据
 * 4. 调用stop()停止采集
 *
 * 音频数据格式：
 * - 采样率：16000Hz
 * - 位深：16位有符号整数
 * - 通道数：1（单声道）
 * - 编码：PCM
 */
class AudioCapture : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象指针
     */
    explicit AudioCapture(QObject *parent = nullptr);

    /**
     * @brief 析构函数，停止采集并释放资源
     */
    ~AudioCapture();

    /**
     * @brief 检测系统中是否有可用的麦克风设备
     * @return 有麦克风设备返回true，否则返回false
     *
     * 在启动语音识别前应先调用此方法检测设备。
     * 如果没有麦克风设备，应弹框提示用户。
     */
    static bool hasMicrophone();

    /**
     * @brief 开始音频采集
     * @return 启动成功返回true，失败返回false
     *
     * 启动流程：
     * 1. 检查是否已在采集
     * 2. 查找可用的音频输入设备
     * 3. 配置音频格式（16kHz, 16bit, 单声道）
     * 4. 创建QAudioInput并启动
     * 5. 连接数据就绪信号
     */
    bool start();

    /**
     * @brief 停止音频采集
     *
     * 停止QAudioInput，释放音频设备资源。
     */
    void stop();

    /**
     * @brief 检查是否正在采集
     * @return 正在采集返回true，否则返回false
     */
    bool isCapturing() const;

    /**
     * @brief 获取当前音频采集格式
     * @return 音频格式描述
     */
    QString formatDescription() const;

signals:
    /**
     * @brief 音频数据就绪信号
     * @param data 音频数据（16位有符号整数PCM格式）
     *
     * 每当音频缓冲区有数据时发出此信号，
     * 接收方应尽快处理数据，避免缓冲区溢出。
     */
    void dataReady(const QByteArray& data);

    /**
     * @brief 音频电平变化信号
     * @param level 当前音频电平（0.0-1.0）
     *
     * 用于UI显示录音状态（如波形动画）。
     * 电平值基于最近一段音频数据的RMS（均方根）计算。
     */
    void levelChanged(qreal level);

    /**
     * @brief 错误信号
     * @param errorMessage 错误描述信息
     *
     * 在音频采集过程中发生错误时发出。
     */
    void error(const QString& errorMessage);

private slots:
    /**
     * @brief 音频数据读取槽
     *
     * 从QAudioInput的IO设备中读取音频数据，
     * 计算音频电平，并发出dataReady信号。
     */
    void onAudioDataReady();

private:
    /**
     * @brief 计算音频数据的RMS电平
     * @param data 音频数据
     * @return RMS电平值（0.0-1.0）
     *
     * 计算步骤：
     * 1. 将字节数据转换为16位有符号整数
     * 2. 计算所有采样点的平方和
     * 3. 求均方根（RMS）
     * 4. 归一化到0.0-1.0范围
     */
    qreal calculateLevel(const QByteArray& data);

    /**
     * @brief 创建Vosk所需的音频格式
     * @return 配置好的QAudioFormat对象
     */
    QAudioFormat createAudioFormat();

    QAudioInput* m_audioInput;  // Qt音频输入对象
    QIODevice* m_audioDevice;   // 音频IO设备（用于读取数据）
    bool m_capturing;           // 是否正在采集
    qreal m_currentLevel;       // 当前音频电平
};

#endif // AUDIOCAPTURE_H
