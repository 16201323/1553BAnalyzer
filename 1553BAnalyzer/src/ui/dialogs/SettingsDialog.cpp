/**
 * @file SettingsDialog.cpp
 * @brief 设置对话框实现
 *
 * 本文件实现了应用程序的设置对话框，包含以下选项卡：
 * - 解析器设置：字节序、包头标识、时间戳单位
 * - 数据库设置：自动切换阈值
 * - AI模型设置：提供商配置、API密钥、系统提示词
 * - 甘特图设置：消息类型颜色（带颜色预览和选择器）
 * - 导出设置：默认格式和编码
 * - 语音设置：Vosk模型路径、采样率、启用开关
 *
 * 设置通过ConfigManager持久化到XML配置文件。
 * UI采用深色主题风格，使用QGroupBox分组、颜色预览按钮等美化元素。
 *
 * @author 1553BTools
 * @date 2024
 */

#include "SettingsDialog.h"
#include "core/config/ConfigManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QLabel>
#include <QColorDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QUrl>
#include <QScrollArea>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
    , m_tabWidget(new QTabWidget(this))
{
    setupUI();
    loadSettings();
}

SettingsDialog::~SettingsDialog()
{
}

void SettingsDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    /* 设置对话框整体边距和间距 */
    mainLayout->setContentsMargins(12, 12, 12, 8);
    mainLayout->setSpacing(8);

    mainLayout->addWidget(m_tabWidget);

    setupParserTab();
    setupDatabaseTab();
    setupModelTab();
    setupSpeechTab();
    setupGanttTab();
    setupExportTab();

    /* 底部按钮区域：确定、取消、重置默认 */
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Reset, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttonBox->button(QDialogButtonBox::Reset), &QPushButton::clicked,
            this, &SettingsDialog::onResetDefaults);

    mainLayout->addWidget(buttonBox);

    /* 配置文件路径区域：显示路径 + 打开按钮 */
    QHBoxLayout* configPathLayout = new QHBoxLayout();
    QLabel* configPathLabel = new QLabel(
        tr(u8"配置文件: %1").arg(ConfigManager::instance()->configFilePath()));
    configPathLabel->setWordWrap(true);
    configPathLabel->setStyleSheet("color: #7f8c8d; font-size: 11px;");
    configPathLayout->addWidget(configPathLabel, 1);

    QPushButton* openConfigBtn = new QPushButton(tr(u8"打开配置文件"));
    openConfigBtn->setToolTip(tr(u8"用系统默认编辑器打开 config.xml"));
    openConfigBtn->setStyleSheet("font-size: 11px; padding: 3px 8px;");
    connect(openConfigBtn, &QPushButton::clicked, this, []() {
        QString path = ConfigManager::instance()->configFilePath();
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });
    configPathLayout->addWidget(openConfigBtn);
    mainLayout->addLayout(configPathLayout);

    setWindowTitle(tr(u8"设置"));
    setMinimumSize(560, 480);
}

/**
 * @brief 创建解析器设置选项卡
 *
 * 使用QGroupBox将设置项分组展示，添加说明文字提示每个配置项的用途。
 * 布局结构：
 * - 协议解析分组：字节序、包头标识、数据头标识
 * - 容错设置分组：最大错误容差
 * - 底部说明提示
 */
