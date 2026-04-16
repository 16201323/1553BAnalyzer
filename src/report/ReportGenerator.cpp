/**
 * @file ReportGenerator.cpp
 * @brief 智能分析报告生成器实现
 *
 * 本文件实现了ReportGenerator类的所有方法，包括：
 * - 异步报告生成主流程（generateReportAsync）
 * - 单次遍历统计数据收集（collectAllStatsSinglePass）
 * - 并行AI分析（performAIAnalysisParallel）
 * - 五种图表生成（饼图/柱状图/折线图/错误分布图/成功率图）
 * - HTML/PDF/DOCX三种格式导出
 * - HTML到DOCX的XML转换
 *
 * 关键设计决策：
 * 1. 使用QImage而非QPixmap生成图表（线程安全，可在后台线程使用）
 * 2. 单次遍历收集所有统计指标（性能优化，避免多次遍历大数据集）
 * 3. AI分析使用QtConcurrent并行执行7个独立任务
 * 4. DOCX导出手动构建ZIP文件（避免依赖第三方库）
 * 5. CRC32表使用std::array静态初始化（避免返回局部变量地址）
 *
 * @author 1553BTools
 * @date 2024
 */

#include "ReportGenerator.h"
#include "ui/widgets/AIQueryPanel.h"
#include "utils/Logger.h"
#include "core/config/ConfigManager.h"
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QPair>
#include <QPainter>
#include <QPrinter>
#include <QPageSize>
#include <QPageLayout>
#include <QPainterPath>
#include <QApplication>
#include <QElapsedTimer>
#include <QtConcurrent>
#include <QThreadPool>
#include <QBuffer>
#include <QRegularExpression>
#include <array>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextFragment>
#include <QTextCursor>
#include <QTextTable>
#include <QAbstractTextDocumentLayout>
#include <cmath>
#include <algorithm>
#include <random>
#include <climits>
#include "core/parser/PacketStruct.h"

/**
 * @brief 图表扇区数据
 *
 * 用于饼图绘制的单个扇区信息
 */
struct ChartSlice {
    QString label;
    double value;
    QColor color;
};

static QString ensureContentAfterHeadings(const QString& content)
{
    QString result = content;
    QRegularExpression headingRegex("<h[34][^>]*>([^<]*)</h[34]>", QRegularExpression::CaseInsensitiveOption);
    QRegularExpression nextHeadingOrEndRegex("<h[34][^>]*>|</div>|$");
    int offset = 0;
    while (offset < result.size()) {
        auto match = headingRegex.match(result, offset);
        if (!match.hasMatch()) break;
        int headingEnd = match.capturedEnd();
        auto nextMatch = nextHeadingOrEndRegex.match(result, headingEnd);
        if (nextMatch.hasMatch()) {
            QString between = result.mid(headingEnd, nextMatch.capturedStart() - headingEnd);
            between.remove(QRegularExpression("<[^>]*>"));
            if (between.trimmed().isEmpty()) {
                result.insert(headingEnd, "<p>暂无相关内容。</p>");
                offset = headingEnd + QString("<p>暂无相关内容。</p>").length();
                continue;
            }
        }
        offset = match.capturedEnd();
    }
    return result;
}

ReportGenerator::ReportGenerator(QObject *parent)
    : QObject(parent)
    , m_dataStore(nullptr)
    , m_modelProvider(nullptr)
    , m_cancelFlag(false)
{
}

ReportGenerator::~ReportGenerator()
{
}

void ReportGenerator::setDataStore(DataStore* store)
{
    m_dataStore = store;
}

void ReportGenerator::cancelReport()
{
    m_cancelFlag = true;
    LOG_INFO("ReportGenerator", "报告生成已请求取消");
}

void ReportGenerator::setModelProvider(ModelAdapter* provider)
{
    m_modelProvider = provider;
}

QString ReportGenerator::lastError() const
{
    return m_lastError;
}

void ReportGenerator::generateReportAsync(const QString& filePath, const QString& format)
{
    m_cancelFlag = false;
    m_startTime = QDateTime::currentMSecsSinceEpoch();

    LOG_INFO("ReportGenerator", QString("[报告生成] 开始生成智能分析报告, 格式: %1").arg(format));

    // 在后台线程中执行报告生成，避免阻塞UI
    // 使用(void)消除QtConcurrent::run返回值未使用的编译警告
    (void)QtConcurrent::run([this, filePath, format]() {
        ReportData data;
        qint64 stepStartTime = 0;

        stepStartTime = QDateTime::currentMSecsSinceEpoch();
        QMetaObject::invokeMethod(this, [this]() {
            emit progressChanged(3, "加载数据...", 0);
        }, Qt::QueuedConnection);

        // ===== 步骤1：加载全量数据 =====
        // 从DataStore获取所有记录的副本，用于后续统计和图表生成
        // 在后台线程中操作数据的只读副本，不修改原始数据
        QVector<DataRecord> cachedRecords = m_dataStore->getAllRecordsForReport();
        m_cachedRecords = cachedRecords;
        qint64 loadTime = QDateTime::currentMSecsSinceEpoch() - stepStartTime;
        LOG_INFO("ReportGenerator", QString("[报告生成] 全量数据加载完成, 记录数: %1, 耗时: %2 ms").arg(cachedRecords.size()).arg(loadTime));

        if (m_cancelFlag) {
            QMetaObject::invokeMethod(this, [this]() {
                m_lastError = tr("报告生成已取消");
                emit reportFinished(false, "");
            }, Qt::QueuedConnection);
            return;
        }

        stepStartTime = QDateTime::currentMSecsSinceEpoch();
        QMetaObject::invokeMethod(this, [this]() {
            emit progressChanged(8, "单次遍历收集统计数据...", 0);
        }, Qt::QueuedConnection);

        // ===== 步骤2：单次遍历收集所有统计数据 =====
        // 性能优化：只遍历数据一次，同时计算概览、消息统计、终端统计、
        // 错误统计、性能指标、通信模式和关键事件
        collectAllStatsSinglePass(cachedRecords, data);

        qint64 statsTime = QDateTime::currentMSecsSinceEpoch() - stepStartTime;
        LOG_INFO("ReportGenerator", QString("[报告生成] 统计数据收集完成, 耗时: %1 ms, 错误数: %2, 错误率: %3%, 成功率: %4%, 关键事件: %5")
            .arg(statsTime).arg(data.errorStats.totalErrors)
            .arg(QString::number(data.errorStats.errorRate, 'f', 2))
            .arg(QString::number(data.overview.successRate, 'f', 2))
            .arg(data.keyEvents.size()));

        if (m_cancelFlag) {
            QMetaObject::invokeMethod(this, [this]() {
                m_lastError = tr("报告生成已取消");
                emit reportFinished(false, "");
            }, Qt::QueuedConnection);
            return;
        }

        stepStartTime = QDateTime::currentMSecsSinceEpoch();
        QMetaObject::invokeMethod(this, [this]() {
            emit progressChanged(30, "生成可视化图表...", 0);
        }, Qt::QueuedConnection);

        // ===== 步骤3：生成可视化图表（并行） =====
        // 使用QImage（非QPixmap）确保线程安全
        // 图表存入data.charts映射表，后续导出时通过键名引用
        // 使用1倍分辨率快速渲染预览图，PDF导出时会以2倍分辨率重新渲染
        {
            QThreadPool chartPool;
            chartPool.setMaxThreadCount(5);

            MessageStats msgStatsCopy = data.messageStats;
            QVector<TerminalStats> termStatsCopy = data.terminalStats;
            QVector<DataRecord> recordsCopy = cachedRecords;
            ErrorStats errStatsCopy = data.errorStats;

            QFuture<QImage> pieFuture = QtConcurrent::run(&chartPool, [msgStatsCopy, this]() {
                return cropImageMargins(generateMessageTypePieChart(msgStatsCopy, 1));
            });
            QFuture<QImage> barFuture = QtConcurrent::run(&chartPool, [termStatsCopy, this]() {
                return cropImageMargins(generateTerminalBarChart(termStatsCopy, 1));
            });
            QFuture<QImage> tsFuture = QtConcurrent::run(&chartPool, [recordsCopy, this]() {
                return cropImageMargins(generateTimeSeriesChart(recordsCopy, 1));
            });
            QFuture<QImage> errFuture = QtConcurrent::run(&chartPool, [errStatsCopy, this]() {
                return cropImageMargins(generateErrorDistributionChart(errStatsCopy, 1));
            });
            QFuture<QImage> srFuture = QtConcurrent::run(&chartPool, [termStatsCopy, this]() {
                return cropImageMargins(generateSuccessRateChart(termStatsCopy, 1));
            });

            data.charts["messageTypePie"] = pieFuture.result();
            data.charts["terminalBar"] = barFuture.result();
            data.charts["timeSeries"] = tsFuture.result();
            data.charts["errorDistribution"] = errFuture.result();
            data.charts["successRate"] = srFuture.result();
        }

        qint64 chartTime = QDateTime::currentMSecsSinceEpoch() - stepStartTime;
        LOG_INFO("ReportGenerator", QString("[报告生成] 图表生成完成, 耗时: %1 ms").arg(chartTime));

        if (m_cancelFlag) {
            QMetaObject::invokeMethod(this, [this]() {
                m_lastError = tr("报告生成已取消");
                emit reportFinished(false, "");
            }, Qt::QueuedConnection);
            return;
        }

        stepStartTime = QDateTime::currentMSecsSinceEpoch();
        QMetaObject::invokeMethod(this, [this]() {
            emit progressChanged(45, "AI深度分析中（并行）...", 0);
        }, Qt::QueuedConnection);

        // ===== 步骤4：AI深度分析（并行） =====
        // 使用QtConcurrent并行调用7个AI分析任务
        // 每个任务独立调用AI接口，互不依赖，总耗时≈单个任务耗时
        data.aiAnalysis = performAIAnalysisParallel(data);

        qint64 aiTime = QDateTime::currentMSecsSinceEpoch() - stepStartTime;
        LOG_INFO("ReportGenerator", QString("[报告生成] AI分析完成, 耗时: %1 ms").arg(aiTime));

        if (m_cancelFlag) {
            QMetaObject::invokeMethod(this, [this]() {
                m_lastError = tr("报告生成已取消");
                emit reportFinished(false, "");
            }, Qt::QueuedConnection);
            return;
        }

        QMetaObject::invokeMethod(this, [this]() {
            emit progressChanged(85, "生成报告文件...", 0);
        }, Qt::QueuedConnection);

        // ===== 步骤5：根据格式导出报告文件 =====
        // 先移除filePath中可能已有的报告格式扩展名，避免产生.html.docx等双重扩展名
        bool success = false;
        QString exportPath = filePath;
        if (exportPath.endsWith(".html", Qt::CaseInsensitive)) {
            exportPath.chop(5);
        } else if (exportPath.endsWith(".docx", Qt::CaseInsensitive)) {
            exportPath.chop(5);
        } else if (exportPath.endsWith(".pdf", Qt::CaseInsensitive)) {
            exportPath.chop(4);
        }

        // 根据format参数添加正确的扩展名
        if (format == "pdf") {
            exportPath += ".pdf";
            success = exportToPdf(exportPath, data);
        } else if (format == "docx") {
            exportPath += ".docx";
            success = exportToDocx(exportPath, data);
        } else {
            exportPath += ".html";
            success = exportToHtml(exportPath, data);
        }

        qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - m_startTime;
        double seconds = elapsed / 1000.0;

        LOG_INFO("ReportGenerator", QString("[报告生成] 报告生成完成, 总耗时: %1 秒, 数据加载: %2 秒, 统计收集: %3 秒, 图表生成: %4 秒, AI分析: %5 秒")
            .arg(seconds).arg(loadTime/1000.0).arg(statsTime/1000.0).arg(chartTime/1000.0).arg(aiTime/1000.0));

        QMetaObject::invokeMethod(this, [this, success, filePath, seconds]() {
            emit progressChanged(100, "完成", seconds);
            emit reportFinished(success, filePath);
        }, Qt::QueuedConnection);
    });
}

// ========== 辅助方法：格式化时间戳为可读字符串 ==========
QString ReportGenerator::formatTimestamp(double timestampMs)
{
    qint64 ms = static_cast<qint64>(timestampMs);
    int h = ms / 3600000;
    int m = (ms % 3600000) / 60000;
    int s = (ms % 60000) / 1000;
    int mis = ms % 1000;
    return QString("%1:%2:%3.%4")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'))
        .arg(mis, 3, 10, QChar('0'));
}

QString ReportGenerator::buildRecommendationPrompt(const ReportData& data) const
{
    int mostActiveAddr = 0;
    double mostActivePercent = 0;
    int maxMsgCount = 0;
    for (const auto& ts : data.terminalStats) {
        if (ts.messageCount > maxMsgCount) {
            maxMsgCount = ts.messageCount;
            mostActiveAddr = ts.terminalAddress;
            mostActivePercent = ts.percent;
        }
    }

    return QString(
        "你是1553B总线系统优化专家。请基于以下全面分析数据，生成综合优化建议：\n\n"
        "数据概览：\n"
        "- 数据量：%1 条，时长 %2 秒\n"
        "- 成功率：%3%\n"
        "- 数据质量：%4\n\n"
        "主要发现：\n"
        "- 消息类型：BC→RT %5%，RT→BC %6%\n"
        "- 最活跃终端：RT%7（%8%消息）\n"
        "- 错误率：%9%\n"
        "- 总线利用率：%10%\n\n"
        "请生成综合建议报告（500-600字）：\n"
        "1. 总体评价（100字）\n"
        "2. 关键问题总结（150字）\n"
        "3. 优化建议（分短期/中期/长期，200字）\n"
        "4. 预期效果（100字）\n"
        "5. 后续行动计划（50字）\n\n"
        "要求：全面系统、重点突出、可操作性强。每个要点必须用<h4>标签作为小标题，然后在下方用<p>标签输出详细内容，确保每个小标题下都有具体内容，只允许使用的标签：<h4></h4>、<p></p>、<ul></ul>、<li></li>、<br/>。"
    ).arg(data.overview.totalRecords)
     .arg(data.overview.duration, 0, 'f', 1)
     .arg(data.overview.successRate, 0, 'f', 1)
     .arg(data.overview.dataQuality)
     .arg(data.messageStats.bcToRtPercent, 0, 'f', 1)
     .arg(data.messageStats.rtToBcPercent, 0, 'f', 1)
     .arg(data.terminalStats.isEmpty() ? 0 : mostActiveAddr)
     .arg(data.terminalStats.isEmpty() ? 0 : mostActivePercent, 0, 'f', 1)
     .arg(data.errorStats.errorRate, 0, 'f', 2)
     .arg(data.performance.busUtilization, 0, 'f', 1);
}

