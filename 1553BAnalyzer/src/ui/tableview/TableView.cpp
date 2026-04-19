/**
 * @file TableView.cpp
 * @brief 数据表格视图类实现
 * 
 * 本文件实现了数据表格视图的交互功能，包括：
 * - 右键菜单的创建和响应
 * - 数据复制到剪贴板
 * - 数据导出功能
 * - 行选择和双击事件处理
 * 
 * @author 1553BTools
 * @date 2024
 */

#include "TableView.h"
#include <QContextMenuEvent>
#include <QClipboard>
#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QKeyEvent>
#include <QInputDialog>

/**
 * @brief 构造函数
 * @param parent 父窗口指针
 * 
 * 初始化表格视图的默认设置：
 * - 选择行为：整行选择
 * - 选择模式：扩展选择（支持多选）
 * - 交替行颜色：启用
 * - 排序功能：启用
 */
TableView::TableView(QWidget *parent)
    : QTableView(parent)
    , m_autoResize(true)
    , m_contextMenu(nullptr)
    , m_currentRow(-1)
{
    setupUI();
    setupConnections();
}

/**
 * @brief 析构函数
 */
TableView::~TableView()
{
}

/**
 * @brief 设置界面
 * 
 * 配置表格视图的外观和行为：
 * - 选择行为：SelectRows（整行选择）
 * - 选择模式：ExtendedSelection（Ctrl/Shift多选）
 * - 交替行颜色：提高可读性
 * - 启用排序：点击表头排序
 * - 隐藏垂直表头
 * 
 * 创建右键菜单项：
 * - 数据详细：显示数据详情对话框
 * - 复制选中行：复制到剪贴板
 * - 导出为CSV：导出为CSV格式
 * - 导出为Excel：导出为Excel格式
 * - 按此值筛选：根据选中单元格筛选
 */
void TableView::setupUI()
{
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setAlternatingRowColors(true);
    setSortingEnabled(true);
    setWordWrap(false);
    
    // 性能优化设置
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    
    horizontalHeader()->setStretchLastSection(true);
    horizontalHeader()->setHighlightSections(false);
    horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    
    verticalHeader()->setVisible(false);
    verticalHeader()->setDefaultSectionSize(24);
    
    setContextMenuPolicy(Qt::CustomContextMenu);
    
    m_contextMenu = new QMenu(this);
    m_actionShowDetail = m_contextMenu->addAction(tr(u8"数据详细"));
    m_contextMenu->addSeparator();
    m_actionCopy = m_contextMenu->addAction(tr(u8"复制选中行"));
    m_actionExportCSV = m_contextMenu->addAction(tr(u8"导出为CSV"));
    m_actionExportExcel = m_contextMenu->addAction(tr(u8"导出为Excel"));
    m_contextMenu->addSeparator();
    m_actionFilter = m_contextMenu->addAction(tr(u8"按此值筛选"));
}

/**
 * @brief 重写setModel，在设置model后连接selectionModel信号
 * @param model 要设置的数据模型
 * 
 * QTableView::selectionModel()在setModel()后才有效，
 * 因此selectionChanged信号必须在setModel()之后连接，
 * 否则selectionModel()返回nullptr导致连接失败。
 */
void TableView::setModel(QAbstractItemModel* model)
{
    QTableView::setModel(model);
    if (selectionModel()) {
        connect(selectionModel(), &QItemSelectionModel::selectionChanged,
                this, &TableView::onSelectionChanged);
    }
}

/**
 * @brief 设置信号连接
 * 
 * 连接以下信号：
 * - 双击：触发recordDoubleClicked信号
 * - 选择变化：触发recordSelected信号
 * - 右键菜单：显示菜单并记录当前行
 * - 菜单项点击：执行对应操作
 */
void TableView::setupConnections()
{
    connect(this, &QTableView::doubleClicked, this, &TableView::onDoubleClicked);
    // selectionModel的连接移到setModel()中，确保model已设置后再连接
    connect(this, &QWidget::customContextMenuRequested, [this](const QPoint& pos) {
        m_currentRow = indexAt(pos).row();
        m_contextMenu->popup(mapToGlobal(pos));
    });
    
    connect(m_actionCopy, &QAction::triggered, this, &TableView::onCopySelected);
    connect(m_actionExportCSV, &QAction::triggered, this, &TableView::onExportCSV);
    connect(m_actionExportExcel, &QAction::triggered, this, &TableView::onExportExcel);
    connect(m_actionFilter, &QAction::triggered, this, &TableView::onFilterByValue);
    connect(m_actionShowDetail, &QAction::triggered, this, &TableView::onShowDetail);
}

/**
 * @brief 设置自动调整列宽
 * @param enabled 是否启用
 * 
 * 启用时自动调整列宽以适应内容
 */
void TableView::setAutoResizeColumns(bool enabled)
{
    m_autoResize = enabled;
    if (enabled && model()) {
        resizeColumnsToContents();
    }
}

