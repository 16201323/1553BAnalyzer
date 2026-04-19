/**
 * @file MainWindow.h
 * @brief 主窗口类定义
 * 
 * MainWindow类是应用程序的主窗口，负责：
 * - 管理整体界面布局
 * - 协调各功能模块
 * - 处理用户交互
 * - 管理文件导入导出
 * - 集成AI智能分析功能
 * 
 * 界面组成：
 * - 菜单栏：文件、编辑、视图、帮助菜单
 * - 工具栏：常用操作快捷按钮和模型选择器
 * - 状态栏：状态信息、数据计数、进度条、计时器
 * - 停靠窗口：文件列表、AI查询面板
 * - 中央区域：数据表格、甘特图、统计图表选项卡
 * 
 * @author 1553BTools
 * @date 2024
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QToolBar>
#include <QStatusBar>
#include <QDockWidget>
#include <QTabWidget>
#include <QLabel>
#include <QProgressBar>
#include <QComboBox>
#include <QTimer>
#include <QElapsedTimer>
#include <QProgressDialog>
#include <QThread>
#include <atomic>

#include "core/parser/BinaryParser.h"
#include "core/datastore/DataStore.h"
#include "core/datastore/DataModel.h"
#include "core/datastore/TestDataGenerator.h"
#include "core/config/ConfigManager.h"
#include "ai/AIToolExecutor.h"
#include "ui/widgets/AIQueryPanel.h"
#include "ui/widgets/PaginationWidget.h"
#include "ui/dialogs/ProgressDialog.h"
#include "ui/dialogs/LoadingDialog.h"
#include "model/ModelAdapter.h"
#include "export/ExportService.h"
#include "report/ReportGenerator.h"
#include "core/analysis/TimeIntervalAnalyzer.h"

// 前向声明
class TableView;
class GanttView;
class ChartWidget;
class FileListPanel;

/**
 * @brief 主窗口类
 * 
 * 该类是应用程序的核心类，继承自QMainWindow，
 * 整合了所有功能模块，提供统一的用户界面。
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针
     * 
     * 初始化所有成员对象，设置界面布局，建立信号连接
     */
    MainWindow(QWidget *parent = nullptr);
    
    /**
     * @brief 析构函数
     * 
     * 保存窗口设置
     */
    ~MainWindow();
    
    /**
     * @brief 加载并解析文件
     * @param filePath 文件路径
     * 
     * 在后台线程中解析文件，显示进度和计时
     */
    void loadFile(const QString& filePath);
    
protected:
    /**
     * @brief 窗口关闭事件
     * @param event 关闭事件对象
     * 
     * 保存窗口设置后接受关闭事件
     */
    void closeEvent(QCloseEvent *event) override;
    
    /**
     * @brief 拖拽进入事件
     * @param event 拖拽事件对象
     * 
     * 支持拖拽文件到窗口进行导入
     */
    void dragEnterEvent(QDragEnterEvent *event) override;
    
    /**
     * @brief 拖拽放下事件
     * @param event 放下事件对象
     * 
     * 处理拖拽放入的文件
     */
    void dropEvent(QDropEvent *event) override;