void SettingsDialog::setupParserTab()
{
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    /* ---- 协议解析参数分组 ---- */
    QGroupBox* protocolGroup = new QGroupBox(tr(u8"协议解析参数"));
    protocolGroup->setToolTip(tr(u8"配置1553B协议的解析规则"));
    QFormLayout* protocolLayout = new QFormLayout(protocolGroup);
    protocolLayout->setHorizontalSpacing(16);
    protocolLayout->setVerticalSpacing(10);
    protocolLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_byteOrderCombo = new QComboBox();
    m_byteOrderCombo->addItem(u8"小端序 (Little-Endian)", "little");
    m_byteOrderCombo->addItem(u8"大端序 (Big-Endian)", "big");
    m_byteOrderCombo->setToolTip(tr(u8"选择数据的字节序格式，通常为小端序"));
    protocolLayout->addRow(tr(u8"字节序:"), m_byteOrderCombo);

    m_header1Edit = new QLineEdit();
    m_header1Edit->setPlaceholderText("0xA5A5");
    m_header1Edit->setToolTip(tr(u8"命令字/状态字的同步头标识（16位十六进制）"));
    protocolLayout->addRow(tr(u8"包头1:"), m_header1Edit);

    m_header2Edit = new QLineEdit();
    m_header2Edit->setPlaceholderText("0xA5");
    m_header2Edit->setToolTip(tr(u8"数据字的同步头标识（8位十六进制）"));
    protocolLayout->addRow(tr(u8"包头2:"), m_header2Edit);

    m_dataHeaderEdit = new QLineEdit();
    m_dataHeaderEdit->setPlaceholderText("0xAABB");
    m_dataHeaderEdit->setToolTip(tr(u8"数据块的起始标识（16位十六进制）"));
    protocolLayout->addRow(tr(u8"数据头:"), m_dataHeaderEdit);

    layout->addWidget(protocolGroup);

    /* ---- 容错设置分组 ---- */
    QGroupBox* toleranceGroup = new QGroupBox(tr(u8"容错设置"));
    toleranceGroup->setToolTip(tr(u8"配置解析器的错误容忍策略"));
    QFormLayout* toleranceLayout = new QFormLayout(toleranceGroup);
    toleranceLayout->setHorizontalSpacing(16);
    toleranceLayout->setVerticalSpacing(10);
    toleranceLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_maxErrorSpin = new QSpinBox();
    m_maxErrorSpin->setRange(0, 100);
    m_maxErrorSpin->setToolTip(tr(u8"连续遇到无法识别的数据时，跳过的最大字节数\n设为0表示遇到错误立即停止"));
    toleranceLayout->addRow(tr(u8"最大错误容差:"), m_maxErrorSpin);

    layout->addWidget(toleranceGroup);

    /* ---- 底部说明 ---- */
    QLabel* hintLabel = new QLabel(tr(
        u8"<i style='color:#7f8c8d;'>提示：修改协议参数后需要重新加载数据文件才能生效</i>"
    ));
    hintLabel->setWordWrap(true);
    layout->addWidget(hintLabel);

    layout->addStretch();

    m_tabWidget->addTab(tab, u8"解析设置");
}

/**
 * @brief 创建数据库设置选项卡
 *
 * 使用QGroupBox展示存储模式设置，添加详细说明帮助用户理解阈值含义。
 * 布局结构：
 * - 存储模式设置分组：数据库模式阈值
 * - 说明信息区域
 */
void SettingsDialog::setupDatabaseTab()
{
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    /* ---- 存储模式设置分组 ---- */
    QGroupBox* storageGroup = new QGroupBox(tr(u8"存储模式设置"));
    storageGroup->setToolTip(tr(u8"配置数据存储的自动切换策略"));
    QFormLayout* formLayout = new QFormLayout(storageGroup);
    formLayout->setHorizontalSpacing(16);
    formLayout->setVerticalSpacing(10);
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_recordThresholdSpin = new QSpinBox();
    m_recordThresholdSpin->setRange(0, 10000000);
    m_recordThresholdSpin->setSingleStep(100);
    m_recordThresholdSpin->setToolTip(tr(u8"当数据记录数超过此阈值时，自动使用数据库模式存储"));
    formLayout->addRow(tr(u8"数据库模式阈值:"), m_recordThresholdSpin);

    /* 阈值单位提示 */
    QLabel* unitLabel = new QLabel(tr(u8"<i style='color:#7f8c8d;'>条记录</i>"));
    formLayout->addRow(QString(), unitLabel);

    layout->addWidget(storageGroup);

    /* ---- 说明信息 ---- */
    QLabel* hintLabel = new QLabel(tr(
        u8"<p style='color:#CCCCCC; font-weight:bold; font-size:12px;'>"
        u8"📋 存储模式说明</p>"
        u8"<table cellpadding='4' style='color:#CCCCCC; font-size:11px;'>"
        u8"<tr><td style='color:#4EC9B0;'>● 内存模式</td>"
        u8"<td>数据量未超过阈值时使用，响应更快，适合小数据量分析</td></tr>"
        u8"<tr><td style='color:#CE9178;'>● 数据库模式</td>"
        u8"<td>数据量超过阈值时自动切换，适合大数据量分析</td></tr>"
        u8"</table>"
        u8"<p style='color:#7f8c8d; font-size:11px;'>默认阈值为 50,000 条记录</p>"
    ));
    hintLabel->setWordWrap(true);
    layout->addWidget(hintLabel);

    layout->addStretch();

    m_tabWidget->addTab(tab, tr(u8"数据库设置"));
}