// ========== 核心优化：单次遍历收集所有统计数据 ==========
void ReportGenerator::collectAllStatsSinglePass(const QVector<DataRecord>& records, ReportData& data)
{
    LOG_INFO("ReportGenerator", QString("[单次遍历] 开始收集统计数据, 记录数: %1").arg(records.size()));

    // 初始化所有统计结构
    // 概览
    data.overview.totalRecords = records.size();
    data.overview.totalMessages = records.size();
    data.overview.importTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

    // 消息统计
    data.messageStats.bcToRtCount = 0;
    data.messageStats.rtToBcCount = 0;
    data.messageStats.rtToRtCount = 0;
    data.messageStats.broadcastCount = 0;

    // 错误统计
    data.errorStats.totalErrors = 0;
    data.errorStats.maxConsecutiveErrors = 0;

    // 性能指标
    data.performance.avgInterval = 0;
    data.performance.minInterval = 0;
    data.performance.maxInterval = 0;
    data.performance.busUtilization = 0;
    data.performance.throughput = 0;
    data.performance.avgLatency = 0;
    data.performance.peakMessageRate = 0;

    // 临时变量
    QSet<int> terminals;           // 活跃终端集合
    QSet<int> subAddresses;        // 活跃子地址集合
    int successCount = 0;          // 成功计数
    int consecutiveErrors = 0;     // 连续错误计数
    QMap<int, TerminalStats> terminalMap;  // 终端统计映射
    QVector<int> messagesPerSecondVec;     // 每秒消息数（索引=秒数）
    int minSecond = INT_MAX;               // 最小秒数
    int maxSecond = 0;                     // 最大秒数
    QMap<QPair<int, int>, QVector<double>> terminalSubAddrTimes;  // 终端-子地址时间序列
    double totalBusTimeUs = 0.0;           // 总线占用时间累计（微秒）

    // 空数据快速返回
    if (records.isEmpty()) {
        data.overview.duration = 0;
        data.overview.successRate = 0;
        data.overview.dataQuality = "无数据";
        data.overview.activeTerminals = 0;
        data.overview.activeSubAddresses = 0;
        LOG_WARNING("ReportGenerator", "[单次遍历] 无数据，快速返回");
        return;
    }

    // 计算时间范围：遍历找出真正的最小和最大时间戳
    // 不能用first/last，因为数据可能未按时间排序
    double minTime = records.first().timestampMs;
    double maxTime = records.first().timestampMs;
    data.overview.duration = (maxTime - minTime) / 1000.0;

    // ===== 单次遍历：同时收集概览、消息统计、终端统计、错误统计、性能指标、通信模式数据 =====
    for (int i = 0; i < records.size(); ++i) {
        const DataRecord& record = records[i];
        CMD cmd1;
        memcpy(&cmd1, &record.packetData.cmd1, sizeof(CMD));
        int addr = cmd1.zhongduandizhi;

        // --- 时间范围更新（修复负数问题） ---
        if (record.timestampMs < minTime) minTime = record.timestampMs;
        if (record.timestampMs > maxTime) maxTime = record.timestampMs;

        // --- 概览统计 ---
        terminals.insert(addr);
        subAddresses.insert(cmd1.zidizhi);
        if (record.packetData.chstt) {
            successCount++;
        }

        // --- 消息类型统计 ---
        switch (record.messageType) {
            case MessageType::BC_TO_RT:
                data.messageStats.bcToRtCount++;
                break;
            case MessageType::RT_TO_BC:
                data.messageStats.rtToBcCount++;
                break;
            case MessageType::RT_TO_RT:
                data.messageStats.rtToRtCount++;
                break;
            case MessageType::Broadcast:
                data.messageStats.broadcastCount++;
                break;
            default:
                break;
        }

        // --- 总线占用时间累计（精确计算） ---
        {
            int wordCount = cmd1.sjzjs_fsdm;
            if (wordCount == 0) wordCount = 32;
            double msgTimeUs = 20.0;
            switch (record.messageType) {
            case MessageType::BC_TO_RT:
                msgTimeUs += 20.0 + wordCount * 20.0 + 4.0;
                break;
            case MessageType::RT_TO_BC:
                msgTimeUs += 4.0 + 20.0 + wordCount * 20.0;
                break;
            case MessageType::RT_TO_RT:
                msgTimeUs += 20.0 + 20.0 + 4.0 + wordCount * 20.0 + 4.0;
                break;
            case MessageType::Broadcast:
                msgTimeUs += wordCount * 20.0;
                break;
            default:
                msgTimeUs += 20.0;
                break;
            }
            totalBusTimeUs += msgTimeUs;
        }

        // --- 终端统计 ---
        if (!terminalMap.contains(addr)) {
            TerminalStats stats;
            stats.terminalAddress = addr;
            stats.messageCount = 0;
            stats.successCount = 0;
            stats.failCount = 0;
            terminalMap[addr] = stats;
        }
        terminalMap[addr].messageCount++;
        if (record.packetData.chstt) {
            terminalMap[addr].successCount++;
        } else {
            terminalMap[addr].failCount++;
        }

        // --- 错误统计 ---
        if (!record.packetData.chstt) {
            data.errorStats.totalErrors++;
            data.errorStats.errorsByTerminal[addr]++;
            consecutiveErrors++;
            data.errorStats.maxConsecutiveErrors = qMax(data.errorStats.maxConsecutiveErrors, consecutiveErrors);
        } else {
            consecutiveErrors = 0;
        }

        // --- 性能指标：每秒消息数 ---
        int second = static_cast<int>(record.timestampMs / 1000.0);
        if (second < minSecond) minSecond = second;
        if (second > maxSecond) maxSecond = second;
        if (messagesPerSecondVec.isEmpty()) {
            messagesPerSecondVec.resize(second + 1, 0);
        }
        if (second >= messagesPerSecondVec.size()) {
            messagesPerSecondVec.resize(second + 1, 0);
        }
        messagesPerSecondVec[second]++;

        // --- 通信模式检测：收集终端-子地址时间序列 ---
        QPair<int, int> key(addr, cmd1.zidizhi);
        terminalSubAddrTimes[key].append(record.timestampMs);
    }

    // ===== 遍历结束，打印关键诊断日志 =====
    LOG_INFO("ReportGenerator", QString("[单次遍历] 遍历完成诊断信息: 总记录: %1, 成功数: %2, 错误数: %3, 连续错误最大: %4, 终端数: %5")
        .arg(records.size()).arg(successCount).arg(data.errorStats.totalErrors)
        .arg(data.errorStats.maxConsecutiveErrors).arg(terminals.size()));
    // 打印前10条记录的chstt值，用于诊断packetData是否正确填充
    int sampleCount = qMin(10, records.size());
    QStringList chsttSamples;
    for (int i = 0; i < sampleCount; ++i) {
        chsttSamples << QString("chstt[%1]=%2").arg(i).arg(records[i].packetData.chstt);
    }
    LOG_INFO("ReportGenerator", QString("[单次遍历] chstt采样: %1").arg(chsttSamples.join(", ")));
    // 打印各终端的错误分布
    if (!data.errorStats.errorsByTerminal.isEmpty()) {
        QStringList errorDist;
        for (auto it = data.errorStats.errorsByTerminal.begin(); it != data.errorStats.errorsByTerminal.end(); ++it) {
            errorDist << QString("RT%1:%2条错误").arg(it.key()).arg(it.value());
        }
        LOG_INFO("ReportGenerator", QString("[单次遍历] 错误终端分布: %1").arg(errorDist.join(", ")));
    }

    // ===== 计算概览派生数据 =====
    // 用遍历中收集的真实minTime/maxTime计算时长，避免负数
    data.overview.duration = (maxTime - minTime) / 1000.0;
    if (data.overview.duration < 0) {
        data.overview.duration = 0;
        LOG_WARNING("ReportGenerator", "[单次遍历] 警告: 计算时长为负值，已修正为0");
    }
    data.overview.successRate = static_cast<double>(successCount) / records.size() * 100.0;
    data.overview.activeTerminals = terminals.size();
    data.overview.activeSubAddresses = subAddresses.size();

    if (data.overview.successRate >= 99) {
        data.overview.dataQuality = "优秀";
    } else if (data.overview.successRate >= 95) {
        data.overview.dataQuality = "良好";
    } else if (data.overview.successRate >= 90) {
        data.overview.dataQuality = "一般";
    } else {
        data.overview.dataQuality = "较差";
    }

    // ===== 计算消息类型百分比 =====
    int total = records.size();
    if (total > 0) {
        data.messageStats.bcToRtPercent = static_cast<double>(data.messageStats.bcToRtCount) / total * 100.0;
        data.messageStats.rtToBcPercent = static_cast<double>(data.messageStats.rtToBcCount) / total * 100.0;
        data.messageStats.rtToRtPercent = static_cast<double>(data.messageStats.rtToRtCount) / total * 100.0;
        data.messageStats.broadcastPercent = static_cast<double>(data.messageStats.broadcastCount) / total * 100.0;
    }

    // ===== 计算终端统计派生数据 =====
    data.terminalStats = terminalMap.values().toVector();
    for (TerminalStats& stats : data.terminalStats) {
        if (total > 0) {
            stats.percent = static_cast<double>(stats.messageCount) / total * 100.0;
        }
        if (stats.messageCount > 0) {
            stats.successRate = static_cast<double>(stats.successCount) / stats.messageCount * 100.0;
        }
    }
    std::sort(data.terminalStats.begin(), data.terminalStats.end(), [](const TerminalStats& a, const TerminalStats& b) {
        return a.terminalAddress < b.terminalAddress;
    });

    // ===== 计算错误率 =====
    data.errorStats.errorRate = total > 0 ? static_cast<double>(data.errorStats.totalErrors) / total * 100.0 : 0;

    // ===== 计算性能指标 =====
    if (records.size() >= 2) {
        // 计算消息间隔（采样方式，避免对大数据量计算所有间隔）
        QVector<double> intervals;
        for (int i = 1; i < records.size(); i++) {
            double interval = records[i].timestampMs - records[i - 1].timestampMs;
            if (interval >= 0) {
                intervals.append(interval);
            }
        }
        if (intervals.size() > 5000) {
            std::shuffle(intervals.begin(), intervals.end(), std::mt19937(42));
            intervals.resize(5000);
        }

        if (!intervals.isEmpty()) {
            data.performance.minInterval = *std::min_element(intervals.begin(), intervals.end());
            data.performance.maxInterval = *std::max_element(intervals.begin(), intervals.end());
            double sum = 0;
            for (double val : intervals) {
                sum += val;
            }
            data.performance.avgInterval = sum / intervals.size();
        }

        // 使用遍历中收集的真实时间范围，避免负数
        double duration = maxTime - minTime;
        if (duration > 0) {
            data.performance.throughput = records.size() / (duration / 1000.0);
            data.performance.busUtilization = (totalBusTimeUs / 1000.0) / duration * 100.0;
        }

        if (!messagesPerSecondVec.isEmpty() && maxSecond >= minSecond) {
            int peakRate = 0;
            int peakSecond = minSecond;
            for (int s = minSecond; s <= maxSecond && s < messagesPerSecondVec.size(); s++) {
                if (messagesPerSecondVec[s] > peakRate) {
                    peakRate = messagesPerSecondVec[s];
                    peakSecond = s;
                }
            }
            data.performance.peakMessageRate = peakRate;
            data.performance.peakTime = formatTimestamp(peakSecond * 1000.0);
        }

        data.performance.avgLatency = data.performance.avgInterval / 2.0;
    }

    // ===== 检测通信模式 =====
    data.patterns.clear();
    for (auto it = terminalSubAddrTimes.begin(); it != terminalSubAddrTimes.end(); ++it) {
        if (it.value().size() < 3) continue;

        const QVector<double>& times = it.value();
        QVector<double> intervals;
        for (int i = 1; i < times.size(); i++) {
            intervals.append(times[i] - times[i-1]);
        }

        if (intervals.isEmpty()) continue;

        double avgInterval = 0;
        for (double val : intervals) avgInterval += val;
        avgInterval /= intervals.size();

        double variance = 0;
        for (double val : intervals) {
            variance += (val - avgInterval) * (val - avgInterval);
        }
        variance /= intervals.size();
        double stdDev = sqrt(variance);

        double stability = avgInterval > 0 ? (1.0 - stdDev / avgInterval) * 100.0 : 0;
        stability = qBound(0.0, stability, 100.0);

        if (stability > 70.0 && avgInterval > 10) {
            CommunicationPattern pattern;
            pattern.terminal = it.key().first;
            pattern.subAddress = it.key().second;
            pattern.period = avgInterval;
            pattern.count = times.size();
            pattern.stability = stability;
            data.patterns.append(pattern);
        }
    }

    std::sort(data.patterns.begin(), data.patterns.end(), [](const CommunicationPattern& a, const CommunicationPattern& b) {
        return a.stability > b.stability;
    });

    // ===== 识别关键事件 =====
    // 优化：相同类型事件合并，避免大量重复行
    data.keyEvents.clear();
    consecutiveErrors = 0;
    int errorStart = -1;
    bool consecutiveErrorEventAdded = false;

    for (int i = 0; i < records.size(); i++) {
        if (!records[i].packetData.chstt) {
            if (consecutiveErrors == 0) {
                errorStart = i;
                consecutiveErrorEventAdded = false;
            }
            consecutiveErrors++;

            // 只在达到阈值时生成一次事件，后续不再重复
            if (consecutiveErrors >= 5 && !consecutiveErrorEventAdded) {
                consecutiveErrorEventAdded = true;
            }
        } else {
            // 连续错误结束时，如果达到阈值则生成一条合并事件
            if (consecutiveErrors >= 5 && consecutiveErrorEventAdded) {
                KeyEvent event;
                event.time = formatTimestamp(records[errorStart].timestampMs);
                event.type = "连续错误";
                event.description = QString("检测到连续%1条错误记录（行%2~%3）")
                    .arg(consecutiveErrors)
                    .arg(records[errorStart].rowIndex + 1)
                    .arg(records[i - 1].rowIndex + 1);
                event.impact = "可能影响数据完整性";
                data.keyEvents.append(event);
            }
            consecutiveErrors = 0;
            errorStart = -1;
            consecutiveErrorEventAdded = false;
        }
    }

    // 处理末尾的连续错误
    if (consecutiveErrors >= 5 && consecutiveErrorEventAdded) {
        KeyEvent event;
        event.time = formatTimestamp(records[errorStart].timestampMs);
        event.type = "连续错误";
        event.description = QString("检测到连续%1条错误记录（行%2~%3）")
            .arg(consecutiveErrors)
            .arg(records[errorStart].rowIndex + 1)
            .arg(records[records.size() - 1].rowIndex + 1);
        event.impact = "可能影响数据完整性";
        data.keyEvents.append(event);
    }

    // 检测峰值负载事件
    double avgRate = 0;
    if (!messagesPerSecondVec.isEmpty() && maxSecond >= minSecond) {
        int count = 0;
        for (int s = minSecond; s <= maxSecond && s < messagesPerSecondVec.size(); s++) {
            avgRate += messagesPerSecondVec[s];
            count++;
        }
        if (count > 0) avgRate /= count;
    }

    if (avgRate > 0 && !messagesPerSecondVec.isEmpty()) {
        for (int s = minSecond; s <= maxSecond && s < messagesPerSecondVec.size(); s++) {
            if (messagesPerSecondVec[s] > avgRate * 2.0) {
                KeyEvent event;
                event.time = formatTimestamp(s * 1000.0);
                event.type = "峰值负载";
                event.description = QString("消息速率达到%1条/秒，为平均值的%2倍")
                    .arg(messagesPerSecondVec[s]).arg(messagesPerSecondVec[s] / avgRate, 0, 'f', 1);
                event.impact = "可能导致通信延迟";
                data.keyEvents.append(event);
            }
        }
    }

    LOG_INFO("ReportGenerator", QString("[单次遍历] 统计收集完成, 总记录: %1, 成功率: %2%, 终端数: %3, 错误数: %4, 通信模式: %5, 关键事件: %6")
        .arg(data.overview.totalRecords)
        .arg(QString::number(data.overview.successRate, 'f', 1))
        .arg(data.overview.activeTerminals)
        .arg(data.errorStats.totalErrors)
        .arg(data.patterns.size())
        .arg(data.keyEvents.size()));
}

// ========== 核心优化：并行AI分析 ==========
AIAnalysisResult ReportGenerator::performAIAnalysisParallel(const ReportData& data)
{
    AIAnalysisResult result;

    // 如果没有AI模型提供者，直接返回默认结果
    if (!m_modelProvider) {
        LOG_WARNING("ReportGenerator", "[AI分析] AI模型未配置，跳过AI分析");
        result.overviewAnalysis = "<p style='color: #999;'>AI分析功能未启用。请在设置中配置AI模型。</p>";
        result.messageAnalysis = result.overviewAnalysis;
        result.terminalAnalysis = result.overviewAnalysis;
        result.errorAnalysis = result.overviewAnalysis;
        result.performanceAnalysis = result.overviewAnalysis;
        result.riskAssessment = result.overviewAnalysis;
        result.recommendations = result.overviewAnalysis;
        return result;
    }

    LOG_INFO("ReportGenerator", "[AI分析] 开始并行AI分析，共7个分析任务");

    // 构建所有AI提示词
    QString overviewPrompt = QString(
        "你是1553B总线数据分析专家。请基于以下数据生成专业的数据概览分析：\n\n"
        "数据信息：\n"
        "- 总数据量：%1 条\n"
        "- 数据时长：%2 秒\n"
        "- 成功率：%3%\n"
        "- 数据质量：%4\n"
        "- 活跃终端数：%5 个\n"
        "- 活跃子地址数：%6 个\n\n"
        "请从以下角度进行分析（200-300字）：\n"
        "1. 数据规模评价\n"
        "2. 数据质量评估\n"
        "3. 整体通信状况\n"
        "4. 关键特征总结\n\n"
        "要求：语言专业、逻辑清晰、有说服力。每个分析要点用<h4>标签作为小标题，下方用<p>标签输出详细内容，只允许使用的标签：<h4></h4>、<p></p>、<ul></ul>、<li></li>、<br/>。"
    ).arg(data.overview.totalRecords)
     .arg(data.overview.duration, 0, 'f', 1)
     .arg(data.overview.successRate, 0, 'f', 1)
     .arg(data.overview.dataQuality)
     .arg(data.overview.activeTerminals)
     .arg(data.overview.activeSubAddresses);

    QString messagePrompt = QString(
        "你是1553B总线通信协议专家。请基于以下消息类型分布进行分析：\n\n"
        "消息类型分布：\n"
        "- BC→RT：%1 条（%2%）\n"
        "- RT→BC：%3 条（%4%）\n"
        "- RT→RT：%5 条（%6%）\n"
        "- 广播：%7 条（%8%）\n\n"
        "请分析（200-300字）：\n"
        "1. 通信架构特征\n"
        "2. 数据流向分析\n"
        "3. 通信模式解读\n"
        "4. 效率评价\n\n"
        "要求：深入解读、发现规律、专业分析。每个分析要点用<h4>标签作为小标题，下方用<p>标签输出详细内容，只允许使用的标签：<h4></h4>、<p></p>、<ul></ul>、<li></li>、<br/>。"
    ).arg(data.messageStats.bcToRtCount).arg(data.messageStats.bcToRtPercent, 0, 'f', 1)
     .arg(data.messageStats.rtToBcCount).arg(data.messageStats.rtToBcPercent, 0, 'f', 1)
     .arg(data.messageStats.rtToRtCount).arg(data.messageStats.rtToRtPercent, 0, 'f', 1)
     .arg(data.messageStats.broadcastCount).arg(data.messageStats.broadcastPercent, 0, 'f', 1);

    QString terminalPrompt = QString(
        "你是1553B总线系统架构专家。请基于以下终端通信数据进行分析：\n\n"
        "终端统计（前5个最活跃）：\n");

    for (int i = 0; i < qMin(5, data.terminalStats.size()); i++) {
        const TerminalStats& ts = data.terminalStats[i];
        terminalPrompt += QString("- RT%1：%2 条消息（%3%），成功率 %4%\n")
            .arg(ts.terminalAddress).arg(ts.messageCount)
            .arg(ts.percent, 0, 'f', 1).arg(ts.successRate, 0, 'f', 1);
    }

    terminalPrompt += QString("\n活跃终端总数：%1 个\n\n")
        .arg(data.terminalStats.size());

    terminalPrompt += "请分析（200-300字）：\n"
        "1. 终端活跃度分布\n"
        "2. 通信负载均衡性\n"
        "3. 终端可靠性评估\n"
        "4. 优化建议\n\n"
        "要求：数据支撑、分析深入、建议可行。每个分析要点用<h4>标签作为小标题，下方用<p>标签输出详细内容，只允许使用的标签：<h4></h4>、<p></p>、<ul></ul>、<li></li>、<br/>。";

    QString errorPrompt = QString(
        "你是1553B总线故障诊断专家。请基于以下错误数据进行分析：\n\n"
        "错误统计：\n"
        "- 总错误数：%1 条\n"
        "- 错误率：%2%\n"
        "- 最大连续错误：%3 条\n\n"
        "错误终端分布：\n");

    for (auto it = data.errorStats.errorsByTerminal.begin(); it != data.errorStats.errorsByTerminal.end(); ++it) {
        errorPrompt += QString("- RT%1：%2 条错误\n").arg(it.key()).arg(it.value());
    }

    errorPrompt += QString("\n请分析（300-400字）：\n"
        "1. 错误严重程度评估\n"
        "2. 错误模式分析\n"
        "3. 影响评估\n"
        "4. 建议排查步骤\n\n"
        "要求：分析深入、原因具体、建议可操作。每个分析要点用<h4>标签作为小标题，下方用<p>标签输出详细内容，只允许使用的标签：<h4></h4>、<p></p>、<ul></ul>、<li></li>、<br/>。");

    QString perfPrompt = QString(
        "你是1553B总线性能优化专家。请基于以下性能数据进行分析：\n\n"
        "性能指标：\n"
        "- 总线利用率：%1%\n"
        "- 吞吐量：%2 条/秒\n"
        "- 平均消息间隔：%3 ms\n"
        "- 峰值消息速率：%4 条/秒（时间：%5）\n\n"
        "请进行性能分析（300-400字）：\n"
        "1. 性能水平评价\n"
        "2. 瓶颈识别\n"
        "3. 优化空间分析\n"
        "4. 具体优化建议\n\n"
        "要求：数据支撑、分析专业、建议可行。每个分析要点用<h4>标签作为小标题，下方用<p>标签输出详细内容，只允许使用的标签：<h4></h4>、<p></p>、<ul></ul>、<li></li>、<br/>。"
    ).arg(data.performance.busUtilization, 0, 'f', 1)
     .arg(data.performance.throughput, 0, 'f', 1)
     .arg(data.performance.avgInterval, 0, 'f', 2)
     .arg(data.performance.peakMessageRate)
     .arg(data.performance.peakTime);

    QString riskPrompt = QString(
        "你是1553B总线系统安全专家。请基于以下数据进行风险评估：\n\n"
        "系统状态：\n"
        "- 数据质量：%1\n"
        "- 成功率：%2%\n"
        "- 错误率：%3%\n"
        "- 活跃终端数：%4 个\n"
        "- 最大连续错误：%5 条\n\n"
        "关键事件数：%6 个\n\n"
        "请进行风险评估（300-400字）：\n"
        "1. 风险等级评定（高/中/低）\n"
        "2. 主要风险点分析\n"
        "3. 潜在影响评估\n"
        "4. 风险缓解措施\n\n"
        "要求：评估客观、风险明确、措施具体。每个分析要点用<h4>标签作为小标题，下方用<p>标签输出详细内容，只允许使用的标签：<h4></h4>、<p></p>、<ul></ul>、<li></li>、<br/>。"
    ).arg(data.overview.dataQuality)
     .arg(data.overview.successRate, 0, 'f', 1)
     .arg(data.errorStats.errorRate, 0, 'f', 2)
     .arg(data.terminalStats.size())
     .arg(data.errorStats.maxConsecutiveErrors)
     .arg(data.keyEvents.size());

    QString recPrompt = buildRecommendationPrompt(data);

    // ===== 并行执行7个AI分析任务 =====
    // 使用独立线程池，避免与外层generateReportAsync的全局线程池产生资源竞争
    // 线程池大小设为8，确保7个AI分析任务+1个余量可同时运行
    QThreadPool aiThreadPool;
    aiThreadPool.setMaxThreadCount(8);

    QFuture<QString> overviewFuture = QtConcurrent::run(&aiThreadPool, [this, overviewPrompt]() {
        return callAIForAnalysis(overviewPrompt);
    });

    QFuture<QString> messageFuture = QtConcurrent::run(&aiThreadPool, [this, messagePrompt]() {
        return callAIForAnalysis(messagePrompt);
    });

    QFuture<QString> terminalFuture = QtConcurrent::run(&aiThreadPool, [this, terminalPrompt]() {
        return callAIForAnalysis(terminalPrompt);
    });

    QFuture<QString> errorFuture = QtConcurrent::run(&aiThreadPool, [this, errorPrompt]() {
        return callAIForAnalysis(errorPrompt);
    });

    QFuture<QString> perfFuture = QtConcurrent::run(&aiThreadPool, [this, perfPrompt]() {
        return callAIForAnalysis(perfPrompt);
    });

    QFuture<QString> riskFuture = QtConcurrent::run(&aiThreadPool, [this, riskPrompt]() {
        return callAIForAnalysis(riskPrompt);
    });

    QFuture<QString> recFuture = QtConcurrent::run(&aiThreadPool, [this, recPrompt]() {
        return callAIForAnalysis(recPrompt);
    });

    // 等待所有AI分析完成并收集结果
    overviewFuture.waitForFinished();
    messageFuture.waitForFinished();
    terminalFuture.waitForFinished();
    errorFuture.waitForFinished();
    perfFuture.waitForFinished();
    riskFuture.waitForFinished();
    recFuture.waitForFinished();

    result.overviewAnalysis = overviewFuture.result();
    result.messageAnalysis = messageFuture.result();
    result.terminalAnalysis = terminalFuture.result();
    result.errorAnalysis = errorFuture.result();
    result.performanceAnalysis = perfFuture.result();
    result.riskAssessment = riskFuture.result();
    result.recommendations = recFuture.result();

    LOG_INFO("ReportGenerator", "[AI分析] 并行AI分析全部完成");

    return result;
}


