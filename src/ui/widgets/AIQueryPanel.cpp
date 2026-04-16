/**
 * @file AIQueryPanel.cpp
 * @brief AI智能查询面板类实现
 * 
 * 本文件实现了AIQueryPanel类的所有方法，包括：
 * - 界面初始化和布局
 * - 模式切换处理
 * - 消息发送和接收
 * - 聊天历史管理
 * 
 * @author 1553BTools
 * @date 2024
 */

#include "AIQueryPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDateTime>
#include <QKeyEvent>

/**
 * @brief 构造函数，初始化AI查询面板
 * @param parent 父窗口指针
 */
AIQueryPanel::AIQueryPanel(QWidget *parent)
    : QDockWidget(tr("AI智能模块"), parent)
    , m_container(nullptr)
    , m_stackedWidget(nullptr)
    , m_modeSelector(nullptr)
    , m_modeDescription(nullptr)
    , m_chatPage(nullptr)
    , m_chatList(nullptr)
    , m_chatInput(nullptr)
    , m_analysisPage(nullptr)
    , m_queryEdit(nullptr)
    , m_responseEdit(nullptr)
    , m_submitBtn(nullptr)
    , m_clearBtn(nullptr)
    , m_cancelBtn(nullptr)
    , m_statusLabel(nullptr)
    , m_loading(false)
    , m_currentMode(AIMode::Chat)
{
    setObjectName("AIQueryPanel");
    setupUI();
}

/**
 * @brief 析构函数
 */
AIQueryPanel::~AIQueryPanel()
{
}

/**
 * @brief 设置界面布局
 * 
 * 创建以下UI元素：
 * - 模式选择下拉框
 * - 模式描述标签
 * - 堆栈窗口（聊天页面/分析页面）
 * - 操作按钮（发送、取消、清空）
 * - 状态标签
 */
void AIQueryPanel::setupUI()
{
    m_container = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(m_container);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    
    // 创建模式选择区域
    QHBoxLayout* modeLayout = new QHBoxLayout();
    modeLayout->addWidget(new QLabel(tr("模式:")));
    
    m_modeSelector = new QComboBox(this);
    m_modeSelector->addItem(tr("💬 聊天模式"), static_cast<int>(AIMode::Chat));
    m_modeSelector->addItem(tr("📊 智能分析模式"), static_cast<int>(AIMode::Analysis));
    modeLayout->addWidget(m_modeSelector);
    
    modeLayout->addSpacing(20);
    
    modeLayout->addWidget(new QLabel(tr("模型:")));
    m_modelSelector = new QComboBox(this);
    m_modelSelector->setMinimumWidth(150);
    modeLayout->addWidget(m_modelSelector);
    
    modeLayout->addStretch();
    mainLayout->addLayout(modeLayout);
    
    // 创建模式描述标签
    m_modeDescription = new QLabel(this);
    m_modeDescription->setWordWrap(true);
    m_modeDescription->setStyleSheet("color: #666; font-size: 11px; padding: 5px; background: #f5f5f5; border-radius: 3px;");
    mainLayout->addWidget(m_modeDescription);
    
    // 创建堆栈窗口
    m_stackedWidget = new QStackedWidget(this);
    mainLayout->addWidget(m_stackedWidget, 1);
    
    // 设置两种模式的界面
    setupChatMode();
    setupAnalysisMode();
    
    // 创建按钮区域
    QHBoxLayout* btnLayout = new QHBoxLayout();
    m_submitBtn = new QPushButton(tr("发送"), this);
    m_cancelBtn = new QPushButton(tr("取消"), this);
    m_clearBtn = new QPushButton(tr("清空"), this);
    m_resetDataBtn = new QPushButton(tr("🔄 恢复原数据"), this);
    m_resetDataBtn->setStyleSheet("QPushButton { background-color: #3498db; color: white; border: none; padding: 5px 10px; border-radius: 3px; } QPushButton:hover { background-color: #2980b9; }");
    btnLayout->addWidget(m_submitBtn);
    btnLayout->addWidget(m_cancelBtn);
    btnLayout->addWidget(m_clearBtn);
    btnLayout->addWidget(m_resetDataBtn);
    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);
    
    // 创建状态标签
    m_statusLabel = new QLabel(tr("就绪"), this);
    mainLayout->addWidget(m_statusLabel);
    
    setWidget(m_container);
    
    // 初始状态：取消按钮禁用
    m_cancelBtn->setEnabled(false);
    
    // 连接信号和槽
    connect(m_submitBtn, &QPushButton::clicked, this, &AIQueryPanel::onSubmitClicked);
    connect(m_clearBtn, &QPushButton::clicked, this, &AIQueryPanel::onClearClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &AIQueryPanel::onCancelClicked);
    connect(m_resetDataBtn, &QPushButton::clicked, this, &AIQueryPanel::onResetDataClicked);
    connect(m_modeSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AIQueryPanel::onModeChanged);
    connect(m_modelSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        QString modelId = m_modelSelector->itemData(index).toString();
        emit modelChanged(modelId);
    });
    
    // 初始化模式
    onModeChanged(0);
}