/**
 * @brief 创建AI模型设置选项卡
 *
 * 使用QGroupBox展示API连接配置，美化测试连接按钮。
 * 布局结构：
 * - API连接设置分组：超时时间、重试次数
 * - 测试连接按钮
 * - 配置文件提示
 */
void SettingsDialog::setupModelTab()
{
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    /* ---- API连接设置分组 ---- */
    QGroupBox* apiGroup = new QGroupBox(tr(u8"API 连接设置"));
    apiGroup->setToolTip(tr(u8"配置AI模型的API连接参数"));
    QFormLayout* apiLayout = new QFormLayout(apiGroup);
    apiLayout->setHorizontalSpacing(16);
    apiLayout->setVerticalSpacing(10);
    apiLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_apiTimeoutEdit = new QLineEdit();
    m_apiTimeoutEdit->setPlaceholderText("60");
    m_apiTimeoutEdit->setToolTip(tr(u8"API请求的超时时间（秒），超过此时间未响应将自动断开"));
    apiLayout->addRow(tr(u8"超时时间 (秒):"), m_apiTimeoutEdit);

    m_retryCountEdit = new QLineEdit();
    m_retryCountEdit->setPlaceholderText("3");
    m_retryCountEdit->setToolTip(tr(u8"API请求失败后的自动重试次数"));
    apiLayout->addRow(tr(u8"重试次数:"), m_retryCountEdit);

    layout->addWidget(apiGroup);

    /* ---- 测试连接按钮 ---- */
    QHBoxLayout* testBtnLayout = new QHBoxLayout();
    testBtnLayout->addStretch();
    QPushButton* testBtn = new QPushButton(tr(u8"🔗 测试连接"));
    testBtn->setToolTip(tr(u8"使用当前配置测试AI模型API是否可用"));
    testBtn->setMinimumWidth(140);
    testBtn->setStyleSheet(
        "QPushButton {"
        "   background-color: #0E639C;"
        "   color: white;"
        "   border: none;"
        "   padding: 8px 20px;"
        "   border-radius: 4px;"
        "   font-weight: bold;"
        "   font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #1177BB;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #0A4F7D;"
        "}"
    );
    connect(testBtn, &QPushButton::clicked, this, &SettingsDialog::onTestConnection);
    testBtnLayout->addWidget(testBtn);
    layout->addLayout(testBtnLayout);

    layout->addSpacing(8);

    /* ---- 配置文件提示 ---- */
    QLabel* hintLabel = new QLabel(tr(
        u8"<p style='color:#CCCCCC; font-weight:bold; font-size:12px;'>"
        u8"⚙ 模型提供商配置</p>"
        u8"<p style='color:#7f8c8d; font-size:11px;'>"
        u8"AI模型提供商（如OpenAI、DeepSeek等）的API密钥、URL和模型名称"
        u8"请在 <b>config.xml</b> 文件中修改。</p>"
    ));
    hintLabel->setWordWrap(true);
    layout->addWidget(hintLabel);

    layout->addStretch();

    m_tabWidget->addTab(tab, tr(u8"模型设置"));
}

/**
 * @brief 创建甘特图颜色设置选项卡
 *
 * 使用QGroupBox展示各消息类型的颜色配置，每个颜色项包含：
 * - 颜色名称标签
 * - 颜色值输入框（支持手动输入十六进制颜色值）
 * - 颜色预览/选择按钮（点击打开颜色选择器，实时预览当前颜色）
 *
 * 布局结构：
 * - 消息类型颜色分组：BC→RT、RT→BC、RT→RT、广播、错误
 * - 底部说明
 */
