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
#include "utils/Logger.h"
#include "core/config/ConfigManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDateTime>
#include <QKeyEvent>
#include <QMessageBox>
#include <QMovie>

/**
 * @brief 构造函数，初始化AI查询面板
 * @param parent 父窗口指针
 */
AIQueryPanel::AIQueryPanel(QWidget *parent)
    : QDockWidget(tr(u8"AI智能模块"), parent)
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
    , m_voiceBtn(nullptr)
    , m_statusLabel(nullptr)
    , m_loading(false)
    , m_currentMode(AIMode::Chat)
    , m_speechThread(nullptr)
    , m_speechRecognizer(nullptr)
    , m_speechInitialized(false)
    , m_textLengthBeforeSpeech(0)
{
    setObjectName("AIQueryPanel");
    setupUI();
    initSpeechRecognition();
}

/**
 * @brief 析构函数
 *
 * 停止语音识别线程并释放资源。
 */
AIQueryPanel::~AIQueryPanel()
{
    /* 停止语音识别线程 */
    if (m_speechThread) {
        m_speechThread->quit();
        m_speechThread->wait(3000);
        if (m_speechThread->isRunning()) {
            m_speechThread->terminate();
            m_speechThread->wait();
        }
    }
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
    modeLayout->addWidget(new QLabel(tr(u8"模式:")));
    
    m_modeSelector = new QComboBox(this);
    m_modeSelector->addItem(u8"💬 聊天模式", static_cast<int>(AIMode::Chat));
    m_modeSelector->addItem(u8"📊 智能分析模式", static_cast<int>(AIMode::Analysis));
    modeLayout->addWidget(m_modeSelector);
    
    modeLayout->addSpacing(20);
    
    modeLayout->addWidget(new QLabel(tr(u8"模型:")));
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
    m_submitBtn = new QPushButton(tr(u8"发送"), this);
    m_cancelBtn = new QPushButton(tr(u8"取消"), this);
    m_clearBtn = new QPushButton(tr(u8"清空"), this);
    m_resetDataBtn = new QPushButton(tr(u8"🔄 恢复原数据"), this);
    m_resetDataBtn->setStyleSheet("QPushButton { background-color: #3498db; color: white; border: none; padding: 5px 10px; border-radius: 3px; } QPushButton:hover { background-color: #2980b9; }");
    
    /* 语音输入按钮：点击开始/停止录音 */
    m_voiceBtn = new QPushButton(tr(u8"🎤 语音"), this);
    m_voiceBtn->setToolTip(tr(u8"点击开始语音输入，再次点击停止"));
    m_voiceBtn->setStyleSheet(
        "QPushButton { background-color: #3498db; color: white; border: none; "
        "padding: 5px 10px; border-radius: 3px; font-weight: bold; } "
        "QPushButton:hover { background-color: #2980b9; } "
        "QPushButton:disabled { background-color: #95a5a6; color: white; }"
    );
    m_voiceBtn->setCheckable(false);
    
    btnLayout->addWidget(m_voiceBtn);
    btnLayout->addWidget(m_submitBtn);
    btnLayout->addWidget(m_cancelBtn);
    btnLayout->addWidget(m_clearBtn);
    btnLayout->addWidget(m_resetDataBtn);
    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);
    
    // 创建状态标签
    m_statusLabel = new QLabel(tr(u8"就绪"), this);
    mainLayout->addWidget(m_statusLabel);
    
    setWidget(m_container);
    
    // 初始状态：取消按钮禁用
    m_cancelBtn->setEnabled(false);
    
    // 连接信号和槽
    connect(m_submitBtn, &QPushButton::clicked, this, &AIQueryPanel::onSubmitClicked);
    connect(m_clearBtn, &QPushButton::clicked, this, &AIQueryPanel::onClearClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &AIQueryPanel::onCancelClicked);
    connect(m_resetDataBtn, &QPushButton::clicked, this, &AIQueryPanel::onResetDataClicked);
    connect(m_voiceBtn, &QPushButton::clicked, this, &AIQueryPanel::onVoiceBtnClicked);
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
    m_chatInput->setPlaceholderText(tr(u8"输入消息与AI对话..."));
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
    layout->addWidget(new QLabel(tr(u8"输入自然语言指令:")));
    
    m_queryEdit = new QTextEdit(this);
    m_queryEdit->setPlaceholderText(tr(u8"例如：\n• 显示终端地址为5的所有数据\n• 画一个饼图展示消息类型分布\n• 统计各终端的数据量\n• 生成甘特图"));
    m_queryEdit->setMaximumHeight(100);
    layout->addWidget(m_queryEdit);
    
    // 创建响应显示区域
    layout->addWidget(new QLabel(tr(u8"AI响应:")));
    
    m_responseEdit = new QTextEdit(this);
    m_responseEdit->setReadOnly(true);
    m_responseEdit->setPlaceholderText(tr(u8"AI分析结果将显示在这里..."));
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
        m_modeDescription->setText(tr(u8"💬 聊天模式：与AI进行自由对话，可以询问1553B协议相关问题、技术细节等。"));
        m_chatInput->setPlaceholderText(tr(u8"输入消息与AI对话..."));
    } else {
        m_modeDescription->setText(tr(u8"📊 智能分析模式：用自然语言查询数据、生成图表和甘特图。AI会理解您的意图并执行相应操作。"));
        m_queryEdit->setPlaceholderText(tr(u8"例如：\n• 显示终端地址为5的所有数据\n• 画一个饼图展示消息类型分布\n• 统计各终端的数据量\n• 生成甘特图"));
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
        prefix = tr(u8"👤 我");
        style = "background: #e3f2fd; color: #1565c0;";
    } else if (role == ChatMessage::Assistant) {
        prefix = tr(u8"🤖 AI");
        style = "background: #f5f5f5; color: #333;";
    } else {
        prefix = tr(u8"⚙️ 系统");
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
        m_statusLabel->setText(tr(u8"正在处理..."));
    } else {
        m_statusLabel->setText(tr(u8"就绪"));
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
        m_statusLabel->setText(tr(u8"已取消"));
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
        m_statusLabel->setText(tr(u8"已取消"));
        appendResponse(tr(u8"[操作已取消]"));
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
    appendResponse(tr(u8"✓ 已恢复显示全部数据"));
    m_statusLabel->setText(tr(u8"数据已重置"));
}

