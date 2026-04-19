/**
 * @file LoadingDialog.h
 * @brief 加载提示对话框类定义
 * 
 * LoadingDialog类提供一个简单的加载提示对话框，
 * 用于在执行耗时操作时显示"正在加载..."提示，
 * 操作完成后自动关闭。
 * 
 * 主要功能：
 * - 显示加载动画和提示文字
 * - 非模态对话框，不阻塞用户操作
 * - 支持设置自定义提示文字
 * 
 * 使用示例：
 * @code
 * LoadingDialog* dialog = new LoadingDialog(this);
 * dialog->setText(tr(u8"正在加载甘特图..."));
 * dialog->show();
 * 
 * // 操作完成后
 * dialog->close();
 * dialog->deleteLater();
 * @endcode
 * 
 * @author 1553BTools
 * @date 2026
 */

#ifndef LOADINGDIALOG_H
#define LOADINGDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QMovie>

/**
 * @brief 加载提示对话框类
 * 
 * 该类提供一个轻量级的非模态对话框，
 * 用于显示加载状态提示。
 */
class LoadingDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针
     * @param flags 窗口标志
     */
    explicit LoadingDialog(QWidget* parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags());
    
    /**
     * @brief 析构函数
     */
    ~LoadingDialog();
    
    /**
     * @brief 设置提示文字
     * @param text 提示文字内容
     */
    void setText(const QString& text);
    
    /**
     * @brief 获取提示文字
     * @return 当前提示文字
     */
    QString text() const;

private:
    /**
     * @brief 初始化UI
     */
    void setupUI();

private:
    QLabel* m_iconLabel;      // 图标标签（显示加载动画）
    QLabel* m_textLabel;      // 文字标签
    QMovie* m_loadingMovie;   // 加载动画（可选）
};

#endif
