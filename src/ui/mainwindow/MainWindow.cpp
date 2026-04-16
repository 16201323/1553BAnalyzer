/**
 * @file MainWindow.cpp
 * @brief 主窗口类实现
 * 
 * 本文件实现了MainWindow类的所有方法，包括：
 * - 界面初始化和布局设置
 * - 文件导入和解析
 * - AI查询和聊天功能
 * - 数据可视化展示
 * - 用户交互处理
 * 
 * 主要功能模块：
 * 1. 文件解析：使用BinaryParser在后台线程解析二进制文件
 * 2. 数据管理：通过DataStore和DataModel管理数据
 * 3. AI集成：支持聊天和分析两种模式
 * 4. 可视化：表格、甘特图、统计图表
 * 
 * @author 1553BTools
 * @date 2024
 */

#include "MainWindow.h"
#include "ui/tableview/TableView.h"
#include "ui/ganttview/GanttView.h"
#include "ui/chartview/ChartWidget.h"
#include "ui/widgets/FileListPanel.h"
#include "ui/widgets/AIQueryPanel.h"
#include "ui/dialogs/SettingsDialog.h"
#include "ui/dialogs/HexViewDialog.h"
#include "core/config/ConfigManager.h"
#include "core/parser/PacketStruct.h"
#include "core/datastore/DataStore.h"
#include "core/datastore/DatabaseManager.h"
#include "utils/Logger.h"
#include "ai/AIToolDefinitions.h"
#include "model/ModelManager.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QApplication>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QSplitter>
#include <QDebug>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QThread>
#include <QThreadPool>
#include <QTreeWidget>
#include <QDialogButtonBox>
#include <QRegularExpression>
#include <QDataStream>
#include <QJsonDocument>
#include <QClipboard>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QGroupBox>
#include <QTabWidget>
#include <QPlainTextEdit>
#include <QRandomGenerator>
#include <QCheckBox>
#include <QProgressDialog>
#include <QRadioButton>
#include <QGroupBox>
#include <QScrollArea>
#include <QJsonDocument>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QDateEdit>
#include <QTimeEdit>

/**
 * @brief 构造函数，初始化主窗口
 * @param parent 父窗口指针
 * 
 * 初始化流程：
 * 1. 创建核心对象（解析器、数据存储、数据模型、工具执行器）
 * 2. 建立数据模型和数据存储的关联
 * 3. 设置界面布局
 * 4. 创建菜单栏、工具栏、状态栏
 * 5. 创建停靠窗口
 * 6. 建立信号连接
 * 7. 加载用户设置
 */
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_parser(new BinaryParser())
    , m_parserThread(new QThread(this))
    , m_dataStore(new DataStore(this))
    , m_dataModel(new DataModel(this))
    , m_testDataGenerator(new TestDataGenerator(this))
    , m_toolExecutor(new AIToolExecutor(this))
    , m_exportService(new ExportService(this))
    , m_reportGenerator(new ReportGenerator(this))
    , m_timerLabel(new QLabel(this))
    , m_parseTimer(new QTimer(this))
    , m_reportProgressDialog(nullptr)
    , m_progressDialog(nullptr)
    , m_paginationWidget(nullptr)
    , m_loadCanceled(false)
{
    qRegisterMetaType<QVector<SMbiMonPacketMsg>>("QVector<SMbiMonPacketMsg>");
    
    m_dataModel->setDataStore(m_dataStore);
    m_toolExecutor->setDataStore(m_dataStore);
    m_reportGenerator->setDataStore(m_dataStore);
    
    setupUI();
    setupMenuBar();
    setupToolBar();
    setupStatusBar();
    setupDockWidgets();
    setupConnections();
    loadSettings();
    
    // 配置变更时即时生效
    connect(ConfigManager::instance(), &ConfigManager::configChanged, this, [this]() {
        LOG_INFO("MainWindow", "配置已变更，即时生效");
        loadSettings();
    });
    
    setAcceptDrops(true);
    setWindowTitle(tr("1553B数据智能分析工具"));
    resize(1280, 800);
    
    loadDatabaseFileList();
}

/**
 * @brief 析构函数，保存设置并清理资源
 */
MainWindow::~MainWindow()
{
    saveSettings();
}

/**
 * @brief 设置界面布局
 * 
 * 创建中央选项卡区域，包含：
 * - 数据表格视图：显示解析后的数据
 * - 分页控件：控制表格分页显示
 * - 甘特图视图：显示数据流时序
 * - 统计图表视图：显示各类统计图表
 */
void MainWindow::setupUI()
{
    // 创建中央部件
    QWidget* centralWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // 创建选项卡
    m_centralTab = new QTabWidget(this);
    
    // 创建表格页（包含表格和分页控件）
    QWidget* tablePage = new QWidget();
    QVBoxLayout* tableLayout = new QVBoxLayout(tablePage);
    tableLayout->setContentsMargins(0, 0, 0, 0);
    tableLayout->setSpacing(0);
    
    m_tableView = new TableView(this);
    m_tableView->setModel(m_dataModel);
    tableLayout->addWidget(m_tableView, 1);
    
    // 添加分页控件
    m_paginationWidget = new PaginationWidget(this);
    tableLayout->addWidget(m_paginationWidget);
    
    m_centralTab->addTab(tablePage, tr("数据表格"));
    
    m_ganttView = new GanttView(this);
    m_centralTab->addTab(m_ganttView, tr("甘特图"));
    
    m_chartWidget = new ChartWidget(this);
    m_centralTab->addTab(m_chartWidget, tr("统计图表"));
    
    mainLayout->addWidget(m_centralTab);
    setCentralWidget(centralWidget);
}

/**
 * @brief 设置菜单栏
 * 
 * 创建以下菜单：
 * - 文件菜单：打开、导出、退出
 * - 编辑菜单：设置
 * - 视图菜单：显示/隐藏停靠窗口
 * - 帮助菜单：关于
 */
void MainWindow::setupMenuBar()
{
    QMenuBar* menuBar = this->menuBar();
    
    QMenu* fileMenu = menuBar->addMenu(tr("文件(&F)"));
    fileMenu->addAction(tr("打开文件(&O)"), this, &MainWindow::onOpenFile, QKeySequence::Open);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("导出数据(&E)"), this, &MainWindow::onSaveExport, QKeySequence::Save);
    
    QMenu* exportMenu = fileMenu->addMenu(tr("导出图表"));
    exportMenu->addAction(tr("导出甘特图(PNG)"), this, &MainWindow::onExportGanttPng);
    exportMenu->addAction(tr("导出甘特图(PDF)"), this, &MainWindow::onExportGanttPdf);
    
    fileMenu->addSeparator();
    fileMenu->addAction(tr("生成智能分析报告(&R)"), this, &MainWindow::onGenerateReport, QKeySequence(Qt::CTRL + Qt::Key_R));
    fileMenu->addAction(tr("生成测试数据(&T)"), this, &MainWindow::onGenerateTestData, QKeySequence(Qt::CTRL + Qt::Key_T));
    fileMenu->addAction(tr("查看16进制(&H)"), this, &MainWindow::onViewHex, QKeySequence(Qt::CTRL + Qt::Key_H));
    
    fileMenu->addSeparator();
    fileMenu->addAction(tr("退出(&X)"), this, &QWidget::close, QKeySequence::Quit);
    
    QMenu* editMenu = menuBar->addMenu(tr("编辑(&E)"));
    editMenu->addAction(tr("设置(&S)"), this, &MainWindow::onSettings, QKeySequence::Preferences);
    
    QMenu* viewMenu = menuBar->addMenu(tr("视图(&V)"));
    viewMenu->addAction(tr("文件列表"), [this]() {
        if (m_fileListPanel) m_fileListPanel->show();
    });
    viewMenu->addAction(tr("AI查询面板"), [this]() {
        if (m_aiQueryPanel) m_aiQueryPanel->show();
    });
    
    QMenu* helpMenu = menuBar->addMenu(tr("帮助(&H)"));
    helpMenu->addAction(tr("关于(&A)"), this, &MainWindow::onAbout);
}

/**
 * @brief 设置工具栏
 * 
 * 添加常用操作按钮：
 * - 打开文件按钮
 * - 导出数据按钮
 * - 模型选择下拉框
 * - 设置按钮
 * - 日志按钮
 */
void MainWindow::setupToolBar()
{
    QToolBar* toolBar = addToolBar(tr("主工具栏"));
    toolBar->setObjectName("MainToolBar");
    toolBar->setMovable(false);
    
    toolBar->addAction(QIcon(":/icons/open.png"), tr("打开"), this, &MainWindow::onOpenFile);
    toolBar->addAction(QIcon(":/icons/export.png"), tr("导出"), this, &MainWindow::onSaveExport);
    toolBar->addSeparator();
    
    toolBar->addWidget(new QLabel(tr(" 数据范围: ")));
    m_dataScopeCombo = new QComboBox(this);
    m_dataScopeCombo->addItem(tr("文件所有数据"), static_cast<int>(DataScope::AllData));
    m_dataScopeCombo->addItem(tr("当前筛选数据"), static_cast<int>(DataScope::FilteredData));
    m_dataScopeCombo->addItem(tr("当前页数据"), static_cast<int>(DataScope::CurrentPage));
    m_dataScopeCombo->setToolTip(tr("选择甘特图和统计图表的数据范围"));
    toolBar->addWidget(m_dataScopeCombo);
    toolBar->addSeparator();
    
    toolBar->addAction(QIcon(":/icons/settings.png"), tr("设置"), this, &MainWindow::onSettings);
    toolBar->addAction(QIcon(":/icons/log.png"), tr("日志"), this, &MainWindow::onOpenLog);
    
    connect(m_dataScopeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onDataScopeChanged);
}

/**
 * @brief 设置状态栏
 * 
 * 状态栏包含：
 * - 状态标签：显示当前操作状态
 * - 数据计数标签：显示已加载的数据条数
 */
void MainWindow::setupStatusBar()
{
    QStatusBar* status = statusBar();
    
    m_statusLabel = new QLabel(tr("就绪"));
    m_countLabel = new QLabel(tr("数据: 0 条"));
    
    status->addWidget(m_statusLabel, 1);
    status->addPermanentWidget(m_countLabel);
}

/**
 * @brief 设置停靠窗口
 * 
 * 创建两个停靠窗口：
 * - 文件列表面板：显示已打开的文件列表
 * - AI查询面板：AI聊天和分析功能界面
 */
void MainWindow::setupDockWidgets()
{
    m_fileListPanel = new FileListPanel(this);
    m_fileListPanel->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, m_fileListPanel);
    
    m_aiQueryPanel = new AIQueryPanel(this);
    m_aiQueryPanel->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, m_aiQueryPanel);
    
    // 将AI面板放在文件列表面板下方，形成左侧同一列布局
    splitDockWidget(m_fileListPanel, m_aiQueryPanel, Qt::Vertical);
}

/**
 * @brief 建立信号连接
 * 
 * 连接各组件的信号和槽：
 * - 解析器信号：进度、完成
 * - 数据模型信号：数据加载完成
 * - 文件列表信号：文件选择、移除
 * - AI查询面板信号：查询提交、聊天、清除、模式切换
 * - 表格视图信号：双击、显示详情
 * - 工具执行器信号：各种操作请求
 * - 模型管理器信号：查询开始、完成、错误
 */
