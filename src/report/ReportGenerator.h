/**
 * @file ReportGenerator.h
 * @brief 智能分析报告生成器类定义
 *
 * ReportGenerator类负责生成1553B总线数据的智能分析报告，
 * 支持HTML、PDF和DOCX三种输出格式。
 *
 * 报告生成流程：
 * 1. 数据收集：从DataStore获取所有统计数据（单次遍历优化）
 * 2. 图表生成：生成消息类型饼图、终端柱状图、时间序列图等
 * 3. AI分析：调用AI模型对数据进行智能分析（支持并行调用）
 * 4. 报告组装：将统计表格、图表、AI分析结果组装为HTML
 * 5. 格式转换：根据需要转换为PDF或DOCX格式
 *
 * 报告结构：
 * - 概览：文件信息、数据总量、成功率、数据质量
 * - 消息分析：消息类型分布、通信模式
 * - 终端分析：各终端数据量、成功率
 * - 错误分析：错误分布、连续错误检测
 * - 性能分析：消息间隔、总线利用率、吞吐量
 * - AI智能分析：风险评估、改进建议
 *
 * 线程安全说明：
 * - generateReportAsync()在后台线程执行
 * - 进度通过信号报告，UI线程接收更新
 * - 使用std::atomic<bool>支持取消操作
 *
 * @author 1553BTools
 * @date 2024
 */

#ifndef REPORTGENERATOR_H
#define REPORTGENERATOR_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QVector>
#include <QPixmap>
#include <QJsonObject>
#include <atomic>
#include "core/datastore/DataStore.h"
#include "model/ModelAdapter.h"

class QFile;

/**
 * @brief 报告概览数据
 *
 * 包含报告的基本信息摘要
 */
struct ReportOverview {
    QString fileName;           ///< 源文件名
    QString importTime;         ///< 导入时间
    int totalRecords;           ///< 总数据记录数
    int totalMessages;          ///< 总消息数
    double duration;            ///< 数据持续时间（秒）
    double successRate;         ///< 传输成功率（百分比）
    QString dataQuality;        ///< 数据质量评估（优/良/中/差）
    int activeTerminals;        ///< 活跃终端数量
    int activeSubAddresses;     ///< 活跃子地址数量
};

/**
 * @brief 消息类型统计
 *
 * 各消息类型的数量和占比
 */
struct MessageStats {
    int bcToRtCount;            ///< BC→RT消息数量
    int rtToBcCount;            ///< RT→BC消息数量
    int rtToRtCount;            ///< RT→RT消息数量
    int broadcastCount;         ///< 广播消息数量
    double bcToRtPercent;       ///< BC→RT占比
    double rtToBcPercent;       ///< RT→BC占比
    double rtToRtPercent;       ///< RT→RT占比
    double broadcastPercent;    ///< 广播占比
};

/**
 * @brief 单个终端的统计数据
 */
struct TerminalStats {
    int terminalAddress;        ///< 终端地址
    int messageCount;           ///< 消息数量
    double percent;             ///< 占总消息数的百分比
    int successCount;           ///< 成功次数
    int failCount;              ///< 失败次数
    double successRate;         ///< 成功率
};

/**
 * @brief 错误统计数据
 */
struct ErrorStats {
    int totalErrors;            ///< 总错误数
    double errorRate;           ///< 错误率
    QMap<int, int> errorsByTerminal;   ///< 按终端统计的错误数
    QMap<int, int> errorsByType;       ///< 按消息类型统计的错误数
    QVector<int> errorTimeline;        ///< 错误时间线（按时间段统计）
    int maxConsecutiveErrors;   ///< 最大连续错误数
};

/**
 * @brief 性能指标数据
 */
struct PerformanceMetrics {
    double avgInterval;         ///< 平均消息间隔（毫秒）
    double minInterval;         ///< 最小消息间隔
    double maxInterval;         ///< 最大消息间隔
    double busUtilization;      ///< 总线利用率
    double throughput;          ///< 吞吐量（消息/秒）
    double avgLatency;          ///< 平均延迟
    int peakMessageRate;        ///< 峰值消息速率
    QString peakTime;           ///< 峰值发生时间
};

/**
 * @brief 通信模式数据
 *
 * 检测到的周期性通信模式
 */
struct CommunicationPattern {
    int terminal;               ///< 终端地址
    int subAddress;             ///< 子地址
    double period;              ///< 通信周期（毫秒）
    int count;                  ///< 出现次数
    double stability;           ///< 周期稳定性（0-1，1为完全稳定）
};

/**
 * @brief 关键事件
 */
struct KeyEvent {
    QString time;               ///< 事件发生时间
    QString type;               ///< 事件类型
    QString description;        ///< 事件描述
    QString impact;             ///< 影响评估
};

/**
 * @brief AI分析结果
 *
 * AI模型对各维度的分析文本
 */
