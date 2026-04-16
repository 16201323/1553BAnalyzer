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

/**
 * @brief AI模式枚举
 */
enum class AIMode
{
    Chat,       ///< 聊天模式：自由对话
    Analysis    ///< 智能分析模式：数据查询和可视化
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
        User,       ///< 用户消息
        Assistant,  ///< AI助手消息
        System      ///< 系统消息
    };
    
    Role role;          ///< 消息角色
    QString content;    ///< 消息内容
    QString timestamp;  ///< 时间戳
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
    
    QWidget* m_container;           ///< 主容器
    QStackedWidget* m_stackedWidget; ///< 堆栈窗口（用于模式切换）
    
    QComboBox* m_modeSelector;       ///< 模式选择下拉框
    QComboBox* m_modelSelector;      ///< 模型选择下拉框
    QLabel* m_modeDescription;       ///< 模式描述标签
    
    QWidget* m_chatPage;             ///< 聊天模式页面
    QListWidget* m_chatList;         ///< 聊天消息列表
    QTextEdit* m_chatInput;          ///< 聊天输入框
    
    QWidget* m_analysisPage;         ///< 分析模式页面
    QTextEdit* m_queryEdit;          ///< 查询输入框
    QTextEdit* m_responseEdit;       ///< 响应显示区域
    
    QPushButton* m_submitBtn;        ///< 发送按钮
    QPushButton* m_clearBtn;         ///< 清空按钮
    QPushButton* m_cancelBtn;        ///< 取消按钮
    QPushButton* m_resetDataBtn;     ///< 恢复原数据按钮
    QLabel* m_statusLabel;           ///< 状态标签
    
    bool m_loading;                  ///< 是否正在加载
    AIMode m_currentMode;            ///< 当前AI模式
    QList<ChatMessage> m_chatHistory;///< 聊天历史
};

#endif
