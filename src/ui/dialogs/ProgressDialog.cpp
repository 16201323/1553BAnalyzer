/**
 * @file ProgressDialog.cpp
 * @brief 进度对话框类实现
 * 
 * 实现了文件加载和数据筛选的进度显示对话框，
 * 提供进度条、状态文本、耗时显示和取消功能。
 * 
 * @author 1553BTools
 * @date 2026
 */

#include "ProgressDialog.h"
#include "utils/Logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>

/**
 * @brief 构造函数
 * @param parent 父窗口指针
 * @param flags 窗口标志
 * 
 * 初始化对话框UI组件和信号连接
 */
ProgressDialog::ProgressDialog(QWidget* parent, Qt::WindowFlags flags)
    : QDialog(parent, flags)
    , m_progressBar(nullptr)
    , m_statusLabel(nullptr)
    , m_timeLabel(nullptr)
    , m_cancelButton(nullptr)
    , m_updateTimer(nullptr)
    , m_canceled(false)
    , m_lastProgress(0)
{
    setupUI();
    
    // 创建更新定时器
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &ProgressDialog::updateElapsedTime);
}

/**
 * @brief 析构函数
 */
ProgressDialog::~ProgressDialog()
{
    if (m_updateTimer && m_updateTimer->isActive()) {
        m_updateTimer->stop();
    }
}

/**
 * @brief 初始化UI
 * 
 * 创建并布局所有UI组件：
 * - 状态标签
 * - 进度条
 * - 耗时标签
 * - 取消按钮
 */
void ProgressDialog::setupUI()
{
    // 设置对话框属性
    setWindowTitle(tr("正在处理"));
    setWindowModality(Qt::ApplicationModal);
    setFixedSize(400, 180);
    setWindowFlags(windowFlags() & ~Qt::WindowCloseButtonHint);
    
    // 创建主布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    
    // 状态标签
    m_statusLabel = new QLabel(tr("正在初始化..."));
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet("QLabel { color: #2c3e50; font-size: 13px; }");
    mainLayout->addWidget(m_statusLabel);
    
    // 进度条
    m_progressBar = new QProgressBar();
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setFormat(tr("%p%"));
    m_progressBar->setStyleSheet(
        "QProgressBar {"
        "   border: 1px solid #bdc3c7;"
        "   border-radius: 5px;"
        "   text-align: center;"
        "   background-color: #ecf0f1;"
        "   height: 25px;"
        "}"
        "QProgressBar::chunk {"
        "   background-color: #3498db;"
        "   border-radius: 4px;"
        "}"
    );
    mainLayout->addWidget(m_progressBar);
    
    // 耗时标签
    m_timeLabel = new QLabel(tr("已用时间: 00:00:00"));
    m_timeLabel->setStyleSheet("QLabel { color: #7f8c8d; font-size: 12px; }");
    mainLayout->addWidget(m_timeLabel);
    
    // 添加弹性空间
    mainLayout->addStretch();
    
    // 取消按钮
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    m_cancelButton = new QPushButton(tr("取消"));
    m_cancelButton->setFixedSize(80, 30);
    m_cancelButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #e74c3c;"
        "   color: white;"
        "   border: none;"
        "   border-radius: 4px;"
        "   font-size: 13px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #c0392b;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #a93226;"
        "}"
    );
    connect(m_cancelButton, &QPushButton::clicked, this, &ProgressDialog::onCancelClicked);
    buttonLayout->addWidget(m_cancelButton);
    
    mainLayout->addLayout(buttonLayout);
}

/**
 * @brief 设置进度值
 * @param value 进度值（0-100）
 */