QString ReportGenerator::callAIForAnalysis(const QString& prompt)
{
    if (!m_modelProvider) {
        return "<p style='color: #999;'>AI分析功能未启用。请在设置中配置AI模型。</p>";
    }

    QList<ChatMessage> messages;
    ChatMessage systemMsg;
    systemMsg.role = ChatMessage::System;
    systemMsg.content = "你是一个专业的1553B总线数据分析专家，擅长从数据中发现问题、分析原因、提出建议。"
                       "你的分析报告语言专业、逻辑清晰、有说服力。请用中文回答，使用HTML格式输出。"
                       "【HTML标签规则】"
                       "1. 允许使用的标签：<h4></h4>、<p></p>、<ul></ul>、<li></li>、<br/>"
                       "2. 严禁使用任何格式化标签：<strong>、<b>、<i>、<em>、<u>等，列表项内容直接写文字即可"
                       "3. 禁止使用的标签：<h1>、<h2>、<h3>、<html>、<body>、<head>、<div>、<span>"
                       "4. 每个开标签必须有对应的闭标签，且标签名必须完全一致"
                       "5. 每个小标题必须使用（1）（2）（3）等中文序号开头，格式为：<h4>（1）问题分析</h4>"
                       "6. 每个分析要点用<h4>标题</h4>开头，紧接着用<p>详细内容</p>或<ul><li>列表项</li></ul>"
                       "7. 不要在回答开头输出标题行，直接输出分析内容"
                       "【正确示例】"
                       "<h4>（1）问题分析</h4><p>经过分析发现...</p>"
                       "<h4>（2）优化建议</h4><ul><li>短期建议：详细说明</li><li>中期建议：详细说明</li></ul>"
                       "【错误示例-绝对禁止】"
                       "<li><strong>短期建议</strong>：说明</li>  这是错误的，不要用strong标签"
                       "<li>短期建议：说明</li>  这是正确的";
    messages.append(systemMsg);

    ChatMessage userMsg;
    userMsg.role = ChatMessage::User;
    userMsg.content = prompt;
    messages.append(userMsg);

    int configTimeout = ConfigManager::instance()->getApiTimeout();

    LOG_INFO("ReportGenerator", QString("[AI请求] ========== 开始AI调用 =========="));
    LOG_INFO("ReportGenerator", QString("[AI请求] 超时设置: %1秒").arg(configTimeout));
    LOG_INFO("ReportGenerator", QString("[AI请求] System提示词: %1").arg(systemMsg.content));
    LOG_INFO("ReportGenerator", QString("[AI请求] User提示词长度: %1 字符").arg(prompt.length()));
    LOG_DEBUG("ReportGenerator", QString("[AI请求] User提示词内容:\n%1").arg(prompt));

    qint64 callStart = QDateTime::currentMSecsSinceEpoch();

    ModelResponse response = m_modelProvider->chat(messages, configTimeout);

    qint64 callDuration = QDateTime::currentMSecsSinceEpoch() - callStart;
    LOG_INFO("ReportGenerator", QString("[AI响应] ========== AI调用返回 =========="));
    LOG_INFO("ReportGenerator", QString("[AI响应] 耗时: %1ms, 成功: %2")
        .arg(callDuration).arg(response.success ? "是" : "否"));

    if (response.success) {
        LOG_INFO("ReportGenerator", QString("[AI响应] 响应内容长度: %1 字符").arg(response.content.length()));
        QString logContent = response.content.length() > 2000 
            ? response.content.left(2000) + "...\n[内容过长已截断]"
            : response.content;
        LOG_DEBUG("ReportGenerator", QString("[AI响应] 响应内容:\n%1").arg(logContent));
        return response.content;
    } else {
        LOG_ERROR("ReportGenerator", QString("[AI响应] 调用失败, 错误信息: %1").arg(response.error));
        if (response.error.contains("超时")) {
            return QString("<p style='color: #e67e22;'>AI分析超时(%1秒)，已跳过此分析项。</p>").arg(configTimeout);
        }
        return QString("<p style='color: #e74c3c;'>AI分析失败: %1</p>").arg(response.error);
    }
}

AIAnalysisResult ReportGenerator::performAIAnalysis(const ReportData& data)
{
    AIAnalysisResult result;

    QString overviewPrompt = QString(
        "你是1553B总线数据分析专家。请基于以下数据生成专业的数据概览分析：\n\n"
        "数据信息：\n"
        "- 总数据量：%1 条\n"
        "- 数据时长：%2 秒\n"
        "- 成功率：%3%\n"
        "- 数据质量：%4\n"
        "- 活跃终端数：%5 个\n"
        "- 活跃子地址数：%6 个\n\n"
        "请从以下角度进行分析（200-300字）：\n"
        "1. 数据规模评价\n"
        "2. 数据质量评估\n"
        "3. 整体通信状况\n"
        "4. 关键特征总结\n\n"
        "要求：语言专业、逻辑清晰、有说服力。每个分析要点用<h4>标签作为小标题，下方用<p>标签输出详细内容，只允许使用的标签：<h4></h4>、<p></p>、<ul></ul>、<li></li>、<br/>。"
    ).arg(data.overview.totalRecords)
     .arg(data.overview.duration, 0, 'f', 1)
     .arg(data.overview.successRate, 0, 'f', 1)
     .arg(data.overview.dataQuality)
     .arg(data.overview.activeTerminals)
     .arg(data.overview.activeSubAddresses);

    result.overviewAnalysis = callAIForAnalysis(overviewPrompt);

    QString messagePrompt = QString(
        "你是1553B总线通信协议专家。请基于以下消息类型分布进行分析：\n\n"
        "消息类型分布：\n"
        "- BC→RT：%1 条（%2%）\n"
        "- RT→BC：%3 条（%4%）\n"
        "- RT→RT：%5 条（%6%）\n"
        "- 广播：%7 条（%8%）\n\n"
        "请分析（200-300字）：\n"
        "1. 通信架构特征\n"
        "2. 数据流向分析\n"
        "3. 通信模式解读\n"
        "4. 效率评价\n\n"
        "要求：深入解读、发现规律、专业分析。每个分析要点用<h4>标签作为小标题，下方用<p>标签输出详细内容，只允许使用的标签：<h4></h4>、<p></p>、<ul></ul>、<li></li>、<br/>。"
    ).arg(data.messageStats.bcToRtCount).arg(data.messageStats.bcToRtPercent, 0, 'f', 1)
     .arg(data.messageStats.rtToBcCount).arg(data.messageStats.rtToBcPercent, 0, 'f', 1)
     .arg(data.messageStats.rtToRtCount).arg(data.messageStats.rtToRtPercent, 0, 'f', 1)
     .arg(data.messageStats.broadcastCount).arg(data.messageStats.broadcastPercent, 0, 'f', 1);

    result.messageAnalysis = callAIForAnalysis(messagePrompt);

    QString terminalPrompt = QString(
        "你是1553B总线系统架构专家。请基于以下终端通信数据进行分析：\n\n"
        "终端统计（前5个最活跃）：\n");

    for (int i = 0; i < qMin(5, data.terminalStats.size()); i++) {
        const TerminalStats& ts = data.terminalStats[i];
        terminalPrompt += QString("- RT%1：%2 条消息（%3%），成功率 %4%\n")
            .arg(ts.terminalAddress).arg(ts.messageCount)
            .arg(ts.percent, 0, 'f', 1).arg(ts.successRate, 0, 'f', 1);
    }

    terminalPrompt += QString("\n活跃终端总数：%1 个\n\n")
        .arg(data.terminalStats.size());

    terminalPrompt += "请分析（200-300字）：\n"
        "1. 终端活跃度分布\n"
        "2. 通信集中度分析\n"
        "3. 成功率差异原因\n"
        "4. 潜在瓶颈识别\n\n"
        "要求：数据支撑、分析专业、建议可行。每个分析要点用<h4>标签作为小标题，下方用<p>标签输出详细内容，只允许使用的标签：<h4></h4>、<p></p>、<ul></ul>、<li></li>、<br/>。";

    result.terminalAnalysis = callAIForAnalysis(terminalPrompt);

    QString errorPrompt = QString(
        "你是1553B总线故障诊断专家。请基于以下错误数据进行分析：\n\n"
        "错误统计：\n"
        "- 总错误数：%1 条\n"
        "- 错误率：%2%\n"
        "- 最大连续错误：%3 条\n\n"
        "错误终端分布：\n").arg(data.errorStats.totalErrors)
        .arg(data.errorStats.errorRate, 0, 'f', 2)
        .arg(data.errorStats.maxConsecutiveErrors);

    int count = 0;
    for (auto it = data.errorStats.errorsByTerminal.begin();
         it != data.errorStats.errorsByTerminal.end() && count < 5; ++it, count++) {
        errorPrompt += QString("- RT%1：%2 条错误\n").arg(it.key()).arg(it.value());
    }

    errorPrompt += "\n请进行深度分析（300-400字）：\n"
        "1. 错误特征总结\n"
        "2. 可能原因分析（至少3点）\n"
        "3. 影响评估\n"
        "4. 建议排查步骤\n\n"
        "要求：分析深入、原因具体、建议可操作。每个分析要点用<h4>标签作为小标题，下方用<p>标签输出详细内容，只允许使用的标签：<h4></h4>、<p></p>、<ul></ul>、<li></li>、<br/>。";

    result.errorAnalysis = callAIForAnalysis(errorPrompt);

    QString perfPrompt = QString(
        "你是1553B总线性能优化专家。请基于以下性能数据进行分析：\n\n"
        "性能指标：\n"
        "- 总线利用率：%1%\n"
        "- 吞吐量：%2 条/秒\n"
        "- 平均消息间隔：%3 ms\n"
        "- 峰值消息速率：%4 条/秒（时间：%5）\n\n"
        "请进行性能分析（300-400字）：\n"
        "1. 性能水平评价\n"
        "2. 瓶颈识别\n"
        "3. 优化空间分析\n"
        "4. 具体优化建议\n\n"
        "要求：数据支撑、分析专业、建议可行。每个分析要点用<h4>标签作为小标题，下方用<p>标签输出详细内容，只允许使用的标签：<h4></h4>、<p></p>、<ul></ul>、<li></li>、<br/>。"
    ).arg(data.performance.busUtilization, 0, 'f', 1)
     .arg(data.performance.throughput, 0, 'f', 1)
     .arg(data.performance.avgInterval, 0, 'f', 2)
     .arg(data.performance.peakMessageRate)
     .arg(data.performance.peakTime);

    result.performanceAnalysis = callAIForAnalysis(perfPrompt);

    QString riskPrompt = QString(
        "你是1553B总线系统安全专家。请基于以下数据进行风险评估：\n\n"
        "系统状态：\n"
        "- 数据质量：%1\n"
        "- 成功率：%2%\n"
        "- 错误率：%3%\n"
        "- 活跃终端数：%4 个\n"
        "- 最大连续错误：%5 条\n\n"
        "关键事件数：%6 个\n\n"
        "请进行风险评估（300-400字）：\n"
        "1. 风险等级评定（高/中/低）\n"
        "2. 主要风险点分析\n"
        "3. 潜在影响评估\n"
        "4. 风险缓解措施\n\n"
        "要求：评估客观、风险明确、措施具体。每个分析要点用<h4>标签作为小标题，下方用<p>标签输出详细内容，只允许使用的标签：<h4></h4>、<p></p>、<ul></ul>、<li></li>、<br/>。"
    ).arg(data.overview.dataQuality)
     .arg(data.overview.successRate, 0, 'f', 1)
     .arg(data.errorStats.errorRate, 0, 'f', 2)
     .arg(data.terminalStats.size())
     .arg(data.errorStats.maxConsecutiveErrors)
     .arg(data.keyEvents.size());

    result.riskAssessment = callAIForAnalysis(riskPrompt);


    result.recommendations = callAIForAnalysis(buildRecommendationPrompt(data));

    return result;
}

/**
 * @brief 生成消息类型饼图
 *
 * 绘制环形饼图，包含：
 * - 外环：各消息类型的扇区（渐变色填充）
 * - 内圆：显示总计数量
 * - 连接线：从扇区引出折线标签
 * - 图例：右侧显示各类型颜色和百分比
 *
 * 技术要点：
 * - 使用scale(0.45, 0.45)缩放渲染，实际输出尺寸约860×608像素
 * - 角度使用Qt的1/16度单位（90*16 = 90°）
 * - 占比<5%的扇区不在内部显示标签，避免文字重叠
 *
 * @param stats 消息统计数据
 * @return 饼图QImage
 */
QImage ReportGenerator::cropImageMargins(const QImage& image, int margin)
{
    if (image.isNull()) return image;

    int w = image.width();
    int h = image.height();

    int left = 0, right = w - 1, top = 0, bottom = h - 1;

    auto isWhiteOrTransparent = [&](int x, int y) -> bool {
        QRgb pixel = image.pixel(x, y);
        int alpha = qAlpha(pixel);
        if (alpha == 0) return true;
        int r = qRed(pixel), g = qGreen(pixel), b = qBlue(pixel);
        return (r >= 250 && g >= 250 && b >= 250);
    };

    bool found = false;
    for (top = 0; top < h && !found; top++) {
        for (int x = 0; x < w; x++) {
            if (!isWhiteOrTransparent(x, top)) { found = true; break; }
        }
    }
    if (!found) return image;
    top--;

    found = false;
    for (bottom = h - 1; bottom >= 0 && !found; bottom--) {
        for (int x = 0; x < w; x++) {
            if (!isWhiteOrTransparent(x, bottom)) { found = true; break; }
        }
    }
    if (!found) return image;
    bottom++;

    found = false;
    for (left = 0; left < w && !found; left++) {
        for (int y = top; y <= bottom; y++) {
            if (!isWhiteOrTransparent(left, y)) { found = true; break; }
        }
    }
    if (!found) return image;
    left--;

    found = false;
    for (right = w - 1; right >= 0 && !found; right--) {
        for (int y = top; y <= bottom; y++) {
            if (!isWhiteOrTransparent(right, y)) { found = true; break; }
        }
    }
    if (!found) return image;
    right++;

    left = qMax(0, left - margin);
    top = qMax(0, top - margin);
    right = qMin(w - 1, right + margin);
    bottom = qMin(h - 1, bottom + margin);

    return image.copy(left, top, right - left + 1, bottom - top + 1);
}

QImage ReportGenerator::generateMessageTypePieChart(const MessageStats& stats, int dpiScale)
{
    int size = 3200;
    QImage image(size * dpiScale, size * dpiScale, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpiScale);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(0.55, 0.55);

    LOG_INFO("ReportGenerator", QString("[饼图数据] BC→RT: %1条, %2%; RT→BC: %3条, %4%; RT→RT: %5条, %6%; 广播: %7条, %8%")
        .arg(stats.bcToRtCount).arg(stats.bcToRtPercent)
        .arg(stats.rtToBcCount).arg(stats.rtToBcPercent)
        .arg(stats.rtToRtCount).arg(stats.rtToRtPercent)
        .arg(stats.broadcastCount).arg(stats.broadcastPercent));

    int scaledSize = static_cast<int>(size / 0.55);
    int centerX = scaledSize / 2;
    int centerY = 1500;
    int radius = 1100;

    painter.setPen(QColor("#2c3e50"));
    painter.setFont(QFont("微软雅黑", 110, QFont::Bold));
    QFontMetrics titleFm(painter.font());
    painter.drawText((scaledSize - titleFm.horizontalAdvance("消息类型分布")) / 2, 200, "消息类型分布");

    QVector<ChartSlice> slices;
    if (stats.bcToRtCount > 0) slices.append({"BC→RT", stats.bcToRtPercent, QColor("#3498db")});
    if (stats.rtToBcCount > 0) slices.append({"RT→BC", stats.rtToBcPercent, QColor("#2ecc71")});
    if (stats.rtToRtCount > 0) slices.append({"RT→RT", stats.rtToRtPercent, QColor("#f39c12")});
    if (stats.broadcastCount > 0) slices.append({"广播", stats.broadcastPercent, QColor("#e74c3c")});

    if (slices.isEmpty()) {
        slices.append({"无数据", 100.0, QColor("#bdc3c7")});
    }

    double total = 0;
    for (const auto& slice : slices) {
        total += slice.value;
    }

    double startAngle = 90 * 16;
    for (int i = 0; i < slices.size(); ++i) {
        const auto& slice = slices[i];
        if (slice.value <= 0) continue;

        double spanAngle = (slice.value / total) * 360.0 * 16;

        QRadialGradient gradient(centerX, centerY, radius);
        gradient.setColorAt(0, slice.color.lighter(120));
        gradient.setColorAt(1, slice.color);
        painter.setBrush(gradient);
        painter.setPen(QPen(Qt::white, 10));
        painter.drawPie(centerX - radius, centerY - radius,
                       radius * 2, radius * 2,
                       startAngle, spanAngle);

        double midAngle = startAngle / 16.0 + (spanAngle / 16.0) / 2.0;
        double rad = qDegreesToRadians(midAngle);

        // 在扇形内部显示名称和百分比（仅占比>5%时）
        bool showInsideLabel = (slice.value / total * 100.0 > 5);
        if (showInsideLabel) {
            int labelRadius = radius * 0.6;
            int labelX = centerX + labelRadius * qCos(rad);
            int labelY = centerY - labelRadius * qSin(rad);

            painter.setFont(QFont("微软雅黑", 50, QFont::Bold));
            painter.setPen(Qt::white);
            QFontMetrics fmName(painter.font());
            int nameWidth = fmName.horizontalAdvance(slice.label);
            painter.drawText(labelX - nameWidth/2, labelY - 15, slice.label);

            painter.setFont(QFont("微软雅黑", 45, QFont::Bold));
            QString percentText = QString("%1%").arg(slice.value / total * 100.0, 0, 'f', 1);
            QFontMetrics fmPercent(painter.font());
            int percentWidth = fmPercent.horizontalAdvance(percentText);
            painter.drawText(labelX - percentWidth/2, labelY + 45, percentText);
        }

        // 连接线 + 末端文字标签（所有扇区都绘制）
        int lineEndRadius = radius + 180;
        int lineX = centerX + lineEndRadius * qCos(rad);
        int lineY = centerY - lineEndRadius * qSin(rad);

        painter.setPen(QPen(slice.color, 10));
        int connectorStart = radius + 25;
        int connectorX = centerX + connectorStart * qCos(rad);
        int connectorY = centerY - connectorStart * qSin(rad);

        // 水平折线段，用于对齐文字
        int horizontalLen = 120;
        double horizontalDir = qCos(rad) >= 0 ? 1 : -1;
        int elbowX = connectorX + horizontalLen * horizontalDir;
        int elbowY = connectorY;
        int textAnchorX = elbowX + 60 * horizontalDir;

        painter.drawLine(connectorX, connectorY, elbowX, elbowY);
        painter.drawLine(elbowX, elbowY, textAnchorX, elbowY);

        // 连接线末端绘制名称和百分比
        painter.setFont(QFont("微软雅黑", 50, QFont::Bold));
        painter.setPen(QColor("#2c3e50"));
        QString labelText = QString("%1  %2%").arg(slice.label).arg(slice.value / total * 100.0, 0, 'f', 1);
        if (horizontalDir > 0) {
            painter.drawText(textAnchorX, elbowY + 18, labelText);
        } else {
            QFontMetrics fmLabel(painter.font());
            painter.drawText(textAnchorX - fmLabel.horizontalAdvance(labelText), elbowY + 18, labelText);
        }

        startAngle += spanAngle;
    }

    // 中心圆
    painter.setBrush(QBrush(QColor(255, 255, 255, 240)));
    painter.setPen(QPen(QColor("#3498db"), 10));
    painter.drawEllipse(centerX - 300, centerY - 300, 600, 600);

    painter.setFont(QFont("微软雅黑", 60));
    painter.setPen(QColor("#7f8c8d"));
    painter.drawText(centerX - 175, centerY - 50, "总计");
    painter.setFont(QFont("微软雅黑", 110, QFont::Bold));
    painter.setPen(QColor("#2c3e50"));
    int totalCount = stats.bcToRtCount + stats.rtToBcCount + stats.rtToRtCount + stats.broadcastCount;
    QString totalStr = QString("%1").arg(totalCount);
    QFontMetrics fmTotal(painter.font());
    painter.drawText(centerX - fmTotal.horizontalAdvance(totalStr)/2, centerY + 100, totalStr);
    painter.setFont(QFont("微软雅黑", 55));
    painter.setPen(QColor("#95a5a6"));
    painter.drawText(centerX - 75, centerY + 200, "100%");

    // 图例
    int legendY = centerY - slices.size() * 120;
    int legendX = centerX + radius + 400;

    QRectF legendBgRect(legendX - 75, legendY - 200, 1450, slices.size() * 240 + 300);
    painter.setBrush(QBrush(QColor(255, 255, 255, 200)));
    painter.setPen(QPen(QColor(200, 210, 220), 5));
    painter.drawRoundedRect(legendBgRect, 50, 50);

    painter.setFont(QFont("微软雅黑", 70, QFont::Bold));
    painter.setPen(QColor("#2c3e50"));
    painter.drawText(legendX, legendY - 60, "图例");

    painter.setRenderHint(QPainter::Antialiasing, false);
    for (const auto& slice : slices) {
        painter.setBrush(QBrush(slice.color));
        painter.setPen(QPen(QColor(180, 180, 180), 5));
        painter.drawRoundedRect(legendX, legendY, 120, 120, 20, 20);

        painter.setFont(QFont("微软雅黑", 65, QFont::Bold));
        painter.setPen(QColor("#2c3e50"));
        painter.drawText(legendX + 160, legendY + 85, slice.label);

        painter.setFont(QFont("微软雅黑", 65));
        painter.setPen(QColor("#7f8c8d"));
        QString valueText = QString("%1%").arg(slice.value / total * 100.0, 0, 'f', 1);
        painter.drawText(legendX + 900, legendY + 85, valueText);

        int barWidth = static_cast<int>(slice.value / total * 400);
        painter.setBrush(QBrush(slice.color.darker(110)));
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(legendX, legendY + 140, barWidth, 20, 10, 10);

        legendY += 240;
    }

    painter.setRenderHint(QPainter::Antialiasing);
    painter.setFont(QFont("微软雅黑", 50));
    painter.setPen(QColor("#bdc3c7"));

    return image;
}