/**
 * @brief 设置聊天模式界面
 * 
 * 创建聊天消息列表和输入框
 */
void AIQueryPanel::setupChatMode()
{
    m_chatPage = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(m_chatPage);
    layout->setContentsMargins(0, 0, 0, 0);
    
    // 创建聊天消息列表
    m_chatList = new QListWidget(this);
    m_chatList->setStyleSheet(
        "QListWidget { border: 1px solid #ddd; border-radius: 5px; background: #fff; }"
        "QListWidget::item { padding: 8px; border-bottom: 1px solid #eee; }"
        "QListWidget::item:selected { background: #e3f2fd; }"
    );
    m_chatList->setSelectionMode(QAbstractItemView::NoSelection);
    m_chatList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    layout->addWidget(m_chatList, 1);
    
    // 创建聊天输入框
    m_chatInput = new QTextEdit(this);
    m_chatInput->setPlaceholderText(tr("输入消息与AI对话..."));
    m_chatInput->setMaximumHeight(80);
    layout->addWidget(m_chatInput);
    
    m_stackedWidget->addWidget(m_chatPage);
}

/**
 * @brief 设置分析模式界面
 * 
 * 创建查询输入框和响应显示区域
 */
void AIQueryPanel::setupAnalysisMode()
{
    m_analysisPage = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(m_analysisPage);
    layout->setContentsMargins(0, 0, 0, 0);
    
    // 创建查询输入区域
    layout->addWidget(new QLabel(tr("输入自然语言指令:")));
    
    m_queryEdit = new QTextEdit(this);
    m_queryEdit->setPlaceholderText(tr("例如：\n• 显示终端地址为5的所有数据\n• 画一个饼图展示消息类型分布\n• 统计各终端的数据量\n• 生成甘特图"));
    m_queryEdit->setMaximumHeight(100);
    layout->addWidget(m_queryEdit);
    
    // 创建响应显示区域
    layout->addWidget(new QLabel(tr("AI响应:")));
    
    m_responseEdit = new QTextEdit(this);
    m_responseEdit->setReadOnly(true);
    m_responseEdit->setPlaceholderText(tr("AI分析结果将显示在这里..."));
    layout->addWidget(m_responseEdit, 1);
    
    m_stackedWidget->addWidget(m_analysisPage);
}

/**
 * @brief 更新输入框占位文本
 * 
 * 根据当前模式更新提示文本
 */
void AIQueryPanel::updatePlaceholder()
{
    if (m_currentMode == AIMode::Chat) {
        m_modeDescription->setText(tr("💬 聊天模式：与AI进行自由对话，可以询问1553B协议相关问题、技术细节等。"));
        m_chatInput->setPlaceholderText(tr("输入消息与AI对话..."));
    } else {
        m_modeDescription->setText(tr("📊 智能分析模式：用自然语言查询数据、生成图表和甘特图。AI会理解您的意图并执行相应操作。"));
        m_queryEdit->setPlaceholderText(tr("例如：\n• 显示终端地址为5的所有数据\n• 画一个饼图展示消息类型分布\n• 统计各终端的数据量\n• 生成甘特图"));
    }
}

