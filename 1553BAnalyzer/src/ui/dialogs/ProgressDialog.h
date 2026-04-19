/**
 * @file ProgressDialog.h
 * @brief 进度对话框类定义
 * 
 * ProgressDialog类提供文件加载和数据筛选的进度显示对话框，
 * 包含进度条、状态文本、耗时显示和取消按钮。
 * 
 * 主要功能：
 * - 显示操作进度（0-100%）
 * - 显示当前操作状态
 * - 显示已用时间（时:分:秒格式）
 * - 支持取消操作
 * 
 * 使用示例：
 * @code
 * ProgressDialog dialog(this);
 * dialog.setWindowTitle("正在加载文件");
 * dialog.show();
 * 
 * // 更新进度
 * dialog.setProgress(50);
 * dialog.setStatusText("正在解析数据...");
 * dialog.setElapsedTime(12345);  // 12.345秒
 * 
 * // 检查是否取消
 * if (dialog.wasCanceled()) {
 *     // 取消操作
 * }
 * @endcode
 * 
 * @author 1553BTools
 * @date 2026
 */

#ifndef PROGRESSDIALOG_H
#define PROGRESSDIALOG_H

#include <QDialog>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QElapsedTimer>
#include <atomic>

/**
 * @brief 进度对话框类
 * 
 * 该类提供了一个模态对话框，用于显示长时间操作的进度。
 * 支持显示进度百分比、状态文本和已用时间。
 */
class ProgressDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针
     * @param flags 窗口标志
     */
    explicit ProgressDialog(QWidget* parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags());
    
    /**
     * @brief 析构函数
     */
    ~ProgressDialog();
    
    /**
     * @brief 设置进度值
     * @param value 进度值（0-100）
     */
    void setProgress(int value);
    
    /**
     * @brief 获取当前进度值
     * @return 进度值（0-100）
     */
    int progress() const;
    
    /**
     * @brief 设置状态文本
     * @param text 状态文本
     */
    void setStatusText(const QString& text);
    
    /**
     * @brief 设置已用时间
     * @param milliseconds 已用时间（毫秒）
     */
    void setElapsedTime(qint64 milliseconds);
    
    /**
     * @brief 设置已用时间（自动格式化）
     * @param milliseconds 已用时间（毫秒）
     * 
     * 自动格式化为 HH:MM:SS 格式
     */
    void setElapsedTimeFormatted(qint64 milliseconds);
    
    /**
     * @brief 检查用户是否点击了取消
     * @return true表示用户已取消，false表示未取消
     */
    bool wasCanceled() const;
    
    /**
     * @brief 重置对话框状态
     * 
     * 重置进度、状态文本和耗时为初始值
     */
    void reset();
    
    /**
     * @brief 启动计时器
     * 
     * 启动内部计时器，自动更新耗时显示
     */
    void startTimer();
    
    /**
     * @brief 停止计时器
     */
    void stopTimer();
    
    /**
     * @brief 获取已用时间（毫秒）
     * @return 从startTimer()调用到现在的毫秒数
     */
    qint64 elapsedMilliseconds() const;

signals:
    /**
     * @brief 取消按钮点击信号
     */
    void canceled();

private slots:
    /**
     * @brief 更新耗时显示
     * 
     * 每秒更新一次耗时显示
     */
    void updateElapsedTime();
    
    /**
     * @brief 取消按钮点击槽函数
     */
    void onCancelClicked();

private:
    /**
     * @brief 初始化UI
     */
    void setupUI();
    
    /**
     * @brief 格式化时间
     * @param milliseconds 毫秒数
     * @return 格式化后的时间字符串（HH:MM:SS）
     */
    QString formatTime(qint64 milliseconds) const;

private:
    QProgressBar* m_progressBar;    // 进度条
    QLabel* m_statusLabel;          // 状态标签
    QLabel* m_timeLabel;            // 耗时标签
    QPushButton* m_cancelButton;    // 取消按钮
    
    QTimer* m_updateTimer;          // 更新定时器
    QElapsedTimer m_elapsedTimer;   // 计时器
    
    std::atomic<bool> m_canceled;   // 是否已取消（线程安全）
    int m_lastProgress;             // 上次设置的进度值（用于确保单调递增）
};

#endif
