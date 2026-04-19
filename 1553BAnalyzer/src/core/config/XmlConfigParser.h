/**
 * @file XmlConfigParser.h
 * @brief XML配置文件解析器
 *
 * XmlConfigParser类负责读写XML格式的配置文件，将XML节点映射为
 * ConfigManager中定义的配置结构体（ParserConfig、GanttConfig等）。
 *
 * 配置文件结构示例：
 * @code
 * <config>
 *   <parser>
 *     <byteOrder>little</byteOrder>
 *     <header1>0xA5A5</header1>
 *     ...
 *   </parser>
 *   <gantt>
 *     <colorBC2RT>#3498db</colorBC2RT>
 *     ...
 *   </gantt>
 *   <models>
 *     <provider id="qwen" name="千问">
 *       <instance name="qwen-turbo" apiKey="..." enabled="true"/>
 *     </provider>
 *   </models>
 * </config>
 * @endcode
 *
 * @author 1553BTools
 * @date 2024
 */

#ifndef XMLCONFIGPARSER_H
#define XMLCONFIGPARSER_H

#include <QString>
#include <QColor>
#include <QVector>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include "ConfigManager.h"

/**
 * @brief XML配置文件解析器类
 *
 * 使用QXmlStreamReader进行读取（基于流式解析，内存效率高），
 * 使用QXmlStreamWriter进行写入（保证XML格式正确）。
 * 所有方法均为无状态的，解析结果通过getter获取。
 */
class XmlConfigParser
{
public:
    XmlConfigParser();

    /**
     * @brief 解析XML配置文件
     * @param filePath 配置文件路径
     * @return 解析成功返回true，失败返回false
     *
     * 使用QXmlStreamReader逐节点解析，遇到错误时停止并记录错误信息
     */
    bool parse(const QString& filePath);

    /**
     * @brief 保存配置到XML文件
     * @param filePath 目标文件路径
     * @param parserConfig 解析器配置
     * @param ganttConfig 甘特图配置
     * @param databaseConfig 数据库配置
     * @param providers AI模型提供商列表
     * @param defaultModel 默认模型ID
     * @param apiTimeout API超时时间（毫秒）
     * @param retryCount 重试次数
     * @param reportFormat 报告格式（html/pdf）
     * @param speechConfig 语音识别配置
     * @return 保存成功返回true，失败返回false
     */
    bool save(const QString& filePath,
              const ParserConfig& parserConfig,
              const GanttConfig& ganttConfig,
              const DatabaseConfig& databaseConfig,
              const QVector<ModelProvider>& providers,
              const QString& defaultModel,
              int apiTimeout,
              int retryCount,
              const QString& reportFormat = "html",
              const SpeechConfig& speechConfig = SpeechConfig());

    ParserConfig parserConfig() const;
    GanttConfig ganttConfig() const;
    DatabaseConfig databaseConfig() const;
    QVector<ModelProvider> modelProviders() const;
    QString defaultModel() const;
    int apiTimeout() const;
    int retryCount() const;
    QString reportFormat() const;
    SpeechConfig speechConfig() const;
    QString lastError() const;

private:
    bool parseParserSection(QXmlStreamReader& xml);
    bool parseGanttSection(QXmlStreamReader& xml);
    bool parseDatabaseSection(QXmlStreamReader& xml);
    bool parseModelsSection(QXmlStreamReader& xml);
    bool parseExportSection(QXmlStreamReader& xml);
    bool parseSpeechSection(QXmlStreamReader& xml);
    bool parseProvider(QXmlStreamReader& xml, ModelProvider& provider);
    bool parseInstance(QXmlStreamReader& xml, ModelInstance& instance);
    QColor parseColor(const QString& colorStr);
    QString colorToString(const QColor& color);

    ParserConfig m_parserConfig;
    GanttConfig m_ganttConfig;
    DatabaseConfig m_databaseConfig;
    QVector<ModelProvider> m_modelProviders;
    QString m_defaultModel;
    int m_apiTimeout;
    int m_retryCount;
    QString m_reportFormat;
    SpeechConfig m_speechConfig;
    QString m_lastError;
};

#endif
