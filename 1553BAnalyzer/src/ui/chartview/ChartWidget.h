/**
 * @file ChartWidget.h
 * @brief 统计图表控件类定义
 * 
 * ChartWidget类提供数据可视化功能，支持多种图表类型：
 * - 饼图：展示数据分布比例
 * - 柱状图：展示数据量对比
 * - 折线图：展示数据趋势变化
 * 
 * 支持的统计主题：
 * - 消息类型分布：BC→RT、RT→BC、RT→RT、广播
 * - 终端数据量：各终端地址的数据条数
 * - 时间分布：按时间段统计数据量
 * - 通道状态：成功/失败比例
 * 
 * 使用示例：
 * @code
 * ChartWidget* chart = new ChartWidget(this);
 * chart->setDataStore(dataStore);
 * chart->setChartType("pie");
 * chart->setChartSubject(ChartSubject::MessageType);
 * chart->refreshChart();
 * @endcode
 * 
 * @author 1553BTools
 * @date 2024
 */

#ifndef CHARTWIDGET_H
#define CHARTWIDGET_H

#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QTimer>
#include "core/datastore/DataStore.h"
#include "core/analysis/TimeIntervalAnalyzer.h"

class QCustomPlot;

/**
 * @brief 图表统计主题枚举
 * 
 * 定义图表可以展示的数据维度
 */
enum class ChartSubject {
    MessageType,    // 消息类型分布（BC→RT、RT→BC、RT→RT、广播）
    Terminal,       // 终端数据量（各终端地址的数据条数）
    Time,           // 时间分布（按时间段统计数据量）
    Chstt           // 通道状态（成功/失败比例）
};

/**
 * @brief 统计图表控件类
 * 
 * 该类继承自QWidget，使用QCustomPlot库实现图表绘制。
 * 支持动态切换图表类型和统计主题。
 */
class ChartWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针
     * 
     * 初始化图表控件，设置默认参数
     */
    explicit ChartWidget(QWidget *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~ChartWidget();
    
    /**
     * @brief 设置数据存储对象
     * @param store DataStore对象指针
     * 
     * 设置数据源并建立信号连接，数据变化时自动刷新图表
     */
    void setDataStore(DataStore* store);
    
    void refreshChart();
    
    void scheduleRefresh();
    
    /**
     * @brief 设置统计主题
     * @param subject 统计主题（消息类型/终端/时间/状态）
     */
    void setChartSubject(ChartSubject subject);
    
    /**
     * @brief 设置图表类型
     * @param type 图表类型（"pie"/"bar"/"line"）
     */
    void setChartType(const QString& type);
    
    /**
     * @brief 绘制时间间隔分析折线图
     * @param analysis 时间间隔分析结果
     * 
     * 展示指定RT和子地址的时间间隔变化趋势，
     * 用于观察周期数据是否稳定发送
     */
    void drawTimeIntervalChart(const TimeIntervalAnalysis& analysis);

private slots:
    /**
     * @brief 图表类型变化槽
     * @param index 选择的图表类型索引
     * 
     * 响应用户切换图表类型的操作
     */
    void onChartTypeChanged(int index);
    
    /**
     * @brief 导出图表槽
     * 
     * 将当前图表导出为PNG图片
     */
    void onExportChart();

signals:
    /**
     * @brief 图表刷新完成信号
     * 
     * 当图表刷新完成时发出此信号，
     * 用于通知外部组件加载已完成。
     */
    void updateFinished();

protected:
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    /**
     * @brief 设置界面布局
     * 
     * 创建图表区域、类型选择器和导出按钮
     */
    void setupUI();
    
    /**
     * @brief 绘制饼图
     * 
     * 绘制饼图展示数据分布比例，支持以下主题：
     * - 消息类型分布
     * - 终端数据量分布
     * - 通道状态分布
     */
    void drawPieChart();
    
    /**
     * @brief 绘制柱状图
     * 
     * 绘制柱状图展示数据量对比，支持以下主题：
     * - 各终端数据量
     * - 各消息类型数据量
     */
    void drawBarChart();
    
    /**
     * @brief 绘制折线图
     * 
     * 绘制折线图展示数据趋势变化，支持以下主题：
     * - 时间序列数据量变化
     */
    void drawLineChart();
    
    /**
     * @brief 绘制通道状态饼图
     * 
     * 专门绘制通道状态的饼图，展示成功/失败比例
     */
    void drawChsttPieChart();
    
    DataStore* m_dataStore;         // 数据存储对象指针
    QCustomPlot* m_chart;           // QCustomPlot图表对象
    QComboBox* m_chartTypeCombo;    // 图表类型选择下拉框
    QPushButton* m_exportBtn;       // 导出按钮
    ChartSubject m_chartSubject;    // 当前统计主题
    QString m_chartType;            // 当前图表类型
    QSize m_lastDrawSize;           // 上次绘制时的控件尺寸
    bool m_dataDirty;               // 数据已变更但图表未刷新标志
    QTimer* m_refreshTimer;         // 防抖刷新定时器
};

#endif
