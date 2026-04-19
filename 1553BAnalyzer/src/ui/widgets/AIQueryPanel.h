/**
 * @file AIQueryPanel.h
 * @brief AI智能查询面板类定义
 * 
 * AIQueryPanel类提供AI智能交互界面，支持两种模式：
 * - 聊天模式：与AI进行自由对话
 * - 智能分析模式：用自然语言查询数据、生成图表
 * 
 * 主要功能：
 * - 模式切换（聊天/分析）
 * - 消息输入和发送
 * - 响应显示
 * - 聊天历史管理
 * - 加载状态显示
 * 
 * @author 1553BTools
 * @date 2024
 */

#ifndef AIQUERYPANEL_H
#define AIQUERYPANEL_H

#include <QDockWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QStackedWidget>
#include <QListWidget>
#include <QSplitter>
#include <QThread>

#include "speech/SpeechRecognizer.h"

/**
 * @brief AI模式枚举
 */
enum class AIMode
{
    Chat,       // 聊天模式：自由对话
    Analysis    // 智能分析模式：数据查询和可视化
};

/**
 * @brief 聊天消息结构
 */
struct ChatMessage
{
    /**
     * @brief 消息角色枚举
     */
    enum Role { 
        User,       // 用户消息
        Assistant,  // AI助手消息
        System      // 系统消息
    };
    
    Role role;          // 消息角色
    QString content;    // 消息内容
    QString timestamp;  // 时间戳
};

/**
 * @brief AI智能查询面板类
 * 
 * 该类继承自QDockWidget，提供可停靠的AI交互界面。
 * 使用QStackedWidget实现两种模式的界面切换。
 */