QImage ReportGenerator::generateTerminalBarChart(const QVector<TerminalStats>& stats, int dpiScale)
{
    int displayCount = stats.size();
    int imageWidth = qMax(4200, displayCount * 140 + 600);
    int imageHeight = displayCount > 16 ? 1900 : 1800;
    QImage image(imageWidth * dpiScale, imageHeight * dpiScale, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpiScale);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);

    LOG_INFO("ReportGenerator", QString("[柱状图数据] 终端数: %1").arg(stats.size()));
    for (const auto& ts : stats) {
        LOG_INFO("ReportGenerator", QString("[柱状图数据] RT%1: 消息数=%2, 占比=%3%, 成功=%4, 失败=%5, 成功率=%6%")
                 .arg(ts.terminalAddress).arg(ts.messageCount).arg(ts.percent, 0, 'f', 1)
                 .arg(ts.successCount).arg(ts.failCount).arg(ts.successRate, 0, 'f', 1));
    }

    // 标题：加大字号
    painter.setPen(QColor("#2c3e50"));
    painter.setFont(QFont("微软雅黑", 44, QFont::Bold));
    QFontMetrics titleFm(painter.font());
    painter.drawText((imageWidth - titleFm.horizontalAdvance("终端消息统计")) / 2, 70, "终端消息统计");

    if (stats.isEmpty()) {
        painter.setFont(QFont("微软雅黑", 32));
        painter.setPen(QColor("#95a5a6"));
        painter.drawText(imageWidth / 2 - 80, imageHeight / 2, "无数据");
        return image;
    }

    int maxCount = 0;
    for (const auto& ts : stats) {
        if (ts.messageCount > maxCount) maxCount = ts.messageCount;
    }
    if (maxCount == 0) maxCount = 1;

    int chartLeft = 200;
    int chartRight = imageWidth - 300;
    int chartTop = 160;
    int chartBottom = imageHeight - (displayCount > 16 ? 520 : 480);
    int chartWidth = chartRight - chartLeft;
    int chartHeight = chartBottom - chartTop;

    // 网格线
    painter.setPen(QPen(QColor("#ecf0f1"), 2));
    for (int i = 1; i < 5; i++) {
        int y = chartTop + i * chartHeight / 5;
        painter.drawLine(chartLeft, y, chartRight, y);
    }

    // 坐标轴
    painter.setPen(QPen(QColor("#bdc3c7"), 3));
    painter.drawLine(chartLeft, chartBottom, chartRight, chartBottom);
    painter.drawLine(chartLeft, chartTop, chartLeft, chartBottom);

    // Y轴刻度值：加大字体
    painter.setFont(QFont("微软雅黑", 24));
    painter.setPen(QColor("#7f8c8d"));
    for (int i = 0; i <= 5; i++) {
        int y = chartBottom - i * chartHeight / 5;
        painter.drawLine(chartLeft - 12, y, chartLeft, y);
        QString valueStr = QString::number(maxCount * i / 5);
        QFontMetrics fm(painter.font());
        painter.drawText(chartLeft - fm.horizontalAdvance(valueStr) - 22, y + 9, valueStr);
    }

    // Y轴标签
    painter.save();
    painter.translate(chartLeft - 110, chartTop + chartHeight/2);
    painter.rotate(-90);
    painter.setFont(QFont("微软雅黑", 26));
    painter.setPen(QColor("#34495e"));
    painter.drawText(-75, 0, "消息数量");
    painter.restore();

    // 所有RT都显示标签，不省略
    int gapWidth = qMax(8, qMin(20, (chartWidth / displayCount) / 6));
    int barWidth = qMin(90, (chartWidth / displayCount) - gapWidth);
    if (barWidth < 24) { barWidth = 24; gapWidth = qMax(4, (chartWidth - displayCount * barWidth) / qMax(1, displayCount - 1)); }
    int totalBarSpace = displayCount * barWidth + (displayCount - 1) * gapWidth;
    int startX = chartLeft + (chartWidth - totalBarSpace) / 2;

    // 统一柱子颜色（蓝色系），不用黑色
    QColor barBaseColor("#3498db");

    for (int i = 0; i < displayCount; i++) {
        const TerminalStats& ts = stats[i];
        int barHeight = (double)ts.messageCount / maxCount * chartHeight;
        if (barHeight < 2) barHeight = 2;
        int x = startX + i * (barWidth + gapWidth);
        int y = chartBottom - barHeight;

        // 统一渐变色（蓝色）
        QLinearGradient barGradient(x, chartBottom, x, y);
        barGradient.setColorAt(0, barBaseColor.darker(115));
        barGradient.setColorAt(0.5, barBaseColor);
        barGradient.setColorAt(1, barBaseColor.lighter(125));
        painter.setBrush(barGradient);
        painter.setPen(QPen(barBaseColor.darker(135), 2));
        painter.drawRoundedRect(x, y, barWidth, barHeight, 6, 6);

        // 柱顶数值：加大字体
        painter.setFont(QFont("微软雅黑", 22, QFont::Bold));
        painter.setPen(QColor("#2c3e50"));
        QString valueText = QString::number(ts.messageCount);
        QFontMetrics fmValue(painter.font());
        painter.drawText(x + barWidth/2 - fmValue.horizontalAdvance(valueText)/2, y - 12, valueText);

        // 百分比
        QString percentText = QString("(%1%)").arg(ts.percent, 0, 'f', 1);
        painter.setFont(QFont("微软雅黑", 18));
        painter.setPen(QColor("#7f8c8d"));
        QFontMetrics fmPercent(painter.font());
        painter.drawText(x + barWidth/2 - fmPercent.horizontalAdvance(percentText)/2, y + 30, percentText);

        painter.setFont(QFont("微软雅黑", 24, QFont::Bold));
        painter.setPen(QColor("#2c3e50"));
        QString rtLabel = QString("RT%1").arg(ts.terminalAddress);
        painter.save();
        painter.translate(x + barWidth/2, chartBottom + 80);
        painter.rotate(-55);
        painter.drawText(0, 0, rtLabel);
        painter.restore();
    }

    // 统计摘要区域
    QRectF summaryBgRect(chartRight + 40, chartTop, 240, chartHeight);
    painter.setBrush(QBrush(QColor(250, 252, 255, 240)));
    painter.setPen(QPen(QColor(200, 210, 220), 2));
    painter.drawRoundedRect(summaryBgRect, 12, 12);

    painter.setFont(QFont("微软雅黑", 26, QFont::Bold));
    painter.setPen(QColor("#2c3e50"));
    painter.drawText(summaryBgRect.x() + 38, summaryBgRect.y() + 48, "统计摘要");

    int summaryY = summaryBgRect.y() + 95;
    painter.setFont(QFont("微软雅黑", 22));
    painter.setPen(QColor("#7f8c8d"));

    int totalCount = 0;
    double avgSuccessRate = 0;
    for (const auto& ts : stats) {
        totalCount += ts.messageCount;
        avgSuccessRate += ts.successRate;
    }
    if (!stats.isEmpty()) avgSuccessRate /= stats.size();

    QStringList summaryLabels = {"终端总数:", "总消息数:", "平均成功率:"};
    QStringList summaryValues = {QString::number(stats.size()),
                                  QString::number(totalCount),
                                  QString("%1%").arg(avgSuccessRate, 0, 'f', 1)};

    for (int i = 0; i < summaryLabels.size(); ++i) {
        painter.setPen(QColor("#7f8c8d"));
        painter.drawText(summaryBgRect.x() + 24, summaryY, summaryLabels[i]);
        painter.setPen(QColor("#2c3e50"));
        painter.setFont(QFont("微软雅黑", 22, QFont::Bold));
        painter.drawText(summaryBgRect.x() + 145, summaryY, summaryValues[i]);
        painter.setFont(QFont("微软雅黑", 22));
        summaryY += 52;
    }

    return image;
}

QImage ReportGenerator::generateTimeSeriesChart(const QVector<DataRecord>& records, int dpiScale)
{
    int imageWidth = 4000;
    int imageHeight = 2000;
    QImage image(imageWidth * dpiScale, imageHeight * dpiScale, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpiScale);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);

    if (records.size() < 2) {
        painter.setFont(QFont("微软雅黑", 36));
        painter.setPen(QColor("#95a5a6"));
        painter.drawText(imageWidth / 2 - 100, imageHeight / 2, "数据不足");
        return image;
    }

    QMap<int, int> messagesPerSecond;
    for (const DataRecord& record : records) {
        int second = (int)(record.timestampMs / 1000.0);
        messagesPerSecond[second]++;
    }

    if (messagesPerSecond.isEmpty()) {
        return image;
    }

    LOG_INFO("ReportGenerator", QString("[折线图数据] 时间点数: %1, 起始秒: %2, 结束秒: %3")
             .arg(messagesPerSecond.size()).arg(messagesPerSecond.firstKey()).arg(messagesPerSecond.lastKey()));

    int maxRate = *std::max_element(messagesPerSecond.begin(), messagesPerSecond.end());
    if (maxRate == 0) maxRate = 1;

    // 标题：加大字号
    painter.setPen(QColor("#2c3e50"));
    painter.setFont(QFont("微软雅黑", 44, QFont::Bold));
    QFontMetrics titleFm(painter.font());
    painter.drawText((imageWidth - titleFm.horizontalAdvance("消息时序分布")) / 2, 70, "消息时序分布");

    int chartLeft = 220;
    int chartRight = imageWidth - 300;
    int chartTop = 160;
    int chartBottom = imageHeight - 500;
    int chartWidth = chartRight - chartLeft;
    int chartHeight = chartBottom - chartTop;

    // 网格线
    painter.setPen(QPen(QColor("#ecf0f1"), 2));
    for (int i = 1; i < 5; i++) {
        int y = chartTop + i * chartHeight / 5;
        painter.drawLine(chartLeft, y, chartRight, y);
    }

    // 坐标轴
    painter.setPen(QPen(QColor("#bdc3c7"), 3));
    painter.drawLine(chartLeft, chartBottom, chartRight, chartBottom);
    painter.drawLine(chartLeft, chartTop, chartLeft, chartBottom);

    // Y轴刻度值：加大字体
    painter.setFont(QFont("微软雅黑", 24));
    painter.setPen(QColor("#7f8c8d"));
    for (int i = 0; i <= 5; i++) {
        int y = chartBottom - i * chartHeight / 5;
        painter.drawLine(chartLeft - 12, y, chartLeft, y);
        QString valueStr = QString::number(maxRate * i / 5);
        QFontMetrics fm(painter.font());
        painter.drawText(chartLeft - fm.horizontalAdvance(valueStr) - 22, y + 9, valueStr);
    }

    // X轴标签
    painter.save();
    painter.translate(chartLeft + chartWidth/2, chartBottom + 200);
    painter.setFont(QFont("微软雅黑", 26));
    painter.setPen(QColor("#34495e"));
    painter.drawText(-30, 0, "时间");
    painter.restore();

    // Y轴标签
    painter.save();
    painter.translate(chartLeft - 130, chartTop + chartHeight/2);
    painter.rotate(-90);
    painter.setFont(QFont("微软雅黑", 26));
    painter.setPen(QColor("#34495e"));
    painter.drawText(-80, 0, "消息数/秒");
    painter.restore();

    QVector<QPoint> points;
    QVector<int> values;
    int totalSeconds = messagesPerSecond.lastKey() - messagesPerSecond.firstKey() + 1;
    if (totalSeconds <= 0) totalSeconds = 1;

    int startSecond = messagesPerSecond.firstKey();
    int endSecond = messagesPerSecond.lastKey();

    for (auto it = messagesPerSecond.begin(); it != messagesPerSecond.end(); ++it) {
        int x = chartLeft + (it.key() - startSecond) * chartWidth / totalSeconds;
        int y = chartBottom - (double)it.value() / maxRate * chartHeight;
        points.append(QPoint(x, y));
        values.append(it.value());
    }

    // 填充区域渐变
    QLinearGradient areaGradient(0, chartTop, 0, chartBottom);
    areaGradient.setColorAt(0, QColor(52, 152, 219, 60));
    areaGradient.setColorAt(1, QColor(52, 152, 219, 10));

    if (points.size() > 1) {
        QPainterPath path;
        path.moveTo(points.first().x(), chartBottom);
        for (const QPoint& pt : points) {
            path.lineTo(pt.x(), pt.y());
        }
        path.lineTo(points.last().x(), chartBottom);
        path.closeSubpath();

        painter.fillPath(path, areaGradient);

        // 折线加粗
        QPen linePen(QColor("#2980b9"), 6);
        linePen.setCapStyle(Qt::RoundCap);
        linePen.setJoinStyle(Qt::RoundJoin);
        painter.setPen(linePen);
        painter.drawPolyline(points);
    } else if (!points.isEmpty()) {
        painter.setPen(QPen(QColor("#2980b9"), 6));
        painter.setBrush(QBrush(QColor("#3498db")));
        painter.drawEllipse(points.first(), 12, 12);
    }

    // 智能稀疏显示数值标签：只显示极值点和均匀采样的点，避免拥挤
    int pointCount = points.size();
    // 计算合适的标签间隔（根据点密度动态调整）
    int labelInterval = qMax(1, pointCount / 15);  // 最多显示约15个标签

    // 先找出局部极大值和极小值点
    QVector<bool> isExtreme(pointCount, false);
    if (pointCount >= 3) {
        for (int i = 1; i < pointCount - 1; i++) {
            bool isPeak = (values[i] >= values[i-1] && values[i] >= values[i+1]);
            bool isValley = (values[i] <= values[i-1] && values[i] <= values[i+1]);
            // 只标记明显变化的极值点
            if ((isPeak || isValley) &&
                (qAbs(values[i] - values[i-1]) >= 3 || qAbs(values[i] - values[i+1]) >= 3)) {
                isExtreme[i] = true;
            }
        }
    }
    // 首尾始终显示
    if (pointCount > 0) { isExtreme[0] = true; }
    if (pointCount > 1) { isExtreme[pointCount - 1] = true; }

    // 绘制数据点和标签
    for (int i = 0; i < points.size(); i++) {
        // 绘制数据点（空心圆 + 实心圆）
        painter.setBrush(QBrush(QColor("#ffffff")));
        painter.setPen(QPen(QColor("#2980b9"), 3));
        painter.drawEllipse(points[i], 9, 9);

        painter.setBrush(QBrush(QColor("#2980b9")));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(points[i], 5, 5);

        // 智能判断是否显示数值标签
        bool showLabel = false;
        if (isExtreme[i]) {
            showLabel = true;
        } else if (i % labelInterval == 0) {
            showLabel = true;
        }

        // 额外检查与前一个已显示标签的距离，避免重叠
        if (showLabel && i > 0) {
            int lastShownIdx = -1;
            for (int j = i - 1; j >= 0; j--) {
                if (isExtreme[j] || j % labelInterval == 0) { lastShownIdx = j; break; }
            }
            if (lastShownIdx >= 0 && (i - lastShownIdx) < labelInterval / 2) {
                showLabel = false;
            }
        }

        if (showLabel && values[i] > 0) {
            QString valueText = QString::number(values[i]);
            QFont labelFont("微软雅黑", 18, QFont::Bold);
            painter.setFont(labelFont);
            QFontMetrics fm(labelFont);
            int textW = fm.horizontalAdvance(valueText) + 16;
            int textH = 30;

            // 根据点的位置决定标签方向（上方或下方）
            bool labelAbove = (points[i].y() > chartTop + chartHeight / 2);
            QRect labelRect(points[i].x() - textW/2,
                           labelAbove ? points[i].y() - textH - 14 : points[i].y() + 18,
                           textW, textH);

            painter.setBrush(QBrush(QColor(44, 62, 80, 225)));
            painter.setPen(Qt::NoPen);
            painter.drawRoundedRect(labelRect, 7, 7);

            painter.setPen(Qt::white);
            painter.drawText(labelRect.x() + (labelRect.width() - fm.horizontalAdvance(valueText))/2,
                           labelRect.y() + 22, valueText);
        }
    }

    int timeLabelCount = qMin(12, qMax(4, totalSeconds / 10));
    int timeStep = qMax(1, totalSeconds / timeLabelCount);
    painter.setFont(QFont("微软雅黑", 22));
    painter.setPen(QColor("#7f8c8d"));
    for (int i = 0; i <= timeLabelCount; i++) {
        int second = startSecond + i * timeStep;
        if (second > endSecond) second = endSecond;
        int x = chartLeft + (second - startSecond) * chartWidth / totalSeconds;

        painter.setPen(QPen(QColor("#bdc3c7"), 2));
        painter.drawLine(x, chartBottom, x, chartBottom + 10);

        painter.setPen(QColor("#7f8c8d"));
        int h = second / 3600;
        int m = (second % 3600) / 60;
        int s = second % 60;
        QString timeText = QString("%1:%2:%3")
            .arg(h, 2, 10, QChar('0'))
            .arg(m, 2, 10, QChar('0'))
            .arg(s, 2, 10, QChar('0'));
        painter.save();
        painter.translate(x, chartBottom + 60);
        painter.rotate(-50);
        painter.drawText(0, 0, timeText);
        painter.restore();
    }

    double avgRate = 0;
    for (auto it = messagesPerSecond.begin(); it != messagesPerSecond.end(); ++it) {
        avgRate += it.value();
    }
    if (!messagesPerSecond.isEmpty()) avgRate /= messagesPerSecond.size();

    int peakValue = maxRate;

    // 统计摘要区域
    QRectF statsBgRect(chartRight + 40, chartTop, 260, 380);
    painter.setBrush(QBrush(QColor(250, 252, 255, 240)));
    painter.setPen(QPen(QColor(200, 210, 220), 2));
    painter.drawRoundedRect(statsBgRect, 12, 12);

    painter.setFont(QFont("微软雅黑", 26, QFont::Bold));
    painter.setPen(QColor("#2c3e50"));
    painter.drawText(statsBgRect.x() + 38, statsBgRect.y() + 48, "统计摘要");

    int statY = statsBgRect.y() + 98;
    QStringList statLabels = {"平均速率:", "峰值速率:", "时间跨度:"};
    QStringList statValues = {QString("%1/s").arg(avgRate, 0, 'f', 1),
                              QString("%1/s").arg(peakValue),
                              QString("%1s").arg(totalSeconds)};

    for (int i = 0; i < statLabels.size(); ++i) {
        painter.setFont(QFont("微软雅黑", 22));
        painter.setPen(QColor("#7f8c8d"));
        painter.drawText(statsBgRect.x() + 24, statY, statLabels[i]);
        painter.setPen(QColor("#2c3e50"));
        painter.setFont(QFont("微软雅黑", 22, QFont::Bold));
        painter.drawText(statsBgRect.x() + 130, statY, statValues[i]);
        statY += 58;
    }

    return image;
}