void SettingsDialog::setupGanttTab()
{
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    /* ---- 消息类型颜色分组 ---- */
    QGroupBox* colorGroup = new QGroupBox(tr(u8"消息类型颜色"));
    colorGroup->setToolTip(tr(u8"配置甘特图中不同消息类型的显示颜色"));
    QFormLayout* colorLayout = new QFormLayout(colorGroup);
    colorLayout->setHorizontalSpacing(12);
    colorLayout->setVerticalSpacing(10);
    colorLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    /* BC→RT 颜色行：输入框 + 预览按钮 */
    QHBoxLayout* bc2rtLayout = new QHBoxLayout();
    m_colorBC2RTEdit = new QLineEdit();
    m_colorBC2RTEdit->setToolTip(tr(u8"BC→RT消息的显示颜色（十六进制，如 #3498DB）"));
    bc2rtLayout->addWidget(m_colorBC2RTEdit, 1);
    m_colorBC2RTBtn = new QPushButton();
    m_colorBC2RTBtn->setFixedSize(32, 24);
    m_colorBC2RTBtn->setToolTip(tr(u8"点击选择颜色"));
    m_colorBC2RTBtn->setCursor(Qt::PointingHandCursor);
    bc2rtLayout->addWidget(m_colorBC2RTBtn);
    colorLayout->addRow(tr(u8"BC→RT:"), bc2rtLayout);

    /* RT→BC 颜色行 */
    QHBoxLayout* rt2bcLayout = new QHBoxLayout();
    m_colorRT2BCEdit = new QLineEdit();
    m_colorRT2BCEdit->setToolTip(tr(u8"RT→BC消息的显示颜色（十六进制，如 #2ECC71）"));
    rt2bcLayout->addWidget(m_colorRT2BCEdit, 1);
    m_colorRT2BCBtn = new QPushButton();
    m_colorRT2BCBtn->setFixedSize(32, 24);
    m_colorRT2BCBtn->setToolTip(tr(u8"点击选择颜色"));
    m_colorRT2BCBtn->setCursor(Qt::PointingHandCursor);
    rt2bcLayout->addWidget(m_colorRT2BCBtn);
    colorLayout->addRow(tr(u8"RT→BC:"), rt2bcLayout);

    /* RT→RT 颜色行 */
    QHBoxLayout* rt2rtLayout = new QHBoxLayout();
    m_colorRT2RTEdit = new QLineEdit();
    m_colorRT2RTEdit->setToolTip(tr(u8"RT→RT消息的显示颜色（十六进制，如 #E67E22）"));
    rt2rtLayout->addWidget(m_colorRT2RTEdit, 1);
    m_colorRT2RTBtn = new QPushButton();
    m_colorRT2RTBtn->setFixedSize(32, 24);
    m_colorRT2RTBtn->setToolTip(tr(u8"点击选择颜色"));
    m_colorRT2RTBtn->setCursor(Qt::PointingHandCursor);
    rt2rtLayout->addWidget(m_colorRT2RTBtn);
    colorLayout->addRow(tr(u8"RT→RT:"), rt2rtLayout);

    /* 广播颜色行 */
    QHBoxLayout* broadcastLayout = new QHBoxLayout();
    m_colorBroadcastEdit = new QLineEdit();
    m_colorBroadcastEdit->setToolTip(tr(u8"广播消息的显示颜色（十六进制，如 #9B59B6）"));
    broadcastLayout->addWidget(m_colorBroadcastEdit, 1);
    m_colorBroadcastBtn = new QPushButton();
    m_colorBroadcastBtn->setFixedSize(32, 24);
    m_colorBroadcastBtn->setToolTip(tr(u8"点击选择颜色"));
    m_colorBroadcastBtn->setCursor(Qt::PointingHandCursor);
    broadcastLayout->addWidget(m_colorBroadcastBtn);
    colorLayout->addRow(tr(u8"广播:"), broadcastLayout);

    /* 错误颜色行 */
    QHBoxLayout* errorLayout = new QHBoxLayout();
    m_colorErrorEdit = new QLineEdit();
    m_colorErrorEdit->setToolTip(tr(u8"错误消息的显示颜色（十六进制，如 #E74C3C）"));
    errorLayout->addWidget(m_colorErrorEdit, 1);
    m_colorErrorBtn = new QPushButton();
    m_colorErrorBtn->setFixedSize(32, 24);
    m_colorErrorBtn->setToolTip(tr(u8"点击选择颜色"));
    m_colorErrorBtn->setCursor(Qt::PointingHandCursor);
    errorLayout->addWidget(m_colorErrorBtn);
    colorLayout->addRow(tr(u8"错误:"), errorLayout);

    layout->addWidget(colorGroup);

    /* ---- 连接颜色选择信号 ---- */
    connect(m_colorBC2RTBtn, &QPushButton::clicked, this, [this]() {
        onPickColor(m_colorBC2RTEdit, m_colorBC2RTBtn);
    });
    connect(m_colorRT2BCBtn, &QPushButton::clicked, this, [this]() {
        onPickColor(m_colorRT2BCEdit, m_colorRT2BCBtn);
    });
    connect(m_colorRT2RTBtn, &QPushButton::clicked, this, [this]() {
        onPickColor(m_colorRT2RTEdit, m_colorRT2RTBtn);
    });
    connect(m_colorBroadcastBtn, &QPushButton::clicked, this, [this]() {
        onPickColor(m_colorBroadcastEdit, m_colorBroadcastBtn);
    });
    connect(m_colorErrorBtn, &QPushButton::clicked, this, [this]() {
        onPickColor(m_colorErrorEdit, m_colorErrorBtn);
    });

    /* ---- 连接文本变化信号，实时更新颜色预览 ---- */
    connect(m_colorBC2RTEdit, &QLineEdit::textChanged, this, [this]() {
        updateColorPreview(m_colorBC2RTEdit, m_colorBC2RTBtn);
    });
    connect(m_colorRT2BCEdit, &QLineEdit::textChanged, this, [this]() {
        updateColorPreview(m_colorRT2BCEdit, m_colorRT2BCBtn);
    });
    connect(m_colorRT2RTEdit, &QLineEdit::textChanged, this, [this]() {
        updateColorPreview(m_colorRT2RTEdit, m_colorRT2RTBtn);
    });
    connect(m_colorBroadcastEdit, &QLineEdit::textChanged, this, [this]() {
        updateColorPreview(m_colorBroadcastEdit, m_colorBroadcastBtn);
    });
    connect(m_colorErrorEdit, &QLineEdit::textChanged, this, [this]() {
        updateColorPreview(m_colorErrorEdit, m_colorErrorBtn);
    });

    /* ---- 底部说明 ---- */
    QLabel* hintLabel = new QLabel(tr(
        u8"<i style='color:#7f8c8d;'>提示：点击颜色方块可打开颜色选择器，也可直接输入十六进制颜色值（如 #3498DB）</i>"
    ));
    hintLabel->setWordWrap(true);
    layout->addWidget(hintLabel);

    layout->addStretch();

    m_tabWidget->addTab(tab, tr(u8"甘特图设置"));
}