void ProgressDialog::setProgress(int value)
{
    if (m_progressBar) {
        // 确保进度单调递增，避免进度条回退
        int newValue = qBound(0, value, 100);
        if (newValue >= m_lastProgress) {
            m_progressBar->setValue(newValue);
            m_lastProgress = newValue;
            LOG_INFO("ProgressDialog", QString("Progress set to %1%").arg(newValue));
        } else {
            LOG_WARNING("ProgressDialog", QString("Ignoring progress decrease from %1 to %2").arg(m_lastProgress).arg(newValue));
        }
    }
    
    // 每次设置进度时更新耗时显示，不依赖定时器
    if (m_elapsedTimer.isValid()) {
        qint64 elapsed = m_elapsedTimer.elapsed();
        setElapsedTimeFormatted(elapsed);
    }
}

/**
 * @brief 获取当前进度值
 * @return 进度值（0-100）
 */
int ProgressDialog::progress() const
{
    return m_progressBar ? m_progressBar->value() : 0;
}

/**
 * @brief 设置状态文本
 * @param text 状态文本
 */
void ProgressDialog::setStatusText(const QString& text)
{
    if (m_statusLabel) {
        m_statusLabel->setText(text);
    }
    
    if (m_elapsedTimer.isValid()) {
        qint64 elapsed = m_elapsedTimer.elapsed();
        setElapsedTimeFormatted(elapsed);
    }
}

/**
 * @brief 设置已用时间
 * @param milliseconds 已用时间（毫秒）
 */
void ProgressDialog::setElapsedTime(qint64 milliseconds)
{
    setElapsedTimeFormatted(milliseconds);
}

/**
 * @brief 设置已用时间（自动格式化）
 * @param milliseconds 已用时间（毫秒）
 */
void ProgressDialog::setElapsedTimeFormatted(qint64 milliseconds)
{
    if (m_timeLabel) {
        QString timeStr = formatTime(milliseconds);
        QString displayText = tr("已用时间: %1").arg(timeStr);
        m_timeLabel->setText(displayText);
    }
}

/**
 * @brief 检查用户是否点击了取消
 * @return true表示用户已取消，false表示未取消
 */
bool ProgressDialog::wasCanceled() const
{
    return m_canceled;
}

/**
 * @brief 重置对话框状态
 */
void ProgressDialog::reset()
{
    m_canceled = false;
    m_lastProgress = 0;
    setProgress(0);
    setStatusText(tr("正在初始化..."));
    setElapsedTime(0);
}

/**
 * @brief 启动计时器
 */
void ProgressDialog::startTimer()
{
    m_elapsedTimer.start();
    if (m_updateTimer) {
        m_updateTimer->start(1000);
        LOG_INFO("ProgressDialog", "Timer started, updating every second");
    } else {
        LOG_WARNING("ProgressDialog", "m_updateTimer is null!");
    }
}

/**
 * @brief 停止计时器
 */
void ProgressDialog::stopTimer()
{
    if (m_updateTimer && m_updateTimer->isActive()) {
        m_updateTimer->stop();
    }
}

/**
 * @brief 获取已用时间（毫秒）
 * @return 从startTimer()调用到现在的毫秒数
 */
qint64 ProgressDialog::elapsedMilliseconds() const
{
    return m_elapsedTimer.elapsed();
}

/**
 * @brief 更新耗时显示
 * 
 * 每秒更新一次耗时显示
 */
void ProgressDialog::updateElapsedTime()
{
    qint64 elapsed = m_elapsedTimer.elapsed();
    setElapsedTimeFormatted(elapsed);
}

/**
 * @brief 取消按钮点击槽函数
 */
void ProgressDialog::onCancelClicked()
{
    m_canceled = true;
    m_cancelButton->setEnabled(false);
    m_cancelButton->setText(tr("取消中..."));
    emit canceled();
}

/**
 * @brief 格式化时间
 * @param milliseconds 毫秒数
 * @return 格式化后的时间字符串（HH:MM:SS）
 */
QString ProgressDialog::formatTime(qint64 milliseconds) const
{
    int totalSeconds = static_cast<int>(milliseconds / 1000);
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;
    
    return QString("%1:%2:%3")
        .arg(hours, 2, 10, QChar('0'))
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}