void MainWindow::setupConnections()
{
    m_parser->moveToThread(m_parserThread);
    m_parserThread->start();
    
    connect(m_parserThread, &QThread::finished, m_parser, &QObject::deleteLater);
    connect(m_parser, &BinaryParser::parseProgress, this, &MainWindow::onParseProgress);
    connect(m_parser, &BinaryParser::parseFinished, this, &MainWindow::onParseFinished);
    connect(m_dataModel, &DataModel::dataLoaded, this, &MainWindow::onDataLoaded);
    connect(m_parseTimer, &QTimer::timeout, this, &MainWindow::onParseTimerTimeout);
    
    // 连接DataStore的分页和筛选进度信号
    connect(m_dataStore, &DataStore::filterProgress, this, &MainWindow::onFilterProgress);
    connect(m_dataStore, &DataStore::pageChanged, this, &MainWindow::onPageChanged);
    
    // 连接分页控件信号
    connect(m_paginationWidget, &PaginationWidget::pageChanged, m_dataStore, &DataStore::setCurrentPage);
    connect(m_paginationWidget, &PaginationWidget::pageSizeChanged, this, &MainWindow::onPageSizeChanged);
    
    m_ganttView->setDataStore(m_dataStore);
    m_chartWidget->setDataStore(m_dataStore);
    
    connect(m_fileListPanel, &FileListPanel::fileSelected, this, &MainWindow::loadFile);
    connect(m_fileListPanel, &FileListPanel::dbFileSelected, this, [this](int fileId, const QString& filePath) {
        m_fileListPanel->setActiveFile(filePath);
        m_currentFile = filePath;
        loadFileFromDatabase(fileId);
    });
    connect(m_fileListPanel, &FileListPanel::fileRemoved, this, [this](const QString& filePath, int fileId) {
        LOG_INFO("MainWindow", QString("文件从列表移除: %1, fileId: %2").arg(filePath).arg(fileId));
        if (fileId > 0 && DatabaseManager::instance()->isInitialized()) {
            QProgressDialog progress(tr("正在删除文件数据..."), tr("取消"), 0, 1, this);
            progress.setWindowModality(Qt::WindowModal);
            progress.setMinimumDuration(0);
            progress.setAutoClose(true);
            progress.setAutoReset(true);
            progress.setLabelText(tr("正在删除文件: %1").arg(QFileInfo(filePath).fileName()));
            progress.setValue(0);
            QCoreApplication::processEvents();
            
            bool ok = DatabaseManager::instance()->deleteFileData(fileId);
            LOG_INFO("MainWindow", QString("数据库删除文件数据: fileId=%1, 结果=%2").arg(fileId).arg(ok));
            
            progress.setValue(1);
        }
    });
    
    connect(m_fileListPanel, &FileListPanel::filesCleared, this, [this](const QVector<int>& fileIds) {
        LOG_INFO("MainWindow", QString("文件列表清空, 共%1个数据库文件需要删除").arg(fileIds.size()));
        if (DatabaseManager::instance()->isInitialized() && !fileIds.isEmpty()) {
            QProgressDialog progress(tr("正在删除文件数据..."), tr("取消"), 0, fileIds.size(), this);
            progress.setWindowModality(Qt::WindowModal);
            progress.setMinimumDuration(0);
            progress.setAutoClose(true);
            progress.setAutoReset(true);
            progress.setLabelText(tr("正在删除数据库数据 (0/%1)").arg(fileIds.size()));
            progress.setValue(0);
            QCoreApplication::processEvents();
            
            for (int i = 0; i < fileIds.size(); ++i) {
                if (progress.wasCanceled()) {
                    LOG_INFO("MainWindow", "用户取消删除数据库数据");
                    break;
                }
                progress.setLabelText(tr("正在删除数据库数据 (%1/%2)").arg(i + 1).arg(fileIds.size()));
                progress.setValue(i);
                QCoreApplication::processEvents();
                
                bool ok = DatabaseManager::instance()->deleteFileData(fileIds[i]);
                LOG_INFO("MainWindow", QString("数据库删除文件数据: fileId=%1, 结果=%2").arg(fileIds[i]).arg(ok));
            }
            progress.setValue(fileIds.size());
        }
    });
    
    connect(m_aiQueryPanel, &AIQueryPanel::querySubmitted, this, &MainWindow::onAIQuerySubmitted);
    connect(m_aiQueryPanel, &AIQueryPanel::chatMessageSent, this, &MainWindow::onAIChatMessageSent);
    connect(m_aiQueryPanel, &AIQueryPanel::clearRequested, this, &MainWindow::onAIQueryClear);
    connect(m_aiQueryPanel, &AIQueryPanel::modeChanged, this, &MainWindow::onAIModeChanged);
    connect(m_aiQueryPanel, &AIQueryPanel::modelChanged, this, [this](const QString& modelId) {
        ConfigManager::instance()->setDefaultModel(modelId);
        m_statusLabel->setText(tr("已切换模型: %1").arg(modelId));
    });
    connect(m_aiQueryPanel, &AIQueryPanel::cancelRequested, this, [this]() {
        m_statusLabel->setText(tr("查询已取消"));
        LOG_INFO("MainWindow", "AI查询已取消");
    });
    connect(m_aiQueryPanel, &AIQueryPanel::resetDataRequested, this, [this]() {
        if (!m_progressDialog) {
            m_progressDialog = new ProgressDialog(this);
            connect(m_progressDialog, &ProgressDialog::canceled, this, &MainWindow::onProgressDialogCanceled);
        }
        m_progressDialog->setWindowTitle(tr("正在恢复数据"));
        m_progressDialog->setStatusText(tr("正在清除筛选条件..."));
        m_progressDialog->setProgress(0);
        m_progressDialog->startTimer();
        m_progressDialog->show();
        QCoreApplication::processEvents();
        
        QElapsedTimer resetTimer;
        resetTimer.start();
        
        m_dataStore->beginBatchFilterUpdate();
        
        m_dataStore->clearFilter();
        m_dataStore->clearTerminalFilter();
        m_dataStore->clearSubAddressFilter();
        m_dataStore->clearMessageTypeFilter();
        m_dataStore->clearChsttFilter();
        m_dataStore->clearMpuIdFilter();
        m_dataStore->clearTimeRange();
        m_dataStore->clearColumnExpressionFilter(-1);
        m_dataStore->clearPacketLenFilter();
        m_dataStore->clearDateRangeFilter();
        m_dataStore->clearStatusBitFilter();
        m_dataStore->clearExcludeTerminalFilter();
        m_dataStore->clearExcludeMessageTypeFilter();
        m_dataStore->clearWordCountFilter();
        m_dataStore->clearErrorFlagFilter();
        
        m_dataStore->endBatchFilterUpdate();
        
        m_progressDialog->setProgress(50);
        m_progressDialog->setStatusText(tr("正在刷新数据..."));
        QCoreApplication::processEvents();
        
        m_dataModel->setDataStore(m_dataStore);
        
        qint64 elapsed = resetTimer.elapsed();
        m_progressDialog->setProgress(100);
        QString elapsedStr = elapsed < 1000 ? tr("%1 毫秒").arg(elapsed) :
                             tr("%1 秒").arg(QString::number(elapsed / 1000.0, 'f', 2));
        m_progressDialog->setStatusText(tr("恢复完成！耗时 %1").arg(elapsedStr));
        m_progressDialog->stopTimer();
        m_progressDialog->setElapsedTime(elapsed);
        QCoreApplication::processEvents();
        QTimer::singleShot(800, this, [this]() {
            if (m_progressDialog) {
                m_progressDialog->hide();
                m_progressDialog->reset();
            }
        });
        
        m_statusLabel->setText(tr("已恢复显示全部数据"));
        LOG_INFO("MainWindow", "数据已重置，显示全部数据");
    });
    
    connect(m_tableView, &TableView::recordDoubleClicked, this, &MainWindow::onRecordDoubleClicked);
    connect(m_tableView, &TableView::showDetailRequested, this, &MainWindow::showDataDetailDialog);
    connect(m_tableView, &TableView::expressionFilterRequested, this, &MainWindow::onExpressionFilterRequested);
    
    connect(m_toolExecutor, &AIToolExecutor::queryDataRequested, this, &MainWindow::onAIQueryDataRequested);
    connect(m_toolExecutor, &AIToolExecutor::generateChartRequested, this, &MainWindow::onAIGenerateChartRequested);
    connect(m_toolExecutor, &AIToolExecutor::generateGanttRequested, this, &MainWindow::onAIGenerateGanttRequested);
    connect(m_toolExecutor, &AIToolExecutor::clearFilterRequested, this, &MainWindow::onAIClearFilterRequested);
    connect(m_toolExecutor, &AIToolExecutor::switchToChartTabRequested, this, &MainWindow::onAISwitchToChartTab);
    connect(m_toolExecutor, &AIToolExecutor::switchToGanttTabRequested, this, &MainWindow::onAISwitchToGanttTab);
    connect(m_toolExecutor, &AIToolExecutor::switchToTableTabRequested, this, &MainWindow::onAISwitchToTableTab);
    connect(m_toolExecutor, &AIToolExecutor::generateReportRequested, this, [this](const QString& format) {
        QString filePath = m_currentFile;
        if (filePath.isEmpty() && m_dataStore && m_dataStore->currentFileId() > 0) {
            filePath = tr("分析报告");
        }
        if (!filePath.isEmpty()) {
            m_reportGenerator->generateReportAsync(filePath, format);
        }
    });
    
    connect(ModelManager::instance(), &ModelManager::queryStarted, this, &MainWindow::onModelQueryStarted);
    connect(ModelManager::instance(), &ModelManager::queryFinished, this, &MainWindow::onModelQueryFinished);
    connect(ModelManager::instance(), &ModelManager::queryError, this, &MainWindow::onModelQueryError);
    
    connect(m_reportGenerator, &ReportGenerator::progressChanged, this, &MainWindow::onReportProgress);
    connect(m_reportGenerator, &ReportGenerator::reportFinished, this, &MainWindow::onReportFinished);
}

/**
 * @brief 加载设置
 * 
 * 从QSettings恢复：
 * - 窗口几何状态
 * - 窗口布局状态
 * - 模型选择器配置
 */
void MainWindow::loadSettings()
{
    QSettings settings;
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
    
    QMap<QString, QString> models;
    auto providers = ConfigManager::instance()->getModelProviders();
    
    if (providers.isEmpty()) {
        LOG_WARNING("MainWindow", "模型提供商列表为空，无法初始化模型选择器");
    } else {
        for (const auto& provider : providers) {
            for (const auto& instance : provider.instances) {
                if (instance.enabled) {
                    QString modelId = QString("%1.%2").arg(provider.id, instance.name);
                    QString displayName = QString("%1 - %2").arg(provider.name, instance.name);
                    models[displayName] = modelId;
                }
            }
        }
    }
    
    m_aiQueryPanel->setModels(models);
    
    QString defaultModel = ConfigManager::instance()->getDefaultModel();
    m_aiQueryPanel->setCurrentModel(defaultModel);
}

/**
 * @brief 保存设置
 * 
 * 将窗口状态保存到QSettings
 */
void MainWindow::saveSettings()
{
    QSettings settings;
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
}

void MainWindow::loadDatabaseFileList()
{
    if (!DatabaseManager::instance()->isInitialized()) {
        return;
    }
    
    QVector<QVariantMap> files = DatabaseManager::instance()->getFileList();
    for (const auto& file : files) {
        int fileId = file["id"].toInt();
        QString filePath = file["file_path"].toString();
        QString fileName = file["file_name"].toString();
        qint64 packetCount = file["packet_count"].toLongLong();
        QString importTime = file["import_time"].toString();
        
        if (packetCount == 0) {
            packetCount = DatabaseManager::instance()->getTotalPacketCount(fileId);
        }
        
        if (!filePath.isEmpty()) {
            m_fileListPanel->addDbFile(fileId, filePath, fileName, packetCount, importTime);
        }
    }
    
    LOG_INFO("MainWindow", QString("从数据库加载了 %1 个文件记录").arg(files.size()));
}

