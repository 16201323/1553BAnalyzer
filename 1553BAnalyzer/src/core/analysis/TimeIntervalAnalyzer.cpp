/**
 * @file TimeIntervalAnalyzer.cpp
 * @brief 时间间隔分析器类实现
 * 
 * 本文件实现了TimeIntervalAnalyzer类的所有方法，包括：
 * - 数据筛选和时间间隔计算
 * - 统计分析（平均值、标准差、极值）
 * - 稳定性判定
 * - CSV导出
 */

#include "TimeIntervalAnalyzer.h"
#include "core/datastore/DataStore.h"
#include "utils/Logger.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <algorithm>
#include <cmath>

/**
 * @brief 将SMbiMonPacketData.timestamp（单位40微秒）转换为"时:分:秒.毫秒"格式字符串
 * 
 * 1553B原始时间戳为32位无符号整数，单位为40微秒。
 * 转换公式：总毫秒数 = timestamp × 40 / 1000
 * 然后将毫秒数分解为时、分、秒、毫秒。
 * 
 * @param rawTimestamp SMbiMonPacketData.timestamp，单位40微秒
 * @return 格式化的时间字符串，如 "01:23:45.678"
 */
static QString formatRawTimestamp(quint32 rawTimestamp)
{
    /* 将40微秒单位的时间戳转换为总毫秒数 */
    quint64 totalMs = static_cast<quint64>(rawTimestamp) * 40 / 1000;
    int milliseconds = static_cast<int>(totalMs % 1000);
    quint64 totalSeconds = totalMs / 1000;
    int hours = static_cast<int>(totalSeconds / 3600);
    int minutes = static_cast<int>((totalSeconds % 3600) / 60);
    int seconds = static_cast<int>(totalSeconds % 60);
    return QString("%1:%2:%3.%4")
               .arg(hours, 2, 10, QChar('0'))
               .arg(minutes, 2, 10, QChar('0'))
               .arg(seconds, 2, 10, QChar('0'))
               .arg(milliseconds, 3, 10, QChar('0'));
}

/**
 * @brief 分析指定RT和子地址的时间间隔
 * 
 * 实现步骤：
 * 1. 从DataStore获取所有记录
 * 2. 筛选指定终端地址和子地址的记录
 * 3. 按时间戳排序
 * 4. 计算相邻记录的时间间隔
 * 5. 进行统计分析
 */
