/**
 * @file FileListPanel.cpp
 * @brief 文件列表面板类实现
 * 
 * 本文件实现了文件列表面板的功能，包括：
 * - 文件列表的显示和管理
 * - 工具栏按钮（添加、移除、清空）
 * - 文件选择和移除信号
 * 
 * @author 1553BTools
 * @date 2024
 */

#include "FileListPanel.h"
#include "core/datastore/DatabaseManager.h"
#include <QVBoxLayout>
#include <QToolBar>
#include <QAction>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QFont>
#include <QColor>

/**
 * @brief 构造函数
 * @param parent 父窗口指针
 * 
 * 初始化文件列表面板，创建列表控件和工具栏
 */
FileListPanel::FileListPanel(QWidget *parent)
    : QDockWidget(tr(u8"文件列表"), parent)
    , m_listWidget(new QListWidget(this))
{
    setObjectName("FileListPanel");
    setupUI();
}

/**
 * @brief 析构函数
 */
FileListPanel::~FileListPanel()
{
}

/**
 * @brief 设置界面
 * 
 * 创建面板布局：
 * - 顶部工具栏：添加、移除、清空按钮
 * - 主区域：文件列表控件
 * 
 * 工具栏功能：
 * - 添加：打开文件对话框选择文件，发出fileSelected信号
 * - 移除：移除列表中选中的文件
 * - 清空：清空整个文件列表
 */
void FileListPanel::setupUI()
{
    QWidget* container = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    
    QToolBar* toolbar = new QToolBar(this);
    toolbar->addAction(tr(u8"添加"), [this]() {
        QString filePath = QFileDialog::getOpenFileName(
            this,
            tr(u8"打开1553B数据文件"),
            QString(),
            tr(u8"二进制文件 (*.bin *.data *.raw);;所有文件 (*.*)")
            );

        if (!filePath.isEmpty()) {
            emit fileSelected(filePath);
        }
    });
    toolbar->addAction(tr(u8"移除"), this, &FileListPanel::onRemoveSelected);
    toolbar->addAction(tr(u8"清空"), this, &FileListPanel::clearFiles);
    layout->addWidget(toolbar);
    
    layout->addWidget(m_listWidget);
    
    setWidget(container);
    
    connect(m_listWidget, &QListWidget::itemDoubleClicked, 
            this, &FileListPanel::onItemDoubleClicked);
}

/**
 * @brief 添加文件到列表
 * @param filePath 文件路径
 * 
 * 如果文件已在列表中则不重复添加。
 * 列表项显示文件名，完整路径存储在UserRole数据中。
 */
void FileListPanel::addFile(const QString& filePath)
{
    if (m_files.contains(filePath)) return;
    
    m_files.append(filePath);
    QFileInfo fi(filePath);
    QListWidgetItem* item = new QListWidgetItem(fi.fileName());
    item->setData(Qt::UserRole, filePath);
    item->setData(Qt::UserRole + 1, -1);
    item->setToolTip(filePath);
    m_listWidget->addItem(item);
}

void FileListPanel::addDbFile(int fileId, const QString& filePath, const QString& fileName,
                               qint64 messageCount, const QString& importTime)
{
    if (m_files.contains(filePath)) return;
    
    m_files.append(filePath);
    QString displayText = QString::fromUtf8(u8"%1 (%2条)").arg(fileName).arg(messageCount);
    QListWidgetItem* item = new QListWidgetItem(displayText);
    item->setData(Qt::UserRole, filePath);
    item->setData(Qt::UserRole + 1, fileId);
    item->setToolTip(filePath);
    m_listWidget->addItem(item);
}

/**
 * @brief 清空文件列表
 * 
 * 清空内部文件路径列表和列表控件
 */
void FileListPanel::updateFileId(const QString& filePath, int fileId)
{
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem* item = m_listWidget->item(i);
        if (item->data(Qt::UserRole).toString() == filePath) {
            item->setData(Qt::UserRole + 1, fileId);
            
            QFileInfo fi(filePath);
            qint64 msgCount = 0;
            if (DatabaseManager::instance()->isInitialized()) {
                msgCount = DatabaseManager::instance()->queryPacketCount(fileId);
            }
            QString displayText = QString::fromUtf8(u8"%1 (%2条)").arg(fi.fileName()).arg(msgCount);
            item->setText(displayText);
            break;
        }
    }
}

