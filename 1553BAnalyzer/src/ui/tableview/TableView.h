/**
 * @file TableView.h
 * @brief 数据表格视图类定义
 * 
 * TableView类继承自QTableView，提供数据表格的显示和交互功能。
 * 
 * 主要功能：
 * - 数据展示：以表格形式显示1553B数据
 * - 右键菜单：复制、导出、筛选、查看详情
 * - 行选择：支持单选和多选
 * - 双击查看：双击行显示数据详情
 * - 数据导出：支持CSV和Excel格式
 * 
 * 右键菜单项：
 * - 复制选中：复制选中单元格到剪贴板
 * - 导出CSV：导出选中数据为CSV格式
 * - 导出Excel：导出选中数据为Excel格式
 * - 按值筛选：根据选中单元格的值筛选数据
 * - 查看详情：打开数据详情对话框
 * 
 * 使用示例：
 * @code
 * TableView* tableView = new TableView(this);
 * tableView->setModel(dataModel);
 * connect(tableView, &TableView::showDetailRequested, this, &MainWindow::showDataDetailDialog);
 * @endcode
 * 
 * @author 1553BTools
 * @date 2024
 */

#ifndef TABLEVIEW_H
#define TABLEVIEW_H

#include <QTableView>
#include <QHeaderView>
#include <QMenu>
#include <QAction>

/**
 * @brief 数据表格视图类
 * 
 * 该类继承自QTableView，提供自定义的表格显示和交互功能。
 * 支持右键菜单、行选择、数据导出等操作。
 */
class TableView : public QTableView
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针
     * 
     * 初始化表格视图，设置选择模式和右键菜单
     */
    explicit TableView(QWidget *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~TableView();
    
    /**
     * @brief 设置自动调整列宽
     * @param enabled 是否启用
     * 
     * 启用后，列宽会根据内容自动调整
     */
    void setAutoResizeColumns(bool enabled);
    
    /**
     * @brief 重写setModel，在设置model后连接selectionModel信号
     * @param model 要设置的数据模型
     */
    void setModel(QAbstractItemModel* model) override;
    
    /**
     * @brief 导出选中数据
     * @param format 导出格式（"csv"或"excel"）
     */
    void exportSelection(const QString& format);

signals:
    /**
     * @brief 行选中信号
     * @param row 选中的行号
     */
    void recordSelected(int row);
    
    /**
     * @brief 行双击信号
     * @param row 双击的行号
     */
    void recordDoubleClicked(int row);
    
    /**
     * @brief 显示详情请求信号
     * @param row 要显示详情的行号
     */
    void showDetailRequested(int row);
    
    /**
     * @brief 算式筛选请求信号
     * @param column 列索引
     * @param expression 筛选表达式
     */
    void expressionFilterRequested(int column, const QString& expression);

protected:
    /**
     * @brief 右键菜单事件
     * @param event 上下文菜单事件对象
     * 
     * 在鼠标位置显示右键菜单
     */
    void contextMenuEvent(QContextMenuEvent *event) override;
    
    /**
     * @brief 键盘按键事件
     * @param event 键盘事件对象
     * 
     * 处理快捷键：
     * - Ctrl+C：复制选中内容
     * - Enter：显示详情
     */
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    /**
     * @brief 双击槽函数
     * @param index 双击的模型索引
     */
    void onDoubleClicked(const QModelIndex& index);
    
    /**
     * @brief 选择变化槽函数
     * @param selected 新选中的项
     * @param deselected 取消选中的项
     */
    void onSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
    
    /**
     * @brief 复制选中内容槽函数
     */
    void onCopySelected();
    
    /**
     * @brief 导出CSV槽函数
     */
    void onExportCSV();
    
    /**
     * @brief 导出Excel槽函数
     */
    void onExportExcel();
    
    /**
     * @brief 按值筛选槽函数
     */
    void onFilterByValue();
    
    /**
     * @brief 显示详情槽函数
     */
    void onShowDetail();

private:
    /**
     * @brief 设置界面
     */
    void setupUI();
    
    /**
     * @brief 设置信号连接
     */
    void setupConnections();
    
    /**
     * @brief 复制到剪贴板
     * @param indexes 要复制的模型索引列表
     */
    void copyToClipboard(const QModelIndexList& indexes);
    
    bool m_autoResize;              // 是否自动调整列宽
    QMenu* m_contextMenu;           // 右键菜单
    QAction* m_actionCopy;          // 复制动作
    QAction* m_actionExportCSV;     // 导出CSV动作
    QAction* m_actionExportExcel;   // 导出Excel动作
    QAction* m_actionFilter;        // 筛选动作
    QAction* m_actionShowDetail;    // 显示详情动作
    int m_currentRow;               // 当前右键点击的行号
};

#endif
