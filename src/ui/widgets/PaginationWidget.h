/**
 * @file PaginationWidget.h
 * @brief 分页控件类定义
 * 
 * PaginationWidget类提供数据表格的分页控制功能，
 * 包含数据统计信息、页码导航和每页条数设置。
 * 
 * 主要功能：
 * - 显示数据总数、筛选后总数、当前显示范围
 * - 上一页/下一页导航
 * - 页码跳转
 * - 每页显示条数设置
 * 
 * 使用示例：
 * @code
 * PaginationWidget* pagination = new PaginationWidget(this);
 * 
 * // 更新分页信息
 * pagination->updatePagination(0, 50, 5000, 100000);
 * 
 * // 连接信号
 * connect(pagination, &PaginationWidget::pageChanged, this, &MyClass::onPageChanged);
 * connect(pagination, &PaginationWidget::pageSizeChanged, this, &MyClass::onPageSizeChanged);
 * @endcode
 * 
 * @author 1553BTools
 * @date 2026
 */

#ifndef PAGINATIONWIDGET_H
#define PAGINATIONWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QComboBox>
#include <QHBoxLayout>

/**
 * @brief 分页控件类
 * 
 * 该类提供了一个完整的分页控制界面，用于大数据表格的分页显示。
 */
class PaginationWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针
     */
    explicit PaginationWidget(QWidget* parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~PaginationWidget();
    
    /**
     * @brief 更新分页信息
     * @param currentPage 当前页码（从0开始）
     * @param totalPages 总页数
     * @param filteredCount 筛选后的记录总数
     * @param totalCount 原始记录总数
     */
    void updatePagination(int currentPage, int totalPages, int filteredCount, int totalCount);
    
    /**
     * @brief 获取当前页码
     * @return 当前页码（从0开始）
     */
    int currentPage() const { return m_currentPage; }
    
    /**
     * @brief 获取每页条数
     * @return 每页显示条数
     */
    int pageSize() const;

signals:
    /**
     * @brief 页码变化信号
     * @param page 新页码（从0开始）
     */
    void pageChanged(int page);
    
    /**
     * @brief 每页条数变化信号
     * @param size 新的每页条数
     */
    void pageSizeChanged(int size);

private slots:
    /**
     * @brief 上一页按钮点击槽函数
     */
    void onPrevPage();
    
    /**
     * @brief 下一页按钮点击槽函数
     */
    void onNextPage();
    
    /**
     * @brief 页码跳转槽函数
     * @param page 目标页码
     */
    void onJumpToPage(int page);
    
    /**
     * @brief 每页条数变化槽函数
     * @param index 下拉框索引
     */
    void onPageSizeChanged(int index);

private:
    /**
     * @brief 初始化UI
     */
    void setupUI();
    
    /**
     * @brief 更新按钮状态
     */
    void updateButtonStates();
    
    /**
     * @brief 更新统计信息显示
     */
    void updateStatistics();

private:
    // UI组件
    QLabel* m_totalCountLabel;      ///< 数据总数标签
    QLabel* m_filteredCountLabel;   ///< 筛选后总数标签
    QLabel* m_rangeLabel;           ///< 显示范围标签
    QPushButton* m_prevBtn;         ///< 上一页按钮
    QPushButton* m_nextBtn;         ///< 下一页按钮
    QLabel* m_pageLabel;            ///< 页码标签
    QSpinBox* m_pageSpinBox;        ///< 页码跳转输入框
    QLabel* m_totalPagesLabel;      ///< 总页数标签
    QLabel* m_pageSizeLabel;        ///< 每页条数标签
    QComboBox* m_pageSizeCombo;     ///< 每页条数下拉框
    
    // 分页数据
    int m_currentPage;              ///< 当前页码（从0开始）
    int m_totalPages;               ///< 总页数
    int m_filteredCount;            ///< 筛选后的记录总数
    int m_totalCount;               ///< 原始记录总数
};

#endif