/**
 * @brief 创建导出设置选项卡
 *
 * 使用QGroupBox分组展示导出格式和编码设置，添加说明帮助用户选择。
 * 布局结构：
 * - 导出格式分组：默认导出格式、文件编码
 * - 报告设置分组：智能分析报告格式
 * - 底部说明
 */
void SettingsDialog::setupExportTab()
{
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    /* ---- 导出格式分组 ---- */
    QGroupBox* formatGroup = new QGroupBox(tr(u8"导出格式"));
    formatGroup->setToolTip(tr(u8"配置数据导出的默认格式和编码方式"));
    QFormLayout* formatLayout = new QFormLayout(formatGroup);
    formatLayout->setHorizontalSpacing(16);
    formatLayout->setVerticalSpacing(10);
    formatLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_exportFormatCombo = new QComboBox();
    m_exportFormatCombo->addItem(tr("CSV (.csv)"), "csv");
    m_exportFormatCombo->addItem(tr("Excel (.xlsx)"), "xlsx");
    m_exportFormatCombo->setToolTip(tr(u8"选择数据导出的默认文件格式"));
    formatLayout->addRow(tr(u8"默认格式:"), m_exportFormatCombo);

    m_encodingCombo = new QComboBox();
    m_encodingCombo->addItem("UTF-8", "UTF-8");
    m_encodingCombo->addItem("GBK", "GBK");
    m_encodingCombo->setToolTip(tr(u8"选择导出文件的文本编码方式\nUTF-8兼容性最好，GBK适合中文Windows系统"));
    formatLayout->addRow(tr(u8"文件编码:"), m_encodingCombo);

    layout->addWidget(formatGroup);

    /* ---- 报告设置分组 ---- */
    QGroupBox* reportGroup = new QGroupBox(tr(u8"智能分析报告"));
    reportGroup->setToolTip(tr(u8"配置AI智能分析报告的输出格式"));
    QFormLayout* reportLayout = new QFormLayout(reportGroup);
    reportLayout->setHorizontalSpacing(16);
    reportLayout->setVerticalSpacing(10);
    reportLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_reportFormatCombo = new QComboBox();
    m_reportFormatCombo->addItem(tr(u8"HTML 网页"), "html");
    m_reportFormatCombo->addItem(tr(u8"PDF 文档"), "pdf");
    m_reportFormatCombo->addItem(tr(u8"Word 文档"), "docx");
    m_reportFormatCombo->setToolTip(tr(u8"选择智能分析报告的输出格式\nHTML可在浏览器中查看，PDF/DOCX便于分享"));
    reportLayout->addRow(tr(u8"报告格式:"), m_reportFormatCombo);

    layout->addWidget(reportGroup);

    /* ---- 底部说明 ---- */
    QLabel* hintLabel = new QLabel(tr(
        u8"<p style='color:#CCCCCC; font-weight:bold; font-size:12px;'>"
        u8"📁 格式说明</p>"
        u8"<table cellpadding='4' style='color:#CCCCCC; font-size:11px;'>"
        u8"<tr><td style='color:#4EC9B0;'>● CSV</td>"
        u8"<td>通用文本格式，兼容性最好，可用Excel打开</td></tr>"
        u8"<tr><td style='color:#4EC9B0;'>● Excel</td>"
        u8"<td>原生Excel格式，支持多Sheet和格式化</td></tr>"
        u8"<tr><td style='color:#CE9178;'>● HTML</td>"
        u8"<td>网页格式报告，支持图表和超链接</td></tr>"
        u8"<tr><td style='color:#CE9178;'>● PDF</td>"
        u8"<td>便携文档格式，适合打印和分享</td></tr>"
        u8"<tr><td style='color:#CE9178;'>● Word</td>"
        u8"<td>可编辑文档格式，便于二次修改</td></tr>"
        u8"</table>"
    ));
    hintLabel->setWordWrap(true);
    layout->addWidget(hintLabel);

    layout->addStretch();

    m_tabWidget->addTab(tab, tr(u8"导出设置"));
}