bool MainWindow::loadFileFromDatabase(int fileId)
{
    QVariantMap fileInfo = DatabaseManager::instance()->getFileInfo(fileId);
    if (fileInfo.isEmpty()) {
        QMessageBox::warning(this, tr("加载失败"), tr("数据库中未找到该文件记录"));
        return false;
    }
    
    QString filePath = fileInfo["file_path"].toString();
    m_currentFile = filePath;
    
    if (!m_progressDialog) {
        m_progressDialog = new ProgressDialog(this);
        connect(m_progressDialog, &ProgressDialog::canceled, this, &MainWindow::onProgressDialogCanceled);
    }
    
    m_progressDialog->setWindowTitle(tr("正在加载数据"));
    m_progressDialog->setStatusText(tr("正在从数据库加载数据..."));
    m_progressDialog->setProgress(0);
    m_progressDialog->startTimer();
    m_progressDialog->show();
    
    QCoreApplication::processEvents();
    
    m_progressDialog->setProgress(10);
    m_progressDialog->setStatusText(tr("正在切换数据源..."));
    QCoreApplication::processEvents();
    
    m_dataStore->setStorageMode(StorageMode::Database);
    m_dataStore->beginBatchFilterUpdate();
    m_dataStore->clearFilter();
    m_dataStore->clearTerminalFilter();
    m_dataStore->clearSubAddressFilter();
    m_dataStore->clearMessageTypeFilter();
    m_dataStore->clearChsttFilter();
    m_dataStore->clearMpuIdFilter();
    m_dataStore->clearTimeRange();
    m_dataStore->clearPacketLenFilter();
    m_dataStore->clearDateRangeFilter();
    m_dataStore->clearStatusBitFilter();
    m_dataStore->clearExcludeTerminalFilter();
    m_dataStore->clearExcludeMessageTypeFilter();
    m_dataStore->clearWordCountFilter();
    m_dataStore->clearErrorFlagFilter();
    m_dataStore->endBatchFilterUpdate();
    
    m_progressDialog->setProgress(20);
    m_progressDialog->setStatusText(tr("正在查询数据记录..."));
    QCoreApplication::processEvents();
    
    m_dataModel->setDataStore(m_dataStore);
    
    bool ok = m_dataStore->switchToFile(fileId);
    if (!ok) {
        m_progressDialog->stopTimer();
        m_progressDialog->hide();
        m_progressDialog->reset();
        QMessageBox::warning(this, tr("加载失败"), tr("无法从数据库加载文件数据"));
        return false;
    }
    
    m_progressDialog->setProgress(60);
    m_progressDialog->setStatusText(tr("正在加载数据页面..."));
    QCoreApplication::processEvents();
    
    m_dataStore->setPageSize(m_paginationWidget->pageSize());
    
    m_progressDialog->setProgress(80);
    m_progressDialog->setStatusText(tr("正在更新界面..."));
    QCoreApplication::processEvents();
    
    m_paginationWidget->updatePagination(
        m_dataStore->currentPage(),
        m_dataStore->totalPages(),
        m_dataStore->filteredCount(),
        m_dataStore->totalCount()
    );
    
    m_statusLabel->setText(tr("已从数据库加载: %1").arg(fileInfo["file_name"].toString()));
    m_countLabel->setText(tr("数据: %1 条").arg(m_dataStore->filteredCount()));
    updateWindowTitle();
    
    m_fileListPanel->setActiveFile(filePath);
    
    m_progressDialog->setProgress(100);
    m_progressDialog->setStatusText(tr("加载完成！"));
    QCoreApplication::processEvents();
    
    QTimer::singleShot(500, this, [this]() {
        if (m_progressDialog) {
            m_progressDialog->stopTimer();
            m_progressDialog->hide();
            m_progressDialog->reset();
        }
    });
    
    LOG_INFO("MainWindow", QString("从数据库加载文件: fileId=%1, path=%2, count=%3")
             .arg(fileId).arg(filePath).arg(m_dataStore->filteredCount()));
    
    return true;
}

/**
 * @brief 加载并解析文件
 * @param filePath 文件路径
 * 
 * 文件加载流程：
 * 1. 添加文件到文件列表
 * 2. 显示进度对话框
 * 3. 从配置获取解析参数
 * 4. 在后台线程中执行解析
 */
void MainWindow::loadFile(const QString& filePath)
{
    LOG_INFO("MainWindow", QString("开始加载文件: %1").arg(filePath));
    
    m_currentFile = filePath;
    m_loadCanceled.store(false);
    
    if (DatabaseManager::instance()->isInitialized()) {
        int fileId = DatabaseManager::instance()->getFileIdByPath(filePath);
        if (fileId > 0) {
            LOG_INFO("MainWindow", QString("文件已在数据库中(fileId=%1)").arg(fileId));
            
            QMessageBox::StandardButton reply = QMessageBox::question(this,
                tr("文件已导入"),
                tr("该文件已导入数据库，是否加载显示数据？"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
            
            if (reply == QMessageBox::Yes) {
                m_fileListPanel->addFile(filePath);
                m_fileListPanel->updateFileId(filePath, fileId);
                m_fileListPanel->setActiveFile(filePath);
                loadFileFromDatabase(fileId);
            }
            return;
        }
        
        QFileInfo fi(filePath);
        int sameNameId = DatabaseManager::instance()->getFileIdByFileName(fi.fileName());
        if (sameNameId > 0) {
            QMessageBox::StandardButton reply = QMessageBox::question(this,
                tr("同名文件已存在"),
                tr("数据库中已存在同名文件\"%1\"。\n\n是否加载已有数据？\n（选择否将取消导入）").arg(fi.fileName()),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
            
            if (reply == QMessageBox::Yes) {
                QVariantMap fileInfo = DatabaseManager::instance()->getFileInfo(sameNameId);
                QString existingPath = fileInfo["file_path"].toString();
                m_fileListPanel->addFile(existingPath);
                m_fileListPanel->updateFileId(existingPath, sameNameId);
                m_fileListPanel->setActiveFile(existingPath);
                m_currentFile = existingPath;
                loadFileFromDatabase(sameNameId);
            }
            return;
        }
    }
    
    m_fileListPanel->addFile(filePath);
    
    // 创建并显示进度对话框
    if (!m_progressDialog) {
        m_progressDialog = new ProgressDialog(this);
        connect(m_progressDialog, &ProgressDialog::canceled, this, &MainWindow::onProgressDialogCanceled);
    }
    
    m_progressDialog->setWindowTitle(tr("正在加载文件"));
    m_progressDialog->setStatusText(tr("正在解析文件..."));
    m_progressDialog->setProgress(0);
    m_progressDialog->startTimer();
    m_progressDialog->show();
    
    m_elapsedTimer.start();
    
    qint64 startMs = m_elapsedTimer.elapsed();
    LOG_INFO("MainWindow", QString("文件导入开始时间: %1 ms (相对时间)").arg(startMs));
    
    ParserConfig config = ConfigManager::instance()->getParserConfig();
    LOG_DEBUG("MainWindow", QString("解析配置: 字节序=%1, Header1=0x%2, Header2=0x%3, DataHeader=0x%4, 错误容差=%5")
              .arg(config.byteOrder)
              .arg(config.header1, 4, 16, QChar('0'))
              .arg(config.header2, 2, 16, QChar('0'))
              .arg(config.dataHeader, 4, 16, QChar('0'))
              .arg(config.maxErrorTolerance));
    
    QMetaObject::invokeMethod(m_parser, "setByteOrder", Qt::BlockingQueuedConnection,
        Q_ARG(bool, config.byteOrder == "little"));
    QMetaObject::invokeMethod(m_parser, "setHeader1", Qt::BlockingQueuedConnection,
        Q_ARG(quint16, config.header1));
    QMetaObject::invokeMethod(m_parser, "setHeader2", Qt::BlockingQueuedConnection,
        Q_ARG(quint16, config.header2));
    QMetaObject::invokeMethod(m_parser, "setDataHeader", Qt::BlockingQueuedConnection,
        Q_ARG(quint16, config.dataHeader));
    QMetaObject::invokeMethod(m_parser, "setMaxErrorTolerance", Qt::BlockingQueuedConnection,
        Q_ARG(int, config.maxErrorTolerance));
    
    QMetaObject::invokeMethod(m_parser, "parseFile", Qt::QueuedConnection,
        Q_ARG(QString, filePath));
}

void MainWindow::onOpenFile()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("打开1553B数据文件"),
        QString(),
        tr("二进制文件 (*.bin *.data *.raw);;所有文件 (*.*)")
    );
    
    if (!filePath.isEmpty()) {
        loadFile(filePath);
    }
}

void MainWindow::onSaveExport()
{
    if (m_dataStore->totalCount() == 0) {
        QMessageBox::warning(this, tr("导出失败"), tr("没有数据可导出，请先导入数据文件。"));
        return;
    }
    
    QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("导出数据"),
        QString(),
        tr("CSV文件 (*.csv);;PDF文件 (*.pdf)")
    );
    
    if (filePath.isEmpty()) {
        return;
    }
    
    QProgressDialog progressDialog(tr("正在导出数据..."), tr("取消"), 0, 100, this);
    progressDialog.setWindowTitle(tr("导出数据"));
    progressDialog.setWindowModality(Qt::WindowModal);
    progressDialog.setMinimumDuration(0);
    progressDialog.setAutoClose(true);
    progressDialog.setAutoReset(true);
    progressDialog.setValue(0);
    
    QMetaObject::Connection progressConn = connect(m_exportService, &ExportService::exportProgress,
        [&progressDialog](int current, int total) {
            if (total > 0) {
                int percent = static_cast<int>(current * 100.0 / total);
                progressDialog.setValue(qMin(percent, 99));
            }
        });
    
    QMetaObject::Connection finishedConn = connect(m_exportService, &ExportService::exportFinished,
        [&progressDialog](bool) {
            progressDialog.setValue(100);
        });
    
    connect(&progressDialog, &QProgressDialog::canceled, this, [this]() {
        m_statusLabel->setText(tr("导出已取消"));
    });
    
    m_statusLabel->setText(tr("导出中..."));
    
    bool success = false;
    if (filePath.endsWith(".csv", Qt::CaseInsensitive)) {
        success = m_exportService->exportToCsv(filePath, m_dataStore);
    } else if (filePath.endsWith(".pdf", Qt::CaseInsensitive)) {
        success = m_exportService->exportToPdf(filePath, m_dataStore);
    }
    
    disconnect(progressConn);
    disconnect(finishedConn);
    
    if (success) {
        m_statusLabel->setText(tr("导出成功"));
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(tr("导出成功"));
        msgBox.setText(tr("数据已导出到: %1").arg(filePath));
        msgBox.setStandardButtons(QMessageBox::Open | QMessageBox::Ok);
        msgBox.setDefaultButton(QMessageBox::Open);
        
        if (msgBox.exec() == QMessageBox::Open) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
        }
    } else {
        m_statusLabel->setText(tr("导出失败"));
        QMessageBox::warning(this, tr("导出失败"), m_exportService->lastError());
    }
}

void MainWindow::onExportGanttPng()
{
    QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("导出甘特图"),
        QString(),
        tr("PNG图片 (*.png)")
    );
    
    if (!filePath.isEmpty()) {
        if (m_ganttView->exportToImage(filePath)) {
            QMessageBox msgBox(this);
            msgBox.setWindowTitle(tr("导出成功"));
            msgBox.setText(tr("甘特图已保存到: %1").arg(filePath));
            msgBox.setStandardButtons(QMessageBox::Open | QMessageBox::Ok);
            msgBox.setDefaultButton(QMessageBox::Open);
            
            if (msgBox.exec() == QMessageBox::Open) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
            }
        } else {
            QMessageBox::warning(this, tr("导出失败"), tr("无法导出甘特图，请确保已加载数据。"));
        }
    }
}

void MainWindow::onExportGanttPdf()
{
    QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("导出甘特图"),
        QString(),
        tr("PDF文件 (*.pdf)")
    );
    
    if (!filePath.isEmpty()) {
        if (m_ganttView->exportToPdf(filePath)) {
            QMessageBox msgBox(this);
            msgBox.setWindowTitle(tr("导出成功"));
            msgBox.setText(tr("甘特图已保存到: %1").arg(filePath));
            msgBox.setStandardButtons(QMessageBox::Open | QMessageBox::Ok);
            msgBox.setDefaultButton(QMessageBox::Open);
            
            if (msgBox.exec() == QMessageBox::Open) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
            }
        } else {
            QMessageBox::warning(this, tr("导出失败"), tr("无法导出甘特图，请确保已加载数据。"));
        }
    }
}

void MainWindow::onSettings()
{
    SettingsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        loadSettings();
    }
}

void MainWindow::onAbout()
{
    QMessageBox::about(this, tr("关于"),
        tr("1553B数据智能分析工具\n\n"
           "版本: 1.0.0\n\n"
           "功能:\n"
           "- 二进制文件解析\n"
           "- 数据表格展示\n"
           "- 甘特图可视化\n"
           "- AI智能分析\n"
           "- 多格式导出"));
}

void MainWindow::onOpenLog()
{
    QString logPath = Logger::instance()->logFilePath();
    
    if (logPath.isEmpty()) {
        QMessageBox::information(this, tr("打开日志"), tr("日志文件尚未创建。"));
        return;
    }
    
    QFileInfo fileInfo(logPath);
    if (!fileInfo.exists()) {
        QMessageBox::warning(this, tr("打开日志"), tr("日志文件不存在:\n%1").arg(logPath));
        return;
    }
    
    LOG_INFO("MainWindow", QString("打开日志文件: %1").arg(logPath));
    
    QUrl url = QUrl::fromLocalFile(logPath);
    if (!QDesktopServices::openUrl(url)) {
        QMessageBox::warning(this, tr("打开日志"), tr("无法打开日志文件:\n%1").arg(logPath));
    }
}