QImage ReportGenerator::generateErrorDistributionChart(const ErrorStats& stats, int dpiScale)
{
    int displayCount = stats.errorsByTerminal.size();
    int imageWidth = 3200;
    int imageHeight = displayCount > 16 ? 1500 : 1400;
    QImage image(imageWidth * dpiScale, imageHeight * dpiScale, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpiScale);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.setPen(QColor("#2c3e50"));
    painter.setFont(QFont("微软雅黑", 32, QFont::Bold));
    QFontMetrics titleFm(painter.font());
    painter.drawText((imageWidth - titleFm.horizontalAdvance("错误终端分布")) / 2, 60, "错误终端分布");

    if (stats.errorsByTerminal.isEmpty()) {
        painter.setFont(QFont("微软雅黑", 24));
        painter.setPen(QColor("#27ae60"));
        painter.drawText(imageWidth / 2 - 80, imageHeight / 2, "✓ 无错误数据");
        return image;
    }

    int maxErrors = *std::max_element(stats.errorsByTerminal.begin(), stats.errorsByTerminal.end());
    if (maxErrors == 0) maxErrors = 1;

    int chartLeft = 180;
    int chartRight = imageWidth - 250;
    int chartTop = 140;
    int chartBottom = imageHeight - (displayCount > 16 ? 420 : 380);
    int chartWidth = chartRight - chartLeft;
    int chartHeight = chartBottom - chartTop;

    painter.setPen(QPen(QColor("#ecf0f1"), 2));
    for (int i = 1; i < 5; i++) {
        int y = chartTop + i * chartHeight / 5;
        painter.drawLine(chartLeft, y, chartRight, y);
    }

    painter.setPen(QPen(QColor("#bdc3c7"), 3));
    painter.drawLine(chartLeft, chartBottom, chartRight, chartBottom);
    painter.drawLine(chartLeft, chartTop, chartLeft, chartBottom);

    painter.setFont(QFont("微软雅黑", 18));
    painter.setPen(QColor("#7f8c8d"));
    for (int i = 0; i <= 5; i++) {
        int y = chartBottom - i * chartHeight / 5;
        painter.drawLine(chartLeft - 10, y, chartLeft, y);
        QString valueStr = QString::number(maxErrors * i / 5);
        QFontMetrics fm(painter.font());
        painter.drawText(chartLeft - fm.horizontalAdvance(valueStr) - 20, y + 7, valueStr);
    }

    painter.save();
    painter.translate(chartLeft - 100, chartTop + chartHeight/2);
    painter.rotate(-90);
    painter.setFont(QFont("微软雅黑", 18));
    painter.setPen(QColor("#34495e"));
    painter.drawText(-55, 0, "错误数量");
    painter.restore();

    int rotateAngle = displayCount > 20 ? -60 : displayCount > 12 ? -50 : -40;
    int errGapWidth = displayCount > 20 ? 10 : displayCount > 12 ? 18 : 30;
    int barWidth = qMin(70, (chartWidth / displayCount) - errGapWidth);
    if (barWidth < 12) barWidth = 12;
    int totalBarSpace = displayCount * barWidth + (displayCount - 1) * errGapWidth;
    int startX = chartLeft + (chartWidth - totalBarSpace) / 2;

    int i = 0;
    for (auto it = stats.errorsByTerminal.begin();
         it != stats.errorsByTerminal.end() && i < displayCount; ++it, i++) {
        int barHeight = (double)it.value() / maxErrors * chartHeight;
        int x = startX + i * (barWidth + errGapWidth);
        int y = chartBottom - barHeight;

        QLinearGradient errorGradient(x, chartBottom, x, y);
        errorGradient.setColorAt(0, QColor(192, 57, 43));
        errorGradient.setColorAt(0.3, QColor(231, 76, 60));
        errorGradient.setColorAt(1, QColor(245, 124, 120));

        painter.setBrush(errorGradient);
        painter.setPen(QPen(QColor(192, 43, 43), 1));
        painter.drawRoundedRect(x, y, barWidth, barHeight, 4, 4);

        double percentage = (double)it.value() / maxErrors * 100;
        if (percentage > 80) {
            painter.setBrush(QBrush(QColor(192, 57, 43)));
        } else if (percentage > 50) {
            painter.setBrush(QBrush(QColor(230, 126, 34)));
        } else {
            painter.setBrush(QBrush(QColor(39, 174, 96)));
        }

        QRect badgeRect(x + barWidth/2 - 26, y - 34, 52, 28);
        painter.drawRoundedRect(badgeRect, 14, 14);

        painter.setFont(QFont("微软雅黑", 13, QFont::Bold));
        painter.setPen(Qt::white);
        QString valueText = QString::number(it.value());
        QFontMetrics fmValue(painter.font());
        painter.drawText(badgeRect.x() + (badgeRect.width() - fmValue.horizontalAdvance(valueText))/2,
                        badgeRect.y() + 19, valueText);

        painter.setFont(QFont("微软雅黑", 18, QFont::Bold));
        painter.setPen(QColor("#c0392b"));
        QString rtLabel = QString("RT%1").arg(it.key());
        painter.save();
        painter.translate(x + barWidth/2, chartBottom + 60);
        painter.rotate(rotateAngle);
        painter.drawText(0, 0, rtLabel);
        painter.restore();
    }

    int totalErrors = 0;
    for (auto count : stats.errorsByTerminal) {
        totalErrors += count;
    }

    QRectF summaryBgRect(chartRight + 30, chartTop, 200, 200);
    painter.setBrush(QBrush(QColor(255, 255, 255, 200)));
    painter.setPen(QPen(QColor(220, 180, 170), 1.5));
    painter.drawRoundedRect(summaryBgRect, 8, 8);

    painter.setFont(QFont("微软雅黑", 18, QFont::Bold));
    painter.setPen(QColor("#c0392b"));
    painter.drawText(summaryBgRect.x() + 35, summaryBgRect.y() + 32, "错误摘要");

    int summaryY = summaryBgRect.y() + 65;
    QStringList errLabels = {"出错终端:", "总错误数:", "最大单端:"};
    QStringList errValues = {QString::number(stats.errorsByTerminal.size()),
                             QString::number(totalErrors),
                             QString::number(maxErrors)};

    for (int j = 0; j < errLabels.size(); ++j) {
        painter.setFont(QFont("微软雅黑", 15));
        painter.setPen(QColor("#7f8c8d"));
        painter.drawText(summaryBgRect.x() + 18, summaryY, errLabels[j]);
        painter.setPen(QColor("#c0392b"));
        painter.setFont(QFont("微软雅黑", 15, QFont::Bold));
        painter.drawText(summaryBgRect.x() + 120, summaryY, errValues[j]);
        painter.setFont(QFont("微软雅黑", 15));
        summaryY += 36;
    }

    return image;
}

QImage ReportGenerator::generateSuccessRateChart(const QVector<TerminalStats>& stats, int dpiScale)
{
    int displayCount = stats.size();
    int imageWidth = 3200;
    int imageHeight = displayCount > 16 ? 1500 : 1400;
    QImage image(imageWidth * dpiScale, imageHeight * dpiScale, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpiScale);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.setPen(QColor("#2c3e50"));
    painter.setFont(QFont("微软雅黑", 32, QFont::Bold));
    QFontMetrics titleFm(painter.font());
    painter.drawText((imageWidth - titleFm.horizontalAdvance("终端成功率统计")) / 2, 60, "终端成功率统计");

    if (stats.isEmpty()) {
        painter.setFont(QFont("微软雅黑", 24));
        painter.setPen(QColor("#95a5a6"));
        painter.drawText(imageWidth / 2 - 60, imageHeight / 2, "无数据");
        return image;
    }

    int chartLeft = 180;
    int chartRight = imageWidth - 250;
    int chartTop = 140;
    int chartBottom = imageHeight - (displayCount > 16 ? 420 : 380);
    int chartWidth = chartRight - chartLeft;
    int chartHeight = chartBottom - chartTop;

    QLinearGradient thresholdLineGrad(chartLeft, chartTop, chartRight, chartTop);
    thresholdLineGrad.setColorAt(0, QColor(231, 76, 60, 150));
    thresholdLineGrad.setColorAt(1, QColor(231, 76, 60, 30));

    painter.setPen(QPen(QColor("#ecf0f1"), 2));
    for (int i = 1; i < 4; i++) {
        int y = chartTop + i * chartHeight / 4;
        painter.drawLine(chartLeft, y, chartRight, y);
    }

    painter.setPen(QPen(QColor("#bdc3c7"), 3));
    painter.drawLine(chartLeft, chartBottom, chartRight, chartBottom);
    painter.drawLine(chartLeft, chartTop, chartLeft, chartBottom);

    painter.setFont(QFont("微软雅黑", 18));
    painter.setPen(QColor("#7f8c8d"));
    for (int i = 0; i <= 4; i++) {
        int y = chartBottom - i * chartHeight / 4;
        painter.drawLine(chartLeft - 10, y, chartLeft, y);
        QString valueStr = QString("%1%").arg(i * 25);
        QFontMetrics fm(painter.font());
        painter.drawText(chartLeft - fm.horizontalAdvance(valueStr) - 20, y + 7, valueStr);
    }

    int warningY = chartBottom - (90.0 / 100.0) * chartHeight;
    painter.setPen(QPen(QColor("#f39c12"), 2, Qt::DashDotLine));
    painter.drawLine(chartLeft + 10, warningY, chartRight - 10, warningY);
    painter.setFont(QFont("微软雅黑", 14));
    painter.setPen(QColor("#f39c12"));
    painter.drawText(chartRight - 100, warningY - 6, "90% 警戒线");

    int goodY = chartBottom - (95.0 / 100.0) * chartHeight;
    painter.setPen(QPen(QColor("#27ae60"), 2, Qt::DashDotLine));
    painter.drawLine(chartLeft + 10, goodY, chartRight - 10, goodY);
    painter.setFont(QFont("微软雅黑", 14));
    painter.setPen(QColor("#27ae60"));
    painter.drawText(chartRight - 100, goodY - 6, "95% 优秀线");

    painter.save();
    painter.translate(chartLeft - 100, chartTop + chartHeight/2);
    painter.rotate(-90);
    painter.setFont(QFont("微软雅黑", 18));
    painter.setPen(QColor("#34495e"));
    painter.drawText(-35, 0, "成功率");
    painter.restore();

    int rotateAngle = displayCount > 20 ? -60 : displayCount > 12 ? -50 : -40;
    int rateGapWidth = displayCount > 20 ? 8 : displayCount > 12 ? 14 : 24;
    int barWidth = qMin(60, (chartWidth / displayCount) - rateGapWidth);
    if (barWidth < 10) barWidth = 10;
    int totalBarSpace = displayCount * barWidth + (displayCount - 1) * rateGapWidth;
    int startX = chartLeft + (chartWidth - totalBarSpace) / 2;

    double avgSuccessRate = 0;
    int excellentCount = 0;
    int warningCount = 0;
    int criticalCount = 0;

    for (int i = 0; i < displayCount; i++) {
        const TerminalStats& ts = stats[i];
        int barHeight = ts.successRate / 100.0 * chartHeight;
        int x = startX + i * (barWidth + rateGapWidth);
        int y = chartBottom - barHeight;

        QColor barColor;
        if (ts.successRate >= 95) {
            barColor = QColor(46, 204, 113);
            excellentCount++;
        } else if (ts.successRate >= 90) {
            barColor = QColor(243, 156, 18);
            warningCount++;
        } else {
            barColor = QColor(231, 76, 60);
            criticalCount++;
        }

        QLinearGradient successGradient(x, chartBottom, x, y);
        successGradient.setColorAt(0, barColor.darker(120));
        successGradient.setColorAt(0.6, barColor);
        successGradient.setColorAt(1, barColor.lighter(115));

        painter.setBrush(successGradient);
        painter.setPen(QPen(barColor.darker(140), 1));
        painter.drawRoundedRect(x, y, barWidth, barHeight, 4, 4);

        QRect badgeRect(x + barWidth/2 - 28, y - 32, 56, 28);
        painter.setBrush(QBrush(barColor.darker(110)));
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(badgeRect, 14, 14);

        painter.setFont(QFont("微软雅黑", 13, QFont::Bold));
        painter.setPen(Qt::white);
        QString valueText = QString("%1%").arg(ts.successRate, 0, 'f', 1);
        QFontMetrics fmValue(painter.font());
        painter.drawText(badgeRect.x() + (badgeRect.width() - fmValue.horizontalAdvance(valueText))/2,
                        badgeRect.y() + 19, valueText);

        painter.setFont(QFont("微软雅黑", 18, QFont::Bold));
        if (ts.successRate >= 95) {
            painter.setPen(QColor("#27ae60"));
        } else if (ts.successRate >= 90) {
            painter.setPen(QColor("#f39c12"));
        } else {
            painter.setPen(QColor("#c0392b"));
        }
        QString rtLabel = QString("RT%1").arg(ts.terminalAddress);
        painter.save();
        painter.translate(x + barWidth/2, chartBottom + 60);
        painter.rotate(rotateAngle);
        painter.drawText(0, 0, rtLabel);
        painter.restore();

        avgSuccessRate += ts.successRate;
    }

    if (!stats.isEmpty()) avgSuccessRate /= displayCount;

    QRectF summaryBgRect(chartRight + 30, chartTop, 220, 280);
    painter.setBrush(QBrush(QColor(255, 255, 255, 200)));
    painter.setPen(QPen(QColor(200, 210, 220), 1.5));
    painter.drawRoundedRect(summaryBgRect, 8, 8);

    painter.setFont(QFont("微软雅黑", 18, QFont::Bold));
    painter.setPen(QColor("#2c3e50"));
    painter.drawText(summaryBgRect.x() + 35, summaryBgRect.y() + 34, "统计摘要");

    int sumY = summaryBgRect.y() + 70;
    QStringList rateLabels = {"平均成功率:", "优秀(≥95%):", "良好(90-95%):", "需改进(<90%):"};
    QStringList rateValues = {QString("%1%").arg(avgSuccessRate, 0, 'f', 1),
                             QString("%1个").arg(excellentCount),
                             QString("%1个").arg(warningCount),
                             QString("%1个").arg(criticalCount)};
    QList<QColor> rateColors = {QColor("#34495e"), QColor("#27ae60"), QColor("#f39c12"), QColor("#e74c3c")};

    for (int k = 0; k < rateLabels.size(); ++k) {
        painter.setFont(QFont("微软雅黑", 15));
        painter.setPen(QColor("#7f8c8d"));
        painter.drawText(summaryBgRect.x() + 18, sumY, rateLabels[k]);
        painter.setPen(rateColors[k]);
        painter.setFont(QFont("微软雅黑", 15, QFont::Bold));
        painter.drawText(summaryBgRect.x() + 130, sumY, rateValues[k]);

        if (k > 0) {
            int dotX = summaryBgRect.x() + 180;
            int dotY = sumY - 5;
            painter.setBrush(rateColors[k]);
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(dotX, dotY, 10, 10);
        }
        sumY += 38;
    }

    return image;
}

QString ReportGenerator::buildProtocolDescription()
{
    return QString(
        "<h2>第一章 1553B协议概述</h2>"

        "<h3>1.1 协议简介</h3>"
        "<p>MIL-STD-1553B是一种军用标准总线协议，广泛应用于航空电子系统。主要特点：</p>"
        "<ul>"
        "<li>数据传输速率：1 Mbps</li>"
        "<li>总线拓扑：主从式，支持最多31个远程终端（RT）</li>"
        "<li>通信方式：半双工、时分复用</li>"
        "<li>字长：20位（16位数据 + 3位同步头 + 1位奇偶校验）</li>"
        "<li>消息类型：BC→RT、RT→BC、RT→RT、广播</li>"
        "</ul>"

        "<h3>1.2 字格式说明</h3>"

        "<h4>1.2.1 命令字格式</h4>"
        "<p>命令字结构（20位）：</p>"
        "<p class='table-caption'>表1 命令字格式</p>"
        "<table class='outer-border'>"
        "<tr><th>同步头(3bit)</th><th>终端地址(5bit)</th><th>T/R(1bit)</th>"
        "<th>子地址(5bit)</th><th>数据计数(5bit)</th><th>奇偶位(1bit)</th></tr>"
        "<tr><td>固定模式</td><td>0-31</td><td>收/发</td>"
        "<td>0-31</td><td>0-31</td><td>奇校验</td></tr>"
        "</table>"

        "<h4>1.2.2 数据字格式</h4>"
        "<p>数据字结构（20位）：</p>"
        "<p class='table-caption'>表2 数据字格式</p>"
        "<table class='outer-border'>"
        "<tr><th>同步头(3bit)</th><th>数据内容(16bit)</th><th>奇偶位(1bit)</th></tr>"
        "<tr><td>数据模式</td><td>0x0000-0xFFFF</td><td>奇校验</td></tr>"
        "</table>"

        "<h4>1.2.3 状态字格式</h4>"
        "<p>状态字结构（20位）：</p>"
        "<p class='table-caption'>表3 状态字格式</p>"
        "<table class='outer-border'>"
        "<tr><th>同步头(3bit)</th><th>终端地址(5bit)</th><th>保留(3bit)</th>"
        "<th>状态标志(8bit)</th><th>奇偶位(1bit)</th></tr>"
        "<tr><td>状态模式</td><td>RT地址</td><td>000</td>"
        "<td>ME/SE/SER/RT/BS</td><td>奇校验</td></tr>"
        "</table>"

        "<h3>1.3 消息传输格式</h3>"

        "<h4>1.3.1 BC→RT传输</h4>"
        "<p>BC发送命令字和数据字给RT，RT接收后返回状态字确认。</p>"

        "<h4>1.3.2 RT→BC传输</h4>"
        "<p>BC发送命令字请求RT发送数据，RT返回状态字后发送数据字。</p>"

        "<h4>1.3.3 RT→RT传输</h4>"
        "<p>BC分别向接收RT和发送RT发送命令字，数据直接从发送RT传输到接收RT。</p>"

        "<h4>1.3.4 广播消息</h4>"
        "<p>BC向所有RT（地址31）发送数据，所有RT同时接收，无状态字返回。</p>"
    );
}