/**
 * @brief 创建语音识别设置选项卡
 *
 * 提供以下配置项：
 * - 启用语音识别：总开关
 * - 模型路径：Vosk 模型目录
 * - 采样率：音频采样率（固定 16000Hz）
 *
 * 注意：此标签页保持原有设计，不做美化修改
 */
void SettingsDialog::setupSpeechTab()
{
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(tab);

    /* 启用语音识别开关 */
    m_enableSpeechCheck = new QCheckBox(tr(u8"启用语音识别功能"));
    m_enableSpeechCheck->setToolTip(tr(u8"禁用后语音按钮将不可用"));
    layout->addWidget(m_enableSpeechCheck);

    layout->addSpacing(10);

    /* 模型路径配置 */
    QGroupBox* modelGroup = new QGroupBox(tr(u8"语音模型配置"));
    QFormLayout* modelLayout = new QFormLayout(modelGroup);

    QHBoxLayout* modelPathLayout = new QHBoxLayout();
    m_modelPathEdit = new QLineEdit();
    m_modelPathEdit->setPlaceholderText(tr("models/vosk-model-cn-0.22"));
    m_modelPathEdit->setToolTip(tr(u8"Vosk 模型目录路径，可以是相对路径或绝对路径"));
    modelPathLayout->addWidget(m_modelPathEdit);

    m_browseModelBtn = new QPushButton(tr(u8"浏览..."));
    m_browseModelBtn->setToolTip(tr(u8"选择模型目录"));
    m_browseModelBtn->setMaximumWidth(60);
    connect(m_browseModelBtn, &QPushButton::clicked, this, &SettingsDialog::onBrowseModelPath);
    modelPathLayout->addWidget(m_browseModelBtn);

    modelLayout->addRow(tr(u8"模型路径:"), modelPathLayout);

    m_sampleRateSpin = new QSpinBox();
    m_sampleRateSpin->setRange(8000, 48000);
    m_sampleRateSpin->setSingleStep(8000);
    m_sampleRateSpin->setValue(16000);
    m_sampleRateSpin->setToolTip(tr(u8"音频采样率，Vosk 要求 16000Hz"));
    m_sampleRateSpin->setEnabled(false); // 目前固定为 16000Hz
    modelLayout->addRow(tr(u8"采样率 (Hz):"), m_sampleRateSpin);

    layout->addWidget(modelGroup);

    layout->addSpacing(10);

    /* 说明信息 */
    QLabel* hintLabel = new QLabel(tr(
        u8"<p><b>说明：</b></p>"
        "<ul>"
        u8"<li><b>中文模型：</b>vosk-model-cn-0.22（约 400MB）</li>"
        u8"<li><b>英文模型：</b>vosk-model-en-us-0.22（约 700MB）</li>"
        u8"<li>模型路径可以是相对路径（相对于程序目录）或绝对路径</li>"
        u8"<li>切换模型后需重启程序才能生效</li>"
        u8"<li>采样率目前固定为 16000Hz，不可修改</li>"
        "</ul>"
        u8"<p><b>下载链接：</b></p>"
        "<ul>"
        u8"<li><a href='https://alphacephei.com/vosk/models/vosk-model-cn-0.22.zip'>中文模型下载</a></li>"
        u8"<li><a href='https://alphacephei.com/vosk/models/vosk-model-en-us-0.22.zip'>英文模型下载</a></li>"
        "</ul>"
    ));
    hintLabel->setWordWrap(true);
    hintLabel->setOpenExternalLinks(true);
    hintLabel->setStyleSheet("color: #7f8c8d; font-size: 11px;");
    layout->addWidget(hintLabel);

    layout->addStretch();

    m_tabWidget->addTab(tab, tr(u8"语音设置"));
}