void MainWindow::onParseProgress(int current, int total)
{
    if (m_progressDialog) {
        // 解析阶段占总进度的 0-60%
        int parsePercent = static_cast<int>(current * 60.0 / total);
        m_progressDialog->setProgress(parsePercent);
        m_progressDialog->setStatusText(tr("正在解析文件... %1%").arg(current * 100 / total));
    }
}

void MainWindow::onParseFinished(bool success, int count)
{
    if (m_loadCanceled.load()) {
        LOG_INFO("MainWindow", "加载已取消，忽略解析完成回调");
        return;
    }
    
    if (success) {
        // 解析完成，更新进度到60%
        if (m_progressDialog) {
            m_progressDialog->setProgress(60);
            m_progressDialog->setStatusText(tr("解析完成，正在准备数据..."));
        }
        
        QThread* loadThread = QThread::create([this]() {
            // 获取解析数据（可能耗时）
            QMetaObject::invokeMethod(this, [this]() {
                if (m_progressDialog) {
                    m_progressDialog->setProgress(62);
                    m_progressDialog->setStatusText(tr("正在获取解析数据..."));
                }
            }, Qt::BlockingQueuedConnection);
            
            QVector<SMbiMonPacketMsg> parsedData;
            QMetaObject::invokeMethod(m_parser, "getParsedData", Qt::BlockingQueuedConnection,
                Q_RETURN_ARG(QVector<SMbiMonPacketMsg>, parsedData));
            int actualCount = parsedData.size();
            
            LOG_DEBUG("MainWindow", QString("实际解析的消息数: %1").arg(actualCount));
            
            if (m_loadCanceled.load()) {
                LOG_INFO("MainWindow", "加载已取消，停止数据处理");
                return;
            }
            
            if (actualCount == 0) {
                QMetaObject::invokeMethod(this, [this]() {
                    if (m_progressDialog) {
                        m_progressDialog->stopTimer();
                        m_progressDialog->hide();
                        m_progressDialog->reset();
                    }
                    
                    m_statusLabel->setText(tr("解析完成，但未找到有效数据"));
                    QMessageBox::warning(this, tr("解析结果"), 
                        tr("文件解析成功，但未找到任何有效的1553B数据。\n\n"
                           "可能的原因:\n"
                           "1. 文件格式与配置不匹配\n"
                           "2. Header1/Header2/DataHeader配置错误\n"
                           "3. 字节序配置错误\n\n"
                           "请检查设置中的解析器配置。"));
                }, Qt::BlockingQueuedConnection);
                return;
            }
            
            // 开始构建索引
            QMetaObject::invokeMethod(this, [this]() {
                if (m_progressDialog) {
                    m_progressDialog->setProgress(65);
                    m_progressDialog->setStatusText(tr("正在构建索引..."));
                }
            }, Qt::BlockingQueuedConnection);
            
            // 构建索引，并添加进度回调
            QVector<DataRecord> index = DataStore::buildIndex(parsedData, 
                [this](int current, int total) {
                    // 构建索引阶段占总进度的 65-85%
                    int buildPercent = static_cast<int>(65 + (current * 20.0 / total));
                    QMetaObject::invokeMethod(this, [this, buildPercent, current, total]() {
                        if (m_progressDialog) {
                            m_progressDialog->setProgress(buildPercent);
                            m_progressDialog->setStatusText(tr("正在构建索引... %1/%2").arg(current).arg(total));
                        }
                    }, Qt::BlockingQueuedConnection);
                }
            );
            
            // 开始加载数据
            QMetaObject::invokeMethod(this, [this]() {
                if (m_progressDialog) {
                    m_progressDialog->setProgress(85);
                    m_progressDialog->setStatusText(tr("正在加载数据..."));
                }
            }, Qt::BlockingQueuedConnection);
            
            // 以下操作在后台线程中执行，避免阻塞主线程事件循环
            // 这样进度回调和计时器都能正常更新
            
            qint64 elapsedMs = m_elapsedTimer.elapsed();
            LOG_INFO("MainWindow", QString("表格加载开始时间: %1 ms (相对时间)").arg(elapsedMs));
            
            QMetaObject::invokeMethod(this, [this]() {
                if (m_progressDialog) {
                    m_progressDialog->setProgress(87);
                    m_progressDialog->setStatusText(tr("正在加载数据到内存..."));
                }
            }, Qt::BlockingQueuedConnection);
            
            // 检查数据量，决定是否使用数据库模式
            qint64 estimatedRecords = 0;
            for (const auto& msg : parsedData) {
                estimatedRecords += msg.packetDatas.size();
            }
            
            int threshold = ConfigManager::instance()->getDatabaseConfig().recordThreshold;
            
            LOG_INFO("MainWindow", QString("estimatedRecords=%1, threshold=%2, isDatabaseMode=%3")
                     .arg(estimatedRecords).arg(threshold).arg(m_dataStore->isDatabaseMode()));
            
            if (estimatedRecords > threshold) {
                LOG_INFO("MainWindow", QString("数据量 %1 超过阈值，使用数据库模式").arg(estimatedRecords));
                
                QMetaObject::invokeMethod(this, [this]() {
                    if (m_progressDialog) {
                        m_progressDialog->setStatusText(tr("正在导入数据到数据库..."));
                    }
                    m_dataStore->setStorageMode(StorageMode::Database);
                }, Qt::BlockingQueuedConnection);
                
                LOG_INFO("MainWindow", QString("setStorageMode(Database) done, isDatabaseMode=%1").arg(m_dataStore->isDatabaseMode()));
                
                // importFromData在后台线程执行，进度回调通过BlockingQueuedConnection安全更新UI
                int fileId = m_dataStore->importFromData(parsedData, m_currentFile,
                    [this](int current, int total, const QString& status) {
                        QMetaObject::invokeMethod(this, [this, current, total, status]() {
                            if (m_progressDialog) {
                                // 数据库导入阶段占总进度的87%-97%
                                // DatabaseManager内部进度：并行导入占90%，合并占10%
                                // current/total范围：0~total，需映射到87~97
                                int percent = qMin(97, 87 + (current * 10 / qMax(1, total)));
                                m_progressDialog->setProgress(percent);
                                m_progressDialog->setStatusText(status);
                            }
                        }, Qt::BlockingQueuedConnection);
                    }
                );
                
                LOG_INFO("MainWindow", QString("importFromData returned fileId=%1").arg(fileId));
                
                if (fileId <= 0) {
                    if (m_loadCanceled.load()) {
                        LOG_INFO("MainWindow", "导入已取消，不回退内存模式");
                        return;
                    }
                    LOG_ERROR("MainWindow", "数据库导入失败，回退到内存模式");
                    QMetaObject::invokeMethod(this, [this, parsedData, index]() {
                        m_dataStore->setStorageMode(StorageMode::Memory);
                        m_dataStore->setIndexedData(parsedData, index);
                    }, Qt::BlockingQueuedConnection);
                } else {
                    QMetaObject::invokeMethod(this, [this, fileId]() {
                        m_fileListPanel->updateFileId(m_currentFile, fileId);
                        m_fileListPanel->setActiveFile(m_currentFile);
                    }, Qt::BlockingQueuedConnection);
                }
            } else {
                LOG_INFO("MainWindow", QString("数据量 %1 未超过阈值，使用内存模式").arg(estimatedRecords));
                QMetaObject::invokeMethod(this, [this, parsedData, index]() {
                    m_dataStore->setIndexedData(parsedData, index);
                    m_fileListPanel->setActiveFile(m_currentFile);
                }, Qt::BlockingQueuedConnection);
            }
            
            // 更新进度：数据加载完成（回到主线程更新UI）
            QMetaObject::invokeMethod(this, [this, actualCount]() {
                if (m_loadCanceled.load()) {
                    LOG_INFO("MainWindow", "加载已取消，跳过UI更新");
                    return;
                }
                
                if (m_progressDialog) {
                    m_progressDialog->setProgress(98);
                    m_progressDialog->setStatusText(tr("正在更新界面..."));
                }
                
                qint64 elapsedMs = m_elapsedTimer.elapsed();
                LOG_INFO("MainWindow", QString("表格加载完毕时间: %1 ms (相对时间)").arg(elapsedMs));
                
                QString timeStr = QString("%1:%2:%3")
                    .arg(elapsedMs / 3600000, 2, 10, QChar('0'))
                    .arg((elapsedMs % 3600000) / 60000, 2, 10, QChar('0'))
                    .arg((elapsedMs % 60000) / 1000, 2, 10, QChar('0'));
                
                LOG_INFO("MainWindow", QString("解析完成: count=%1, 总耗时=%2ms (%3)")
                         .arg(actualCount).arg(elapsedMs).arg(timeStr));
                
                // 更新状态栏
                m_statusLabel->setText(tr("解析完成"));
                m_countLabel->setText(tr("数据: %1 条").arg(m_dataStore->filteredCount()));
                updateWindowTitle();
                
                // 更新进度：更新分页控件
                if (m_progressDialog) {
                    m_progressDialog->setProgress(99);
                    m_progressDialog->setStatusText(tr("正在更新分页控件..."));
                }
                
                // 同步分页大小设置
                m_dataStore->setPageSize(m_paginationWidget->pageSize());
                
                // 更新分页控件
                m_paginationWidget->updatePagination(
                    m_dataStore->currentPage(),
                    m_dataStore->totalPages(),
                    m_dataStore->filteredCount(),
                    m_dataStore->totalCount()
                );
                
                // 隐藏进度对话框（数据已完全加载）
                if (m_progressDialog) {
                    m_progressDialog->setProgress(100);
                    m_progressDialog->setStatusText(tr("加载完成！"));
                    m_progressDialog->stopTimer();
                    m_progressDialog->hide();
                    m_progressDialog->reset();
                }
                
                LOG_INFO("MainWindow", QString("数据加载完成，共 %1 条消息，%2 条数据记录")
                         .arg(actualCount)
                         .arg(m_dataStore->totalCount()));
                
                // 延迟刷新图表和显示消息，避免阻塞UI
                QTimer::singleShot(100, this, [this, actualCount, timeStr]() {
                    // 刷新图表
                    m_chartWidget->refreshChart();
                    
                    // 显示完成消息
                    QMessageBox::information(this, tr("解析完成"), 
                        tr("文件解析成功！\n\n"
                           "解析消息数: %1 条\n"
                           "数据记录数: %2 条\n"
                           "耗时: %3")
                        .arg(actualCount)
                        .arg(m_dataStore->totalCount())
                        .arg(timeStr));
                });
            }, Qt::BlockingQueuedConnection);
        });
        connect(loadThread, &QThread::finished, loadThread, &QObject::deleteLater);
        loadThread->start();
    } else {
        if (m_progressDialog) {
            m_progressDialog->stopTimer();
            m_progressDialog->hide();
            m_progressDialog->reset();
        }
        
        QString error;
        QMetaObject::invokeMethod(m_parser, "getLastError", Qt::BlockingQueuedConnection,
            Q_RETURN_ARG(QString, error));
        m_statusLabel->setText(tr("解析失败: %1").arg(error));
        LOG_ERROR("MainWindow", QString("解析失败: %1").arg(error));
        
        QMessageBox::critical(this, tr("解析错误"), 
            tr("文件解析失败:\n%1\n\n"
               "请检查文件格式和解析器配置。").arg(error));
    }
}

void MainWindow::onDataLoaded(int count)
{
    m_countLabel->setText(tr("数据: %1 条").arg(count));
}

void MainWindow::updateWindowTitle()
{
    QString title = tr("1553B数据智能分析工具");
    if (!m_currentFile.isEmpty()) {
        QFileInfo fi(m_currentFile);
        title = QString("%1 - %2").arg(fi.fileName(), title);
    }
    setWindowTitle(title);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    
    // 停止解析器线程
    if (m_parserThread && m_parserThread->isRunning()) {
        m_parserThread->quit();
        if (!m_parserThread->wait(3000)) {
            m_parserThread->terminate();
            m_parserThread->wait();
        }
    }
    
    // 取消正在进行的报告生成
    if (m_reportGenerator) {
        m_reportGenerator->cancelReport();
    }
    
    // 取消正在进行的数据库导入
    DatabaseManager::instance()->cancelImport();
    
    // 等待全局线程池中的任务完成
    QThreadPool::globalInstance()->waitForDone(3000);
    
    event->accept();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls = event->mimeData()->urls();
    if (!urls.isEmpty()) {
        QString filePath = urls.first().toLocalFile();
        if (!filePath.isEmpty()) {
            loadFile(filePath);
        }
    }
}

