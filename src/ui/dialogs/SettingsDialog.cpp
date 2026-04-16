/**
 * @file SettingsDialog.cpp
 * @brief 设置对话框实现
 *
 * 本文件实现了应用程序的设置对话框，包含以下选项卡：
 * - 解析器设置：字节序、包头标识、时间戳单位
 * - 数据库设置：自动切换阈值
 * - AI模型设置：提供商配置、API密钥、系统提示词
 * - 甘特图设置：消息类型颜色
 * - 导出设置：默认格式和编码
 *
 * 设置通过ConfigManager持久化到XML配置文件。
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
#include <QUrl>

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
    mainLayout->addWidget(m_tabWidget);
    
    setupParserTab();
    setupDatabaseTab();
    setupModelTab();
    setupGanttTab();
    setupExportTab();
    
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Reset, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttonBox->button(QDialogButtonBox::Reset), &QPushButton::clicked,
            this, &SettingsDialog::onResetDefaults);
    
    mainLayout->addWidget(buttonBox);
    
    // 配置文件路径区域：显示路径 + 打开按钮
    QHBoxLayout* configPathLayout = new QHBoxLayout();
    QLabel* configPathLabel = new QLabel(
        tr("配置文件: %1").arg(ConfigManager::instance()->configFilePath()));
    configPathLabel->setWordWrap(true);
    configPathLabel->setStyleSheet("color: #7f8c8d; font-size: 11px;");
    configPathLayout->addWidget(configPathLabel, 1);
    
    QPushButton* openConfigBtn = new QPushButton(tr("打开配置文件"));
    openConfigBtn->setToolTip(tr("用系统默认编辑器打开config.xml"));
    openConfigBtn->setStyleSheet("font-size: 11px; padding: 3px 8px;");
    connect(openConfigBtn, &QPushButton::clicked, this, []() {
        QString path = ConfigManager::instance()->configFilePath();
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });
    configPathLayout->addWidget(openConfigBtn);
    mainLayout->addLayout(configPathLayout);
    
    setWindowTitle(tr("设置"));
    setMinimumSize(500, 400);
}

void SettingsDialog::setupParserTab()
{
    QWidget* tab = new QWidget();
    QFormLayout* layout = new QFormLayout(tab);
    
    m_byteOrderCombo = new QComboBox();
    m_byteOrderCombo->addItem(tr("小端序"), "little");
    m_byteOrderCombo->addItem(tr("大端序"), "big");
    layout->addRow(tr("字节序:"), m_byteOrderCombo);
    
    m_header1Edit = new QLineEdit();
    m_header1Edit->setPlaceholderText("0xA5A5");
    layout->addRow(tr("包头1:"), m_header1Edit);
    
    m_header2Edit = new QLineEdit();
    m_header2Edit->setPlaceholderText("0xA5");
    layout->addRow(tr("包头2:"), m_header2Edit);
    
    m_dataHeaderEdit = new QLineEdit();
    m_dataHeaderEdit->setPlaceholderText("0xAABB");
    layout->addRow(tr("数据头:"), m_dataHeaderEdit);
    
    m_maxErrorSpin = new QSpinBox();
    m_maxErrorSpin->setRange(0, 100);
    layout->addRow(tr("最大错误容差:"), m_maxErrorSpin);
    
    m_tabWidget->addTab(tab, tr("解析设置"));
}

void SettingsDialog::setupDatabaseTab()
{
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(tab);
    
    QGroupBox* storageGroup = new QGroupBox(tr("存储模式设置"));
    QFormLayout* formLayout = new QFormLayout(storageGroup);
    
    m_recordThresholdSpin = new QSpinBox();
    m_recordThresholdSpin->setRange(0, 10000000);
    m_recordThresholdSpin->setSingleStep(100);
    m_recordThresholdSpin->setToolTip(tr("当数据记录数超过此阈值时，自动使用数据库模式存储"));
    formLayout->addRow(tr("数据库模式阈值(记录数):"), m_recordThresholdSpin);
    
    layout->addWidget(storageGroup);
    
    QLabel* hintLabel = new QLabel(tr(
        "<p><b>说明：</b></p>"
        "<ul>"
        "<li>数据量超过阈值时使用数据库模式，适合大数据量分析</li>"
        "<li>数据量未超过阈值时使用内存模式，响应更快</li>"
        "<li>默认阈值为50000条记录</li>"
        "</ul>"
    ));
    hintLabel->setWordWrap(true);
    layout->addWidget(hintLabel);
    
    layout->addStretch();
    
    m_tabWidget->addTab(tab, tr("数据库设置"));
}

void SettingsDialog::setupModelTab()
{
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(tab);
    
    QLabel* hintLabel = new QLabel(tr("模型配置请在config.xml文件中修改"));
    layout->addWidget(hintLabel);
    
    QFormLayout* formLayout = new QFormLayout();
    
    m_apiTimeoutEdit = new QLineEdit();
    m_apiTimeoutEdit->setPlaceholderText("60");
    formLayout->addRow(tr("API超时(秒):"), m_apiTimeoutEdit);
    
    m_retryCountEdit = new QLineEdit();
    m_retryCountEdit->setPlaceholderText("3");
    formLayout->addRow(tr("重试次数:"), m_retryCountEdit);
    
    layout->addLayout(formLayout);
    
    QPushButton* testBtn = new QPushButton(tr("测试连接"));
    connect(testBtn, &QPushButton::clicked, this, &SettingsDialog::onTestConnection);
    layout->addWidget(testBtn);
    
    layout->addStretch();
    
    m_tabWidget->addTab(tab, tr("模型设置"));
}

void SettingsDialog::setupGanttTab()
{
    QWidget* tab = new QWidget();
    QFormLayout* layout = new QFormLayout(tab);
    
    m_colorBC2RTEdit = new QLineEdit();
    layout->addRow(tr("BC→RT颜色:"), m_colorBC2RTEdit);
    
    m_colorRT2BCEdit = new QLineEdit();
    layout->addRow(tr("RT→BC颜色:"), m_colorRT2BCEdit);
    
    m_colorRT2RTEdit = new QLineEdit();
    layout->addRow(tr("RT→RT颜色:"), m_colorRT2RTEdit);
    
    m_colorBroadcastEdit = new QLineEdit();
    layout->addRow(tr("广播颜色:"), m_colorBroadcastEdit);
    
    m_colorErrorEdit = new QLineEdit();
    layout->addRow(tr("错误颜色:"), m_colorErrorEdit);
    
    m_tabWidget->addTab(tab, tr("甘特图设置"));
}

void SettingsDialog::setupExportTab()
{
    QWidget* tab = new QWidget();
    QFormLayout* layout = new QFormLayout(tab);
    
    m_exportFormatCombo = new QComboBox();
    m_exportFormatCombo->addItem(tr("CSV"), "csv");
    m_exportFormatCombo->addItem(tr("Excel"), "xlsx");
    layout->addRow(tr("默认导出格式:"), m_exportFormatCombo);
    
    m_encodingCombo = new QComboBox();
    m_encodingCombo->addItem("UTF-8", "UTF-8");
    m_encodingCombo->addItem("GBK", "GBK");
    layout->addRow(tr("文件编码:"), m_encodingCombo);
    
    m_reportFormatCombo = new QComboBox();
    m_reportFormatCombo->addItem(tr("HTML"), "html");
    m_reportFormatCombo->addItem(tr("PDF"), "pdf");
    m_reportFormatCombo->addItem(tr("DOCX"), "docx");
    layout->addRow(tr("智能分析报告格式:"), m_reportFormatCombo);
    
    m_tabWidget->addTab(tab, tr("导出设置"));
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
    
    QString reportFormat = cfg->getReportFormat();
    int reportIdx = m_reportFormatCombo->findData(reportFormat);
    if (reportIdx >= 0) {
        m_reportFormatCombo->setCurrentIndex(reportIdx);
    } else {
        m_reportFormatCombo->setCurrentIndex(0);
    }
}

void SettingsDialog::saveSettings()
{
    ConfigManager* cfg = ConfigManager::instance();
    
    // 先获取当前配置再修改，避免丢失未在UI中显示的字段（如timestampUnit）
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
    QMessageBox::information(this, tr("测试连接"), tr("连接测试功能开发中..."));
}
