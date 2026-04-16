/**
 * @file PaginationWidget.cpp
 * @brief 分页控件类实现
 * 
 * 实现了数据表格的分页控制功能，
 * 提供数据统计、页码导航和每页条数设置。
 * 
 * @author 1553BTools
 * @date 2026
 */

#include "PaginationWidget.h"
#include <QDebug>

/**
 * @brief 构造函数
 * @param parent 父窗口指针
 * 
 * 初始化UI组件和分页数据
 */
PaginationWidget::PaginationWidget(QWidget* parent)
    : QWidget(parent)
    , m_totalCountLabel(nullptr)
    , m_filteredCountLabel(nullptr)
    , m_rangeLabel(nullptr)
    , m_prevBtn(nullptr)
    , m_nextBtn(nullptr)
    , m_pageLabel(nullptr)
    , m_pageSpinBox(nullptr)
    , m_totalPagesLabel(nullptr)
    , m_pageSizeLabel(nullptr)
    , m_pageSizeCombo(nullptr)
    , m_currentPage(0)
    , m_totalPages(0)
    , m_filteredCount(0)
    , m_totalCount(0)
{
    setupUI();
}

/**
 * @brief 析构函数
 */
PaginationWidget::~PaginationWidget()
{
}

/**
 * @brief 初始化UI
 * 
 * 创建并布局所有UI组件：
 * - 数据统计信息
 * - 页码导航按钮
 * - 页码跳转输入框
 * - 每页条数下拉框
 */
void PaginationWidget::setupUI()
{
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(10, 5, 10, 5);
    mainLayout->setSpacing(15);
    
    // 左侧：数据统计信息
    m_totalCountLabel = new QLabel(tr("数据总数: 0 条"));
    m_totalCountLabel->setStyleSheet("QLabel { color: #2c3e50; font-weight: bold; }");
    mainLayout->addWidget(m_totalCountLabel);
    
    m_filteredCountLabel = new QLabel(tr("筛选后: 0 条"));
    m_filteredCountLabel->setStyleSheet("QLabel { color: #3498db; font-weight: bold; }");
    mainLayout->addWidget(m_filteredCountLabel);
    
    m_rangeLabel = new QLabel(tr("显示: 0-0 条"));
    m_rangeLabel->setStyleSheet("QLabel { color: #7f8c8d; }");
    mainLayout->addWidget(m_rangeLabel);
    
    // 添加弹性空间
    mainLayout->addStretch();
    
    // 中间：页码导航
    m_prevBtn = new QPushButton(tr("上一页"));
    m_prevBtn->setFixedSize(80, 30);
    m_prevBtn->setEnabled(false);
    m_prevBtn->setStyleSheet(
        "QPushButton {"
        "   background-color: #3498db;"
        "   color: white;"
        "   border: none;"
        "   border-radius: 4px;"
        "   font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #2980b9;"
        "}"
        "QPushButton:disabled {"
        "   background-color: #bdc3c7;"
        "}"
    );
    connect(m_prevBtn, &QPushButton::clicked, this, &PaginationWidget::onPrevPage);
    mainLayout->addWidget(m_prevBtn);
    
    // 页码显示和跳转
    m_pageLabel = new QLabel(tr("第"));
    mainLayout->addWidget(m_pageLabel);
    
    m_pageSpinBox = new QSpinBox();
    m_pageSpinBox->setMinimum(1);
    m_pageSpinBox->setMaximum(1);
    m_pageSpinBox->setValue(1);
    m_pageSpinBox->setFixedSize(60, 30);
    m_pageSpinBox->setStyleSheet(
        "QSpinBox {"
        "   border: 1px solid #bdc3c7;"
        "   border-radius: 4px;"
        "   padding: 2px;"
        "}"
    );
    connect(m_pageSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &PaginationWidget::onJumpToPage);
    mainLayout->addWidget(m_pageSpinBox);
    
    m_totalPagesLabel = new QLabel(tr("页 / 共 0 页"));
    mainLayout->addWidget(m_totalPagesLabel);
    
    m_nextBtn = new QPushButton(tr("下一页"));
    m_nextBtn->setFixedSize(80, 30);
    m_nextBtn->setEnabled(false);
    m_nextBtn->setStyleSheet(
        "QPushButton {"
        "   background-color: #3498db;"
        "   color: white;"
        "   border: none;"
        "   border-radius: 4px;"
        "   font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #2980b9;"
        "}"
        "QPushButton:disabled {"
        "   background-color: #bdc3c7;"
        "}"
    );
    connect(m_nextBtn, &QPushButton::clicked, this, &PaginationWidget::onNextPage);
    mainLayout->addWidget(m_nextBtn);
    
    // 添加弹性空间
    mainLayout->addStretch();
    
    // 右侧：每页条数设置
    m_pageSizeLabel = new QLabel(tr("每页显示:"));
    mainLayout->addWidget(m_pageSizeLabel);
    
    m_pageSizeCombo = new QComboBox();
    m_pageSizeCombo->addItem("1000", 1000);
    m_pageSizeCombo->addItem("2000", 2000);
    m_pageSizeCombo->addItem("5000", 5000);
    m_pageSizeCombo->addItem("10000", 10000);
    m_pageSizeCombo->addItem("20000", 20000);
    m_pageSizeCombo->setCurrentIndex(0);  // 默认选中
    m_pageSizeCombo->setFixedSize(80, 30);
    m_pageSizeCombo->setStyleSheet(
        "QComboBox {"
        "   border: 1px solid #bdc3c7;"
        "   border-radius: 4px;"
        "   padding: 2px;"
        "}"
        "QComboBox::drop-down {"
        "   border: none;"
        "}"
        "QComboBox::down-arrow {"
        "   image: none;"
        "   border-left: 4px solid transparent;"
        "   border-right: 4px solid transparent;"
        "   border-top: 6px solid #3498db;"
        "   margin-right: 5px;"
        "}"
    );
    connect(m_pageSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PaginationWidget::onPageSizeChanged);
    mainLayout->addWidget(m_pageSizeCombo);
    
    QLabel* unitLabel = new QLabel(tr("条"));
    mainLayout->addWidget(unitLabel);
}

