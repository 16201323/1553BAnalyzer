/**
 * @file AIToolExecutor.h
 * @brief AI工具执行器类定义
 * 
 * AIToolExecutor类负责执行AI模型返回的工具调用。
 * 当AI分析用户请求后，会返回结构化的工具调用指令，
 * 本类负责解析并执行这些指令。
 * 
 * 支持的工具：
 * - query_data：查询数据，支持多条件筛选
 * - generate_chart：生成统计图表（饼图/柱状图/折线图）
 * - generate_gantt：生成甘特图
 * - get_statistics：获取统计信息
 * - clear_filter：清除筛选条件
 * 
 * 工作流程：
 * 1. AI返回工具调用JSON
 * 2. MainWindow调用executeTool执行工具
 * 3. 工具执行结果返回给AI或直接更新界面
 * 
 * 使用示例：
 * @code
 * AIToolExecutor* executor = new AIToolExecutor(this);
 * executor->setDataStore(dataStore);
 * 
 * QJsonObject args;
 * args["terminal"] = 5;
 * ToolResult result = executor->executeTool("query_data", args);
 * @endcode
 * 
 * @author 1553BTools
 * @date 2024
 */

#ifndef AITOOLEXECUTOR_H
#define AITOOLEXECUTOR_H

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include "core/datastore/DataStore.h"
#include "core/analysis/TimeIntervalAnalyzer.h"

/**
 * @brief 工具执行结果结构
 * 
 * 存储工具执行后的返回结果，包括成功状态、消息和数据
 */
struct ToolResult
{
    bool success;           // 执行是否成功
    QString message;        // 结果消息或错误描述
    QJsonObject data;       // 返回的数据（JSON格式）
    QString actionType;     // 动作类型（用于界面更新）
};

/**
 * @brief AI工具执行器类
 * 
 * 该类作为AI与数据处理之间的桥梁，将AI的工具调用
 * 转换为实际的数据操作和界面更新。
 */
class AIToolExecutor : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象指针
     */
    explicit AIToolExecutor(QObject *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~AIToolExecutor();
    
    /**
     * @brief 设置数据存储对象
     * @param store DataStore对象指针
     * 
     * 设置数据源，工具执行时从此获取数据
     */
    void setDataStore(DataStore* store);
    
    /**
     * @brief 执行工具调用
     * @param toolName 工具名称
     * @param arguments 工具参数（JSON对象）
     * @return 执行结果
     * 
     * 根据工具名称分发到对应的执行方法：
     * - query_data -> executeQueryData
     * - generate_chart -> executeGenerateChart
     * - generate_gantt -> executeGenerateGantt
     * - get_statistics -> executeGetStatistics
     * - clear_filter -> executeClearFilter
     */
    ToolResult executeTool(const QString& toolName, const QJsonObject& arguments);
    
signals:
    /**
     * @brief 查询数据请求信号
     * @param filters 筛选条件JSON对象
     * 
     * 请求MainWindow更新数据筛选条件
     */
    void queryDataRequested(const QJsonObject& filters);
    
    /**
     * @brief 生成图表请求信号
     * @param chartType 图表类型（pie/bar/line）
     * @param subject 统计主题（message_type/terminal/time）
     * @param title 图表标题
     * 
     * 请求MainWindow生成并显示统计图表
     */
    void generateChartRequested(const QString& chartType, const QString& subject, const QString& title);
    
    /**
     * @brief 生成甘特图请求信号
     * @param filters 筛选条件JSON对象
     * 
     * 请求MainWindow生成并显示甘特图
     */
    void generateGanttRequested(const QJsonObject& filters);
    
    /**
     * @brief 清除筛选条件请求信号
     * 
     * 请求MainWindow清除所有筛选条件
     */
    void clearFilterRequested();
    
    /**
     * @brief 切换到图表选项卡请求信号
     * 
     * 请求MainWindow切换到统计图表选项卡
     */
    void switchToChartTabRequested();
    
    /**
     * @brief 切换到甘特图选项卡请求信号
     * 
     * 请求MainWindow切换到甘特图选项卡
     */
    void switchToGanttTabRequested();
    
    /**
     * @brief 切换到表格选项卡请求信号
     * 
     * 请求MainWindow切换到数据表格选项卡
     */
    void switchToTableTabRequested();
    
    void generateReportRequested(const QString& format);
    
    /**
     * @brief 时间间隔分析请求信号
     * @param analysis 分析结果结构
     * 
     * 请求MainWindow显示时间间隔分析图表
     */
    void timeIntervalAnalysisRequested(const TimeIntervalAnalysis& analysis);

private:
    /**
     * @brief 执行数据查询工具
     * @param args 查询参数
     * @return 执行结果
     * 
     * 支持的参数：
     * - terminal: 终端地址（整数或数组）
     * - message_type: 消息类型（字符串或数组）
     * - time_start: 起始时间戳
     * - time_end: 结束时间戳
     * - sub_address: 子地址
     */
    ToolResult executeQueryData(const QJsonObject& args);
    
    /**
     * @brief 执行生成图表工具
     * @param args 图表参数
     * @return 执行结果
     * 
     * 支持的参数：
     * - chart_type: 图表类型（pie/bar/line）
     * - subject: 统计主题（message_type/terminal/time/chstt）
     * - title: 图表标题（可选）
     */
    ToolResult executeGenerateChart(const QJsonObject& args);
    
    /**
     * @brief 执行生成甘特图工具
     * @param args 甘特图参数
     * @return 执行结果
     * 
     * 支持的参数：
     * - filters: 筛选条件（可选）
     */
    ToolResult executeGenerateGantt(const QJsonObject& args);
    
    /**
     * @brief 执行获取统计信息工具
     * @param args 统计参数
     * @return 执行结果
     * 
     * 返回数据的统计摘要，包括：
     * - 总数据量
     * - 各终端数据量
     * - 各消息类型数据量
     * - 时间范围
     */
    ToolResult executeGetStatistics(const QJsonObject& args);
    
    /**
     * @brief 执行清除筛选条件工具
     * @param args 无参数
     * @return 执行结果
     */
    ToolResult executeClearFilter(const QJsonObject& args);
    
    ToolResult executeAddFilter(const QJsonObject& args);
    
    ToolResult executeApplyFiltersBatch(const QJsonObject& args);
    
    ToolResult executePreviewFilter(const QJsonObject& args);
    
    ToolResult executeGetAnomalies(const QJsonObject& args);
    
    ToolResult executeGetTopN(const QJsonObject& args);
    
    ToolResult executeGenerateReport(const QJsonObject& args);
    
    /**
     * @brief 执行时间间隔分析工具
     * @param args 分析参数
     * @return 执行结果
     * 
     * 支持的参数：
     * - terminal_address: 终端地址（必填，0-31）
     * - sub_address: 子地址（必填，0-31）
     * - time_range: 时间范围（可选）
     * - export_csv: 是否导出CSV（可选）
     */
    ToolResult executeAnalyzeTimeInterval(const QJsonObject& args);
    
    void applyFilterArgs(const QJsonObject& args, bool clearExisting);
    
    qint64 countFilteredRecords(const QJsonObject& args);
    
    /**
     * @brief 格式化统计结果
     * @param stats 统计数据JSON对象
     * @return 格式化后的文本描述
     * 
     * 将统计数据转换为人类可读的文本格式
     */
    QString formatStatisticsResult(const QJsonObject& stats);
    
    DataStore* m_dataStore;     ///< 数据存储对象指针
};

#endif