struct AIAnalysisResult {
    QString overviewAnalysis;       ///< 概览分析
    QString messageAnalysis;        ///< 消息分析
    QString terminalAnalysis;       ///< 终端分析
    QString errorAnalysis;          ///< 错误分析
    QString performanceAnalysis;    ///< 性能分析
    QString riskAssessment;         ///< 风险评估
    QString recommendations;        ///< 改进建议
};

/**
 * @brief 完整报告数据
 *
 * 包含报告的所有数据，在生成过程中逐步填充
 */
struct ReportData {
    ReportOverview overview;                        ///< 概览数据
    MessageStats messageStats;                      ///< 消息统计
    QVector<TerminalStats> terminalStats;           ///< 终端统计列表
    ErrorStats errorStats;                          ///< 错误统计
    PerformanceMetrics performance;                 ///< 性能指标
    QVector<CommunicationPattern> patterns;         ///< 通信模式列表
    QVector<KeyEvent> keyEvents;                    ///< 关键事件列表
    AIAnalysisResult aiAnalysis;                    ///< AI分析结果
    QMap<QString, QImage> charts;                   ///< 图表图像映射（键为图表名称）
};

/**
 * @brief 智能分析报告生成器类
 *
 * 该类负责从DataStore收集数据、生成图表、调用AI分析，
 * 并将结果组装为完整报告。支持异步生成和取消操作。
 */
class ReportGenerator : public QObject
{
    Q_OBJECT

public:
    explicit ReportGenerator(QObject *parent = nullptr);
    ~ReportGenerator();

    /**
     * @brief 设置数据存储对象
     * @param store DataStore对象指针
     */
    void setDataStore(DataStore* store);

    /**
     * @brief 设置AI模型提供商
     * @param provider ModelAdapter对象指针
     *
     * 用于生成AI智能分析内容，不设置则跳过AI分析部分
     */
    void setModelProvider(ModelAdapter* provider);

    /**
     * @brief 异步生成报告
     * @param filePath 输出文件路径
     * @param format 输出格式（"html"/"pdf"/"docx"）
     *
     * 在后台线程中执行报告生成，通过信号报告进度和结果
     */
    void generateReportAsync(const QString& filePath, const QString& format = "html");

    /**
     * @brief 取消报告生成
     *
     * 设置取消标志，生成过程中定期检查此标志
     */
    void cancelReport();

    /**
     * @brief 获取最后的错误信息
     * @return 错误描述字符串
     */
    QString lastError() const;

signals:
    /**
     * @brief 进度变化信号
     * @param percent 进度百分比（0-100）
     * @param stage 当前阶段描述
     * @param elapsedSeconds 已耗时（秒）
     */
    void progressChanged(int percent, const QString& stage, double elapsedSeconds);

    /**
     * @brief 报告生成完成信号
     * @param success 是否成功
     * @param filePath 输出文件路径
     */
    void reportFinished(bool success, const QString& filePath);

    /**
     * @brief 错误发生信号
     * @param error 错误描述
     */
    void errorOccurred(const QString& error);

private:
    /**
     * @brief 单次遍历收集所有统计数据
     * @param records 数据记录列表
     * @param data 输出的报告数据
     *
     * 性能优化：只遍历数据一次，同时计算所有统计指标
     */
    void collectAllStatsSinglePass(const QVector<DataRecord>& records, ReportData& data);

    /**
     * @brief 并行执行AI分析
     * @param data 报告数据
     * @return AI分析结果
     *
     * 使用QtConcurrent并行调用AI分析各维度，
     * 显著减少总分析时间
     */
    AIAnalysisResult performAIAnalysisParallel(const ReportData& data);

    /**
     * @brief AI单次调用（供并行使用）
     * @param prompt 分析提示词
     * @return AI响应内容
     */
    QString callAIForAnalysis(const QString& prompt);

    /**
     * @brief 串行执行AI分析（备选方案）
     */
    AIAnalysisResult performAIAnalysis(const ReportData& data);

    /**
     * @brief 生成消息类型饼图
     * @param stats 消息统计数据
     * @return 图表图像（QImage，线程安全）
     */
    QImage generateMessageTypePieChart(const MessageStats& stats, int dpiScale = 2);

    QImage cropImageMargins(const QImage& image, int margin = 20);

    /**
     * @brief 生成终端数据量柱状图
     * @param stats 终端统计列表
     * @return 图表图像
     */
    QImage generateTerminalBarChart(const QVector<TerminalStats>& stats, int dpiScale = 2);

    /**
     * @brief 生成时间序列折线图
     * @param records 数据记录列表
     * @return 图表图像
     */
    QImage generateTimeSeriesChart(const QVector<DataRecord>& records, int dpiScale = 2);

