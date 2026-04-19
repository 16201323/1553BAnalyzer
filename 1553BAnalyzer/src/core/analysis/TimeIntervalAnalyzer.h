/**
 * @file TimeIntervalAnalyzer.h
 * @brief 时间间隔分析器类定义
 * 
 * TimeIntervalAnalyzer类用于分析指定RT和子地址的时间间隔，
 * 主要用途是观察周期数据是否稳定同时间间隔发送。
 * 
 * 主要功能：
 * - 筛选指定RT和子地址的数据记录
 * - 计算相邻记录的时间间隔
 * - 统计平均间隔、标准差、极值等
 * - 判断发送周期是否稳定
 * - 导出CSV文件供外部分析
 * 
 * 使用示例：
 * @code
 * TimeIntervalAnalysis result = TimeIntervalAnalyzer::analyze(
 *     dataStore, 5, 10, 0, 0);
 * if (result.recordCount > 1) {
 *     qDebug() << u8"平均间隔:" << result.avgIntervalMs << u8"ms";
 *     qDebug() << u8"是否稳定:" << result.isStable;
 * }
 * @endcode
 * 
 * @author 1553BTools
 * @date 2024
 */

#ifndef TIMEINTERVALANALYZER_H
#define TIMEINTERVALANALYZER_H

#include <QtGlobal>
#include <QVector>
#include <QString>

class DataStore;

/**
 * @brief 时间间隔分析结果结构
 * 
 * 存储时间间隔分析的完整结果，包括统计数据和原始数据序列
 */
struct TimeIntervalAnalysis {
    int terminalAddress;                    ///< 终端地址（0-31）
    int subAddress;                         ///< 子地址（0-31）
    int recordCount;                        ///< 记录总数
    double avgIntervalMs;                   ///< 平均时间间隔（毫秒）
    double minIntervalMs;                   ///< 最小时间间隔（毫秒）
    double maxIntervalMs;                   ///< 最大时间间隔（毫秒）
    double stdDevMs;                        ///< 标准差（毫秒）
    double jitterPercent;                   ///< 抖动百分比（标准差/平均值*100%）
    QVector<double> timestamps;             ///< 时间戳序列（毫秒）
    QVector<double> intervals;              ///< 时间间隔序列（毫秒）
    bool isStable;                          ///< 是否稳定（抖动<10%）
    QString stabilityAssessment;            ///< 稳定性评估描述
    
    /**
     * @brief 默认构造函数
     * 
     * 初始化所有成员为默认值
     */
    TimeIntervalAnalysis()
        : terminalAddress(-1)
        , subAddress(-1)
        , recordCount(0)
        , avgIntervalMs(0.0)
        , minIntervalMs(0.0)
        , maxIntervalMs(0.0)
        , stdDevMs(0.0)
        , jitterPercent(0.0)
        , isStable(false)
    {}
};

/**
 * @brief 时间间隔分析器类
 * 
 * 提供时间间隔分析的核心功能，用于判断周期数据发送是否稳定
 */
class TimeIntervalAnalyzer
{
public:
    /**
     * @brief 分析指定RT和子地址的时间间隔
     * 
     * 从DataStore中筛选指定终端地址和子地址的数据记录，
     * 按时间戳排序后计算相邻记录的时间间隔，
     * 并进行统计分析。
     * 
     * @param store DataStore对象指针
     * @param terminalAddress 终端地址（0-31）
     * @param subAddress 子地址（0-31）
     * @param startTime 起始时间戳（毫秒），0表示不限制
     * @param endTime 结束时间戳（毫秒），0表示不限制
     * @return 分析结果结构
     * 
     * @note 至少需要2条记录才能计算时间间隔
     */
    static TimeIntervalAnalysis analyze(
        DataStore* store,
        int terminalAddress,
        int subAddress,
        qint64 startTime = 0,
        qint64 endTime = 0
    );
    
    /**
     * @brief 导出分析结果为CSV文件
     * 
     * 将时间间隔分析结果导出为CSV格式文件，
     * 包含序号、时间戳、时间间隔三列数据。
     * 
     * @param analysis 分析结果
     * @param filePath 目标文件路径
     * @return 是否导出成功
     */
    static bool exportToCsv(
        const TimeIntervalAnalysis& analysis,
        const QString& filePath
    );
    
    /**
     * @brief 生成稳定性评估描述
     * 
     * 根据抖动百分比生成人类可读的稳定性评估描述
     * 
     * @param jitterPercent 抖动百分比
     * @return 稳定性评估描述字符串
     */
    static QString generateStabilityAssessment(double jitterPercent);

private:
    /**
     * @brief 计算标准差
     * 
     * 计算数据序列的标准差
     * 
     * @param values 数据序列
     * @param mean 平均值
     * @return 标准差
     */
    static double calculateStdDev(const QVector<double>& values, double mean);
};

#endif