void SettingsDialog::loadSettings()
{
    ConfigManager* cfg = ConfigManager::instance();
    ParserConfig parserCfg = cfg->getParserConfig();
    GanttConfig ganttCfg = cfg->getGanttConfig();
    DatabaseConfig dbCfg = cfg->getDatabaseConfig();

    m_byteOrderCombo->setCurrentIndex(
        parserCfg.byteOrder == "little" ? 0 : 1);
    m_header1Edit->setText(QString("0x%1").arg(parserCfg.header1, 4, 16, QChar('0')));
    m_header2Edit->setText(QString("0x%1").arg(parserCfg.header2 & 0xFF, 2, 16, QChar('0')));
    m_dataHeaderEdit->setText(QString("0x%1").arg(parserCfg.dataHeader, 4, 16, QChar('0')));
    m_maxErrorSpin->setValue(parserCfg.maxErrorTolerance);

    m_recordThresholdSpin->setValue(dbCfg.recordThreshold);

    m_apiTimeoutEdit->setText(QString::number(cfg->getApiTimeout()));
    m_retryCountEdit->setText(QString::number(cfg->getRetryCount()));

    m_colorBC2RTEdit->setText(ganttCfg.colorBC2RT.name());
    m_colorRT2BCEdit->setText(ganttCfg.colorRT2BC.name());
    m_colorRT2RTEdit->setText(ganttCfg.colorRT2RT.name());
    m_colorBroadcastEdit->setText(ganttCfg.colorBroadcast.name());
    m_colorErrorEdit->setText(ganttCfg.colorError.name());

    /* 加载颜色后更新预览按钮 */
    updateColorPreview(m_colorBC2RTEdit, m_colorBC2RTBtn);
    updateColorPreview(m_colorRT2BCEdit, m_colorRT2BCBtn);
    updateColorPreview(m_colorRT2RTEdit, m_colorRT2RTBtn);
    updateColorPreview(m_colorBroadcastEdit, m_colorBroadcastBtn);
    updateColorPreview(m_colorErrorEdit, m_colorErrorBtn);

    QString reportFormat = cfg->getReportFormat();
    int reportIdx = m_reportFormatCombo->findData(reportFormat);
    if (reportIdx >= 0) {
        m_reportFormatCombo->setCurrentIndex(reportIdx);
    } else {
        m_reportFormatCombo->setCurrentIndex(0);
    }

    /* 加载语音设置 */
    SpeechConfig speechCfg = cfg->getSpeechConfig();
    m_modelPathEdit->setText(speechCfg.engine.modelPath);
    m_sampleRateSpin->setValue(speechCfg.engine.sampleRate);
    m_enableSpeechCheck->setChecked(speechCfg.engine.enabled);
}