    /**
     * @brief 生成错误分布图
     * @param stats 错误统计数据
     * @return 图表图像
     */
    QImage generateErrorDistributionChart(const ErrorStats& stats, int dpiScale = 2);

    /**
     * @brief 生成成功率对比图
     * @param stats 终端统计列表
     * @return 图表图像
     */
    QImage generateSuccessRateChart(const QVector<TerminalStats>& stats, int dpiScale = 2);

    /**
     * @brief 构建1553B协议说明段落
     * @return HTML格式的协议说明
     */
    QString buildProtocolDescription();

    /**
     * @brief 构建统计数据章节
     * @param data 报告数据
     * @return HTML格式的统计表格
     */
    QString buildStatisticsSection(const ReportData& data);

    /**
     * @brief 构建图表章节
     * @param data 报告数据（包含图表图像）
     * @return HTML格式的图表展示
     */
    QString buildChartsSection(const ReportData& data);

    /**
     * @brief 构建AI分析章节
     * @param aiResult AI分析结果
     * @return HTML格式的AI分析内容
     */
    QString buildAIAnalysisSection(const AIAnalysisResult& aiResult);

    struct StyleConfig {
        QString bodyFontSize;
        QString bodyLineHeight;
        QString bodyTextIndent;
        QString h1FontSize;
        QString h2FontSize;
        QString h3FontSize;
        QString h4FontSize;
        QString h4FontWeight;
        QString thFontSize;
        QString tdFontSize;
        QString captionFontSize;
        QString tableBorderWidth;
        QString cellBorderWidth;
        QString chartMaxWidth;
        bool isPdf;
    };
    
    static StyleConfig htmlStyle();
    static StyleConfig pdfStyle();
    
    QString buildReportHtml(const ReportData& data, const StyleConfig& style);

    QString buildHtmlReport(const ReportData& data);

    QString buildPdfHtmlReport(const ReportData& data);

    /**
     * @brief 导出为PDF格式
     * @param filePath 输出路径
     * @param data 报告数据
     * @return 成功返回true
     */
    bool exportToPdf(const QString& filePath, const ReportData& data);

    /**
     * @brief 导出为HTML格式
     * @param filePath 输出路径
     * @param data 报告数据
     * @return 成功返回true
     */
    bool exportToHtml(const QString& filePath, const ReportData& data);

    /**
     * @brief 导出为DOCX格式
     * @param filePath 输出路径
     * @param data 报告数据
     * @return 成功返回true
     *
     * DOCX文件本质是ZIP压缩包，包含：
     * - [Content_Types].xml：内容类型定义
     * - _rels/.rels：包关系
     * - word/document.xml：主文档内容
     * - word/_rels/document.xml.rels：文档关系
     */
    bool exportToDocx(const QString& filePath, const ReportData& data);

    /**
     * @brief 将HTML内容转换为DOCX文档主体XML
     * @param html HTML源内容
     * @return OOXML格式的document.xml内容
     */
    QString convertHtmlToDocxBody(const QString& html, const QMap<QString, int>& imageRIdMap = QMap<QString, int>(), const QMap<QString, QSize>& imageSizeMap = QMap<QString, QSize>());

    /**
     * @brief XML特殊字符转义
     * @param input 原始字符串
     * @return 转义后的XML安全字符串
     */
    static QString escapeXml(const QString& input);

    /**
     * @brief 将包含<strong>标签的文本转换为DOCX格式
     * @param text 包含HTML标签的文本
     * @param fontSize 字体大小（半磅）
     * @return DOCX格式的<w:r>元素字符串
     */
    static QString convertTextWithBold(const QString& text, int fontSize);

    /**
     * @brief 向ZIP文件写入16位小端整数
     * @param f 文件对象
     * @param val 要写入的值
     */
    static void writeZipUint16(QFile& f, quint16 val);

    /**
     * @brief 向ZIP文件写入32位小端整数
     * @param f 文件对象
     * @param val 要写入的值
     */
    static void writeZipUint32(QFile& f, quint32 val);

    /**
     * @brief 格式化时间戳为可读字符串
     * @param timestampMs 时间戳（毫秒）
     * @return 格式化的时间字符串
     */
    static QString formatTimestamp(double timestampMs);

    /**
     * @brief 构建综合优化建议的AI Prompt
     * @param data 报告数据
     * @return 格式化的Prompt字符串
     */
    QString buildRecommendationPrompt(const ReportData& data) const;

    DataStore* m_dataStore;             ///< 数据存储对象
    ModelAdapter* m_modelProvider;      ///< AI模型提供商
    QString m_lastError;                ///< 最后的错误信息
    std::atomic<bool> m_cancelFlag;     ///< 取消标志（原子操作，线程安全）

    qint64 m_startTime;                 ///< 报告生成开始时间戳
    QVector<DataRecord> m_cachedRecords; ///< 缓存的数据记录（用于PDF高分辨率图表重渲染）
};

#endif