private slots:
    /**
     * @brief 打开文件对话框
     * 
     * 显示文件选择对话框，选择后调用loadFile
     */
    void onOpenFile();
    
    /**
     * @brief 保存/导出数据
     * 
     * 导出当前数据为CSV或Excel格式
     */
    void onSaveExport();
    
    /**
     * @brief 导出甘特图为PNG图片
     */
    void onExportGanttPng();
    
    /**
     * @brief 导出甘特图为PDF文档
     */
    void onExportGanttPdf();
    
    /**
     * @brief 生成智能分析报告
     */
    void onGenerateReport();
    
    /**
     * @brief 生成测试数据文件
     */
    void onGenerateTestData();
    
    /**
     * @brief 查看16进制内容
     */
    void onViewHex();
    
    /**
     * @brief 报告生成进度更新槽
     * @param percent 进度百分比
     * @param stage 当前阶段描述
     * @param elapsedSeconds 已耗时（秒）
     */
    void onReportProgress(int percent, const QString& stage, double elapsedSeconds);
    
    /**
     * @brief 报告生成完成槽
     * @param success 是否成功
     * @param filePath 报告文件路径
     */
    void onReportFinished(bool success, const QString& filePath);
    
    /**
     * @brief 打开设置对话框
     * 
     * 显示设置对话框，修改后重新加载配置
     */
    void onSettings();
    
    /**
     * @brief 显示关于对话框
     */
    void onAbout();
    
    /**
     * @brief 打开日志文件
     * 
     * 使用系统默认程序打开日志文件
     */
    void onOpenLog();
    
    /**
     * @brief 解析进度更新槽
     * @param current 当前已解析字节数
     * @param total 文件总字节数
     */
    void onParseProgress(int current, int total);
    
    /**
     * @brief 解析完成槽
     * @param success 是否成功
     * @param count 解析的消息数量
     * 
     * 在后台线程中加载数据到DataStore
     */
    void onParseFinished(bool success, int count);
    
    /**
     * @brief 数据加载完成槽
     * @param count 加载的数据条数
     */
    void onDataLoaded(int count);
    
    /**
     * @brief AI查询提交槽
     * @param query 用户输入的查询文本
     * 
     * 处理AI分析模式下的查询请求
     */
    void onAIQuerySubmitted(const QString& query);
    
    /**
     * @brief AI聊天消息发送槽
     * @param message 用户消息
     * @param history 聊天历史
     * 
     * 处理AI聊天模式下的消息
     */
    void onAIChatMessageSent(const QString& message, const QList<struct ChatMessage>& history);
    
    /**
     * @brief AI查询清除槽
     * 
     * 清除所有筛选条件
     */
    void onAIQueryClear();
    
    /**
     * @brief 数据范围切换槽
     * 
     * 当用户切换数据范围时，更新DataStore的数据范围
     */
    void onDataScopeChanged();
    
    /**
     * @brief AI模式切换槽
     * @param mode 新的AI模式（聊天/分析）
     */
    void onAIModeChanged(AIMode mode);
    
    /**
     * @brief 表格行双击槽
     * @param row 双击的行号
     * 
     * 显示数据详细对话框
     */
    void onRecordDoubleClicked(int row);
    
    /**
     * @brief 算式筛选请求槽
     * @param column 列索引
     * @param expression 筛选表达式
     * 
     * 根据用户输入的算式进行数据筛选
     */
    void onExpressionFilterRequested(int column, const QString& expression);
    
    /**
     * @brief 列筛选清除槽
     * @param column 列索引
     * 
     * 清除指定列的算式筛选条件
     */
    void onColumnFilterCleared(int column);
    
    /**
     * @brief 解析计时器超时槽
     * 
     * 更新状态栏中的计时显示
     */
    void onParseTimerTimeout();
    
    /**
     * @brief AI数据查询请求槽
     * @param filters 筛选条件JSON对象
     */
    void onAIQueryDataRequested(const QJsonObject& filters);
    
    /**
     * @brief AI生成图表请求槽
     * @param chartType 图表类型
     * @param subject 图表主题
     * @param title 图表标题
     */
    void onAIGenerateChartRequested(const QString& chartType, const QString& subject, const QString& title);
    
    /**
     * @brief AI生成甘特图请求槽
     * @param filters 筛选条件JSON对象
     */
    void onAIGenerateGanttRequested(const QJsonObject& filters);
    
    /**
     * @brief AI清除筛选请求槽
     */
    void onAIClearFilterRequested();
    
    /**
     * @brief AI切换到图表选项卡槽
     */
    void onAISwitchToChartTab();
    
    /**
     * @brief AI切换到甘特图选项卡槽
     */
    void onAISwitchToGanttTab();
    
    /**
     * @brief AI切换到表格选项卡槽
     */
    void onAISwitchToTableTab();
    
    /**
     * @brief AI时间间隔分析请求槽
     * @param analysis 时间间隔分析结果
     * 
     * 接收时间间隔分析结果，切换到图表选项卡并绘制时间间隔折线图
     */
    void onAITimeIntervalAnalysisRequested(const TimeIntervalAnalysis& analysis);
    
    /**
     * @brief 模型查询开始槽
     */
    void onModelQueryStarted();
    
    /**
     * @brief 模型查询完成槽
     * @param response 模型响应
     */
    void onModelQueryFinished(const ModelResponse& response);
    
    /**
     * @brief 模型查询错误槽
     * @param error 错误信息
     */
    void onModelQueryError(const QString& error);
    
    /**
     * @brief 筛选进度更新槽
     * @param percent 进度百分比（0-100）
     * @param processed 已处理记录数
     * @param total 总记录数
     */
    void onFilterProgress(int percent, int processed, int total);
    
    /**
     * @brief 筛选进度轮询槽
     * 
     * 由主线程定时器触发，轮询DataStore的原子进度变量并更新进度对话框。
     * 使用轮询方式替代跨线程信号，确保在Qt 5.9.9下进度更新可靠。
     */
    void onFilterProgressPoll();
    
    /**
     * @brief DataStore筛选条件变化槽
     * 
     * 当DataStore完成筛选操作后关闭进度对话框
     */
    void onDataStoreFilterChanged();
    
    /**
     * @brief 分页变化槽
     * @param currentPage 当前页码（从0开始）
     * @param totalPages 总页数
     * @param filteredCount 筛选后的记录总数
     */
    void onPageChanged(int currentPage, int totalPages, int filteredCount);
    
    /**
     * @brief 每页条数变化槽
     * @param size 新的每页条数
     */
    void onPageSizeChanged(int size);
    
    /**
     * @brief 进度对话框取消槽
     */
    void onProgressDialogCanceled();
    
    /**
     * @brief 甘特图更新完成槽
     * 
     * 关闭加载提示对话框
     */
    void onGanttUpdateFinished();
    
    /**
     * @brief 图表更新完成槽
     * 
     * 关闭加载提示对话框
     */
    void onChartUpdateFinished();