/**
 * @brief 模式选择变化槽
 * @param index 选择的模式索引
 * 
 * 切换堆栈窗口页面并更新界面
 */
void AIQueryPanel::onModeChanged(int index)
{
    m_currentMode = static_cast<AIMode>(m_modeSelector->itemData(index).toInt());
    
    // 切换堆栈窗口页面
    if (m_currentMode == AIMode::Chat) {
        m_stackedWidget->setCurrentWidget(m_chatPage);
    } else {
        m_stackedWidget->setCurrentWidget(m_analysisPage);
    }
    
    updatePlaceholder();
    emit modeChanged(m_currentMode);
}

/**
 * @brief 添加消息到聊天列表
 * @param role 消息角色
 * @param content 消息内容
 * @param timestamp 时间戳
 * 
 * 根据角色设置不同的显示样式
 */
void AIQueryPanel::addMessageToList(ChatMessage::Role role, const QString& content, const QString& timestamp)
{
    QListWidgetItem* item = new QListWidgetItem(m_chatList);
    
    QString prefix;
    QString style;
    
    // 根据角色设置前缀和样式
    if (role == ChatMessage::User) {
        prefix = tr("👤 我");
        style = "background: #e3f2fd; color: #1565c0;";
    } else if (role == ChatMessage::Assistant) {
        prefix = tr("🤖 AI");
        style = "background: #f5f5f5; color: #333;";
    } else {
        prefix = tr("⚙️ 系统");
        style = "background: #fff3e0; color: #e65100;";
    }
    
    // 格式化显示文本
    QString displayText = QString("%1 [%2]\n%3").arg(prefix, timestamp, content);
    item->setText(displayText);
    item->setData(Qt::UserRole, static_cast<int>(role));
    
    m_chatList->addItem(item);
    m_chatList->scrollToBottom();
}

/**
 * @brief 获取当前模式
 * @return 当前AI模式
 */
AIMode AIQueryPanel::currentMode() const
{
    return m_currentMode;
}

/**
 * @brief 设置AI模式
 * @param mode 要设置的模式
 */
void AIQueryPanel::setMode(AIMode mode)
{
    int index = (mode == AIMode::Chat) ? 0 : 1;
    m_modeSelector->setCurrentIndex(index);
}

/**
 * @brief 添加聊天消息
 * @param role 消息角色
 * @param content 消息内容
 * 
 * 添加到聊天历史并显示在列表中
 */
void AIQueryPanel::addChatMessage(ChatMessage::Role role, const QString& content)
{
    ChatMessage msg;
    msg.role = role;
    msg.content = content;
    msg.timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    m_chatHistory.append(msg);
    
    addMessageToList(role, content, msg.timestamp);
}

/**
 * @brief 清除聊天历史
 */
void AIQueryPanel::clearChatHistory()
{
    m_chatHistory.clear();
    m_chatList->clear();
}

/**
 * @brief 获取聊天历史
 * @return 聊天消息列表
 */
QList<ChatMessage> AIQueryPanel::getChatHistory() const
{
    return m_chatHistory;
}

/**
 * @brief 设置查询文本
 * @param text 要设置的文本
 */
void AIQueryPanel::setQueryText(const QString& text)
{
    if (m_currentMode == AIMode::Chat) {
        m_chatInput->setText(text);
    } else {
        m_queryEdit->setText(text);
    }
}

/**
 * @brief 获取当前查询文本
 * @return 当前输入框中的文本
 */
QString AIQueryPanel::getQueryText() const
{
    if (m_currentMode == AIMode::Chat) {
        return m_chatInput->toPlainText();
    } else {
        return m_queryEdit->toPlainText();
    }
}