void AIQueryPanel::setModels(const QMap<QString, QString>& models)
{
    m_modelSelector->blockSignals(true);
    m_modelSelector->clear();
    
    if (models.isEmpty()) {
        m_modelSelector->addItem(tr(u8"未配置模型"), QString());
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

/**
 * @brief 初始化语音识别模块
 *
 * 创建语音识别工作线程和控制器对象。
 * 语音识别控制器运行在独立线程中，避免阻塞UI。
 *
 * 初始化流程：
 * 1. 创建工作线程
 * 2. 创建SpeechRecognizer对象并移动到工作线程
 * 3. 连接信号槽（跨线程使用Qt::QueuedConnection）
 * 4. 启动工作线程
 * 5. 在工作线程中初始化Vosk引擎（加载模型）
 * 6. 检测麦克风设备
 *
 * 如果没有麦克风设备，语音按钮将被禁用。
 */
void AIQueryPanel::initSpeechRecognition()
{
    /* 检测麦克风设备 */
    if (!SpeechRecognizer::hasMicrophone()) {
        m_voiceBtn->setEnabled(false);
        m_voiceBtn->setToolTip(tr(u8"未检测到麦克风设备"));
        m_statusLabel->setText(tr(u8"语音：未检测到麦克风"));
        return;
    }

    /* 创建工作线程 */
    m_speechThread = new QThread(this);
    m_speechThread->setObjectName("SpeechThread");

    /* 创建语音识别控制器（不指定父对象，以便移动到工作线程） */
    m_speechRecognizer = new SpeechRecognizer();
    m_speechRecognizer->moveToThread(m_speechThread);

    /* 连接信号槽（跨线程通信） */
    connect(m_speechThread, &QThread::finished, m_speechRecognizer, &QObject::deleteLater);
    
    /* 语音识别结果信号 → UI更新槽 */
    connect(m_speechRecognizer, &SpeechRecognizer::partialResult,
            this, &AIQueryPanel::onSpeechPartialResult, Qt::QueuedConnection);
    connect(m_speechRecognizer, &SpeechRecognizer::finalResult,
            this, &AIQueryPanel::onSpeechFinalResult, Qt::QueuedConnection);
    connect(m_speechRecognizer, &SpeechRecognizer::stateChanged,
            this, &AIQueryPanel::onSpeechStateChanged, Qt::QueuedConnection);
    connect(m_speechRecognizer, &SpeechRecognizer::audioLevel,
            this, &AIQueryPanel::onSpeechAudioLevel, Qt::QueuedConnection);
    connect(m_speechRecognizer, &SpeechRecognizer::error,
            this, &AIQueryPanel::onSpeechError, Qt::QueuedConnection);

    /* 初始化完成信号 → UI更新槽（替代invokeMethod lambda，兼容Qt5.9.9） */
    connect(m_speechRecognizer, &SpeechRecognizer::initializationDone,
            this, &AIQueryPanel::onSpeechInitDone, Qt::QueuedConnection);

    /* 启动工作线程 */
    m_speechThread->start();

    /* 通过信号槽机制在工作线程中初始化Vosk引擎 */
    /* 使用ConfigManager的语音配置，而非硬编码默认值 */
    SpeechConfig speechCfg = ConfigManager::instance()->getSpeechConfig();
    QMetaObject::invokeMethod(m_speechRecognizer, "doInitializeWithConfig", Qt::QueuedConnection,
        Q_ARG(SpeechConfig, speechCfg));

    m_statusLabel->setText(tr(u8"语音：正在加载模型..."));
}

/**
 * @brief 语音按钮点击槽
 *
 * 切换录音状态：
 * - 空闲状态 → 启动录音，保存当前输入框文本，按钮变红
 * - 录音状态 → 停止录音，识别结果填入输入框，按钮恢复
 */
void AIQueryPanel::onVoiceBtnClicked()
{
    LOG_INFO("Speech", QString::fromUtf8(u8"语音按钮被点击，当前状态: %1, 引擎已初始化: %2")
        .arg(m_speechRecognizer ? QString::number(static_cast<int>(m_speechRecognizer->state())) : "nullptr")
        .arg(m_speechRecognizer ? QString::number(static_cast<int>(m_speechRecognizer->isInitialized())) : "false"));

    if (!m_speechRecognizer) {
        QMessageBox::warning(this, tr(u8"语音输入"), tr(u8"语音识别引擎未初始化"));
        return;
    }

    if (m_speechRecognizer->state() == SpeechState::Recording) {
        /* 当前正在录音，立即更新按钮为空闲状态，再异步停止录音 */
        m_voiceBtn->setEnabled(true);
        m_voiceBtn->setText(tr(u8"🎤 语音"));
        m_voiceBtn->setToolTip(tr(u8"点击开始语音输入"));
        m_voiceBtn->setStyleSheet(
            "QPushButton { background-color: #3498db; color: white; border: none; "
            "padding: 5px 10px; border-radius: 3px; font-weight: bold; } "
            "QPushButton:hover { background-color: #2980b9; }"
        );
        m_statusLabel->setText(tr(u8"语音：就绪（停止录音中...）"));

        LOG_INFO("Speech", QString::fromUtf8(u8"调用 stopRecording"));
        QMetaObject::invokeMethod(m_speechRecognizer, "stopRecording", Qt::QueuedConnection);
    } else {
        /* 当前空闲，立即更新按钮为"录音中"状态，再异步启动 */
        m_voiceBtn->setEnabled(true);
        m_voiceBtn->setText(tr(u8"⏹ 停止"));
        m_voiceBtn->setToolTip(tr(u8"点击停止录音"));
        m_voiceBtn->setStyleSheet(
            "QPushButton { background-color: #e74c3c; color: white; border: none; "
            "padding: 5px 10px; border-radius: 3px; font-weight: bold; } "
            "QPushButton:hover { background-color: #c0392b; }"
        );
        m_statusLabel->setText(tr(u8"语音：就绪（录音中...）"));

        /* 保存录音前的输入框文本长度，用于后续拼接语音识别结果 */
        m_textLengthBeforeSpeech = currentInputEdit()->toPlainText().length();
        
        LOG_INFO("Speech", QString::fromUtf8(u8"调用 startRecording，录音前文本长度: %1").arg(m_textLengthBeforeSpeech));
        QMetaObject::invokeMethod(m_speechRecognizer, "startRecording", Qt::QueuedConnection);
    }
}

/**
 * @brief 语音识别部分结果槽
 * @param text 实时识别的文字
 *
 * 将实时识别结果显示在输入框中。
 * 策略：保留当前输入框中录音开始位置之前的内容，替换之后的内容为语音识别结果。
 * 这样用户在录音过程中修改输入框内容时，不会被覆盖。
 */
void AIQueryPanel::onSpeechPartialResult(const QString& text)
{
    QTextEdit* edit = currentInputEdit();
    if (!edit) return;

    /* 获取当前输入框内容，截取录音开始位置之前的部分 */
    QString currentText = edit->toPlainText();
    QString baseText = currentText.left(m_textLengthBeforeSpeech);
    
    /* 拼接基础文本和实时识别结果 */
    QString displayText = baseText;
    if (!displayText.isEmpty() && !text.isEmpty()) {
        displayText += " ";
    }
    displayText += text;
    edit->setPlainText(displayText);
}

/**
 * @brief 语音识别最终结果槽
 * @param text 最终识别的文字
 *
 * 将最终识别结果替换输入框中的内容。
 * 策略：保留当前输入框中录音开始位置之前的内容，替换之后的内容为最终识别结果。
 * 这样用户在录音过程中修改输入框内容时，不会被覆盖。
 */
void AIQueryPanel::onSpeechFinalResult(const QString& text)
{
    QTextEdit* edit = currentInputEdit();
    if (!edit) return;

    /* 获取当前输入框内容，截取录音开始位置之前的部分 */
    QString currentText = edit->toPlainText();
    QString baseText = currentText.left(m_textLengthBeforeSpeech);
    
    /* 拼接基础文本和最终识别结果 */
    QString finalText = baseText;
    if (!finalText.isEmpty() && !text.isEmpty()) {
        finalText += " ";
    }
    finalText += text;
    edit->setPlainText(finalText);
}

/**
 * @brief 语音识别状态变化槽
 * @param state 新的语音识别状态
 *
 * 更新语音按钮的显示状态和状态标签：
 * - Idle：蓝色按钮，显示"语音：就绪"
 * - Recording：红色按钮，显示"语音：就绪（录音中...）"
 * - Processing：黄色按钮，显示"语音：处理中..."
 */
void AIQueryPanel::onSpeechStateChanged(SpeechState state)
{
    updateVoiceBtnState();

    switch (state) {
    case SpeechState::Idle:
        m_statusLabel->setText(tr(u8"语音：就绪"));
        break;
    case SpeechState::Recording:
        m_statusLabel->setText(tr(u8"语音：就绪（录音中...）"));
        break;
    case SpeechState::Processing:
        /* 处理中状态不再单独显示，录音停止后直接回到空闲状态 */
        m_statusLabel->setText(tr(u8"语音：就绪"));
        break;
    }
}

/**
 * @brief 语音识别音频电平槽
 * @param level 当前音频电平（0.0-1.0）
 *
 * 音频电平信号仅用于内部处理，不直接修改按钮样式。
 * 按钮样式由onSpeechStateChanged统一管理。
 */
void AIQueryPanel::onSpeechAudioLevel(qreal level)
{
    /* 音频电平目前不直接用于按钮样式，保留用于后续扩展 */
    Q_UNUSED(level);
}

/**
 * @brief 语音识别错误槽
 * @param message 错误信息
 *
 * 弹框提示错误，恢复语音按钮状态。
 */
void AIQueryPanel::onSpeechError(const QString& message)
{
    LOG_ERROR("Speech", QString::fromUtf8(u8"语音识别错误: %1").arg(message));
    updateVoiceBtnState();
    QMessageBox::warning(this, tr(u8"语音识别错误"), message);
}

/**
 * @brief 语音识别初始化完成槽
 * @param success 初始化是否成功
 *
 * 在后台线程完成语音引擎初始化后由initializationDone信号触发。
 * 根据初始化结果更新UI状态：
 * - 成功：启用语音按钮，状态标签显示"语音：就绪"
 * - 失败：禁用语音按钮，状态标签显示"语音：初始化失败"
 *
 * 使用此槽替代QMetaObject::invokeMethod的lambda形式，
 * 以兼容Qt5.9.9（lambda形式从Qt5.10才开始支持）。
 */
void AIQueryPanel::onSpeechInitDone(bool success)
{
    m_speechInitialized = success;
    updateVoiceBtnState();
    if (success) {
        m_statusLabel->setText(tr(u8"语音：就绪"));
    } else {
        m_voiceBtn->setEnabled(false);
        m_voiceBtn->setToolTip(tr(u8"语音识别引擎初始化失败"));
        m_statusLabel->setText(tr(u8"语音：初始化失败"));
    }
}

/**
 * @brief 更新语音按钮的显示状态
 *
 * 根据当前语音识别状态更新按钮的文字、提示和样式：
 * - 空闲状态：蓝色背景，文字"🎤 语音"，提示"点击开始语音输入"
 * - 录音状态：红色背景，文字"⏹ 停止"，提示"点击停止录音"
 * - 处理中状态：黄色背景，文字"⏳ 处理中"，提示"正在处理语音..."
 * - 引擎未初始化：禁用按钮
 */
void AIQueryPanel::updateVoiceBtnState()
{
    if (!m_voiceBtn) return;

    if (!m_speechRecognizer || !m_speechInitialized) {
        m_voiceBtn->setEnabled(false);
        m_voiceBtn->setText(tr(u8"🎤 语音"));
        m_voiceBtn->setToolTip(tr(u8"语音识别引擎未就绪"));
        m_voiceBtn->setStyleSheet(
            "QPushButton { background-color: #95a5a6; color: white; border: none; "
            "padding: 5px 10px; border-radius: 3px; font-weight: bold; }"
        );
        return;
    }

    SpeechState state = m_speechRecognizer->state();

    switch (state) {
    case SpeechState::Idle:
    case SpeechState::Processing:
        /* 空闲和处理中都显示为"开始语音"状态，只有两种按钮状态 */
        m_voiceBtn->setEnabled(true);
        m_voiceBtn->setText(tr(u8"🎤 语音"));
        m_voiceBtn->setToolTip(tr(u8"点击开始语音输入"));
        m_voiceBtn->setStyleSheet(
            "QPushButton { background-color: #3498db; color: white; border: none; "
            "padding: 5px 10px; border-radius: 3px; font-weight: bold; } "
            "QPushButton:hover { background-color: #2980b9; }"
        );
        break;

    case SpeechState::Recording:
        m_voiceBtn->setEnabled(true);
        m_voiceBtn->setText(tr(u8"⏹ 停止"));
        m_voiceBtn->setToolTip(tr(u8"点击停止录音"));
        m_voiceBtn->setStyleSheet(
            "QPushButton { background-color: #e74c3c; color: white; border: none; "
            "padding: 5px 10px; border-radius: 3px; font-weight: bold; } "
            "QPushButton:hover { background-color: #c0392b; }"
        );
        break;
    }
}

/**
 * @brief 获取当前模式的输入框
 * @return 当前模式的QTextEdit指针
 *
 * 根据当前AI模式返回对应的输入框控件：
 * - 聊天模式 → m_chatInput
 * - 分析模式 → m_queryEdit
 */
QTextEdit* AIQueryPanel::currentInputEdit()
{
    if (m_currentMode == AIMode::Chat) {
        return m_chatInput;
    } else {
        return m_queryEdit;
    }
}