private:
    /**
     * @brief 设置界面布局
     * 
     * 创建中央选项卡区域，添加表格、甘特图、图表视图
     */
    void setupUI();
    
    /**
     * @brief 设置菜单栏
     * 
     * 创建文件、编辑、视图、帮助菜单
     */
    void setupMenuBar();
    
    /**
     * @brief 设置工具栏
     * 
     * 添加常用操作按钮和模型选择器
     */
    void setupToolBar();
    
    /**
     * @brief 设置状态栏
     * 
     * 添加状态标签、计数标签、计时器、进度条
     */
    void setupStatusBar();
    
    /**
     * @brief 设置停靠窗口
     * 
     * 创建文件列表面板和AI查询面板
     */
    void setupDockWidgets();
    
    /**
     * @brief 建立信号连接
     * 
     * 连接各组件的信号和槽
     */
    void setupConnections();
    
    /**
     * @brief 加载设置
     * 
     * 从QSettings恢复窗口状态和模型配置
     */
    void loadSettings();
    
    /**
     * @brief 保存设置
     * 
     * 将窗口状态保存到QSettings
     */
    void saveSettings();
    
    /**
     * @brief 加载深色主题样式
     */
    void loadDarkTheme();
    
    /**
     * @brief 从数据库加载文件列表
     * 
     * 查询数据库中已导入的文件，填充到文件列表面板
     */
    void loadDatabaseFileList();
    
    /**
     * @brief 从数据库直接加载文件数据
     * @param fileId 数据库中的文件ID
     * @return 是否成功
     */
    bool loadFileFromDatabase(int fileId);
    
    /**
     * @brief 更新窗口标题
     * 
     * 显示当前文件名
     */
    void updateWindowTitle();
    
    /**
     * @brief 更新状态栏
     */
    void updateStatusBar();
    
    /**
     * @brief 处理简单查询
     * @param query 查询文本
     * @return 处理结果消息
     * 
     * 处理不需要AI的简单查询，如"显示所有"
     */
    QString processSimpleQuery(const QString& query);
    
    TestDataGenerator* m_testDataGenerator;
    
    /**
     * @brief 显示数据详细对话框
     * @param row 数据行号
     * 
     * 显示包含包头、数据包、命令字等详细信息的对话框
     */
    void showDataDetailDialog(int row);
    
    /**
     * @brief 执行AI工具调用
     * @param toolCalls 工具调用JSON数组
     */
    void executeAITools(const QJsonArray& toolCalls);
    
    /**
     * @brief 从响应内容提取工具调用
     * @param content AI响应内容
     * @return 工具调用JSON数组
     */
    QJsonArray extractToolCalls(const QString& content);
    
    BinaryParser* m_parser;           // 二进制解析器
    QThread* m_parserThread;          // 解析器工作线程
    DataStore* m_dataStore;           // 数据存储
    DataModel* m_dataModel;           // 数据模型
    AIToolExecutor* m_toolExecutor;   // AI工具执行器
    ExportService* m_exportService;   // 导出服务
    ReportGenerator* m_reportGenerator; // 报告生成器
    
    QTabWidget* m_centralTab;         // 中央选项卡
    TableView* m_tableView;           // 数据表格视图
    GanttView* m_ganttView;           // 甘特图视图
    ChartWidget* m_chartWidget;       // 统计图表视图
    
    FileListPanel* m_fileListPanel;   // 文件列表面板
    AIQueryPanel* m_aiQueryPanel;     // AI查询面板
    
    QLabel* m_statusLabel;            // 状态标签
    QLabel* m_countLabel;             // 数据计数标签
    QLabel* m_timerLabel;             // 计时器标签
    QProgressBar* m_progressBar;      // 进度条
    QTimer* m_parseTimer;             // 解析计时器
    QElapsedTimer m_elapsedTimer;     // 计时器
    
    QProgressDialog* m_reportProgressDialog; // 报告生成进度对话框
    
    ProgressDialog* m_progressDialog;    // 文件加载/筛选进度对话框
    QTimer* m_filterPollTimer;           // 筛选进度轮询定时器（主线程定时器，轮询原子变量）
    LoadingDialog* m_loadingDialog;      // 图表加载提示对话框
    PaginationWidget* m_paginationWidget; // 分页控件
    QComboBox* m_dataScopeCombo;        // 数据范围选择框
    
    QString m_currentFile;            // 当前打开的文件路径
    std::atomic<bool> m_loadCanceled; // 加载是否被取消
};

#endif
