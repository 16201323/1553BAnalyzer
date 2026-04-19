/**
 * @file AudioCapture.cpp
 * @brief 音频采集模块类实现
 *
 * 本文件实现了AudioCapture类的所有方法，包括：
 * - 麦克风设备检测
 * - 音频采集的启动和停止
 * - 音频数据的读取和转发
 * - 音频电平的实时计算
 *
 * 使用Qt Multimedia的QAudioInput进行音频采集。
 * 兼容Qt5和Qt6的音频API。
 *
 * @author 1553BTools
 * @date 2026
 */

#include "AudioCapture.h"
#include "core/config/ConfigManager.h"
#include "utils/Logger.h"

#include <QAudioDeviceInfo>
#include <QBuffer>
#include <QtMath>

/**
 * @brief 构造函数，初始化成员变量
 * @param parent 父对象指针
 */
AudioCapture::AudioCapture(QObject *parent)
    : QObject(parent)
    , m_audioInput(nullptr)
    , m_audioDevice(nullptr)
    , m_capturing(false)
    , m_currentLevel(0.0)
{
}

/**
 * @brief 析构函数，确保停止采集
 */
AudioCapture::~AudioCapture()
{
    stop();
}

/**
 * @brief 检测系统中是否有可用的麦克风设备
 *
 * 遍历所有可用的音频输入设备，检查是否有设备
 * 支持Vosk所需的音频格式（16kHz, 16bit, 单声道PCM）。
 *
 * @return 有可用麦克风返回true，否则返回false
 */
bool AudioCapture::hasMicrophone()
{
    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleSize(16);
    format.setSampleType(QAudioFormat::SignedInt);
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setCodec("audio/pcm");

    /* 遍历所有音频输入设备，查找支持所需格式的设备 */
    foreach (const QAudioDeviceInfo &deviceInfo, QAudioDeviceInfo::availableDevices(QAudio::AudioInput)) {
        if (deviceInfo.isFormatSupported(format)) {
            return true;
        }
    }

    /* 如果没有设备支持精确格式，检查是否有任何音频输入设备 */
    QList<QAudioDeviceInfo> devices = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
    if (!devices.isEmpty()) {
        LOG_WARNING("Speech", QString::fromUtf8(u8"有音频输入设备但不支持精确格式，将使用最接近的格式"));
        return true;
    }

    return false;
}

/**
 * @brief 创建Vosk所需的音频格式
 *
 * 配置QAudioFormat为Vosk引擎所需的格式：
 * - 采样率：16000Hz
 * - 通道数：1（单声道）
 * - 采样位深：16位
 * - 采样类型：有符号整数
 * - 字节序：小端
 * - 编码：PCM
 *
 * @return 配置好的QAudioFormat对象
 */
QAudioFormat AudioCapture::createAudioFormat()
{
    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleSize(16);
    format.setSampleType(QAudioFormat::SignedInt);
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setCodec("audio/pcm");
    return format;
}

/**
 * @brief 开始音频采集
 *
 * 启动流程：
 * 1. 检查是否已在采集中
 * 2. 创建目标音频格式
 * 3. 查找支持该格式的音频输入设备
 * 4. 如果没有设备支持精确格式，使用最接近的格式
 * 5. 创建QAudioInput并启动
 * 6. 连接数据就绪信号
 *
 * @return 启动成功返回true，失败返回false
 */
bool AudioCapture::start()
{
    /* 防止重复启动 */
    if (m_capturing) {
        return true;
    }

    QAudioFormat format = createAudioFormat();

    /* 查找默认音频输入设备 */
    QAudioDeviceInfo deviceInfo = QAudioDeviceInfo::defaultInputDevice();
    if (deviceInfo.isNull()) {
        emit error(tr(u8"未检测到麦克风设备，请检查设备连接"));
        LOG_ERROR("Speech", QString::fromUtf8(u8"未检测到麦克风设备"));
        return false;
    }

    /* 如果设备不支持精确格式，使用最接近的格式 */
    if (!deviceInfo.isFormatSupported(format)) {
        LOG_WARNING("Speech", QString::fromUtf8(u8"默认设备不支持16kHz格式，尝试使用最接近的格式"));
        format = deviceInfo.nearestFormat(format);
    }

    LOG_INFO("Speech", QString::fromUtf8(u8"音频采集格式: %1Hz, %2bit, %3ch")
        .arg(format.sampleRate())
        .arg(format.sampleSize())
        .arg(format.channelCount()));

    /* 创建QAudioInput并启动 */
    m_audioInput = new QAudioInput(deviceInfo, format, this);
    m_audioInput->setBufferSize(16000 * 2 * 100 / 1000); /* 100ms缓冲区 */

    /* 启动音频采集，获取IO设备用于读取数据 */
    m_audioDevice = m_audioInput->start();
    if (!m_audioDevice) {
        emit error(tr(u8"麦克风启动失败，请检查设备是否被其他程序占用"));
        LOG_ERROR("Speech", QString::fromUtf8(u8"QAudioInput启动失败"));
        delete m_audioInput;
        m_audioInput = nullptr;
        return false;
    }

    /* 连接数据就绪信号 */
    connect(m_audioDevice, &QIODevice::readyRead, this, &AudioCapture::onAudioDataReady);

    m_capturing = true;
    LOG_INFO("Speech", QString::fromUtf8(u8"音频采集已启动"));
    return true;
}

