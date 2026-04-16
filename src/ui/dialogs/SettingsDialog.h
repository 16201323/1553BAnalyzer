/**
 * @file SettingsDialog.h
 * @brief 设置对话框类定义
 *
 * SettingsDialog类提供应用程序的全局配置界面，
 * 使用选项卡组织不同类别的设置项。
 *
 * 选项卡结构：
 * - 解析器设置：字节序、包头标识、数据头标识、错误容差
 * - 数据库设置：自动切换到数据库模式的记录数阈值
 * - 模型设置：AI模型提供商配置（API密钥、URL、模型名称等）
 * - 甘特图设置：各消息类型的显示颜色
 * - 导出设置：默认导出格式、编码方式、报告格式
 *
 * 操作流程：
 * 1. 打开对话框时从ConfigManager加载当前配置
 * 2. 用户修改配置项
 * 3. 点击"确定"时保存到ConfigManager并发出configChanged信号
 * 4. 点击"重置默认"时恢复所有配置为默认值
 *
 * @author 1553BTools
 * @date 2024
 */

#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QTableWidget>

/**
 * @brief 设置对话框类
 *
 * 该对话框使用QTabWidget组织多个设置页面，
 * 修改后的配置通过ConfigManager持久化到XML文件。
 */
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();

    /**
     * @brief 重写accept方法
     *
     * 点击"确定"时保存所有配置到ConfigManager，
     * 然后调用QDialog::accept()关闭对话框
     */
    void accept() override;

private slots:
    /**
     * @brief 重置为默认配置
     *
     * 将所有设置项恢复为默认值
     */
    void onResetDefaults();

    /**
     * @brief 测试AI模型连接
     *
     * 使用当前配置的API密钥和URL发送测试请求，
     * 验证连接是否正常
     */
    void onTestConnection();

private:
    void setupUI();

    /**
     * @brief 从ConfigManager加载当前配置到界面控件
     */
    void loadSettings();

    /**
     * @brief 将界面控件的值保存到ConfigManager
     */
    void saveSettings();

    /**
     * @brief 创建解析器设置选项卡
     */
    void setupParserTab();

    /**
     * @brief 创建数据库设置选项卡
     */
    void setupDatabaseTab();

    /**
     * @brief 创建AI模型设置选项卡
     */
    void setupModelTab();

    /**
     * @brief 创建甘特图颜色设置选项卡
     */
    void setupGanttTab();

    /**
     * @brief 创建导出设置选项卡
     */
    void setupExportTab();

    QTabWidget* m_tabWidget;          ///< 选项卡容器

    QComboBox* m_byteOrderCombo;      ///< 字节序选择（大端/小端）
    QLineEdit* m_header1Edit;         ///< 包头1标识输入
    QLineEdit* m_header2Edit;         ///< 包头2标识输入
    QLineEdit* m_dataHeaderEdit;      ///< 数据头标识输入
    QSpinBox* m_maxErrorSpin;         ///< 最大错误容差次数

    QSpinBox* m_recordThresholdSpin;  ///< 数据库模式切换阈值

    QTableWidget* m_modelTable;       ///< AI模型配置表格
    QLineEdit* m_apiTimeoutEdit;      ///< API超时时间输入
    QLineEdit* m_retryCountEdit;      ///< 重试次数输入

    QLineEdit* m_colorBC2RTEdit;      ///< BC→RT颜色输入
    QLineEdit* m_colorRT2BCEdit;      ///< RT→BC颜色输入
    QLineEdit* m_colorRT2RTEdit;      ///< RT→RT颜色输入
    QLineEdit* m_colorBroadcastEdit;  ///< 广播颜色输入
    QLineEdit* m_colorErrorEdit;      ///< 错误颜色输入

    QComboBox* m_exportFormatCombo;   ///< 导出格式选择
    QComboBox* m_encodingCombo;       ///< 编码方式选择
    QComboBox* m_reportFormatCombo;   ///< 报告格式选择（HTML/PDF/DOCX）
};

#endif