void SettingsDialog::saveSettings()
{
    ConfigManager* cfg = ConfigManager::instance();

    /* 先获取当前配置再修改，避免丢失未在UI中显示的字段（如timestampUnit） */
    ParserConfig parserCfg = cfg->getParserConfig();
    parserCfg.byteOrder = m_byteOrderCombo->currentData().toString();
    parserCfg.header1 = m_header1Edit->text().toUShort(nullptr, 0);
    parserCfg.header2 = m_header2Edit->text().toUShort(nullptr, 0) & 0xFF;
    parserCfg.dataHeader = m_dataHeaderEdit->text().toUShort(nullptr, 0);
    parserCfg.maxErrorTolerance = m_maxErrorSpin->value();
    cfg->setParserConfig(parserCfg);

    DatabaseConfig dbCfg = cfg->getDatabaseConfig();
    dbCfg.recordThreshold = m_recordThresholdSpin->value();
    cfg->setDatabaseConfig(dbCfg);

    GanttConfig ganttCfg = cfg->getGanttConfig();
    ganttCfg.colorBC2RT = QColor(m_colorBC2RTEdit->text());
    ganttCfg.colorRT2BC = QColor(m_colorRT2BCEdit->text());
    ganttCfg.colorRT2RT = QColor(m_colorRT2RTEdit->text());
    ganttCfg.colorBroadcast = QColor(m_colorBroadcastEdit->text());
    ganttCfg.colorError = QColor(m_colorErrorEdit->text());
    cfg->setGanttConfig(ganttCfg);

    cfg->setApiTimeout(m_apiTimeoutEdit->text().toInt());
    cfg->setRetryCount(m_retryCountEdit->text().toInt());

    cfg->setReportFormat(m_reportFormatCombo->currentData().toString());

    /* 保存语音设置 */
    SpeechConfig speechCfg = cfg->getSpeechConfig();
    speechCfg.engine.modelPath = m_modelPathEdit->text();
    speechCfg.engine.sampleRate = m_sampleRateSpin->value();
    speechCfg.engine.enabled = m_enableSpeechCheck->isChecked();
    cfg->setSpeechConfig(speechCfg);

    cfg->saveConfig();
}

void SettingsDialog::accept()
{
    saveSettings();
    QDialog::accept();
}

void SettingsDialog::onResetDefaults()
{
    ConfigManager::instance()->loadDefaults();
    loadSettings();
}

void SettingsDialog::onTestConnection()
{
    QMessageBox::information(this, tr(u8"测试连接"), tr(u8"连接测试功能开发中..."));
}

/**
 * @brief 浏览选择模型目录槽
 *
 * 打开目录选择对话框，让用户选择 Vosk 模型目录。
 * 选中的路径会填入模型路径输入框。
 */
void SettingsDialog::onBrowseModelPath()
{
    QString selectedDir = QFileDialog::getExistingDirectory(
        this,
        tr(u8"选择语音模型目录"),
        m_modelPathEdit->text(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );

    if (!selectedDir.isEmpty()) {
        m_modelPathEdit->setText(selectedDir);
    }
}

/**
 * @brief 打开颜色选择器并更新颜色值
 *
 * 点击甘特图颜色预览按钮时触发，打开系统颜色选择对话框，
 * 用户选择颜色后更新输入框文本和预览按钮背景色。
 *
 * @param edit 关联的颜色输入框，用于存储颜色值
 * @param btn 关联的颜色预览按钮，用于显示当前颜色
 */
void SettingsDialog::onPickColor(QLineEdit* edit, QPushButton* btn)
{
    if (!edit || !btn) return;

    /* 获取当前颜色作为初始值 */
    QColor currentColor(edit->text());
    QColor newColor = QColorDialog::getColor(
        currentColor.isValid() ? currentColor : QColor("#FFFFFF"),
        this,
        tr(u8"选择颜色")
    );

    /* 用户确认选择后更新输入框（会自动触发textChanged信号更新预览） */
    if (newColor.isValid()) {
        edit->setText(newColor.name());
    }
}

/**
 * @brief 更新颜色预览按钮的背景色
 *
 * 根据颜色输入框中的文本值，更新预览按钮的背景色。
 * 如果颜色值无效，显示为灰色并添加边框提示。
 *
 * @param edit 颜色输入框，从中读取颜色值
 * @param btn 颜色预览按钮，更新其背景色
 */
void SettingsDialog::updateColorPreview(QLineEdit* edit, QPushButton* btn)
{
    if (!edit || !btn) return;

    QColor color(edit->text());
    if (color.isValid()) {
        /* 有效颜色：设置背景色，添加微妙的边框 */
        btn->setStyleSheet(
            QString("QPushButton {"
                    "   background-color: %1;"
                    "   border: 2px solid #3E3E42;"
                    "   border-radius: 3px;"
                    "}"
                    "QPushButton:hover {"
                    "   border: 2px solid #0E639C;"
                    "}")
                .arg(color.name())
        );
    } else {
        /* 无效颜色：显示为灰色带斜线，提示用户输入有误 */
        btn->setStyleSheet(
            "QPushButton {"
            "   background-color: #555555;"
            "   border: 2px solid #E74C3C;"
            "   border-radius: 3px;"
            "}"
        );
    }
}