void MainWindow::onAIQuerySubmitted(const QString& query)
{
    LOG_INFO("MainWindow", QString("收到AI分析请求: %1").arg(query));
    
    QString modelId = m_aiQueryPanel->currentModelId();
    if (modelId.isEmpty()) {
        m_aiQueryPanel->appendResponse(tr("错误: 未选择模型，请先在设置中配置模型。"));
        m_aiQueryPanel->setLoading(false);
        return;
    }
    
    ModelManager::instance()->setCurrentModel(modelId);
    
    if (m_dataStore->totalCount() == 0) {
        m_aiQueryPanel->appendResponse(tr("错误: 没有数据，请先导入1553B数据文件。"));
        m_aiQueryPanel->setLoading(false);
        return;
    }
    
    QString lowerQuery = query.toLower().trimmed();
    bool isSimpleQuery = lowerQuery == "所有" || lowerQuery == "全部" || 
                         lowerQuery == "清空" || lowerQuery == "重置" || 
                         lowerQuery == "取消筛选" || lowerQuery == "显示所有数据" ||
                         lowerQuery == "清除筛选";
    
    if (isSimpleQuery) {
        QString response = processSimpleQuery(query);
        m_aiQueryPanel->appendResponse(response);
        m_aiQueryPanel->setLoading(false);
        return;
    }
    
    QJsonObject context;
    context["data_count"] = m_dataStore->totalCount();
    context["mode"] = "analysis";
    context["tools"] = AIToolDefinitions::getTools();
    context["system_prompt"] = AIToolDefinitions::getSystemPromptForAnalysis();
    
    ModelManager::instance()->sendQuery(query, context);
}

void MainWindow::onAIChatMessageSent(const QString& message, const QList<ChatMessage>& history)
{
    LOG_INFO("MainWindow", QString("收到AI聊天消息: %1").arg(message));
    
    QString modelId = m_aiQueryPanel->currentModelId();
    if (modelId.isEmpty()) {
        m_aiQueryPanel->appendResponse(tr("错误: 未选择模型，请先在设置中配置模型。"));
        m_aiQueryPanel->setLoading(false);
        return;
    }
    
    ModelManager::instance()->setCurrentModel(modelId);
    
    QJsonObject context;
    context["mode"] = "chat";
    context["system_prompt"] = AIToolDefinitions::getSystemPromptForChat();
    
    QJsonArray historyArray;
    for (const ChatMessage& msg : history) {
        QJsonObject msgObj;
        msgObj["role"] = msg.role == ChatMessage::User ? "user" : 
                         (msg.role == ChatMessage::Assistant ? "assistant" : "system");
        msgObj["content"] = msg.content;
        historyArray.append(msgObj);
    }
    context["history"] = historyArray;
    
    ModelManager::instance()->sendQuery(message, context);
}

void MainWindow::onAIModeChanged(AIMode mode)
{
    LOG_INFO("MainWindow", QString("AI模式切换: %1").arg(mode == AIMode::Chat ? "聊天" : "分析"));
    m_statusLabel->setText(mode == AIMode::Chat ? tr("AI聊天模式") : tr("AI智能分析模式"));
}

void MainWindow::onAIQueryClear()
{
    // clearFilter() 已经清除了所有筛选条件并调用了 applyFilters()
    // 不需要再调用其他 clear* 方法，避免重复执行筛选操作
    m_dataStore->clearFilter();
    m_statusLabel->setText(tr("筛选已清除"));
}

void MainWindow::onDataScopeChanged()
{
    int scopeValue = m_dataScopeCombo->currentData().toInt();
    DataScope scope = static_cast<DataScope>(scopeValue);
    m_dataStore->setDataScope(scope);
    
    QString scopeName;
    switch (scope) {
    case DataScope::AllData: scopeName = tr("文件所有数据"); break;
    case DataScope::FilteredData: scopeName = tr("当前筛选数据"); break;
    case DataScope::CurrentPage: scopeName = tr("当前页数据"); break;
    }
    m_statusLabel->setText(tr("数据范围: %1").arg(scopeName));
}

void MainWindow::onRecordDoubleClicked(int row)
{
    showDataDetailDialog(row);
}

/**
 * @brief 算式筛选请求槽
 * @param column 列索引
 * @param expression 筛选表达式
 * 
 * 根据用户输入的算式进行数据筛选
 */
void MainWindow::onExpressionFilterRequested(int column, const QString& expression)
{
    // 设置列算式筛选
    if (m_dataStore->setColumnExpressionFilter(column, expression)) {
        m_dataModel->setDataStore(m_dataStore);
        
        QString columnName = m_dataModel->headerData(column, Qt::Horizontal).toString();
        m_statusLabel->setText(tr("已应用筛选: %1 %2").arg(columnName).arg(expression));
        LOG_INFO("MainWindow", QString("应用算式筛选: 列=%1, 表达式=%2").arg(column).arg(expression));
    } else {
        QMessageBox::warning(this, tr("筛选错误"), 
            tr("筛选表达式无效: %1\n\n请检查表达式格式。").arg(expression));
        LOG_WARNING("MainWindow", QString("无效的筛选表达式: %1").arg(expression));
    }
}

QString MainWindow::processSimpleQuery(const QString& query)
{
    QString lowerQuery = query.toLower().trimmed();
    
    if (lowerQuery.contains("所有") || lowerQuery.contains("全部") || lowerQuery.contains("清空") || 
        lowerQuery.contains("重置") || lowerQuery.contains("取消筛选") || lowerQuery.contains("清除筛选")) {
        m_dataStore->clearFilter();
        m_dataStore->clearTerminalFilter();
        m_dataStore->clearSubAddressFilter();
        m_dataStore->clearMessageTypeFilter();
        m_dataStore->clearChsttFilter();
        m_dataStore->clearMpuIdFilter();
        m_dataStore->clearTimeRange();
        m_dataModel->setDataStore(m_dataStore);
        return tr("已清除所有筛选条件，显示全部 %1 条数据").arg(m_dataStore->totalCount());
    }
    
    return tr("无法理解查询内容，请尝试更明确的表达，或切换到AI智能分析模式获取更智能的帮助。");
}

void MainWindow::executeAITools(const QJsonArray& toolCalls)
{
    bool hasFilterTool = false;
    bool hasClearFilter = false;
    for (const QJsonValue& call : toolCalls) {
        QString toolName = call.toObject()["name"].toString();
        if (toolName == "query_data" || toolName == "add_filter" || 
            toolName == "apply_filters_batch" || toolName == "clear_filter") {
            hasFilterTool = true;
        }
        if (toolName == "clear_filter") {
            hasClearFilter = true;
        }
    }
    
    bool hadExistingFilter = m_dataStore && m_dataStore->filteredCount() != m_dataStore->totalCount();
    
    if (hasFilterTool) {
        if (!m_progressDialog) {
            m_progressDialog = new ProgressDialog(this);
            connect(m_progressDialog, &ProgressDialog::canceled, this, &MainWindow::onProgressDialogCanceled);
        }
        m_progressDialog->setWindowTitle(tr("正在筛选数据"));
        m_progressDialog->setStatusText(tr("正在应用筛选条件..."));
        m_progressDialog->setProgress(0);
        m_progressDialog->startTimer();
        m_progressDialog->show();
        QCoreApplication::processEvents();
    }
    
    QElapsedTimer filterTimer;
    filterTimer.start();
    
    for (const QJsonValue& call : toolCalls) {
        QJsonObject callObj = call.toObject();
        QString toolName = callObj["name"].toString();
        QJsonObject arguments = callObj["arguments"].toObject();
        
        if (hasFilterTool && m_progressDialog) {
            m_progressDialog->setStatusText(tr("正在执行: %1...").arg(toolName));
            m_progressDialog->setProgress(50);
            QCoreApplication::processEvents();
        }
        
        if (hadExistingFilter && (toolName == "query_data" || toolName == "apply_filters_batch")) {
            m_aiQueryPanel->appendResponse(tr("↻ 已恢复原始数据，重新应用筛选条件..."));
        }
        
        ToolResult result = m_toolExecutor->executeTool(toolName, arguments);
        
        if (hasFilterTool && m_progressDialog) {
            m_progressDialog->setProgress(80);
            QCoreApplication::processEvents();
        }
        
        if (result.success) {
            m_aiQueryPanel->appendResponse(tr("✓ %1").arg(result.message));
        } else {
            m_aiQueryPanel->appendResponse(tr("✗ 执行失败: %1").arg(result.message));
        }
    }
    
    if (hasFilterTool && m_progressDialog) {
        qint64 elapsed = filterTimer.elapsed();
        m_progressDialog->setProgress(100);
        QString elapsedStr = elapsed < 1000 ? tr("%1 毫秒").arg(elapsed) :
                             tr("%1 秒").arg(QString::number(elapsed / 1000.0, 'f', 2));
        m_progressDialog->setStatusText(tr("筛选完成！耗时 %1").arg(elapsedStr));
        m_progressDialog->stopTimer();
        m_progressDialog->setElapsedTime(elapsed);
        QCoreApplication::processEvents();
        QTimer::singleShot(800, this, [this]() {
            if (m_progressDialog) {
                m_progressDialog->hide();
                m_progressDialog->reset();
            }
        });
        m_aiQueryPanel->appendResponse(tr("⏱ 筛选耗时: %1").arg(elapsedStr));
    }
}

void MainWindow::onAIQueryDataRequested(const QJsonObject& filters)
{
    LOG_INFO("MainWindow", "执行数据查询");
    m_dataModel->setDataStore(m_dataStore);
    m_centralTab->setCurrentIndex(0);
    m_statusLabel->setText(tr("数据查询完成"));
}

void MainWindow::onAIGenerateChartRequested(const QString& chartType, const QString& subject, const QString& title)
{
    LOG_INFO("MainWindow", QString("生成图表: type=%1, subject=%2").arg(chartType, subject));
    
    ChartSubject chartSubject = ChartSubject::MessageType;
    if (subject == "message_type") chartSubject = ChartSubject::MessageType;
    else if (subject == "terminal") chartSubject = ChartSubject::Terminal;
    else if (subject == "chstt") chartSubject = ChartSubject::Chstt;
    else if (subject == "time") chartSubject = ChartSubject::Time;
    
    m_chartWidget->setChartSubject(chartSubject);
    m_chartWidget->setChartType(chartType);
    m_chartWidget->setDataStore(m_dataStore);
    m_chartWidget->refreshChart();
    
    m_centralTab->setCurrentIndex(2);
    m_statusLabel->setText(tr("图表生成完成"));
}

void MainWindow::onAIGenerateGanttRequested(const QJsonObject& filters)
{
    LOG_INFO("MainWindow", "生成甘特图");
    m_ganttView->setDataStore(m_dataStore);
    m_ganttView->updateView();
    m_centralTab->setCurrentIndex(1);
    m_statusLabel->setText(tr("甘特图生成完成"));
}

void MainWindow::onAIClearFilterRequested()
{
    LOG_INFO("MainWindow", "清除筛选条件");
    m_dataStore->beginBatchFilterUpdate();
    m_dataStore->clearFilter();
    m_dataStore->clearTerminalFilter();
    m_dataStore->clearSubAddressFilter();
    m_dataStore->clearMessageTypeFilter();
    m_dataStore->clearChsttFilter();
    m_dataStore->clearMpuIdFilter();
    m_dataStore->clearTimeRange();
    m_dataStore->clearPacketLenFilter();
    m_dataStore->clearDateRangeFilter();
    m_dataStore->clearStatusBitFilter();
    m_dataStore->clearExcludeTerminalFilter();
    m_dataStore->clearExcludeMessageTypeFilter();
    m_dataStore->clearWordCountFilter();
    m_dataStore->clearErrorFlagFilter();
    m_dataStore->endBatchFilterUpdate();
    m_dataModel->setDataStore(m_dataStore);
    m_statusLabel->setText(tr("筛选已清除"));
}