/**
 * @brief 追加响应内容
 * @param response 要追加的响应文本
 */
void AIQueryPanel::appendResponse(const QString& response)
{
    if (m_currentMode == AIMode::Chat) {
        // 聊天模式：添加为AI消息
        addChatMessage(ChatMessage::Assistant, response);
    } else {
        // 分析模式：追加到响应区域
        m_responseEdit->append(response);
        m_responseEdit->append("\n");
    }
}

/**
 * @brief 清除响应内容
 */
void AIQueryPanel::clearResponse()
{
    m_responseEdit->clear();
}

/**
 * @brief 设置加载状态
 * @param loading 是否正在加载
 */
void AIQueryPanel::setLoading(bool loading)
{
    m_loading = loading;
    m_submitBtn->setEnabled(!loading);
    m_cancelBtn->setEnabled(loading);
    m_modeSelector->setEnabled(!loading);
    
    if (loading) {
        m_statusLabel->setText(tr("正在处理..."));
    } else {
        m_statusLabel->setText(tr("就绪"));
    }
}

/**
 * @brief 发送按钮点击槽
 * 
 * 根据当前模式发送查询或聊天消息
 */
void AIQueryPanel::onSubmitClicked()
{
    QString query = getQueryText().trimmed();
    if (query.isEmpty() || m_loading) return;
    
    if (m_currentMode == AIMode::Chat) {
        // 聊天模式：添加用户消息到列表
        addChatMessage(ChatMessage::User, query);
        m_chatInput->clear();
        setLoading(true);
        emit chatMessageSent(query, m_chatHistory);
    } else {
        // 分析模式：清空响应区域并发送查询
        clearResponse();
        setLoading(true);
        emit querySubmitted(query);
    }
}

/**
 * @brief 清空按钮点击槽
 * 
 * 清空输入和响应内容
 */
void AIQueryPanel::onClearClicked()
{
    // 如果正在加载，先取消当前请求
    if (m_loading) {
        setLoading(false);
        m_statusLabel->setText(tr("已取消"));
        emit cancelRequested();
    }
    
    if (m_currentMode == AIMode::Chat) {
        clearChatHistory();
    } else {
        m_queryEdit->clear();
        clearResponse();
    }
    emit clearRequested();
}

/**
 * @brief 取消按钮点击槽
 * 
 * 取消当前正在进行的查询
 */
void AIQueryPanel::onCancelClicked()
{
    if (m_loading) {
        setLoading(false);
        m_statusLabel->setText(tr("已取消"));
        appendResponse(tr("[操作已取消]"));
        emit cancelRequested();
    }
}

/**
 * @brief 恢复原数据按钮点击槽
 * 
 * 清除所有筛选条件，恢复显示全部数据
 */
void AIQueryPanel::onResetDataClicked()
{
    emit resetDataRequested();
    appendResponse(tr("✓ 已恢复显示全部数据"));
    m_statusLabel->setText(tr("数据已重置"));
}

void AIQueryPanel::setModels(const QMap<QString, QString>& models)
{
    m_modelSelector->blockSignals(true);
    m_modelSelector->clear();
    
    if (models.isEmpty()) {
        m_modelSelector->addItem(tr("未配置模型"), QString());
    } else {
        for (auto it = models.begin(); it != models.end(); ++it) {
            m_modelSelector->addItem(it.key(), it.value());
        }
    }
    
    m_modelSelector->blockSignals(false);
}

void AIQueryPanel::setCurrentModel(const QString& modelId)
{
    m_modelSelector->blockSignals(true);
    for (int i = 0; i < m_modelSelector->count(); ++i) {
        if (m_modelSelector->itemData(i).toString() == modelId) {
            m_modelSelector->setCurrentIndex(i);
            break;
        }
    }
    m_modelSelector->blockSignals(false);
}

QString AIQueryPanel::currentModelId() const
{
    return m_modelSelector->currentData().toString();
}