class AIQueryPanel : public QDockWidget
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针
     */
    explicit AIQueryPanel(QWidget *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~AIQueryPanel();
    
    /**
     * @brief 设置查询文本
     * @param text 要设置的文本
     */
    void setQueryText(const QString& text);
    
    /**
     * @brief 获取当前查询文本
     * @return 当前输入框中的文本
     */
    QString getQueryText() const;
    
    /**
     * @brief 追加响应内容
     * @param response 要追加的响应文本
     * 
     * 在分析模式下追加到响应区域
     * 在聊天模式下添加为AI消息
     */
    void appendResponse(const QString& response);
    
    /**
     * @brief 清除响应内容
     */
    void clearResponse();
    
    /**
     * @brief 设置加载状态
     * @param loading 是否正在加载
     * 
     * 加载时禁用发送按钮，启用取消按钮
     */
    void setLoading(bool loading);
    
    /**
     * @brief 获取当前模式
     * @return 当前AI模式
     */
    AIMode currentMode() const;
    
    /**
     * @brief 设置AI模式
     * @param mode 要设置的模式
     */
    void setMode(AIMode mode);
    
    /**
     * @brief 添加聊天消息
     * @param role 消息角色
     * @param content 消息内容
     */
    void addChatMessage(ChatMessage::Role role, const QString& content);
    
    /**
     * @brief 清除聊天历史
     */
    void clearChatHistory();
    
    /**
     * @brief 获取聊天历史
     * @return 聊天消息列表
     */
    QList<ChatMessage> getChatHistory() const;
    
    /**
     * @brief 设置模型列表
     * @param models 模型列表（名称到ID的映射）
     */
    void setModels(const QMap<QString, QString>& models);
    
    /**
     * @brief 设置当前模型
     * @param modelId 模型ID
     */
    void setCurrentModel(const QString& modelId);
    
    /**
     * @brief 获取当前模型ID
     * @return 当前模型ID
     */
    QString currentModelId() const;

signals:
    /**
     * @brief 查询提交信号（分析模式）
     * @param query 用户输入的查询文本
     */
    void querySubmitted(const QString& query);
    
    /**
     * @brief 清除请求信号
     */
    void clearRequested();
    
    /**
     * @brief 取消请求信号
     */
    void cancelRequested();
    
    /**
     * @brief 恢复原数据信号
     */
    void resetDataRequested();
    
    /**
     * @brief 模式切换信号
     * @param mode 新的AI模式
     */
    void modeChanged(AIMode mode);
    
    /**
     * @brief 聊天消息发送信号（聊天模式）
     * @param message 用户消息
     * @param history 聊天历史
     */
    void chatMessageSent(const QString& message, const QList<ChatMessage>& history);
    
    /**
     * @brief 模型切换信号
     * @param modelId 模型ID
     */
    void modelChanged(const QString& modelId);
    
    /**
     * @brief 刷新模型列表信号
     */
    void refreshModelsRequested();

private slots:
    /**
     * @brief 发送按钮点击槽
     */
    void onSubmitClicked();
    
    /**
     * @brief 清空按钮点击槽
     */
    void onClearClicked();
    
    /**
     * @brief 取消按钮点击槽
     */
    void onCancelClicked();
    
    /**
     * @brief 恔回原数据按钮点击槽
     */
    void onResetDataClicked();
    
    /**
     * @brief 模式选择变化槽
     * @param index 选择的模式索引
     */
    void onModeChanged(int index);
    
    /**
     * @brief 语音按钮点击槽
     *
     * 切换录音状态：
     * - 空闲状态：启动录音，按钮变为停止状态
     * - 录音状态：停止录音，将识别结果填入输入框
     */
    void onVoiceBtnClicked();
    
    /**
     * @brief 语音识别部分结果槽
     * @param text 实时识别的文字
     *
     * 将实时识别结果显示在输入框中，
     * 用户可以看到正在说的文字。
     */
    void onSpeechPartialResult(const QString& text);
    
    /**
     * @brief 语音识别最终结果槽
     * @param text 最终识别的文字
     *
     * 将最终识别结果替换输入框中的内容。
     */
    void onSpeechFinalResult(const QString& text);
    
    /**
     * @brief 语音识别状态变化槽
     * @param state 新的语音识别状态
     *
     * 更新语音按钮的显示状态。
     */
    void onSpeechStateChanged(SpeechState state);
    
    /**
     * @brief 语音识别音频电平槽
     * @param level 当前音频电平（0.0-1.0）
     *
     * 更新语音按钮的样式（如脉冲动画效果）。
     */
    void onSpeechAudioLevel(qreal level);
    
    /**
     * @brief 语音识别错误槽
     * @param message 错误信息
     *
     * 弹框提示错误，恢复语音按钮状态。
     */
    void onSpeechError(const QString& message);

    /**
     * @brief 语音识别初始化完成槽
     * @param success 初始化是否成功
     *
     * 在后台线程完成语音引擎初始化后由信号触发，
     * 更新UI状态（按钮启用/禁用、状态标签文字）。
     * 使用此槽替代QMetaObject::invokeMethod的lambda形式，
     * 以兼容Qt5.9.9（lambda形式从Qt5.10才开始支持）。
     */
    void onSpeechInitDone(bool success);

private:
    /**
     * @brief 设置界面布局
     */
    void setupUI();
    
    /**
     * @brief 设置聊天模式界面
     */
    void setupChatMode();
    
    /**
     * @brief 设置分析模式界面
     */
    void setupAnalysisMode();
    
    /**
     * @brief 更新输入框占位文本
     */
    void updatePlaceholder();
    
    /**
     * @brief 添加消息到聊天列表
     * @param role 消息角色
     * @param content 消息内容
     * @param timestamp 时间戳
     */
    void addMessageToList(ChatMessage::Role role, const QString& content, const QString& timestamp);
    
    /**
     * @brief 初始化语音识别模块
     *
     * 创建语音识别线程和控制器，加载模型。
     * 在后台线程中执行模型加载，避免阻塞UI。
     */
    void initSpeechRecognition();
    
    /**
     * @brief 更新语音按钮的显示状态
     *
     * 根据当前录音状态更新按钮图标、提示文字和样式：
     * - 空闲状态：显示麦克风图标，灰色
     * - 录音状态：显示停止图标，红色脉冲效果
     */
    void updateVoiceBtnState();
    
    /**
     * @brief 获取当前模式的输入框
     * @return 当前模式的QTextEdit指针
     */
    QTextEdit* currentInputEdit();
    
    QWidget* m_container;           // 主容器
    QStackedWidget* m_stackedWidget; // 堆栈窗口（用于模式切换）
    
    QComboBox* m_modeSelector;       // 模式选择下拉框
    QComboBox* m_modelSelector;      // 模型选择下拉框
    QLabel* m_modeDescription;       // 模式描述标签
    
    QWidget* m_chatPage;             // 聊天模式页面
    QListWidget* m_chatList;         // 聊天消息列表
    QTextEdit* m_chatInput;          // 聊天输入框
    
    QWidget* m_analysisPage;         // 分析模式页面
    QTextEdit* m_queryEdit;          // 查询输入框
    QTextEdit* m_responseEdit;       // 响应显示区域
    
    QPushButton* m_submitBtn;        // 发送按钮
    QPushButton* m_clearBtn;         // 清空按钮
    QPushButton* m_cancelBtn;        // 取消按钮
    QPushButton* m_resetDataBtn;     // 恢复原数据按钮
    QPushButton* m_voiceBtn;         // 语音输入按钮（开始/停止录音）
    QLabel* m_statusLabel;           // 状态标签
    
    bool m_loading;                  // 是否正在加载
    AIMode m_currentMode;            // 当前AI模式
    QList<ChatMessage> m_chatHistory;// 聊天历史
    
    QThread* m_speechThread;         // 语音识别工作线程
    SpeechRecognizer* m_speechRecognizer; // 语音识别控制器（运行在工作线程中）
    bool m_speechInitialized;        // 语音识别引擎是否已初始化
    int m_textLengthBeforeSpeech;    // 开始录音前输入框中文本的长度（用于确定语音插入位置）
};

#endif