/**
 * @brief 停止音频采集
 *
 * 停止QAudioInput并释放音频设备资源。
 * 断开数据就绪信号的连接。
 */
void AudioCapture::stop()
{
    if (!m_capturing) {
        return;
    }

    m_capturing = false;

    /* 停止音频输入 */
    if (m_audioInput) {
        m_audioInput->stop();
        delete m_audioInput;
        m_audioInput = nullptr;
    }

    m_audioDevice = nullptr;
    m_currentLevel = 0.0;

    LOG_INFO("Speech", QString::fromUtf8(u8"音频采集已停止"));
}

/**
 * @brief 检查是否正在采集
 * @return 正在采集返回true
 */
bool AudioCapture::isCapturing() const
{
    return m_capturing;
}

/**
 * @brief 获取当前音频采集格式描述
 * @return 格式描述字符串
 */
QString AudioCapture::formatDescription() const
{
    if (!m_audioInput) {
        return tr(u8"未启动");
    }
    QAudioFormat format = m_audioInput->format();
    return QString("%1Hz, %2bit, %3ch")
        .arg(format.sampleRate())
        .arg(format.sampleSize())
        .arg(format.channelCount());
}

/**
 * @brief 音频数据读取槽
 *
 * 从QAudioInput的IO设备中读取可用的音频数据，
 * 计算当前音频电平，并通过信号发送给识别引擎。
 *
 * 数据处理流程：
 * 1. 读取IO设备中的所有可用数据
 * 2. 检查数据有效性
 * 3. 计算音频电平（用于UI显示）
 * 4. 发出dataReady信号（发送给识别引擎）
 * 5. 发出levelChanged信号（发送给UI）
 */
void AudioCapture::onAudioDataReady()
{
    if (!m_audioDevice || !m_capturing) {
        return;
    }

    /* 读取所有可用的音频数据 */
    QByteArray data = m_audioDevice->readAll();
    if (data.isEmpty()) {
        return;
    }

    /* 计算音频电平 */
    m_currentLevel = calculateLevel(data);
    emit levelChanged(m_currentLevel);

    /* 发送音频数据给识别引擎 */
    emit dataReady(data);
}

/**
 * @brief 计算音频数据的RMS电平
 *
 * RMS（均方根）计算步骤：
 * 1. 将字节数据转换为16位有符号整数数组
 * 2. 计算所有采样点的平方和
 * 3. 求均方根值
 * 4. 归一化到0.0-1.0范围（32768为16位最大值）
 *
 * @param data 音频数据
 * @return RMS电平值（0.0-1.0）
 */
qreal AudioCapture::calculateLevel(const QByteArray& data)
{
    if (data.size() < 2) {
        return 0.0;
    }

    /* 将字节数据转换为16位有符号整数指针 */
    const qint16* samples = reinterpret_cast<const qint16*>(data.constData());
    int sampleCount = data.size() / 2; /* 每个采样点2字节 */

    /* 计算平方和 */
    double sumSquares = 0.0;
    for (int i = 0; i < sampleCount; ++i) {
        double sample = static_cast<double>(samples[i]);
        sumSquares += sample * sample;
    }

    /* 计算RMS并归一化 */
    double rms = qSqrt(sumSquares / sampleCount);
    qreal level = static_cast<qreal>(rms / 32768.0);

    /* 限制在0.0-1.0范围内 */
    if (level > 1.0) {
        level = 1.0;
    }

    return level;
}