/**
 * @brief 导出选中数据
 * @param format 导出格式（"csv"或"xlsx"）
 * 
 * 打开文件保存对话框，导出选中的数据
 */
void TableView::exportSelection(const QString& format)
{
    QString filePath = QFileDialog::getSaveFileName(
        this,
        tr(u8"导出数据"),
        QString(),
        format == "csv" ? tr(u8"CSV文件 (*.csv)") : tr(u8"Excel文件 (*.xlsx)")
    );
    
    if (!filePath.isEmpty()) {
        QMessageBox::information(this, tr(u8"导出"), tr(u8"导出功能开发中..."));
    }
}

/**
 * @brief 右键菜单事件处理
 * @param event 上下文菜单事件对象
 * 
 * 在有效索引位置显示右键菜单
 */
void TableView::contextMenuEvent(QContextMenuEvent *event)
{
    QModelIndex index = indexAt(event->pos());
    if (index.isValid()) {
        m_contextMenu->exec(event->globalPos());
    }
    QTableView::contextMenuEvent(event);
}

/**
 * @brief 键盘按键事件处理
 * @param event 键盘事件对象
 * 
 * 处理快捷键：
 * - Ctrl+C：复制选中内容到剪贴板
 */
void TableView::keyPressEvent(QKeyEvent *event)
{
    if (event->matches(QKeySequence::Copy)) {
        onCopySelected();
        return;
    }
    QTableView::keyPressEvent(event);
}

/**
 * @brief 双击事件槽函数
 * @param index 双击的模型索引
 * 
 * 发出recordDoubleClicked信号，通知主窗口显示详情
 */
void TableView::onDoubleClicked(const QModelIndex& index)
{
    if (index.isValid()) {
        emit recordDoubleClicked(index.row());
    }
}

/**
 * @brief 选择变化槽函数
 * @param selected 新选中的项
 * @param deselected 取消选中的项（未使用）
 * 
 * 发出recordSelected信号，通知主窗口更新状态
 */
void TableView::onSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
    Q_UNUSED(deselected)
    if (!selected.isEmpty()) {
        emit recordSelected(selected.indexes().first().row());
    }
}

/**
 * @brief 复制选中内容槽函数
 * 
 * 获取选中的所有单元格，复制到剪贴板
 */
void TableView::onCopySelected()
{
    QModelIndexList indexes = selectionModel()->selectedIndexes();
    if (!indexes.isEmpty()) {
        copyToClipboard(indexes);
    }
}

/**
 * @brief 导出CSV槽函数
 */
void TableView::onExportCSV()
{
    exportSelection("csv");
}

/**
 * @brief 导出Excel槽函数
 */
void TableView::onExportExcel()
{
    exportSelection("xlsx");
}

/**
 * @brief 按值筛选槽函数
 * 
 * 弹出输入对话框，让用户输入算式筛选条件
 */
void TableView::onFilterByValue()
{
    QModelIndex index = currentIndex();
    if (!index.isValid() || !model()) {
        return;
    }
    
    int column = index.column();
    QVariant value = model()->data(index);
    
    // 构建提示信息
    QString columnName = model()->headerData(column, Qt::Horizontal).toString();
    QString currentValue = value.toString();
    
    // 弹出输入对话框
    bool ok;
    QString expression = QInputDialog::getText(
        this,
        tr(u8"算式筛选 - %1").arg(columnName),
        tr(u8"当前值: %1\n\n"
           u8"请输入筛选表达式:\n"
           u8"支持: >、<、=、>=、<=、!=\n"
           u8"逻辑: && (与)、|| (或)\n"
           u8"分隔: ; (多个条件)\n\n"
           u8"示例:\n"
           u8"  >1          大于1\n"
           u8"  <3&&>1      小于3且大于1\n"
           u8"  <3||>10     小于3或大于10\n"
           u8"  >1;<10      大于1或小于10").arg(currentValue),
        QLineEdit::Normal,
        QString(),
        &ok
    );
    
    if (ok && !expression.isEmpty()) {
        // 发出算式筛选请求信号
        emit expressionFilterRequested(column, expression);
    }
}

/**
 * @brief 复制到剪贴板
 * @param indexes 要复制的模型索引列表
 * 
 * 将选中单元格的内容格式化为制表符分隔的文本，
 * 每行用换行符分隔，然后复制到系统剪贴板
 */
void TableView::copyToClipboard(const QModelIndexList& indexes)
{
    if (indexes.isEmpty()) return;
    
    QString text;
    int prevRow = -1;
    
    for (const QModelIndex& index : indexes) {
        if (prevRow != -1 && index.row() != prevRow) {
            text += "\n";
        } else if (prevRow != -1) {
            text += "\t";
        }
        text += model()->data(index).toString();
        prevRow = index.row();
    }
    
    QApplication::clipboard()->setText(text);
}

/**
 * @brief 显示详情槽函数
 * 
 * 发出showDetailRequested信号，通知主窗口显示数据详情对话框
 */
void TableView::onShowDetail()
{
    if (m_currentRow >= 0) {
        emit showDetailRequested(m_currentRow);
    }
}