void FileListPanel::setActiveFile(const QString& filePath)
{
    m_activeFilePath = filePath;
    
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem* item = m_listWidget->item(i);
        QString itemPath = item->data(Qt::UserRole).toString();
        
        if (itemPath == filePath) {
            QFont font = item->font();
            font.setBold(true);
            item->setFont(font);
            
            item->setForeground(QBrush(QColor(0, 90, 180)));
            item->setBackground(QBrush(QColor(220, 235, 255)));
            
            m_listWidget->setCurrentItem(item);
        } else {
            QFont font = item->font();
            font.setBold(false);
            item->setFont(font);
            item->setForeground(QBrush());
            item->setBackground(QBrush());
        }
    }
}

void FileListPanel::clearFiles()
{
    if (m_listWidget->count() == 0) return;
    
    // 弹框确认
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr(u8"确认清空"),
        tr(u8"确定要清空所有 %1 个文件吗？此操作不可撤销。").arg(m_listWidget->count()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    
    if (reply != QMessageBox::Yes) {
        return;
    }
    
    // 收集所有文件ID，用于数据库模式删除
    QVector<int> fileIds;
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem* item = m_listWidget->item(i);
        int fileId = item->data(Qt::UserRole + 1).toInt();
        if (fileId > 0) {
            fileIds.append(fileId);
        }
    }
    
    m_files.clear();
    m_listWidget->clear();
    
    // 发出清空信号，通知主窗口删除数据库数据
    emit filesCleared(fileIds);
}

void FileListPanel::removeFile(const QString& filePath)
{
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem* item = m_listWidget->item(i);
        if (item->data(Qt::UserRole).toString() == filePath) {
            int fileId = item->data(Qt::UserRole + 1).toInt();
            m_files.removeOne(filePath);
            emit fileRemoved(filePath, fileId);
            delete m_listWidget->takeItem(i);
            break;
        }
    }
}

/**
 * @brief 获取所有文件路径
 * @return 文件路径列表
 */
QStringList FileListPanel::getFiles() const
{
    return m_files;
}

/**
 * @brief 列表项双击槽函数
 * @param item 双击的列表项
 * 
 * 发出fileSelected信号，通知主窗口加载该文件
 */
void FileListPanel::onItemDoubleClicked(QListWidgetItem* item)
{
    QString filePath = item->data(Qt::UserRole).toString();
    int fileId = item->data(Qt::UserRole + 1).toInt();
    
    if (filePath == m_activeFilePath) {
        return;
    }
    
    if (!m_activeFilePath.isEmpty()) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, tr(u8"切换文件"),
            tr(u8"当前正在显示其他文件数据，确定要切换到新文件吗？"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        
        if (reply != QMessageBox::Yes) {
            return;
        }
    }
    
    if (fileId > 0) {
        emit dbFileSelected(fileId, filePath);
    } else {
        emit fileSelected(filePath);
    }
}

/**
 * @brief 移除选中文件槽函数
 * 
 * 从列表中移除选中的文件项，
 * 并发出fileRemoved信号通知主窗口
 */
void FileListPanel::onRemoveSelected()
{
    QList<QListWidgetItem*> selected = m_listWidget->selectedItems();
    if (selected.isEmpty()) return;
    
    // 弹框确认
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr(u8"确认移除"),
        tr(u8"确定要移除选中的 %1 个文件吗？").arg(selected.size()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    
    if (reply != QMessageBox::Yes) {
        return;
    }
    
    for (QListWidgetItem* item : selected) {
        QString filePath = item->data(Qt::UserRole).toString();
        int fileId = item->data(Qt::UserRole + 1).toInt();
        m_files.removeOne(filePath);
        emit fileRemoved(filePath, fileId);
        delete m_listWidget->takeItem(m_listWidget->row(item));
    }
}