void MainWindow::onAISwitchToChartTab()
{
    m_centralTab->setCurrentIndex(2);
}

void MainWindow::onAISwitchToGanttTab()
{
    m_centralTab->setCurrentIndex(1);
}

void MainWindow::onAISwitchToTableTab()
{
    m_centralTab->setCurrentIndex(0);
}

void MainWindow::showDataDetailDialog(int row)
{
    if (row < 0 || row >= m_dataStore->totalCount()) return;
    
    // DataRecord record = m_dataStore->getRecord(row);
    DataRecord record = m_dataModel->getRecord(row);
    
    const SMbiMonPacketMsg& msg = m_dataStore->getMessage(record.msgIndex);
    const SMbiMonPacketHeader& hdr = msg.header;
    
    QDialog* dialog = new QDialog(this);
    dialog->setWindowTitle(tr("数据详细 - 第%1条").arg(row + 1));
    dialog->setMinimumSize(900, 700);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(dialog);
    
    QTabWidget* tabWidget = new QTabWidget(dialog);
    mainLayout->addWidget(tabWidget);
    
    auto createTable = [](QWidget* parent, int rows) {
        QTableWidget* table = new QTableWidget(parent);
        table->setColumnCount(4);
        table->setHorizontalHeaderLabels(QStringList() << tr("属性名称") << tr("16进制") << tr("10进制") << tr("含义"));
        table->setRowCount(rows);
        table->horizontalHeader()->setStretchLastSection(true);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        return table;
    };
    
    auto setItem = [](QTableWidget* table, int row, const QString& name, const QString& hex, const QString& dec, const QString& meaning) {
        table->setItem(row, 0, new QTableWidgetItem(name));
        table->setItem(row, 1, new QTableWidgetItem(hex));
        table->setItem(row, 2, new QTableWidgetItem(dec));
        table->setItem(row, 3, new QTableWidgetItem(meaning));
    };
    
    auto formatHexWithSpace = [](const QByteArray& data) {
        QString result;
        for (int i = 0; i < data.size(); i += 2) {
            if (i > 0) result += " ";
            if (i + 1 < data.size()) {
                result += QString("%1%2").arg((unsigned char)data[i], 2, 16, QChar('0')).arg((unsigned char)data[i+1], 2, 16, QChar('0')).toUpper();
            } else {
                result += QString("%1").arg((unsigned char)data[i], 2, 16, QChar('0')).toUpper();
            }
        }
        return result;
    };
    
    QByteArray headerSource;
    QDataStream headerStream(&headerSource, QIODevice::WriteOnly);
    headerStream.setByteOrder(QDataStream::BigEndian);
    headerStream << hdr.header1;
    headerStream << hdr.header2;
    headerStream << hdr.mpuProduceId;
    headerStream << hdr.packetLen;
    headerStream << hdr.year;
    headerStream << hdr.month;
    headerStream << hdr.day;
    headerStream << hdr.timestamp;
    
    QByteArray allDataSource;
    for (const auto& pkt : msg.packetDatas) {
        QDataStream dataStream(&allDataSource, QIODevice::WriteOnly | QIODevice::Append);
        dataStream.setByteOrder(QDataStream::BigEndian);
        dataStream << pkt.header;
        dataStream << pkt.cmd1;
        dataStream << pkt.cmd2;
        dataStream << pkt.states1;
        dataStream << pkt.states2;
        dataStream << pkt.chstt;
        dataStream << pkt.timestamp;
        dataStream.writeRawData(pkt.datas.constData(), pkt.datas.size());
    }
    
    QByteArray fullSource = headerSource + allDataSource;
    
    QWidget* sourceTab = new QWidget(tabWidget);
    QVBoxLayout* sourceLayout = new QVBoxLayout(sourceTab);
    
    QLabel* sourceInfoLabel = new QLabel(tr("完整数据源码 (%1 字节)").arg(fullSource.size()), sourceTab);
    sourceLayout->addWidget(sourceInfoLabel);
    
    QPlainTextEdit* sourceEdit = new QPlainTextEdit(sourceTab);
    sourceEdit->setReadOnly(true);
    sourceEdit->setFont(QFont("Consolas", 10));
    sourceEdit->setPlainText(formatHexWithSpace(fullSource));
    sourceLayout->addWidget(sourceEdit);
    
    tabWidget->addTab(sourceTab, tr("源码"));
    
    QWidget* headerTab = new QWidget(tabWidget);
    QVBoxLayout* headerLayout = new QVBoxLayout(headerTab);
    QTableWidget* headerTable = createTable(headerTab, 8);
    headerLayout->addWidget(headerTable);
    
    QLabel* headerSourceLabel = new QLabel(tr("包头源码 (%1 字节): %2").arg(headerSource.size()).arg(formatHexWithSpace(headerSource)), headerTab);
    headerSourceLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    headerSourceLabel->setWordWrap(true);
    headerLayout->addWidget(headerSourceLabel);
    
    tabWidget->addTab(headerTab, tr("消息包头 (SMbiMonPacketHeader)"));
    
    setItem(headerTable, 0, "header1", QString("0x%1").arg(hdr.header1, 4, 16, QChar('0')).toUpper(), QString::number(hdr.header1), "固定值：A5 A5");
    setItem(headerTable, 1, "header2", QString("0x%1").arg(hdr.header2, 2, 16, QChar('0')).toUpper(), QString::number(hdr.header2), "固定值：A5");
    setItem(headerTable, 2, "mpuProduceId", QString("0x%1").arg(hdr.mpuProduceId, 2, 16, QChar('0')).toUpper(), QString::number(hdr.mpuProduceId), hdr.mpuProduceId == 1 ? "MPU1" : (hdr.mpuProduceId == 2 ? "MPU2" : "未知"));
    setItem(headerTable, 3, "packetLen", QString("0x%1").arg(hdr.packetLen, 4, 16, QChar('0')).toUpper(), QString::number(hdr.packetLen), "包总长度(字节)");
    setItem(headerTable, 4, "year", QString("0x%1").arg(hdr.year, 4, 16, QChar('0')).toUpper(), QString::number(hdr.year), QString("%1年").arg(hdr.year));
    setItem(headerTable, 5, "month", QString("0x%1").arg(hdr.month, 2, 16, QChar('0')).toUpper(), QString::number(hdr.month), QString("%1月").arg(hdr.month));
    setItem(headerTable, 6, "day", QString("0x%1").arg(hdr.day, 2, 16, QChar('0')).toUpper(), QString::number(hdr.day), QString("%1日").arg(hdr.day));
    
    quint32 ts40us = hdr.timestamp;
    double tsMs = ts40us * 40.0 / 1000.0;
    int totalMs = static_cast<int>(tsMs);
    int hours = totalMs / 3600000;
    int minutes = (totalMs % 3600000) / 60000;
    int seconds = (totalMs % 60000) / 1000;
    int millis = totalMs % 1000;
    QString timeStr = QString("%1:%2:%3.%4").arg(hours, 2, 10, QChar('0')).arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0')).arg(millis, 3, 10, QChar('0'));
    setItem(headerTable, 7, "timestamp", QString("0x%1").arg(ts40us, 8, 16, QChar('0')).toUpper(), QString::number(ts40us), QString("时间戳(40us) = %1").arg(timeStr));
    
    headerTable->resizeColumnsToContents();
    
    QWidget* dataTab = new QWidget(tabWidget);
    QVBoxLayout* dataLayout = new QVBoxLayout(dataTab);
    
    QLabel* dataCountLabel = new QLabel(tr("本消息包含 %1 个数据包").arg(msg.packetDatas.size()), dataTab);
    dataLayout->addWidget(dataCountLabel);
    
    QScrollArea* scrollArea = new QScrollArea(dataTab);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    QWidget* scrollContent = new QWidget();
    QVBoxLayout* scrollLayout = new QVBoxLayout(scrollContent);
    
    for (int dataIdx = 0; dataIdx < msg.packetDatas.size(); ++dataIdx) {
        const SMbiMonPacketData& pkt = msg.packetDatas[dataIdx];
        
        QGroupBox* dataGroup = new QGroupBox(tr("数据包 #%1 (索引: %2)").arg(dataIdx + 1).arg(dataIdx), scrollContent);
        QVBoxLayout* groupLayout = new QVBoxLayout(dataGroup);
        
        bool isCurrentData = (dataIdx == record.dataIndex);
        if (isCurrentData) {
            QPalette pal = dataGroup->palette();
            pal.setColor(QPalette::Window, QColor(230, 255, 230));
            dataGroup->setAutoFillBackground(true);
            dataGroup->setPalette(pal);
            dataGroup->setTitle(tr("数据包 #%1 (索引: %2) - 当前选中").arg(dataIdx + 1).arg(dataIdx));
        }
        
        QTableWidget* dataTable = createTable(dataGroup, 8);
        groupLayout->addWidget(dataTable);
        
        setItem(dataTable, 0, "header", QString("0x%1").arg(pkt.header, 4, 16, QChar('0')).toUpper(), QString::number(pkt.header), "固定值：AA BB");
        setItem(dataTable, 1, "cmd1", QString("0x%1").arg(pkt.cmd1, 4, 16, QChar('0')).toUpper(), QString::number(pkt.cmd1), "命令字1");
        setItem(dataTable, 2, "cmd2", QString("0x%1").arg(pkt.cmd2, 4, 16, QChar('0')).toUpper(), QString::number(pkt.cmd2), "命令字2");
        setItem(dataTable, 3, "states1", QString("0x%1").arg(pkt.states1, 4, 16, QChar('0')).toUpper(), QString::number(pkt.states1), "状态字1");
        setItem(dataTable, 4, "states2", QString("0x%1").arg(pkt.states2, 4, 16, QChar('0')).toUpper(), QString::number(pkt.states2), "状态字2");
        setItem(dataTable, 5, "chstt", QString("0x%1").arg(pkt.chstt, 4, 16, QChar('0')).toUpper(), QString::number(pkt.chstt), pkt.chstt ? "成功" : "失败");
        
        quint32 dataTs40us = pkt.timestamp;
        double dataTsMs = dataTs40us * 40.0 / 1000.0;
        int dataTotalMs = static_cast<int>(dataTsMs);
        int dataHours = dataTotalMs / 3600000;
        int dataMinutes = (dataTotalMs % 3600000) / 60000;
        int dataSeconds = (dataTotalMs % 60000) / 1000;
        int dataMillis = dataTotalMs % 1000;
        QString dataTimeStr = QString("%1:%2:%3.%4").arg(dataHours, 2, 10, QChar('0')).arg(dataMinutes, 2, 10, QChar('0')).arg(dataSeconds, 2, 10, QChar('0')).arg(dataMillis, 3, 10, QChar('0'));
        setItem(dataTable, 6, "timestamp", QString("0x%1").arg(dataTs40us, 8, 16, QChar('0')).toUpper(), QString::number(dataTs40us), QString("时间戳(40us) = %1").arg(dataTimeStr));
        setItem(dataTable, 7, "datas", "-", QString("%1 bytes").arg(pkt.datas.size()), pkt.datas.left(32).toHex(' ').toUpper() + (pkt.datas.size() > 32 ? " ..." : ""));
        
        dataTable->resizeColumnsToContents();
        
        QByteArray pktSource;
        QDataStream pktStream(&pktSource, QIODevice::WriteOnly);
        pktStream.setByteOrder(QDataStream::BigEndian);
        pktStream << pkt.header;
        pktStream << pkt.cmd1;
        pktStream << pkt.cmd2;
        pktStream << pkt.states1;
        pktStream << pkt.states2;
        pktStream << pkt.chstt;
        pktStream << pkt.timestamp;
        pktStream.writeRawData(pkt.datas.constData(), pkt.datas.size());
        
        QLabel* pktSourceLabel = new QLabel(tr("源码 (%1 字节): %2").arg(pktSource.size()).arg(formatHexWithSpace(pktSource)), dataGroup);
        pktSourceLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        pktSourceLabel->setWordWrap(true);
        groupLayout->addWidget(pktSourceLabel);
        
        scrollLayout->addWidget(dataGroup);
    }
    
    scrollLayout->addStretch();
    scrollArea->setWidget(scrollContent);
    dataLayout->addWidget(scrollArea);
    
    tabWidget->addTab(dataTab, tr("数据包 (SMbiMonPacketData)"));
    
    QWidget* cmdTab = new QWidget(tabWidget);
    QVBoxLayout* cmdLayout = new QVBoxLayout(cmdTab);
    
    const SMbiMonPacketData& currentPkt = record.packetData;
    
    QGroupBox* cmd1Group = new QGroupBox(tr("CMD1 命令字1 (0x%1)").arg(currentPkt.cmd1, 4, 16, QChar('0')).toUpper(), cmdTab);
    QVBoxLayout* cmd1Layout = new QVBoxLayout(cmd1Group);
    QTableWidget* cmd1Table = createTable(cmd1Group, 4);
    cmd1Layout->addWidget(cmd1Table);
    cmdLayout->addWidget(cmd1Group);
    
    CMD cmd1;
    memcpy(&cmd1, &currentPkt.cmd1, sizeof(CMD));
    setItem(cmd1Table, 0, "zhongduandizhi", QString("0x%1").arg(cmd1.zhongduandizhi, 2, 16, QChar('0')).toUpper(), QString::number(cmd1.zhongduandizhi), QString("终端地址 (bit15-11) %1").arg(cmd1.zhongduandizhi == 31 ? "= 广播" : ""));
    setItem(cmd1Table, 1, "T_R", QString("0x%1").arg(cmd1.T_R, 1, 16, QChar('0')).toUpper(), QString::number(cmd1.T_R), cmd1.T_R ? "RT→BC (bit10=1)" : "BC→RT (bit10=0)");
    setItem(cmd1Table, 2, "zidizhi", QString("0x%1").arg(cmd1.zidizhi, 2, 16, QChar('0')).toUpper(), QString::number(cmd1.zidizhi), QString("子地址 (bit9-5) %1").arg(cmd1.zidizhi == 0 || cmd1.zidizhi == 31 ? "= datas长度2字节" : ""));
    setItem(cmd1Table, 3, "sjzjs_fsdm", QString("0x%1").arg(cmd1.sjzjs_fsdm, 2, 16, QChar('0')).toUpper(), QString::number(cmd1.sjzjs_fsdm), QString("数据计数/发送码 (bit4-0) %1").arg(cmd1.sjzjs_fsdm == 0 ? "= datas长度64字节" : QString("= datas长度%1字节").arg(cmd1.sjzjs_fsdm * 2)));
    cmd1Table->resizeColumnsToContents();
    
    QGroupBox* cmd2Group = new QGroupBox(tr("CMD2 命令字2 (0x%1)").arg(currentPkt.cmd2, 4, 16, QChar('0')).toUpper(), cmdTab);
    QVBoxLayout* cmd2Layout = new QVBoxLayout(cmd2Group);
    QTableWidget* cmd2Table = createTable(cmd2Group, 4);
    cmd2Layout->addWidget(cmd2Table);
    cmdLayout->addWidget(cmd2Group);
    
    CMD cmd2;
    memcpy(&cmd2, &currentPkt.cmd2, sizeof(CMD));
    setItem(cmd2Table, 0, "zhongduandizhi", QString("0x%1").arg(cmd2.zhongduandizhi, 2, 16, QChar('0')).toUpper(), QString::number(cmd2.zhongduandizhi), QString("终端地址 (bit15-11) %1").arg(cmd2.zhongduandizhi == 31 ? "= 广播" : ""));
    setItem(cmd2Table, 1, "T_R", QString("0x%1").arg(cmd2.T_R, 1, 16, QChar('0')).toUpper(), QString::number(cmd2.T_R), cmd2.T_R ? "RT→BC (bit10=1)" : "BC→RT (bit10=0)");
    setItem(cmd2Table, 2, "zidizhi", QString("0x%1").arg(cmd2.zidizhi, 2, 16, QChar('0')).toUpper(), QString::number(cmd2.zidizhi), QString("子地址 (bit9-5) %1").arg(cmd2.zidizhi == 0 || cmd2.zidizhi == 31 ? "= datas长度2字节" : ""));
    setItem(cmd2Table, 3, "sjzjs_fsdm", QString("0x%1").arg(cmd2.sjzjs_fsdm, 2, 16, QChar('0')).toUpper(), QString::number(cmd2.sjzjs_fsdm), QString("数据计数/发送码 (bit4-0) %1").arg(cmd2.sjzjs_fsdm == 0 ? "= datas长度64字节" : QString("= datas长度%1字节").arg(cmd2.sjzjs_fsdm * 2)));
    cmd2Table->resizeColumnsToContents();
    
    tabWidget->addTab(cmdTab, tr("命令字 (CMD)"));
    
    QWidget* metaTab = new QWidget(tabWidget);
    QVBoxLayout* metaLayout = new QVBoxLayout(metaTab);
    QTableWidget* metaTable = createTable(metaTab, 5);
    metaLayout->addWidget(metaTable);
    tabWidget->addTab(metaTab, tr("元信息"));
    
    setItem(metaTable, 0, "msgIndex", "-", QString::number(record.msgIndex), "消息索引");
    setItem(metaTable, 1, "dataIndex", "-", QString::number(record.dataIndex), "数据索引");
    setItem(metaTable, 2, "rowIndex", "-", QString::number(record.rowIndex), "行索引");
    setItem(metaTable, 3, "messageType", "-", "-", messageTypeToString(record.messageType));
    setItem(metaTable, 4, "timestampMs", "-", QString::number(record.timestampMs, 'f', 3), "时间戳(毫秒)");
    metaTable->resizeColumnsToContents();
    
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    connect(buttonBox, &QDialogButtonBox::rejected, dialog, &QDialog::close);
    mainLayout->addWidget(buttonBox);
    
    dialog->exec();
    dialog->deleteLater();
}