TimeIntervalAnalysis TimeIntervalAnalyzer::analyze(
    DataStore* store,
    int terminalAddress,
    int subAddress,
    qint64 startTime,
    qint64 endTime)
{
    TimeIntervalAnalysis result;
    result.terminalAddress = terminalAddress;
    result.subAddress = subAddress;
    
    // 参数校验
    if (!store) {
        LOG_ERROR("TimeIntervalAnalyzer", QString::fromUtf8(u8"DataStore指针为空"));
        result.stabilityAssessment = QString::fromUtf8(u8"错误：数据存储对象无效");
        return result;
    }
    
    if (terminalAddress < 0 || terminalAddress > 31) {
        LOG_ERROR("TimeIntervalAnalyzer", QString::fromUtf8(u8"终端地址无效: %1").arg(terminalAddress));
        result.stabilityAssessment = QString::fromUtf8(u8"错误：终端地址无效（应在0-31范围内）");
        return result;
    }
    
    if (subAddress < 0 || subAddress > 31) {
        LOG_ERROR("TimeIntervalAnalyzer", QString::fromUtf8(u8"子地址无效: %1").arg(subAddress));
        result.stabilityAssessment = QString::fromUtf8(u8"错误：子地址无效（应在0-31范围内）");
        return result;
    }
    
    LOG_INFO("TimeIntervalAnalyzer", QString::fromUtf8(u8"开始分析 - RT: %1, 子地址: %2, 时间范围: %3-%4")
             .arg(terminalAddress).arg(subAddress)
             .arg(startTime).arg(endTime));
    
    // 获取所有记录
    QVector<DataRecord> allRecords = store->getAllRecordsForReport();
    
    if (allRecords.isEmpty()) {
        LOG_WARNING("TimeIntervalAnalyzer", QString::fromUtf8(u8"数据存储中没有记录"));
        result.stabilityAssessment = QString::fromUtf8(u8"无数据");
        return result;
    }
    
    // 筛选符合条件的记录
    QVector<DataRecord> filteredRecords;
    filteredRecords.reserve(allRecords.size());
    
    for (const DataRecord& record : allRecords) {
        // 检查终端地址和子地址
        if (record.terminalAddr != terminalAddress) {
            continue;
        }
        if (record.subAddr != subAddress) {
            continue;
        }
        
        // 检查时间范围
        if (startTime > 0 && record.timestampMs < startTime) {
            continue;
        }
        if (endTime > 0 && record.timestampMs > endTime) {
            continue;
        }
        
        filteredRecords.append(record);
    }
    
    result.recordCount = filteredRecords.size();
    LOG_INFO("TimeIntervalAnalyzer", QString::fromUtf8(u8"筛选后记录数: %1").arg(result.recordCount));
    
    // 至少需要2条记录才能计算时间间隔
    if (result.recordCount < 2) {
        LOG_WARNING("TimeIntervalAnalyzer", QString::fromUtf8(u8"记录数不足，无法计算时间间隔"));
        result.stabilityAssessment = QString::fromUtf8(u8"记录数不足（需要至少2条记录）");
        return result;
    }
    
    // 按时间戳排序
    std::sort(filteredRecords.begin(), filteredRecords.end(),
              [](const DataRecord& a, const DataRecord& b) {
                  return a.timestampMs < b.timestampMs;
              });
    
    // 提取时间戳序列，同时保存原始时间戳（SMbiMonPacketData.timestamp，单位40微秒）
    result.timestamps.reserve(result.recordCount);
    result.rawTimestamps.reserve(result.recordCount);
    for (const DataRecord& record : filteredRecords) {
        result.timestamps.append(record.timestampMs);
        result.rawTimestamps.append(record.packetData.timestamp);
    }
    
    // 计算时间间隔序列
    result.intervals.reserve(result.recordCount - 1);
    for (int i = 1; i < result.recordCount; ++i) {
        double interval = result.timestamps[i] - result.timestamps[i - 1];
        result.intervals.append(interval);
    }
    
    // 计算统计数据
    double sum = 0.0;
    result.minIntervalMs = result.intervals[0];
    result.maxIntervalMs = result.intervals[0];
    
    for (double interval : result.intervals) {
        sum += interval;
        if (interval < result.minIntervalMs) {
            result.minIntervalMs = interval;
        }
        if (interval > result.maxIntervalMs) {
            result.maxIntervalMs = interval;
        }
    }
    
    result.avgIntervalMs = sum / result.intervals.size();
    
    // 计算标准差
    result.stdDevMs = calculateStdDev(result.intervals, result.avgIntervalMs);
    
    // 计算抖动百分比
    if (result.avgIntervalMs > 0) {
        result.jitterPercent = (result.stdDevMs / result.avgIntervalMs) * 100.0;
    } else {
        result.jitterPercent = 0.0;
    }
    
    // 判断稳定性（抖动小于10%视为稳定）
    result.isStable = (result.jitterPercent < 10.0);
    
    // 生成稳定性评估描述
    result.stabilityAssessment = generateStabilityAssessment(result.jitterPercent);
    
    LOG_INFO("TimeIntervalAnalyzer", QString::fromUtf8(u8"分析完成 - 记录数: %1, 平均间隔: %2ms, 标准差: %3ms, 抖动: %4%, 稳定: %5")
             .arg(result.recordCount)
             .arg(result.avgIntervalMs, 0, 'f', 2)
             .arg(result.stdDevMs, 0, 'f', 2)
             .arg(result.jitterPercent, 0, 'f', 2)
             .arg(result.isStable));
    
    return result;
}

