/**
 * @file FileListPanel.h
 * @brief 文件列表面板类定义
 * 
 * FileListPanel类继承自QDockWidget，提供已导入文件的管理界面。
 * 
 * 主要功能：
 * - 显示已导入的文件列表
 * - 支持双击切换文件
 * - 支持移除文件
 * - 文件路径管理
 * 
 * 使用示例：
 * @code
 * FileListPanel* panel = new FileListPanel(this);
 * panel->addFile("/path/to/file.bin");
 * connect(panel, &FileListPanel::fileSelected, this, &MainWindow::loadFile);
 * @endcode
 * 
 * @author 1553BTools
 * @date 2024
 */

#ifndef FILELISTPANEL_H
#define FILELISTPANEL_H

#include <QDockWidget>
#include <QListWidget>
#include <QStringList>

/**
 * @brief 文件列表面板类
 * 
 * 该类提供文件列表的可视化管理界面，
 * 支持添加、移除、选择文件等操作。
 */
class FileListPanel : public QDockWidget
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针
     */
    explicit FileListPanel(QWidget *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~FileListPanel();
    
    /**
     * @brief 添加文件到列表
     * @param filePath 文件路径
     */
    void addFile(const QString& filePath);
    
    /**
     * @brief 添加数据库文件到列表
     * @param fileId 文件ID
     * @param filePath 文件路径
     * @param fileName 文件名
     * @param messageCount 消息数量
     * @param importTime 导入时间
     */
    void addDbFile(int fileId, const QString& filePath, const QString& fileName,
                   qint64 messageCount, const QString& importTime);
    
    void updateFileId(const QString& filePath, int fileId);
    
    void setActiveFile(const QString& filePath);
    
    void clearFiles();
    
    void removeFile(const QString& filePath);
    
    /**
     * @brief 获取所有文件路径
     * @return 文件路径列表
     */
    QStringList getFiles() const;

signals:
    /**
     * @brief 文件选中信号
     * @param filePath 选中的文件路径
     */
    void fileSelected(const QString& filePath);
    
    /**
     * @brief 数据库文件选中信号（双击已导入数据库的文件）
     * @param fileId 文件ID
     * @param filePath 文件路径
     */
    void dbFileSelected(int fileId, const QString& filePath);
    
    /**
     * @brief 文件移除信号
     * @param filePath 移除的文件路径
     * @param fileId 文件ID（数据库模式，-1表示内存模式）
     */
    void fileRemoved(const QString& filePath, int fileId);
    
    /**
     * @brief 文件清空信号
     * @param fileIds 被清空的所有文件ID列表（数据库模式）
     */
    void filesCleared(const QVector<int>& fileIds);

private slots:
    /**
     * @brief 列表项双击槽函数
     * @param item 双击的列表项
     */
    void onItemDoubleClicked(QListWidgetItem* item);
    
    /**
     * @brief 移除选中文件槽函数
     */
    void onRemoveSelected();

private:
    /**
     * @brief 设置界面
     */
    void setupUI();
    
    QListWidget* m_listWidget;  // 文件列表控件
    QStringList m_files;        // 文件路径列表
    QString m_activeFilePath;   // 当前活动文件路径
};

#endif