void MainWindow::onParseTimerTimeout()
{
    qint64 elapsedMs = m_elapsedTimer.elapsed();
    int hours = elapsedMs / 3600000;
    int minutes = (elapsedMs % 3600000) / 60000;
    int seconds = (elapsedMs % 60000) / 1000;
    
    m_timerLabel->setText(QString(" %1:%2:%3 ")
                          .arg(hours, 2, 10, QChar('0'))
                          .arg(minutes, 2, 10, QChar('0'))
                          .arg(seconds, 2, 10, QChar('0')));
}

void MainWindow::onModelQueryStarted()
{
    LOG_INFO("MainWindow", "AI查询开始");
    m_elapsedTimer.start();
    m_aiQueryPanel->setLoading(true);
    m_statusLabel->setText(tr("正在等待AI响应..."));
}

void MainWindow::onModelQueryFinished(const ModelResponse& response)
{
    LOG_INFO("MainWindow", QString("AI查询完成, 成功=%1").arg(response.success));
    m_aiQueryPanel->setLoading(false);
    
    qint64 aiElapsed = m_elapsedTimer.elapsed();
    QString elapsedStr = aiElapsed < 1000 ? tr("%1 毫秒").arg(aiElapsed) :
                         tr("%1 秒").arg(QString::number(aiElapsed / 1000.0, 'f', 2));
    
    if (response.success) {
        QString content = response.content;
        
        QJsonArray toolCalls = extractToolCalls(content);
        if (!toolCalls.isEmpty()) {
            m_aiQueryPanel->appendResponse(tr("⏱ AI响应耗时: %1").arg(elapsedStr));
            executeAITools(toolCalls);
        } else {
            m_aiQueryPanel->appendResponse(content);
            m_aiQueryPanel->appendResponse(tr("\n⏱ 响应耗时: %1").arg(elapsedStr));
        }
        
        m_statusLabel->setText(tr("AI响应完成 (耗时 %1)").arg(elapsedStr));
    } else {
        m_aiQueryPanel->appendResponse(tr("错误: %1").arg(response.error));
        m_statusLabel->setText(tr("AI响应失败"));
    }
}

void MainWindow::onModelQueryError(const QString& error)
{
    LOG_ERROR("MainWindow", QString("AI查询错误: %1").arg(error));
    m_aiQueryPanel->setLoading(false);
    m_aiQueryPanel->appendResponse(tr("错误: %1").arg(error));
    m_statusLabel->setText(tr("查询失败"));
    
    QMessageBox::warning(this, tr("AI连接失败"), 
        QString(tr("无法连接到AI服务：\n%1\n\n请检查：\n1. 网络连接是否正常\n2. API密钥是否正确配置\n3. API服务是否可用")).arg(error));
}

QJsonArray MainWindow::extractToolCalls(const QString& content)
{
    QJsonArray toolCalls;
    
    QString cleanedContent = content;
    
    QRegularExpression codeBlockRegex(R"(```json\s*([\s\S]*?)\s*```)");
    QRegularExpressionMatch codeBlockMatch = codeBlockRegex.match(cleanedContent);
    if (codeBlockMatch.hasMatch()) {
        cleanedContent = codeBlockMatch.captured(1).trimmed();
    }
    
    QRegularExpression codeBlockRegex2(R"(```\s*([\s\S]*?)\s*```)");
    QRegularExpressionMatch codeBlockMatch2 = codeBlockRegex2.match(cleanedContent);
    if (codeBlockMatch2.hasMatch()) {
        QString extracted = codeBlockMatch2.captured(1).trimmed();
        if (extracted.startsWith("{")) {
            cleanedContent = extracted;
        }
    }
    
    int braceCount = 0;
    int startIndex = -1;
    
    for (int i = 0; i < cleanedContent.length(); ++i) {
        QChar c = cleanedContent[i];
        if (c == '{') {
            if (braceCount == 0) {
                startIndex = i;
            }
            braceCount++;
        } else if (c == '}') {
            braceCount--;
            if (braceCount == 0 && startIndex >= 0) {
                QString jsonStr = cleanedContent.mid(startIndex, i - startIndex + 1);
                
                QJsonParseError error;
                QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &error);
                if (error.error == QJsonParseError::NoError && doc.isObject()) {
                    QJsonObject obj = doc.object();
                    if (obj.contains("name")) {
                        QJsonObject toolCall;
                        toolCall["name"] = obj["name"].toString();
                        if (obj.contains("arguments")) {
                            toolCall["arguments"] = obj["arguments"].toObject();
                        } else {
                            toolCall["arguments"] = QJsonObject();
                        }
                        toolCalls.append(toolCall);
                        LOG_INFO("MainWindow", QString("提取到工具调用: %1, 参数: %2")
                            .arg(toolCall["name"].toString())
                            .arg(QString::fromUtf8(QJsonDocument(toolCall["arguments"].toObject()).toJson())));
                    }
                }
                startIndex = -1;
            }
        }
    }
    
    return toolCalls;
}

void MainWindow::onGenerateReport()
{
    if (m_dataStore->totalCount() == 0) {
        QMessageBox::warning(this, tr("无法生成报告"), tr("没有数据可分析，请先导入数据文件。"));
        return;
    }
    
    QString defaultFormat = ConfigManager::instance()->getReportFormat();
    QString filterStr;
    int defaultFilterIndex = 0;
    
    if (defaultFormat == "pdf") {
        filterStr = tr("PDF报告 (*.pdf);;HTML报告 (*.html);;DOCX报告 (*.docx)");
        defaultFilterIndex = 0;
    } else if (defaultFormat == "docx") {
        filterStr = tr("DOCX报告 (*.docx);;HTML报告 (*.html);;PDF报告 (*.pdf)");
        defaultFilterIndex = 0;
    } else {
        filterStr = tr("HTML报告 (*.html);;PDF报告 (*.pdf);;DOCX报告 (*.docx)");
        defaultFilterIndex = 0;
    }
    
    QString defaultPath = QDir(QDir::homePath()).filePath("1553B总线数据分析报告");
    QString selectedFilter;
    QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("生成智能分析报告"),
        defaultPath,
        filterStr,
        &selectedFilter
    );
    
    if (filePath.isEmpty()) {
        return;
    }
    
    QString format = defaultFormat;
    if (selectedFilter.contains("pdf", Qt::CaseInsensitive)) {
        format = "pdf";
    } else if (selectedFilter.contains("docx", Qt::CaseInsensitive)) {
        format = "docx";
    } else if (selectedFilter.contains("html", Qt::CaseInsensitive)) {
        format = "html";
    }
    
    QString currentModelId = m_aiQueryPanel->currentModelId();
    ModelAdapter* provider = ModelManager::instance()->getProvider(currentModelId);
    
    if (!provider) {
        QMessageBox::warning(this, tr("无法生成报告"), 
            tr("未配置AI模型，无法进行深度分析。\n请先在设置中配置AI模型。"));
        return;
    }
    
    m_reportGenerator->setModelProvider(provider);
    
    if (m_reportProgressDialog) {
        m_reportProgressDialog->deleteLater();
        m_reportProgressDialog = nullptr;
    }
    
    m_reportProgressDialog = new QProgressDialog(tr("正在生成智能分析报告..."), tr("取消"), 0, 100, this);
    m_reportProgressDialog->setWindowTitle(tr("报告生成中"));
    m_reportProgressDialog->setWindowModality(Qt::WindowModal);
    m_reportProgressDialog->setMinimumDuration(0);
    m_reportProgressDialog->setAutoReset(false);
    m_reportProgressDialog->setAutoClose(false);
    m_reportProgressDialog->setValue(0);
    m_reportProgressDialog->setLabelText(tr("初始化..."));
    
    connect(m_reportProgressDialog, &QProgressDialog::canceled, this, [this]() {
        m_statusLabel->setText(tr("报告生成已取消"));
        LOG_INFO("MainWindow", "用户取消了报告生成");
        if (m_reportGenerator) {
            m_reportGenerator->cancelReport();
        }
    });
    
    m_statusLabel->setText(tr("正在生成智能分析报告..."));
    LOG_INFO("MainWindow", QString("开始生成智能分析报告: %1").arg(filePath));
    
    m_reportGenerator->generateReportAsync(filePath, format);
}