/**
 * @brief 导出分析结果为CSV文件
 * 
 * CSV格式：
 * - 第一行：标题行（序号,RT,子地址,数据包时间(时:分:秒.毫秒),与上一包间隔时间(单位：毫秒)）
 * - 后续行：数据行
 * 
 * 文件编码：UTF-8 with BOM
 */
bool TimeIntervalAnalyzer::exportToCsv(
    const TimeIntervalAnalysis& analysis,
    const QString& filePath)
{
    if (analysis.recordCount < 2) {
        LOG_ERROR("TimeIntervalAnalyzer", QString::fromUtf8(u8"记录数不足，无法导出"));
        return false;
    }
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        LOG_ERROR("TimeIntervalAnalyzer", QString::fromUtf8(u8"无法打开文件: %1").arg(filePath));
        return false;
    }
    
    QTextStream out(&file);
    out.setCodec("UTF-8");
    out.setGenerateByteOrderMark(true);  // 添加BOM以支持中文
    
    /* 写入标题行，包含序号、RT、子地址、数据包时间、间隔时间 */
    out << QString::fromUtf8(u8"序号,RT,子地址,数据包时间(时:分:秒.毫秒),与上一包间隔时间(单位：毫秒)\n");
    
    /* 写入数据行 */
    /* 数据包时间使用SMbiMonPacketData.timestamp（单位40微秒）转换为"时:分:秒.毫秒"格式 */
    /* 第一条记录没有时间间隔，间隔时间列为空 */
    out << QString("1,%1,%2,%3,\n")
           .arg(analysis.terminalAddress)
           .arg(analysis.subAddress)
           .arg(formatRawTimestamp(analysis.rawTimestamps[0]));
    
    for (int i = 1; i < analysis.recordCount; ++i) {
        out << QString("%1,%2,%3,%4,%5\n")
               .arg(i + 1)
               .arg(analysis.terminalAddress)
               .arg(analysis.subAddress)
               .arg(formatRawTimestamp(analysis.rawTimestamps[i]))
               .arg(analysis.intervals[i - 1], 0, 'f', 3);
    }
    
    file.close();
    
    LOG_INFO("TimeIntervalAnalyzer", QString::fromUtf8(u8"CSV导出成功: %1").arg(filePath));
    return true;
}

/**
 * @brief 生成稳定性评估描述
 * 
 * 根据抖动百分比生成人类可读的评估结果：
 * - < 5%: 非常稳定
 * - 5-10%: 稳定
 * - 10-20%: 轻微波动
 * - 20-50%: 明显波动
 * - > 50%: 不稳定
 */
QString TimeIntervalAnalyzer::generateStabilityAssessment(double jitterPercent)
{
    if (jitterPercent < 5.0) {
        return QString::fromUtf8(u8"非常稳定（抖动<5%）");
    } else if (jitterPercent < 10.0) {
        return QString::fromUtf8(u8"稳定（抖动5-10%）");
    } else if (jitterPercent < 20.0) {
        return QString::fromUtf8(u8"轻微波动（抖动10-20%）");
    } else if (jitterPercent < 50.0) {
        return QString::fromUtf8(u8"明显波动（抖动20-50%）");
    } else {
        return QString::fromUtf8(u8"不稳定（抖动>50%）");
    }
}

/**
 * @brief 计算标准差
 * 
 * 使用总体标准差公式：σ = sqrt(Σ(xi-μ)²/N)
 */
double TimeIntervalAnalyzer::calculateStdDev(const QVector<double>& values, double mean)
{
    if (values.isEmpty()) {
        return 0.0;
    }
    
    double sumSquaredDiff = 0.0;
    for (double value : values) {
        double diff = value - mean;
        sumSquaredDiff += diff * diff;
    }
    
    return std::sqrt(sumSquaredDiff / values.size());
}