QString ReportGenerator::buildStatisticsSection(const ReportData& data)
{
    QString html;

    html += "<h2>第二章 报告概览</h2>";

    html += "<h3>2.1 数据基本信息</h3>";
    html += "<p class='table-caption'>表4 数据基本信息</p>";
    html += "<table class='outer-border'>";
    html += "<tr><th>项目</th><th>数值</th></tr>";
    html += QString("<tr><td>总数据量</td><td>%1 条</td></tr>").arg(data.overview.totalRecords);
    html += QString("<tr><td>数据时长</td><td>%1 秒</td></tr>").arg(data.overview.duration, 0, 'f', 2);
    html += QString("<tr><td>成功率</td><td>%1%</td></tr>").arg(data.overview.successRate, 0, 'f', 2);
    html += QString("<tr><td>数据质量</td><td>%1</td></tr>").arg(data.overview.dataQuality);
    html += QString("<tr><td>活跃终端数</td><td>%1 个</td></tr>").arg(data.overview.activeTerminals);
    html += QString("<tr><td>活跃子地址数</td><td>%1 个</td></tr>").arg(data.overview.activeSubAddresses);
    html += QString("<tr><td>报告生成时间</td><td>%1</td></tr>").arg(data.overview.importTime);
    html += "</table>";

    html += "<h3>2.2 消息类型统计</h3>";
    html += "<p class='table-caption'>表5 消息类型统计</p>";
    html += "<table class='outer-border'>";
    html += "<tr><th>消息类型</th><th>数量</th><th>占比</th></tr>";
    html += QString("<tr><td>BC→RT</td><td>%1 条</td><td>%2%</td></tr>")
        .arg(data.messageStats.bcToRtCount).arg(data.messageStats.bcToRtPercent, 0, 'f', 1);
    html += QString("<tr><td>RT→BC</td><td>%1 条</td><td>%2%</td></tr>")
        .arg(data.messageStats.rtToBcCount).arg(data.messageStats.rtToBcPercent, 0, 'f', 1);
    html += QString("<tr><td>RT→RT</td><td>%1 条</td><td>%2%</td></tr>")
        .arg(data.messageStats.rtToRtCount).arg(data.messageStats.rtToRtPercent, 0, 'f', 1);
    html += QString("<tr><td>广播</td><td>%1 条</td><td>%2%</td></tr>")
        .arg(data.messageStats.broadcastCount).arg(data.messageStats.broadcastPercent, 0, 'f', 1);
    html += "</table>";

    html += "<h3>2.3 终端通信统计</h3>";
    html += "<p class='table-caption'>表6 终端通信统计</p>";
    html += "<table class='outer-border'>";
    html += "<tr><th>终端地址</th><th>消息数</th><th>占比</th><th>成功数</th><th>失败数</th><th>成功率</th></tr>";

    QVector<TerminalStats> sortedStats = data.terminalStats;
    std::sort(sortedStats.begin(), sortedStats.end(),
        [](const TerminalStats& a, const TerminalStats& b) {
            return a.terminalAddress < b.terminalAddress;
        });

    for (const TerminalStats& ts : sortedStats) {
        html += QString("<tr><td>RT%1</td><td>%2</td><td>%3%</td><td>%4</td><td>%5</td><td>%6%</td></tr>")
            .arg(ts.terminalAddress)
            .arg(ts.messageCount)
            .arg(ts.percent, 0, 'f', 1)
            .arg(ts.successCount)
            .arg(ts.failCount)
            .arg(ts.successRate, 0, 'f', 1);
    }
    html += "</table>";

    html += "<h3>2.4 性能指标</h3>";
    html += "<p class='table-caption'>表7 性能指标</p>";
    html += "<table class='outer-border'>";
    html += "<tr><th>指标</th><th>数值</th></tr>";
    html += QString("<tr><td>总线利用率</td><td>%1%</td></tr>").arg(data.performance.busUtilization, 0, 'f', 2);
    html += QString("<tr><td>吞吐量</td><td>%1 条/秒</td></tr>").arg(data.performance.throughput, 0, 'f', 1);
    html += QString("<tr><td>平均消息间隔</td><td>%1 ms</td></tr>").arg(data.performance.avgInterval, 0, 'f', 2);
    html += QString("<tr><td>峰值消息速率</td><td>%1 条/秒</td></tr>").arg(data.performance.peakMessageRate);
    html += QString("<tr><td>峰值时间</td><td>%1</td></tr>").arg(data.performance.peakTime);
    html += "</table>";

    html += "<h3>2.5 错误统计</h3>";
    html += "<p class='table-caption'>表8 错误统计</p>";
    html += "<table class='outer-border'>";
    html += "<tr><th>项目</th><th>数值</th></tr>";
    html += QString("<tr><td>总错误数</td><td>%1 条</td></tr>").arg(data.errorStats.totalErrors);
    html += QString("<tr><td>错误率</td><td>%1%</td></tr>").arg(data.errorStats.errorRate, 0, 'f', 2);
    html += QString("<tr><td>最大连续错误</td><td>%1 条</td></tr>").arg(data.errorStats.maxConsecutiveErrors);
    html += "</table>";

    if (!data.patterns.isEmpty()) {
        html += "<h3>2.6 检测到的通信模式</h3>";
        html += "<p class='table-caption'>表9 检测到的通信模式</p>";
        html += "<table class='outer-border'>";
        html += "<tr><th>终端</th><th>子地址</th><th>周期(ms)</th><th>消息数</th><th>稳定性</th></tr>";

        for (const CommunicationPattern& p : data.patterns) {
            html += QString("<tr><td>RT%1</td><td>SA%2</td><td>%3</td><td>%4</td><td>%5%</td></tr>")
                .arg(p.terminal)
                .arg(p.subAddress)
                .arg(p.period, 0, 'f', 1)
                .arg(p.count)
                .arg(p.stability, 0, 'f', 1);
        }
        html += "</table>";
    }

    if (!data.keyEvents.isEmpty()) {
        html += "<h3>2.7 关键事件</h3>";
        html += "<p class='table-caption'>表10 关键事件</p>";
        html += "<table class='outer-border'>";
        html += "<tr><th>时间</th><th>类型</th><th>描述</th><th>影响</th></tr>";

        for (const KeyEvent& e : data.keyEvents) {
            html += QString("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td></tr>")
                .arg(e.time).arg(e.type).arg(e.description).arg(e.impact);
        }
        html += "</table>";
    }

    return html;
}

QString ReportGenerator::buildChartsSection(const ReportData& data)
{
    QString html;
    html += "<h2>第三章 可视化图表</h2>";

    html += "<div class='chart-container'>";
    html += "<h3>3.1 消息类型分布</h3>";
    html += "<img src='chart_messageTypePie'>";
    html += "<p class='figure-caption'>图1 消息类型分布饼图</p>";
    html += "</div>";

    html += "<div class='chart-container'>";
    html += "<h3>3.2 终端消息统计</h3>";
    html += "<img src='chart_terminalBar'>";
    html += "<p class='figure-caption'>图2 终端消息统计柱状图</p>";
    html += "</div>";

    html += "<div class='chart-container'>";
    html += "<h3>3.3 消息时序分布</h3>";
    html += "<img src='chart_timeSeries'>";
    html += "<p class='figure-caption'>图3 消息时序分布折线图</p>";
    html += "</div>";

    html += "<div class='chart-container'>";
    html += "<h3>3.4 错误终端分布</h3>";
    html += "<img src='chart_errorDistribution'>";
    html += "<p class='figure-caption'>图4 错误终端分布图</p>";
    html += "</div>";

    html += "<div class='chart-container'>";
    html += "<h3>3.5 终端成功率统计</h3>";
    html += "<img src='chart_successRate'>";
    html += "<p class='figure-caption'>图5 终端成功率统计图</p>";
    html += "</div>";

    return html;
}

QString ReportGenerator::buildAIAnalysisSection(const AIAnalysisResult& aiResult)
{
    auto wrapPlainText = [](const QString& content) -> QString {
        if (content.trimmed().isEmpty()) {
            return "<p>暂无分析内容。</p>";
        }
        if (content.contains("<p") || content.contains("<h") || content.contains("<ul") || content.contains("<ol")) {
            return content;
        }
        QStringList paragraphs = content.split("\n", Qt::SkipEmptyParts);
        QString result;
        for (const QString& para : paragraphs) {
            QString trimmed = para.trimmed();
            if (!trimmed.isEmpty()) {
                result += QString("<p>%1</p>").arg(trimmed);
            }
        }
        return result.isEmpty() ? "<p>暂无分析内容。</p>" : result;
    };

    QString html;
    html += "<h2>第四章 AI深度分析</h2>";

    html += "<div class='ai-section'>";
    html += "<h3>4.1 数据概览分析</h3>";
    html += ensureContentAfterHeadings(wrapPlainText(aiResult.overviewAnalysis));
    html += "</div>";

    html += "<div class='ai-section'>";
    html += "<h3>4.2 消息类型分析</h3>";
    html += ensureContentAfterHeadings(wrapPlainText(aiResult.messageAnalysis));
    html += "</div>";

    html += "<div class='ai-section'>";
    html += "<h3>4.3 终端通信分析</h3>";
    html += ensureContentAfterHeadings(wrapPlainText(aiResult.terminalAnalysis));
    html += "</div>";

    html += "<div class='ai-section'>";
    html += "<h3>4.4 错误模式分析</h3>";
    html += ensureContentAfterHeadings(wrapPlainText(aiResult.errorAnalysis));
    html += "</div>";

    html += "<div class='ai-section'>";
    html += "<h3>4.5 性能瓶颈分析</h3>";
    html += ensureContentAfterHeadings(wrapPlainText(aiResult.performanceAnalysis));
    html += "</div>";

    html += "<div class='ai-section'>";
    html += "<h3>4.6 风险评估</h3>";
    html += ensureContentAfterHeadings(wrapPlainText(aiResult.riskAssessment));
    html += "</div>";

    return html;
}

ReportGenerator::StyleConfig ReportGenerator::htmlStyle()
{
    StyleConfig s;
    s.bodyFontSize = "12pt";
    s.bodyLineHeight = "22pt";
    s.bodyTextIndent = "2em";
    s.h1FontSize = "22pt";
    s.h2FontSize = "16pt";
    s.h3FontSize = "14pt";
    s.h4FontSize = "14pt";
    s.h4FontWeight = "bold";
    s.thFontSize = "10.5pt";
    s.tdFontSize = "10.5pt";
    s.captionFontSize = "10.5pt";
    s.tableBorderWidth = "0.75pt";
    s.cellBorderWidth = "0.75pt";
    s.chartMaxWidth = "24cm";
    s.isPdf = false;
    return s;
}

ReportGenerator::StyleConfig ReportGenerator::pdfStyle()
{
    StyleConfig s;
    s.bodyFontSize = "9pt";
    s.bodyLineHeight = "16pt";
    s.bodyTextIndent = "18pt";
    s.h1FontSize = "22pt";
    s.h2FontSize = "16pt";
    s.h3FontSize = "14pt";
    s.h4FontSize = "12pt";
    s.h4FontWeight = "normal";
    s.thFontSize = "9pt";
    s.tdFontSize = "7.5pt";
    s.captionFontSize = "7.5pt";
    s.tableBorderWidth = "1.5px";
    s.cellBorderWidth = "1px";
    s.chartMaxWidth = "24cm";
    s.isPdf = true;
    return s;
}

QString ReportGenerator::buildReportHtml(const ReportData& data, const StyleConfig& style)
{
    QString html;

    if (style.isPdf) {
        html += "<html><head><style>";
        html += "@page { size: A4; }";
    } else {
        html += "<!DOCTYPE html>";
        html += "<html lang='zh-CN'>";
        html += "<head>";
        html += "<meta charset='UTF-8'>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
        html += "<title>1553B总线数据分析报告</title>";
        html += "<style>";
        html += "@page { size: A4; margin: 2.5cm 2.5cm 2.5cm 3cm; }";
    }

    html += QString("body { font-family: 'SimSun', '宋体', serif; font-size: %1; color: #000000; "
            "line-height: %2; text-align: justify;").arg(style.bodyFontSize, style.bodyLineHeight);
    if (!style.isPdf) {
        html += " margin: 0 auto; max-width: 210mm; padding: 2.5cm 2.5cm 2.5cm 3cm; "
                "overflow-wrap: break-word; word-wrap: break-word;";
    }
    html += " }";

    html += QString("h1 { font-family: 'SimHei', '黑体', sans-serif; font-size: %1; font-weight: bold; "
            "text-align: center; color: #000000; line-height: 30pt; margin-top: 24pt; margin-bottom: 18pt; }")
            .arg(style.h1FontSize);
    if (!style.isPdf) {
        html.replace("h1 {", "h1.title {");
    }

    html += QString("h2 { font-family: 'SimHei', '黑体', sans-serif; font-size: %1; font-weight: bold; "
            "text-align: left; color: #000000; line-height: 26pt; margin-top: 24pt; margin-bottom: 12pt; "
            "border-bottom: 1px solid #333333; padding-bottom: 4pt; }").arg(style.h2FontSize);

    html += QString("h3 { font-family: 'SimSun', '宋体', serif; font-size: %1; font-weight: bold; "
            "text-align: left; color: #000000; line-height: 24pt; margin-top: 18pt; margin-bottom: 6pt; }")
            .arg(style.h3FontSize);

    html += QString("h4 { font-family: 'SimSun', '宋体', serif; font-size: %1; font-weight: %2; "
            "text-align: left; color: #000000; line-height: 24pt; margin-top: 12pt; margin-bottom: 3pt; }")
            .arg(style.h4FontSize, style.h4FontWeight);

    html += QString("p { font-family: 'SimSun', '宋体', serif; font-size: %1; color: #000000; "
            "line-height: %2; text-align: justify; text-indent: %3; margin: 3pt 0; }")
            .arg(style.bodyFontSize, style.bodyLineHeight, style.bodyTextIndent);
    html += "p.no-indent { text-indent: 0; }";

    html += QString("ul, ol { font-family: 'SimSun', '宋体', serif; font-size: %1; color: #000000; "
            "line-height: %2; margin: 6pt 0; padding-left: 2em; }")
            .arg(style.bodyFontSize, style.bodyLineHeight);
    html += "li { margin: 3pt 0; }";

    html += QString("table { width: 100%; border-collapse: collapse; margin: 12pt auto; "
            "border: %1 solid #000000; }").arg(style.tableBorderWidth);

    html += QString("th { font-family: 'SimSun', '宋体', serif; font-size: %1; font-weight: bold; "
            "color: #000000; background-color: #D9D9D9; text-align: center; "
            "line-height: 16pt; padding: 6pt 8pt; border: %2 solid #000000; text-indent: 0; }")
            .arg(style.thFontSize, style.cellBorderWidth);

    html += QString("td { font-family: 'SimSun', '宋体', serif; font-size: %1; color: #000000; "
            "line-height: 14pt; padding: 6pt 8pt; border: %2 solid #000000; text-align: left; text-indent: 0; }")
            .arg(style.tdFontSize, style.cellBorderWidth);

    html += QString(".table-caption { font-family: 'SimSun', '宋体', serif; font-size: %1; font-weight: bold; "
            "color: #000000; text-align: center; line-height: 14pt; margin-top: 12pt; margin-bottom: 6pt; }")
            .arg(style.captionFontSize);

    html += QString(".figure-caption { font-family: 'SimSun', '宋体', serif; font-size: %1; font-weight: bold; "
            "color: #000000; text-align: center; line-height: 14pt; margin-top: 6pt; margin-bottom: 12pt; }")
            .arg(style.captionFontSize);

    if (style.isPdf) {
        html += "img { display: block; margin: 0 auto; }";
    } else {
        html += QString(".chart-container img { width: 100%; max-width: %1; height: auto; border: none; }")
                .arg(style.chartMaxWidth);
    }

    html += ".chart-container { text-align: center; margin: 12pt 0; }";
    if (!style.isPdf) html += ".chart-container { page-break-inside: avoid; }";

    html += ".ai-section { margin: 12pt 0; padding: 12pt; background-color: #F8F9FA; border-left: 3pt solid #3498DB; }";
    html += ".ai-section p { text-indent: 2em; }";

    if (style.isPdf) {
        html += ".footer-text { font-size: 9pt; color: #CC0000; text-align: center; "
                "margin-top: 30pt; padding-top: 8pt; }";
        html += ".header-text { font-size: 10.5pt; color: #666666; text-align: center; "
                "margin-bottom: 18pt; }";
    } else {
        html += ".footer { text-align: center; margin-top: 30pt; padding-top: 12pt; "
                "color: #CC0000; font-size: 9pt; }";
        html += ".header-info { text-align: center; margin-bottom: 18pt; color: #666666; font-size: 10.5pt; }";
    }

    html += QString("table.outer-border { border: %1 solid #000000; }").arg(style.tableBorderWidth);
    html += QString("table.outer-border th, table.outer-border td { border: %1 solid #000000; }").arg(style.cellBorderWidth);

    if (style.isPdf) {
        html += ".page-break { page-break-before: always; }";
    }

    html += "</style>";
    html += "</head>";
    html += "<body>";

    if (style.isPdf) {
        html += "<div class='header-text'></div>";
        html += "<h1>1553B总线数据分析报告</h1>";
    } else {
        html += "<div class='header-info'></div>";
        html += "<h1 class='title'>1553B总线数据分析报告</h1>";
    }

    html += buildProtocolDescription();
    html += buildStatisticsSection(data);
    html += buildChartsSection(data);
    html += buildAIAnalysisSection(data.aiAnalysis);

    html += "<h2>第五章 结论与建议</h2>";
    html += "<div class='ai-section'>";
    {
        QString recContent = data.aiAnalysis.recommendations;
        QString processedContent;
        if (recContent.trimmed().isEmpty()) {
            processedContent = "<p>暂无建议内容。</p>";
        } else if (recContent.contains("<p") || recContent.contains("<h") || recContent.contains("<ul") || recContent.contains("<ol")) {
            processedContent = recContent;
        } else {
            QStringList paragraphs = recContent.split("\n", Qt::SkipEmptyParts);
            for (const QString& para : paragraphs) {
                QString trimmed = para.trimmed();
                if (!trimmed.isEmpty()) {
                    processedContent += QString("<p>%1</p>").arg(trimmed);
                }
            }
        }
        html += ensureContentAfterHeadings(processedContent);
    }
    html += "</div>";

    if (style.isPdf) {
        html += "<div class='footer-text'>";
    } else {
        html += "<div class='footer'>";
    }
    html += QString("<p class='no-indent'>本报告由1553B数据智能分析工具自动生成，报告生成时间：%1</p>").arg(data.overview.importTime);
    html += "</div>";

    html += "</body></html>";

    return html;
}

QString ReportGenerator::buildHtmlReport(const ReportData& data)
{
    return buildReportHtml(data, htmlStyle());
}

QString ReportGenerator::buildPdfHtmlReport(const ReportData& data)
{
    return buildReportHtml(data, pdfStyle());
}

