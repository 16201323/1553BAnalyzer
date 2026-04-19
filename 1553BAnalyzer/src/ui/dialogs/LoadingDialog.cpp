/**
 * @file LoadingDialog.cpp
 * @brief 加载提示对话框类实现
 * 
 * 本文件实现了LoadingDialog类的所有方法，包括：
 * - 对话框界面初始化
 * - 加载动画显示
 * - 提示文字设置
 * 
 * @author 1553BTools
 * @date 2026
 */

#include "LoadingDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QProgressBar>

/**
 * @brief 构造函数，初始化加载提示对话框
 * @param parent 父窗口指针
 * @param flags 窗口标志
 *
 * 设置对话框为非模态，固定大小，无标题栏按钮
 */
LoadingDialog::LoadingDialog(QWidget* parent, Qt::WindowFlags flags)
    : QDialog(parent, flags)
    , m_iconLabel(nullptr)
    , m_textLabel(nullptr)
    , m_loadingMovie(nullptr)
{
    setupUI();
}

/**
 * @brief 析构函数
 *
 * 清理加载动画资源
 */
LoadingDialog::~LoadingDialog()
{
    if (m_loadingMovie) {
        m_loadingMovie->stop();
        delete m_loadingMovie;
        m_loadingMovie = nullptr;
    }
}

/**
 * @brief 初始化界面布局
 *
 * 布局结构：
 * - 水平布局：加载图标 + 提示文字
 * - 设置对话框为固定大小、无边框、居中显示
 */
void LoadingDialog::setupUI()
{
    /* 设置对话框属性：无边框、固定大小、居中 */
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setFixedSize(280, 100);
    
    /* 创建主布局 */
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    
    /* 创建水平布局用于图标和文字 */
    QHBoxLayout* contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(15);
    
    /* 创建加载图标标签，使用Unicode字符模拟加载动画 */
    m_iconLabel = new QLabel(this);
    m_iconLabel->setText(QString::fromUtf8(u8"⏳"));
    m_iconLabel->setStyleSheet(
        "QLabel {"
        "  font-size: 32px;"
        "  color: #3498db;"
        "}"
    );
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setFixedSize(50, 50);
    contentLayout->addWidget(m_iconLabel);
    
    /* 创建提示文字标签 */
    m_textLabel = new QLabel(this);
    m_textLabel->setText(QString::fromUtf8(u8"正在加载..."));
    m_textLabel->setStyleSheet(
        "QLabel {"
        "  font-size: 14px;"
        "  color: #2c3e50;"
        "  font-weight: bold;"
        "}"
    );
    m_textLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    contentLayout->addWidget(m_textLabel, 1);
    
    mainLayout->addLayout(contentLayout);
    
    /* 添加进度条作为视觉提示 */
    QProgressBar* progressBar = new QProgressBar(this);
    progressBar->setRange(0, 0);  // 设置为不确定进度模式（滚动样式）
    progressBar->setTextVisible(false);
    progressBar->setFixedHeight(6);
    progressBar->setStyleSheet(
        "QProgressBar {"
        "  border: none;"
        "  background-color: #ecf0f1;"
        "  border-radius: 3px;"
        "}"
        "QProgressBar::chunk {"
        "  background-color: #3498db;"
        "  border-radius: 3px;"
        "}"
    );
    mainLayout->addWidget(progressBar);
    
    /* 设置对话框整体样式 */
    setStyleSheet(
        "QDialog {"
        "  background-color: white;"
        "  border: 1px solid #bdc3c7;"
        "  border-radius: 8px;"
        "}"
    );
}

/**
 * @brief 设置提示文字
 * @param text 提示文字内容
 */
void LoadingDialog::setText(const QString& text)
{
    if (m_textLabel) {
        m_textLabel->setText(text);
    }
}

/**
 * @brief 获取提示文字
 * @return 当前提示文字
 */
QString LoadingDialog::text() const
{
    return m_textLabel ? m_textLabel->text() : QString();
}