void MainWindow::onReportProgress(int percent, const QString& stage, double elapsedSeconds)
{
    if (m_reportProgressDialog) {
        m_reportProgressDialog->setValue(percent);
        
        QString timeStr;
        if (elapsedSeconds > 0) {
            timeStr = QString(" (%1秒)").arg(elapsedSeconds, 0, 'f', 1);
        }
        
        m_reportProgressDialog->setLabelText(tr("%1%2").arg(stage).arg(timeStr));
    }
    
    m_statusLabel->setText(tr("报告生成: %1 (%2%)").arg(stage).arg(percent));
}

void MainWindow::onReportFinished(bool success, const QString& filePath)
{
    if (m_reportProgressDialog) {
        m_reportProgressDialog->close();
        m_reportProgressDialog->deleteLater();
        m_reportProgressDialog = nullptr;
    }
    
    if (success) {
        m_statusLabel->setText(tr("报告生成成功"));
        LOG_INFO("MainWindow", QString("报告生成成功: %1").arg(filePath));
        
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(tr("报告生成成功"));
        msgBox.setText(tr("智能分析报告已生成完成！"));
        msgBox.setInformativeText(tr("文件路径: %1").arg(filePath));
        msgBox.setStandardButtons(QMessageBox::Open | QMessageBox::Ok);
        msgBox.setDefaultButton(QMessageBox::Open);
        
        int ret = msgBox.exec();
        if (ret == QMessageBox::Open) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
        }
    } else {
        m_statusLabel->setText(tr("报告生成失败"));
        QString error = m_reportGenerator->lastError();
        LOG_ERROR("MainWindow", QString("报告生成失败: %1").arg(error));
        QMessageBox::warning(this, tr("报告生成失败"), tr("无法生成报告: %1").arg(error));
    }
}

void MainWindow::onGenerateTestData()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("选择测试数据保存目录"),
                                                     QDir::currentPath(),
                                                     QFileDialog::ShowDirsOnly);
    if (dir.isEmpty()) {
        return;
    }
    
    QDialog* dialog = new QDialog(this);
    dialog->setWindowTitle(tr("生成测试数据"));
    dialog->setMinimumWidth(500);
    
    QVBoxLayout* layout = new QVBoxLayout(dialog);
    
    QGroupBox* byteOrderGroup = new QGroupBox(tr("字节序设置"));
    QHBoxLayout* byteOrderLayout = new QHBoxLayout(byteOrderGroup);
    QRadioButton* littleEndianRadio = new QRadioButton(tr("小端序"));
    QRadioButton* bigEndianRadio = new QRadioButton(tr("大端序"));
    littleEndianRadio->setChecked(true);
    byteOrderLayout->addWidget(littleEndianRadio);
    byteOrderLayout->addWidget(bigEndianRadio);
    layout->addWidget(byteOrderGroup);
    
    QGroupBox* dateRangeGroup = new QGroupBox(tr("日期范围设置"));
    QVBoxLayout* dateRangeLayout = new QVBoxLayout(dateRangeGroup);
    
    QHBoxLayout* startDateLayout = new QHBoxLayout();
    startDateLayout->addWidget(new QLabel(tr("起始日期:")));
    QDateEdit* startDateEdit = new QDateEdit(QDate::currentDate());
    startDateEdit->setCalendarPopup(true);
    startDateEdit->setDisplayFormat("yyyy-MM-dd");
    startDateLayout->addWidget(startDateEdit);
    dateRangeLayout->addLayout(startDateLayout);
    
    QHBoxLayout* endDateLayout = new QHBoxLayout();
    endDateLayout->addWidget(new QLabel(tr("结束日期:")));
    QDateEdit* endDateEdit = new QDateEdit(QDate::currentDate().addDays(1));
    endDateEdit->setCalendarPopup(true);
    endDateEdit->setDisplayFormat("yyyy-MM-dd");
    endDateLayout->addWidget(endDateEdit);
    dateRangeLayout->addLayout(endDateLayout);
    
    layout->addWidget(dateRangeGroup);
    
    QGroupBox* timeRangeGroup = new QGroupBox(tr("时间范围设置"));
    QVBoxLayout* timeRangeLayout = new QVBoxLayout(timeRangeGroup);
    
    QHBoxLayout* startTimeLayout = new QHBoxLayout();
    startTimeLayout->addWidget(new QLabel(tr("起始时间:")));
    QTimeEdit* startTimeEdit = new QTimeEdit(QTime(0, 0, 0));
    startTimeEdit->setDisplayFormat("HH:mm:ss");
    startTimeLayout->addWidget(startTimeEdit);
    timeRangeLayout->addLayout(startTimeLayout);
    
    QHBoxLayout* endTimeLayout = new QHBoxLayout();
    endTimeLayout->addWidget(new QLabel(tr("结束时间:")));
    QTimeEdit* endTimeEdit = new QTimeEdit(QTime(23, 59, 59));
    endTimeEdit->setDisplayFormat("HH:mm:ss");
    endTimeLayout->addWidget(endTimeEdit);
    timeRangeLayout->addLayout(endTimeLayout);
    
    layout->addWidget(timeRangeGroup);
    
    QLabel* label = new QLabel(tr("选择要生成的测试数据规模："));
    layout->addWidget(label);
    
    QCheckBox* check10k = new QCheckBox(tr("1万条数据"));
    QCheckBox* check50k = new QCheckBox(tr("5万条数据"));
    QCheckBox* check200k = new QCheckBox(tr("20万条数据"));
    QCheckBox* check500k = new QCheckBox(tr("50万条数据"));
    QCheckBox* check1m = new QCheckBox(tr("100万条数据"));
    
    check10k->setChecked(true);
    check50k->setChecked(true);
    check200k->setChecked(true);
    check500k->setChecked(true);
    check1m->setChecked(true);
    
    layout->addWidget(check10k);
    layout->addWidget(check50k);
    layout->addWidget(check200k);
    layout->addWidget(check500k);
    layout->addWidget(check1m);
    
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttonBox);
    
    connect(buttonBox, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    
    if (dialog->exec() == QDialog::Accepted) {
        QVector<int> counts;
        if (check10k->isChecked()) counts.append(10000);
        if (check50k->isChecked()) counts.append(50000);
        if (check200k->isChecked()) counts.append(200000);
        if (check500k->isChecked()) counts.append(500000);
        if (check1m->isChecked()) counts.append(1000000);
        
        if (counts.isEmpty()) {
            QMessageBox::warning(this, tr("提示"), tr("请至少选择一个数据规模"));
            return;
        }
        
        QString byteOrder = littleEndianRadio->isChecked() ? "little" : "big";
        QDate startDate = startDateEdit->date();
        QDate endDate = endDateEdit->date();
        QTime startTimeVal = startTimeEdit->time();
        QTime endTimeVal = endTimeEdit->time();
        
        QProgressDialog* progress = new QProgressDialog(tr("正在生成测试数据..."), tr("取消"), 0, counts.size(), this);
        progress->setWindowModality(Qt::WindowModal);
        progress->setMinimumDuration(0);
        progress->setValue(0);
        
        int successCount = 0;
        for (int i = 0; i < counts.size(); i++) {
            if (progress->wasCanceled()) {
                break;
            }
            
            progress->setLabelText(tr("正在生成 %1 万条数据...").arg(counts[i] / 10000));
            progress->setValue(i);
            QCoreApplication::processEvents();
            
            QString filename = QString("%1/test_data_%2W.bin").arg(dir).arg(counts[i] / 10000);
            if (m_testDataGenerator->generateTestDataFile(filename, counts[i], byteOrder, startDate, endDate, startTimeVal, endTimeVal)) {
                successCount++;
            }
        }
        
        progress->setValue(counts.size());
        progress->deleteLater();
        
        if (successCount == counts.size()) {
            QMessageBox::information(this, tr("完成"), tr("成功生成 %1 个测试数据文件").arg(successCount));
        } else {
            QMessageBox::warning(this, tr("部分完成"), tr("成功生成 %1/%2 个测试数据文件").arg(successCount).arg(counts.size()));
        }
    }
    
    dialog->deleteLater();
}

void MainWindow::onViewHex()
{
    QString filePath = QFileDialog::getOpenFileName(this, tr("选择要查看的文件"),
                                                     QDir::currentPath(),
                                                     tr("二进制文件 (*.bin);;所有文件 (*.*)"));
    if (filePath.isEmpty()) {
        return;
    }
    
    HexViewDialog* dialog = new HexViewDialog(filePath, this);
    dialog->exec();
    dialog->deleteLater();
}

/**
 * @brief 筛选进度更新槽
 * @param percent 进度百分比（0-100）
 * @param processed 已处理记录数
 * @param total 总记录数
 */
void MainWindow::onFilterProgress(int percent, int processed, int total)
{
    if (m_progressDialog) {
        m_progressDialog->setProgress(percent);
        m_progressDialog->setStatusText(tr("正在筛选... %1/%2 (%3%)")
            .arg(processed)
            .arg(total)
            .arg(percent));
    }
}

/**
 * @brief 分页变化槽
 * @param currentPage 当前页码（从0开始）
 * @param totalPages 总页数
 * @param filteredCount 筛选后的记录总数
 */
void MainWindow::onPageChanged(int currentPage, int totalPages, int filteredCount)
{
    // 更新分页控件
    m_paginationWidget->updatePagination(
        currentPage,
        totalPages,
        filteredCount,
        m_dataStore->totalCount()
    );
    
    // 更新状态栏
    m_countLabel->setText(tr("数据: %1 条").arg(filteredCount));
    
    // 刷新数据模型
    m_dataModel->setDataStore(m_dataStore);
}

/**
 * @brief 每页条数变化槽
 * @param size 新的每页条数
 */
void MainWindow::onPageSizeChanged(int size)
{
    m_dataStore->setPageSize(size);
    
    // 更新分页控件
    m_paginationWidget->updatePagination(
        m_dataStore->currentPage(),
        m_dataStore->totalPages(),
        m_dataStore->filteredCount(),
        m_dataStore->totalCount()
    );
    
    // 刷新数据模型
    m_dataModel->setDataStore(m_dataStore);
    
    LOG_INFO("MainWindow", QString("每页条数已更改为: %1").arg(size));
}

/**
 * @brief 进度对话框取消槽
 */
void MainWindow::onProgressDialogCanceled()
{
    LOG_INFO("MainWindow", "用户取消了操作");
    
    m_loadCanceled.store(true);
    m_dataStore->cancelAsyncFilter();
    
    if (m_parser) {
        QMetaObject::invokeMethod(m_parser, "cancel", Qt::QueuedConnection);
    }
    DatabaseManager::instance()->cancelImport();
    
    m_dataStore->clear();
    m_dataModel->setDataStore(m_dataStore);
    m_countLabel->setText(tr("数据: 0 条"));
    
    if (!m_currentFile.isEmpty() && m_fileListPanel) {
        m_fileListPanel->removeFile(m_currentFile);
    }
    
    m_statusLabel->setText(tr("操作已取消"));
}