bool ReportGenerator::exportToPdf(const QString& filePath, const ReportData& data)
{
    // 初始化PDF打印机，设置A4页面和高分辨率
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(filePath);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setResolution(300);
    // 设置页边距：上下左右均为25mm
    printer.setPageMargins(QMarginsF(25, 25, 25, 25), QPageLayout::Millimeter);

    // 使用专门为PDF优化的HTML模板
    QString html = buildPdfHtmlReport(data);

    // 计算内容区域宽度，用于缩放图表
    QSizeF pageSizeMm = printer.pageLayout().pageSize().size(QPageSize::Millimeter);
    QMarginsF marginsMm = printer.pageLayout().margins(QPageLayout::Millimeter);
    qreal contentWidthMm = pageSizeMm.width() - marginsMm.left() - marginsMm.right();

    // 计算QTextDocument页面宽度（点为单位，72点/英寸）
    qreal contentWidthPt = contentWidthMm * 72.0 / 25.4;
    // 图表显示宽度：在原75%基础上放大50%（即112.5%），封顶98%避免溢出页面
    int chartDisplayWidthPt = static_cast<int>(qMin(contentWidthPt * 0.75 * 1.5, contentWidthPt * 0.98));

    // 为所有chart img标签添加HTML width属性，控制图片在QTextDocument中的显示尺寸
    // QTextDocument不支持CSS百分比宽度，但支持HTML width属性（单位为点）
    // 在img标签前添加<br>强制换行，避免图片与上方标题显示在同一行
    // 饼图单独使用90%宽度（缩小10%）
    int pieDisplayWidthPt = static_cast<int>(chartDisplayWidthPt * 0.9);
    html.replace("<img src='chart_messageTypePie'",
                 QString("<br><img width='%1' src='chart_messageTypePie'").arg(pieDisplayWidthPt));
    html.replace("<img src='chart_",
                 QString("<br><img width='%1' src='chart_").arg(chartDisplayWidthPt));

    // 图表实际像素宽度：使用较高分辨率以保证打印清晰度
    // 按图表显示宽度的2倍计算像素值，确保打印质量
    int chartWidthPx = static_cast<int>(contentWidthMm * 0.98 / 25.4 * printer.resolution() * 2);

    // PDF导出时使用2倍分辨率重新渲染图表，确保打印清晰度
    QMap<QString, QImage> hiResCharts;
    if (!data.charts.isEmpty()) {
        hiResCharts["messageTypePie"] = cropImageMargins(generateMessageTypePieChart(data.messageStats, 2));
        hiResCharts["terminalBar"] = cropImageMargins(generateTerminalBarChart(data.terminalStats, 2));
        hiResCharts["timeSeries"] = cropImageMargins(generateTimeSeriesChart(m_cachedRecords, 2));
        hiResCharts["errorDistribution"] = cropImageMargins(generateErrorDistributionChart(data.errorStats, 2));
        hiResCharts["successRate"] = cropImageMargins(generateSuccessRateChart(data.terminalStats, 2));
    }

    for (auto it = hiResCharts.begin(); it != hiResCharts.end(); ++it) {
        int targetWidth = (it.key() == "messageTypePie") ? static_cast<int>(chartWidthPx * 0.9) : chartWidthPx;
        QImage scaledChart = it.value().scaledToWidth(targetWidth, Qt::SmoothTransformation);
        QByteArray byteArray;
        QBuffer buffer(&byteArray);
        buffer.open(QIODevice::WriteOnly);
        scaledChart.save(&buffer, "PNG", 95);
        QString base64 = QString("data:image/png;base64,%1").arg(QString(byteArray.toBase64()));
        QString chartKey = QString("chart_%1").arg(it.key());
        html.replace(QString("src='%1'").arg(chartKey),
                     QString("src='%1'").arg(base64));
        html.replace(QString("src=\"%1\"").arg(chartKey),
                     QString("src=\"%1\"").arg(base64));
    }

    // 创建文本文档并设置页面大小（使用毫米转点，72点/英寸）
    QTextDocument doc;
    doc.setPageSize(QSizeF(contentWidthMm, pageSizeMm.height() - marginsMm.top() - marginsMm.bottom()) * 72.0 / 25.4);
    doc.setHtml(html);

    // ===== 使用QTextCursor API手动设置文档格式 =====
    // QTextDocument的CSS解析器对text-indent和margin支持不完善，
    // 需要在setHtml之后通过QTextCursor直接操作QTextBlockFormat来确保格式生效

    // 首行缩进值：2个中文字符宽度（9pt字号下约18pt）
    const qreal paragraphIndent = 18.0;
    // 图片前间距（点）
    const qreal imageTopMargin = 12.0;

    QTextCursor cursor(&doc);
    cursor.movePosition(QTextCursor::Start);

    // 遍历所有文本块，设置首行缩进和图片间距
    do {
        QTextBlock block = cursor.block();
        QTextBlockFormat blockFmt = block.blockFormat();
        bool needUpdate = false;

        // 跳过表格单元格内的块，表格内文字不需要首行缩进
        if (cursor.currentTable()) {
            cursor.movePosition(QTextCursor::NextBlock);
            continue;
        }

        // 检查当前块是否包含图片（通过遍历块内片段查找图片格式）
        bool hasImage = false;
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            QTextFragment fragment = it.fragment();
            if (fragment.isValid() && fragment.charFormat().isImageFormat()) {
                hasImage = true;
                break;
            }
        }

        if (hasImage) {
            // 图片块：添加上方间距，确保图片与上方内容之间有换行效果
            if (blockFmt.topMargin() < imageTopMargin) {
                blockFmt.setTopMargin(imageTopMargin);
                needUpdate = true;
            }
        } else {
            // 非图片块：检查是否为正文段落（非标题、非表格标题、非图名）
            // 判断依据：块内字符的字号小于12pt的为正文段落
            bool isBodyParagraph = true;
            for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
                QTextFragment fragment = it.fragment();
                if (fragment.isValid()) {
                    qreal fontSize = fragment.charFormat().fontPointSize();
                    // 字号>=12pt的是标题或特殊文本，不需要首行缩进
                    if (fontSize > 0 && fontSize >= 12.0) {
                        isBodyParagraph = false;
                        break;
                    }
                }
            }

            if (isBodyParagraph && blockFmt.textIndent() == 0) {
                // 正文段落：设置首行缩进2字符
                blockFmt.setTextIndent(paragraphIndent);
                needUpdate = true;
            }
        }

        if (needUpdate) {
            cursor.setBlockFormat(blockFmt);
        }
    } while (cursor.movePosition(QTextCursor::NextBlock));

    // 设置所有表格宽度为100%且居中
    // QTextDocument不支持CSS的table width:100%，需通过QTextTable API设置
    QSet<QTextTable*> processedTables;
    QTextCursor tableCursor(&doc);
    tableCursor.movePosition(QTextCursor::Start);
    do {
        QTextTable* table = tableCursor.currentTable();
        if (table && !processedTables.contains(table)) {
            processedTables.insert(table);
            QTextTableFormat tableFmt = table->format();
            tableFmt.setWidth(QTextLength(QTextLength::PercentageLength, 100));
            tableFmt.setAlignment(Qt::AlignCenter);
            table->setFormat(tableFmt);
        }
    } while (tableCursor.movePosition(QTextCursor::NextBlock));

    // 使用QTextDocument的标准打印方法，自动处理分页
    doc.print(&printer);

    return true;
}

bool ReportGenerator::exportToDocx(const QString& filePath, const ReportData& data)
{
    QString html = buildHtmlReport(data);

    // ===== DOCX文件构建 =====
    // DOCX文件本质是ZIP压缩包，包含多个XML文件和图片资源
    // 手动构建ZIP文件，避免引入第三方库依赖

    // 图表图片映射：将HTML中的chart_xxx引用替换为imageN.png文件名
    QMap<QString, QString> chartImageMap;
    QMap<QString, QByteArray> chartDataMap;
    QMap<QString, int> imageRIdMap;
    QMap<QString, QSize> imageSizeMap;
    int imageIndex = 1;
    for (auto it = data.charts.begin(); it != data.charts.end(); ++it) {
        QByteArray byteArray;
        QBuffer buffer(&byteArray);
        buffer.open(QIODevice::WriteOnly);
        it.value().save(&buffer, "PNG");
        QString imageName = QString("image%1.png").arg(imageIndex);
        chartImageMap[it.key()] = imageName;
        chartDataMap[imageName] = byteArray;
        imageRIdMap[imageName] = imageIndex;
        imageSizeMap[imageName] = it.value().size();
        QString chartSrc = QString("chart_%1").arg(it.key());
        html.replace(QString("src='%1'").arg(chartSrc),
                     QString("src='%1'").arg(imageName));
        html.replace(QString("src=\"%1\"").arg(chartSrc),
                     QString("src=\"%1\"").arg(imageName));
        imageIndex++;
    }

    QString docXmlContent = convertHtmlToDocxBody(html, imageRIdMap, imageSizeMap);

    // ZIP文件条目列表：每个条目对应DOCX包中的一个文件
    struct ZipEntry { QString path; QByteArray data; };
    QVector<ZipEntry> entries;

    // [Content_Types].xml - 定义包中各文件的内容类型
    // 这是OOXML规范要求的文件，告诉Word如何解析各部分内容

    QString contentTypes = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n"
        " <Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n"
        " <Default Extension=\"xml\" ContentType=\"application/xml\"/>\n"
        " <Default Extension=\"png\" ContentType=\"image/png\"/>\n"
        " <Override PartName=\"/word/document.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/>\n"
        " <Override PartName=\"/docProps/core.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.core-properties+xml\"/>\n"
        " <Override PartName=\"/docProps/app.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.extended-properties+xml\"/>\n";
    for (auto it = chartImageMap.begin(); it != chartImageMap.end(); ++it) {
        contentTypes += QString(" <Override PartName=\"/word/media/%1\" ContentType=\"image/png\"/>\n").arg(it.value());
    }
    contentTypes += "</Types>\n";
    entries.append(ZipEntry{"[Content_Types].xml", contentTypes.toUtf8()});

    // _rels/.rels - 包级别关系文件，定义文档入口点
    entries.append(ZipEntry{"_rels/.rels", "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
        " <Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"word/document.xml\"/>\n"
        " <Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties\" Target=\"docProps/core.xml\"/>\n"
        " <Relationship Id=\"rId3\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/extended-properties\" Target=\"docProps/app.xml\"/>\n"
        "</Relationships>\n"});

    // word/_rels/document.xml.rels - 文档级别关系文件，定义图片引用
    QString docRels = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n";
    imageIndex = 1;
    for (auto it = chartImageMap.begin(); it != chartImageMap.end(); ++it) {
        docRels += QString(" <Relationship Id=\"rId%1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/image\" Target=\"media/%2\"/>\n")
            .arg(imageIndex++).arg(it.value());
    }
    docRels += "</Relationships>\n";
    entries.append(ZipEntry{"word/_rels/document.xml.rels", docRels.toUtf8()});

    QDateTime now = QDateTime::currentDateTimeUtc();
    QString tsIso = now.toString("yyyy-MM-ddThh:mm:ssZ");

    entries.append(ZipEntry{"docProps/core.xml",
        QString("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<cp:coreProperties xmlns:cp=\"http://schemas.openxmlformats.org/package/2006/metadata/core-properties\"\n"
        " xmlns:dc=\"http://purl.org/dc/elements/1.1/\" xmlns:dcterms=\"http://purl.org/dc/terms/\"\n"
        " xmlns:dcmitype=\"http://purl.org/dc/dcmitype/\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">\n"
        " <dc:title>1553B智能分析报告</dc:title>\n"
        " <dc:creator>1553BAnalyzer</dc:creator>\n"
        " <dcterms:created xsi:type=\"dcterms:W3CDTF\">%1</dcterms:created>\n"
        " <dcterms:modified xsi:type=\"dcterms:W3CDTF\">%1</dcterms:modified>\n"
        "</cp:coreProperties>\n").arg(tsIso).toUtf8()});

    entries.append(ZipEntry{"docProps/app.xml",
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Properties xmlns=\"http://schemas.openxmlformats.org/officeDocument/2006/extended-properties\">\n"
        " <Application>1553BAnalyzer</Application>\n"
        " <Lines>0</Lines>\n"
        " <Characters>0</Characters>\n"
        " <CharactersWithSpaces>0</CharactersWithSpaces>\n"
        "</Properties>\n"});

    QString documentXml = QString("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\"\n"
        "             xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\"\n"
        "             xmlns:wp=\"http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing\"\n"
        "             xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\"\n"
        "             xmlns:pic=\"http://schemas.openxmlformats.org/drawingml/2006/picture\">\n"
        "<w:body>\n%1\n"
        "<w:sectPr>"
        "<w:pgSz w:w=\"11906\" w:h=\"16838\"/>"
        "<w:pgMar w:top=\"1417\" w:right=\"1417\" w:bottom=\"1417\" w:left=\"1701\" w:header=\"708\" w:footer=\"708\" w:gutter=\"0\"/>"
        "<w:pgNumType w:fmt=\"decimal\"/>"
        "<w:docGrid w:type=\"lines\" w:linePitch=\"312\"/>"
        "</w:sectPr>"
        "\n</w:body>\n</w:document>")
        .arg(docXmlContent);
    entries.append(ZipEntry{"word/document.xml", documentXml.toUtf8()});

    for (auto it = chartDataMap.begin(); it != chartDataMap.end(); ++it) {
        entries.append(ZipEntry{QString("word/media/%1").arg(it.key()), it.value()});
    }

    QFile outFile(filePath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_lastError = QString("无法创建DOCX文件: %1").arg(filePath);
        return false;
    }

    // ===== 手动构建ZIP文件 =====
    // ZIP文件格式规范（PKZIP APPNOTE）：
    // [Local File Header + File Data] × N + [Central Directory Entry] × N + [End of Central Directory]

    // CRC32查找表：使用std::array静态初始化，避免返回局部变量地址的编译警告
    static const auto s_crc32Table = []() {
        std::array<quint32, 256> table;
        for (quint32 i = 0; i < 256; i++) {
            quint32 c = i;
            for (int j = 0; j < 8; j++) {
                if (c & 1) c = 0xEDB88320u ^ (c >> 1);
                else c >>= 1;
            }
            table[i] = c;
        }
        return table;
    }();

    // CRC32计算函数：使用查找表加速
    auto computeCrc32 = [](const QByteArray& data) -> quint32 {
        quint32 crc = 0xFFFFFFFFu;
        for (int i = 0; i < data.size(); i++) {
            crc = s_crc32Table[(crc ^ static_cast<quint8>(data[i])) & 0xFF] ^ (crc >> 8);
        }
        return crc ^ 0xFFFFFFFFu;
    };

    // 预计算每个条目的CRC32、压缩数据和存储策略
    QVector<quint32> entryCrc(entries.size());
    QVector<QByteArray> compressedData(entries.size());
    QVector<bool> useStored(entries.size());       // true=不压缩(Stored), false=deflate压缩
    QVector<quint32> localHeaderOffsets(entries.size());  // 各条目在ZIP中的偏移量

    for (int i = 0; i < entries.size(); i++) {
        const auto& entry = entries[i];
        const QByteArray& rawData = entry.data;

        entryCrc[i] = computeCrc32(rawData);

        // Qt的qCompress输出格式：4字节大小 + 2字节zlib头 + deflate数据 + 4字节Adler-32
        // 需要剥离zlib头（2字节）和Adler-32尾（4字节），只保留deflate数据
        QByteArray zlibData = qCompress(rawData, 9);

        if (zlibData.size() > 10) {
            compressedData[i] = zlibData.mid(6, zlibData.size() - 10);
        } else {
            compressedData[i] = rawData;
        }

        // 如果压缩后反而更大，则使用Stored模式（不压缩）
        useStored[i] = (compressedData[i].size() >= rawData.size());

        // 记录Local File Header的偏移量，Central Directory需要引用
        localHeaderOffsets[i] = static_cast<quint32>(outFile.pos());

        QByteArray nameUtf8 = entry.path.toUtf8();
        QByteArray finalData = useStored[i] ? rawData : compressedData[i];

        // 写入Local File Header（ZIP格式签名 0x04034b50）
        outFile.write("\x50\x4b\x03\x04", 4);
        writeZipUint16(outFile, 20);            // 版本号：2.0
        writeZipUint16(outFile, 0);             // 通用位标志
        writeZipUint16(outFile, useStored[i] ? 0 : 8);  // 压缩方法：0=Stored, 8=Deflate
        writeZipUint16(outFile, 0);             // 最后修改时间
        writeZipUint16(outFile, 0);             // 最后修改日期
        writeZipUint32(outFile, entryCrc[i]);
        writeZipUint32(outFile, static_cast<quint32>(finalData.size()));
        writeZipUint32(outFile, static_cast<quint32>(rawData.size()));
        writeZipUint16(outFile, static_cast<quint16>(nameUtf8.size()));
        writeZipUint16(outFile, 0);
        outFile.write(nameUtf8);
        outFile.write(finalData);
    }

    // Central Directory起始偏移量
    quint32 cdStart = static_cast<quint32>(outFile.pos());

    // 写入Central Directory条目（ZIP格式签名 0x02014b50）
    for (int i = 0; i < entries.size(); i++) {
        const auto& entry = entries[i];
        const QByteArray& rawData = entry.data;
        QByteArray nameUtf8 = entry.path.toUtf8();
        QByteArray finalData = useStored[i] ? rawData : compressedData[i];

        outFile.write("\x50\x4b\x01\x02", 4);
        writeZipUint16(outFile, 20);
        writeZipUint16(outFile, 20);
        writeZipUint16(outFile, 0);
        writeZipUint16(outFile, useStored[i] ? 0 : 8);
        writeZipUint16(outFile, 0);
        writeZipUint16(outFile, 0);
        writeZipUint32(outFile, entryCrc[i]);
        writeZipUint32(outFile, static_cast<quint32>(finalData.size()));
        writeZipUint32(outFile, static_cast<quint32>(rawData.size()));
        writeZipUint16(outFile, static_cast<quint16>(nameUtf8.size()));
        writeZipUint16(outFile, 0);
        writeZipUint16(outFile, 0);
        writeZipUint16(outFile, 0);
        writeZipUint16(outFile, 0);
        writeZipUint32(outFile, 0);
        writeZipUint32(outFile, localHeaderOffsets[i]);
        outFile.write(nameUtf8);
    }

    quint32 cdEnd = static_cast<quint32>(outFile.pos());
    quint32 cdSize = cdEnd - cdStart;

    // 写入End of Central Directory Record（ZIP格式签名 0x06054b50）
    outFile.write("\x50\x4b\x05\x06", 4);
    writeZipUint16(outFile, 0);
    writeZipUint16(outFile, 0);
    writeZipUint16(outFile, static_cast<quint16>(entries.size()));
    writeZipUint16(outFile, static_cast<quint16>(entries.size()));
    writeZipUint32(outFile, cdSize);
    writeZipUint32(outFile, cdStart);
    writeZipUint16(outFile, 0);

    outFile.close();

    LOG_INFO("ReportGenerator", QString("[DOCX导出] 报告已导出至: %1, 文件数: %2").arg(filePath).arg(entries.size()));
    return true;
}

void ReportGenerator::writeZipUint16(QFile& f, quint16 val) {
    f.putChar(val & 0xFF);
    f.putChar((val >> 8) & 0xFF);
}

void ReportGenerator::writeZipUint32(QFile& f, quint32 val) {
    f.putChar(val & 0xFF);
    f.putChar((val >> 8) & 0xFF);
    f.putChar((val >> 16) & 0xFF);
    f.putChar((val >> 24) & 0xFF);
}

/**
 * @brief 将HTML内容转换为DOCX文档主体XML（WordProcessingML格式）
 *
 * 转换策略：
 * - 使用正则表达式逐标签解析HTML
 * - 将HTML标签映射为OOXML的<w:p>（段落）、<w:r>（文本运行）、<w:tbl>（表格）等元素
 * - 图表图片通过r:embed属性引用word/_rels/document.xml.rels中定义的关系ID
 *
 * 支持的HTML标签映射：
 * - <h1>/<h3> → 居中加粗36号段落
 * - <h2> → 蓝色加粗28号段落
 * - <h4> → 加粗22号段落
 * - <p> → 21号常规段落
 * - <table>/<tr>/<td>/<th> → OOXML表格
 * - <img> → 内嵌图片（wp:inline）
 * - <ul>/<li> → 列表项
 *
 * @param html HTML源内容
 * @return OOXML格式的<w:body>内部内容
 */