/**
 * @brief 更新分页信息
 * @param currentPage 当前页码（从0开始）
 * @param totalPages 总页数
 * @param filteredCount 筛选后的记录总数
 * @param totalCount 原始记录总数
 */
void PaginationWidget::updatePagination(int currentPage, int totalPages, int filteredCount, int totalCount)
{
    m_currentPage = currentPage;
    m_totalPages = totalPages;
    m_filteredCount = filteredCount;
    m_totalCount = totalCount;
    
    // 更新统计信息
    updateStatistics();
    
    // 更新页码显示
    m_pageSpinBox->blockSignals(true);
    m_pageSpinBox->setMaximum(qMax(1, totalPages));
    m_pageSpinBox->setValue(currentPage + 1);  // 显示时从1开始
    m_pageSpinBox->blockSignals(false);
    
    m_totalPagesLabel->setText(tr("页 / 共 %1 页").arg(totalPages));
    
    // 更新按钮状态
    updateButtonStates();
}

/**
 * @brief 获取每页条数
 * @return 每页显示条数
 */
int PaginationWidget::pageSize() const
{
    return m_pageSizeCombo->currentData().toInt();
}

/**
 * @brief 上一页按钮点击槽函数
 */
void PaginationWidget::onPrevPage()
{
    if (m_currentPage > 0) {
        emit pageChanged(m_currentPage - 1);
    }
}

/**
 * @brief 下一页按钮点击槽函数
 */
void PaginationWidget::onNextPage()
{
    if (m_currentPage < m_totalPages - 1) {
        emit pageChanged(m_currentPage + 1);
    }
}

/**
 * @brief 页码跳转槽函数
 * @param page 目标页码（从1开始）
 */
void PaginationWidget::onJumpToPage(int page)
{
    // pageSpinBox显示的是从1开始的页码，需要转换为从0开始
    emit pageChanged(page - 1);
}

/**
 * @brief 每页条数变化槽函数
 * @param index 下拉框索引
 */
void PaginationWidget::onPageSizeChanged(int index)
{
    Q_UNUSED(index);
    emit pageSizeChanged(pageSize());
}

/**
 * @brief 更新按钮状态
 */
void PaginationWidget::updateButtonStates()
{
    m_prevBtn->setEnabled(m_currentPage > 0);
    m_nextBtn->setEnabled(m_currentPage < m_totalPages - 1);
}

/**
 * @brief 更新统计信息显示
 */
void PaginationWidget::updateStatistics()
{
    // 更新数据总数
    m_totalCountLabel->setText(tr("数据总数: %1 条").arg(m_totalCount));
    
    // 更新筛选后总数
    if (m_filteredCount != m_totalCount) {
        m_filteredCountLabel->setText(tr("筛选后: %1 条").arg(m_filteredCount));
        m_filteredCountLabel->show();
    } else {
        m_filteredCountLabel->hide();
    }
    
    // 更新显示范围
    if (m_filteredCount > 0 && m_totalPages > 0) {
        int pageSize = this->pageSize();
        int start = m_currentPage * pageSize + 1;
        int end = qMin(start + pageSize - 1, m_filteredCount);
        m_rangeLabel->setText(tr("显示: %1-%2 条").arg(start).arg(end));
    } else {
        m_rangeLabel->setText(tr("显示: 0-0 条"));
    }
}