QString ReportGenerator::convertHtmlToDocxBody(const QString& html, const QMap<QString, int>& imageRIdMap, const QMap<QString, QSize>& imageSizeMap)
{
    QString body;

    QRegularExpression imgRegex("<img\\s+[^>]*src=['\"]([^'\"]+)['\"][^>]*>", QRegularExpression::CaseInsensitiveOption);
    QRegularExpression tagStripRegex("<[^>]+>");
    QRegularExpression mainTagRegex("(</?(?:h[1-4]|p|table|/table|tr|/tr|td|th|/td|/th|ul|/ul|ol|/ol|li|img|br|div|/div)[^>]*>)",
                                     QRegularExpression::CaseInsensitiveOption);

    QVector<QString> tokens;
    int lastEnd = 0;
    auto matchIter = mainTagRegex.globalMatch(html);
    while (matchIter.hasNext()) {
        auto m = matchIter.next();
        if (m.capturedStart() > lastEnd) {
            QString text = html.mid(lastEnd, m.capturedStart() - lastEnd).trimmed();
            if (!text.isEmpty()) {
                tokens.append(text);
            }
        }
        tokens.append(m.captured());
        lastEnd = m.capturedEnd();
    }
    if (lastEnd < html.size()) {
        QString text = html.mid(lastEnd).trimmed();
        if (!text.isEmpty()) {
            tokens.append(text);
        }
    }

    bool inTable = false;
    bool inRow = false;
    bool inCell = false;
    bool cellIsHeader = false;
    int listLevel = 0;
    int figureCounter = 0;
    bool inFooter = false;

    enum Context { None, H1, H2, H3, H4, P };
    Context currentContext = None;
    QString currentTagAttrs;
    QString collectedText;

    auto flushText = [&]() {
        if (collectedText.isEmpty() && currentContext == None) return;

        QString text = collectedText.trimmed();
        QString originalText = text;
        collectedText.clear();
        if (text.isEmpty()) return;

        text.remove(tagStripRegex);

        switch (currentContext) {
        case H1: {
            bool isTitle = currentTagAttrs.contains("title");
            if (isTitle) {
                body += QString("<w:p><w:pPr><w:jc w:val=\"center\"/><w:spacing w:before=\"480\" w:after=\"360\"/>"
                    "<w:rPr><w:rFonts w:eastAsia=\"SimHei\"/><w:b/><w:sz w:val=\"44\"/><w:szCs w:val=\"44\"/></w:rPr></w:pPr>"
                    "<w:r><w:rPr><w:rFonts w:eastAsia=\"SimHei\"/><w:b/><w:sz w:val=\"44\"/><w:szCs w:val=\"44\"/>"
                    "</w:rPr><w:t>%1</w:t></w:r></w:p>\n").arg(escapeXml(text));
            } else {
                body += QString("<w:p><w:pPr><w:jc w:val=\"center\"/><w:spacing w:before=\"480\" w:after=\"240\"/>"
                    "<w:rPr><w:rFonts w:eastAsia=\"SimHei\"/><w:b/><w:sz w:val=\"32\"/><w:szCs w:val=\"32\"/></w:rPr></w:pPr>"
                    "<w:r><w:rPr><w:rFonts w:eastAsia=\"SimHei\"/><w:b/><w:sz w:val=\"32\"/><w:szCs w:val=\"32\"/>"
                    "</w:rPr><w:t>%1</w:t></w:r></w:p>\n").arg(escapeXml(text));
            }
            break;
        }
        case H2:
            body += QString("<w:p><w:pPr><w:spacing w:before=\"480\" w:after=\"240\"/>"
                "<w:rPr><w:rFonts w:eastAsia=\"SimHei\"/><w:b/><w:sz w:val=\"32\"/><w:szCs w:val=\"32\"/>"
                "</w:rPr></w:pPr>"
                "<w:r><w:rPr><w:rFonts w:eastAsia=\"SimHei\"/><w:b/><w:sz w:val=\"32\"/><w:szCs w:val=\"32\"/>"
                "</w:rPr><w:t>%1</w:t></w:r></w:p>\n").arg(escapeXml(text));
            break;
        case H3:
            body += QString("<w:p><w:pPr><w:spacing w:before=\"360\" w:after=\"120\"/>"
                "<w:rPr><w:rFonts w:eastAsia=\"SimSun\"/><w:b/><w:sz w:val=\"28\"/><w:szCs w:val=\"28\"/>"
                "</w:rPr></w:pPr>"
                "<w:r><w:rPr><w:rFonts w:eastAsia=\"SimSun\"/><w:b/><w:sz w:val=\"28\"/><w:szCs w:val=\"28\"/>"
                "</w:rPr><w:t>%1</w:t></w:r></w:p>\n").arg(escapeXml(text));
            break;
        case H4:
            body += QString("<w:p><w:pPr><w:spacing w:before=\"240\" w:after=\"60\"/>"
                "<w:rPr><w:rFonts w:eastAsia=\"SimSun\"/><w:sz w:val=\"24\"/><w:szCs w:val=\"24\"/>"
                "</w:rPr></w:pPr>"
                "<w:r><w:rPr><w:rFonts w:eastAsia=\"SimSun\"/><w:sz w:val=\"24\"/><w:szCs w:val=\"24\"/>"
                "</w:rPr><w:t>%1</w:t></w:r></w:p>\n").arg(escapeXml(text));
            break;
        case P: {
            bool isTableCaption = currentTagAttrs.contains("table-caption");
            bool isFigureCaption = currentTagAttrs.contains("figure-caption");
            bool noIndent = currentTagAttrs.contains("no-indent");
            bool isListItem = (currentTagAttrs == "li");

            if (isListItem) {
                QString bullet = listLevel <= 1 ? QString(QChar(0x2022)) : QString(QChar(0x25E6));
                body += QString("<w:p><w:pPr><w:ind w:left=\"420\" w:hanging=\"210\"/></w:pPr>"
                    "<w:r><w:rPr><w:rFonts w:eastAsia=\"SimSun\"/><w:sz w:val=\"24\"/><w:szCs w:val=\"24\"/></w:rPr>"
                    "<w:t xml:space=\"preserve\">%1 </w:t></w:r>%2</w:p>\n")
                    .arg(bullet)
                    .arg(convertTextWithBold(originalText, 24));
            } else if (isTableCaption) {
                body += QString("<w:p><w:pPr><w:jc w:val=\"center\"/><w:spacing w:before=\"120\" w:after=\"60\"/>"
                    "<w:rPr><w:rFonts w:eastAsia=\"SimSun\"/><w:b/><w:sz w:val=\"21\"/><w:szCs w:val=\"21\"/></w:rPr></w:pPr>"
                    "<w:r><w:rPr><w:rFonts w:eastAsia=\"SimSun\"/><w:b/><w:sz w:val=\"21\"/><w:szCs w:val=\"21\"/>"
                    "</w:rPr><w:t>%1</w:t></w:r></w:p>\n").arg(escapeXml(text));
            } else if (isFigureCaption) {
                body += QString("<w:p><w:pPr><w:jc w:val=\"center\"/><w:spacing w:before=\"60\" w:after=\"120\"/>"
                    "<w:rPr><w:rFonts w:eastAsia=\"SimSun\"/><w:b/><w:sz w:val=\"21\"/><w:szCs w:val=\"21\"/></w:rPr></w:pPr>"
                    "<w:r><w:rPr><w:rFonts w:eastAsia=\"SimSun\"/><w:b/><w:sz w:val=\"21\"/><w:szCs w:val=\"21\"/>"
                    "</w:rPr><w:t>%1</w:t></w:r></w:p>\n").arg(escapeXml(text));
            } else if (inFooter) {
                body += QString("<w:p><w:pPr><w:jc w:val=\"center\"/><w:spacing w:before=\"60\" w:after=\"60\"/>"
                    "<w:rPr><w:rFonts w:eastAsia=\"SimSun\"/><w:sz w:val=\"18\"/><w:szCs w:val=\"18\"/>"
                    "<w:color w:val=\"CC0000\"/></w:rPr></w:pPr>"
                    "<w:r><w:rPr><w:rFonts w:eastAsia=\"SimSun\"/><w:sz w:val=\"18\"/><w:szCs w:val=\"18\"/>"
                    "<w:color w:val=\"CC0000\"/></w:rPr><w:t>%1</w:t></w:r></w:p>\n").arg(escapeXml(text));
            } else {
                QString indentXml = noIndent ? "" : "<w:ind w:firstLineChars=\"200\" w:firstLine=\"480\"/>";
                body += QString("<w:p><w:pPr><w:jc w:val=\"both\"/><w:spacing w:before=\"60\" w:after=\"60\"/>%1</w:pPr>%2</w:p>\n")
                    .arg(indentXml)
                    .arg(convertTextWithBold(originalText, 24));
            }
            break;
        }
        default:
            if (!text.isEmpty()) {
                body += QString("<w:p><w:pPr><w:jc w:val=\"both\"/><w:spacing w:before=\"60\" w:after=\"60\"/>"
                    "<w:ind w:firstLineChars=\"200\" w:firstLine=\"480\"/></w:pPr>%1</w:p>\n")
                    .arg(convertTextWithBold(originalText, 24));
            }
            break;
        }
    };

    for (int i = 0; i < tokens.size(); i++) {
        const QString& token = tokens[i];

        if (token.startsWith("<h1") || token.startsWith("<h2") || token.startsWith("<h3") || token.startsWith("<h4") ||
            token.startsWith("<p")) {
        }

        bool isStartTag = token.startsWith("<") && !token.startsWith("</");
        bool isEndTag = token.startsWith("</");

        if (isEndTag) {
            if (token.startsWith("</h1") || token.startsWith("</h2") || token.startsWith("</h3") || token.startsWith("</h4") ||
                token.startsWith("</p")) {
                flushText();
                currentContext = None;
                currentTagAttrs.clear();
                continue;
            }
            if (token == "</table>") {
                inTable = false;
                body += "</w:tbl>\n";
                continue;
            }
            if (token == "</tr>" && inTable) {
                inRow = false;
                body += "</w:tr>\n";
                continue;
            }
            if ((token == "</td>" || token == "</th>") && inTable && inRow) {
                inCell = false;
                body += "</w:t></w:r></w:p></w:tc>";
                continue;
            }
            if (token == "</ul>" || token == "</ol>") { listLevel--; continue; }
            if (token == "</li>") {
                if (currentContext == P && currentTagAttrs == "li") {
                    flushText();
                    currentContext = None;
                    currentTagAttrs.clear();
                }
                continue;
            }
            if (token == "</div>") { inFooter = false; continue; }
            continue;
        }

        if (isStartTag) {
            if (token.startsWith("<h1")) {
                flushText();
                currentContext = H1;
                currentTagAttrs = token;
                collectedText.clear();
                continue;
            }
            if (token.startsWith("<h2")) {
                flushText();
                currentContext = H2;
                currentTagAttrs = token;
                collectedText.clear();
                continue;
            }
            if (token.startsWith("<h3")) {
                flushText();
                currentContext = H3;
                currentTagAttrs = token;
                collectedText.clear();
                continue;
            }
            if (token.startsWith("<h4")) {
                flushText();
                currentContext = H4;
                currentTagAttrs = token;
                collectedText.clear();
                continue;
            }
            if (token.startsWith("<p")) {
                flushText();
                currentContext = P;
                currentTagAttrs = token;
                collectedText.clear();
                continue;
            }
            if (token.startsWith("<img")) {
                auto m = imgRegex.match(token);
                if (m.hasMatch()) {
                    QString src = m.captured(1);
                    if (!src.startsWith("data:") && !src.startsWith("chart_")) {
                        int rId = 0;
                        if (imageRIdMap.contains(src)) {
                            rId = imageRIdMap[src];
                        } else {
                            rId = figureCounter + 1;
                            figureCounter++;
                        }

                        const int maxCx = 5040000;
                        int cx = maxCx;
                        int cy = 3600000;
                        if (imageSizeMap.contains(src)) {
                            QSize imgSize = imageSizeMap[src];
                            if (imgSize.width() > 0 && imgSize.height() > 0) {
                                double aspectRatio = static_cast<double>(imgSize.width()) / imgSize.height();
                                cx = maxCx;
                                cy = static_cast<int>(maxCx / aspectRatio);
                                if (cy > 7200000) {
                                    cy = 7200000;
                                    cx = static_cast<int>(cy * aspectRatio);
                                }
                            }
                        }

                        body += QString("<w:p><w:pPr><w:jc w:val=\"center\"/><w:spacing w:before=\"240\" w:after=\"120\"/></w:pPr>"
                            "<w:r><w:drawing>"
                            "<wp:inline distT=\"0\" distB=\"0\" distL=\"0\" distR=\"0\">"
                            "<wp:extent cx=\"%1\" cy=\"%2\"/>"
                            "<wp:effectExtent l=\"0\" t=\"0\" r=\"0\" b=\"0\"/>"
                            "<wp:docPr id=\"%3\" name=\"Picture %4\"/>"
                            "<wp:cNvGraphicFramePr><a:graphicFrameLocks noChangeAspect=\"1\"/></wp:cNvGraphicFramePr>"
                            "<a:graphic><a:graphicData uri=\"http://schemas.openxmlformats.org/drawingml/2006/picture\">"
                            "<pic:pic><pic:nvPicPr><pic:cNvPr id=\"0\" name=\"Picture %4\"/><pic:cNvPicPr/></pic:nvPicPr>"
                            "<pic:blipFill><a:blip r:embed=\"rId%5\"/><a:stretch><a:fillRect/></a:stretch></pic:blipFill>"
                            "<pic:spPr><a:xfrm><a:off x=\"0\" y=\"0\"/><a:ext cx=\"%1\" cy=\"%2\"/></a:xfrm>"
                            "<a:prstGeom prst=\"rect\"><a:avLst/></a:prstGeom></pic:spPr></pic:pic>"
                            "</a:graphicData></a:graphic></wp:inline></w:drawing></w:r></w:p>\n")
                            .arg(cx).arg(cy).arg(rId).arg(rId).arg(rId);
                    }
                }
                continue;
            }
            if (token.startsWith("<table")) {
                flushText();
                inTable = true;
                body += "<w:tbl><w:tblPr><w:tblW w:w=\"5000\" w:type=\"pct\"/>"
                    "<w:jc w:val=\"center\"/>"
                    "<w:tblBorders>\n"
                    "<w:top w:val=\"single\" w:sz=\"12\" w:space=\"0\" w:color=\"000000\"/>\n"
                    "<w:left w:val=\"single\" w:sz=\"12\" w:space=\"0\" w:color=\"000000\"/>\n"
                    "<w:bottom w:val=\"single\" w:sz=\"12\" w:space=\"0\" w:color=\"000000\"/>\n"
                    "<w:right w:val=\"single\" w:sz=\"12\" w:space=\"0\" w:color=\"000000\"/>\n"
                    "<w:insideH w:val=\"single\" w:sz=\"6\" w:space=\"0\" w:color=\"000000\"/>\n"
                    "<w:insideV w:val=\"single\" w:sz=\"6\" w:space=\"0\" w:color=\"000000\"/>\n"
                    "</w:tblBorders>"
                    "<w:tblCellMar><w:top w:w=\"60\" w:type=\"dxa\"/>"
                    "<w:left w:w=\"100\" w:type=\"dxa\"/>"
                    "<w:bottom w:w=\"60\" w:type=\"dxa\"/>"
                    "<w:right w:w=\"100\" w:type=\"dxa\"/>"
                    "</w:tblCellMar>"
                    "</w:tblPr>\n";
                continue;
            }
            if ((token == "<tr>" || token.startsWith("<tr ")) && inTable) {
                inRow = true;
                body += "<w:tr>";
                continue;
            }
            if ((token.startsWith("<td") || token.startsWith("<th")) && inTable && inRow) {
                inCell = true;
                cellIsHeader = token.startsWith("<th");
                body += "<w:tc><w:tcPr>";
                if (cellIsHeader) {
                    body += "<w:shd w:val=\"clear\" w:color=\"auto\" w:fill=\"D9D9D9\"/>";
                }
                body += "</w:tcPr><w:p><w:pPr><w:jc w:val=\"center\"/>"
                    "<w:rPr><w:rFonts w:eastAsia=\"SimSun\"/><w:sz w:val=\"21\"/><w:szCs w:val=\"21\"/>";
                if (cellIsHeader) body += "<w:b/>";
                body += "</w:rPr></w:pPr><w:r><w:rPr><w:rFonts w:eastAsia=\"SimSun\"/><w:sz w:val=\"21\"/><w:szCs w:val=\"21\"/>";
                if (cellIsHeader) body += "<w:b/>";
                body += "</w:rPr><w:t>";
                continue;
            }
            if (token == "<ul>" || token.startsWith("<ul ") || token == "<ol>" || token.startsWith("<ol ")) {
                listLevel++;
                continue;
            }
            if (token.startsWith("<li")) {
                QRegularExpression liContentRegex(">([^<]*)<");
                auto liMatch = liContentRegex.match(token);
                QString liText;
                if (liMatch.hasMatch()) {
                    liText = liMatch.captured(1).trimmed();
                }
                if (!liText.isEmpty()) {
                    body += QString("<w:p><w:pPr><w:ind w:left=\"420\" w:hanging=\"210\"/>"
                        "<w:rPr><w:rFonts w:eastAsia=\"SimSun\"/><w:sz w:val=\"24\"/><w:szCs w:val=\"24\"/>"
                        "</w:rPr></w:pPr>"
                        "<w:r><w:rPr><w:rFonts w:eastAsia=\"SimSun\"/><w:sz w:val=\"24\"/><w:szCs w:val=\"24\"/>"
                        "</w:rPr><w:t xml:space=\"preserve\">%1 %2</w:t></w:r></w:p>\n")
                        .arg(listLevel <= 1 ? QChar(0x2022) : QChar(0x25E6))
                        .arg(escapeXml(liText));
                } else {
                    currentContext = P;
                    currentTagAttrs = "li";
                    collectedText.clear();
                }
                continue;
            }
            if (token == "<br>" || token == "<br/>" || token == "<br />") {
                body += "<w:p><w:pPr><w:rPr><w:sz w:val=\"24\"/><w:szCs w:val=\"24\"/></w:rPr></w:pPr>"
                    "<w:r><w:rPr><w:sz w:val=\"24\"/><w:szCs w:val=\"24\"/></w:rPr><w:br/></w:r></w:p>\n";
                continue;
            }
            if (token.startsWith("<div")) {
                if (token.contains("footer")) {
                    inFooter = true;
                }
                continue;
            }
            continue;
        }

        if (inCell) {
            if (!token.startsWith("<")) {
                body += escapeXml(token.trimmed());
            }
            continue;
        }

        if (currentContext != None) {
            collectedText += token;
            continue;
        }
    }

    flushText();

    return body;
}

QString ReportGenerator::escapeXml(const QString& input)
{
    QString result = input;
    result.replace("&", "&amp;");
    result.replace("<", "&lt;");
    result.replace(">", "&gt;");
    result.replace("\"", "&quot;");
    result.replace("'", "&apos;");
    return result;
}

QString ReportGenerator::convertTextWithBold(const QString& text, int fontSize)
{
    QString normalized = text;
    normalized.replace(QRegularExpression("<b\\s*>", QRegularExpression::CaseInsensitiveOption), "<strong>");
    normalized.replace(QRegularExpression("</b\\s*>", QRegularExpression::CaseInsensitiveOption), "</strong>");

    QString result;
    QString szVal = QString::number(fontSize);

    QRegularExpression boldRegex("<strong[^>]*>([^<]*)</strong>", QRegularExpression::CaseInsensitiveOption);

    int lastEnd = 0;
    auto it = boldRegex.globalMatch(normalized);

    bool hasBold = false;
    while (it.hasNext()) {
        hasBold = true;
        auto match = it.next();

        if (match.capturedStart() > lastEnd) {
            QString normalText = normalized.mid(lastEnd, match.capturedStart() - lastEnd);
            normalText.remove(QRegularExpression("<[^>]*>"));
            if (!normalText.isEmpty()) {
                result += QString("<w:r><w:rPr><w:rFonts w:eastAsia=\"SimSun\"/><w:sz w:val=\"%1\"/><w:szCs w:val=\"%1\"/></w:rPr>"
                    "<w:t xml:space=\"preserve\">%2</w:t></w:r>").arg(szVal, escapeXml(normalText));
            }
        }

        QString boldText = match.captured(1);
        if (!boldText.isEmpty()) {
            result += QString("<w:r><w:rPr><w:rFonts w:eastAsia=\"SimSun\"/><w:b/><w:sz w:val=\"%1\"/><w:szCs w:val=\"%1\"/></w:rPr>"
                "<w:t xml:space=\"preserve\">%2</w:t></w:r>").arg(szVal, escapeXml(boldText));
        }

        lastEnd = match.capturedEnd();
    }

    if (hasBold) {
        if (lastEnd < normalized.length()) {
            QString remainingText = normalized.mid(lastEnd);
            remainingText.remove(QRegularExpression("<[^>]*>"));
            if (!remainingText.isEmpty()) {
                result += QString("<w:r><w:rPr><w:rFonts w:eastAsia=\"SimSun\"/><w:sz w:val=\"%1\"/><w:szCs w:val=\"%1\"/></w:rPr>"
                    "<w:t xml:space=\"preserve\">%2</w:t></w:r>").arg(szVal, escapeXml(remainingText));
            }
        }
    } else {
        QString plainText = normalized;
        plainText.remove(QRegularExpression("<[^>]*>"));
        if (!plainText.isEmpty()) {
            result = QString("<w:r><w:rPr><w:rFonts w:eastAsia=\"SimSun\"/><w:sz w:val=\"%1\"/><w:szCs w:val=\"%1\"/></w:rPr>"
                "<w:t xml:space=\"preserve\">%2</w:t></w:r>").arg(szVal, escapeXml(plainText));
        }
    }

    return result;
}

bool ReportGenerator::exportToHtml(const QString& filePath, const ReportData& data)
{
    QString html = buildHtmlReport(data);

    QString finalHtml = html;

    for (auto it = data.charts.begin(); it != data.charts.end(); ++it) {
        QByteArray byteArray;
        QBuffer buffer(&byteArray);
        buffer.open(QIODevice::WriteOnly);
        it.value().save(&buffer, "PNG");
        QString base64 = QString("data:image/png;base64,%1").arg(QString(byteArray.toBase64()));
        QString chartKey = QString("chart_%1").arg(it.key());
        finalHtml.replace(QString("src='%1'").arg(chartKey),
                         QString("src='%1'").arg(base64));
        finalHtml.replace(QString("src=\"%1\"").arg(chartKey),
                         QString("src=\"%1\"").arg(base64));
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_lastError = QString("无法打开文件: %1").arg(filePath);
        return false;
    }

    QTextStream out(&file);
    out << finalHtml;
    file.close();

    return true;
}
