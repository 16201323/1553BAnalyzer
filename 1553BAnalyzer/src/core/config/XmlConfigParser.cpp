/**
 * @file XmlConfigParser.cpp
 * @brief XML配置文件解析器实现
 * 
 * 该文件实现了XML配置文件的读取和保存功能，包括：
 * - 解析器配置（字节序、包头标识等）
 * - 甘特图颜色配置
 * - 大模型提供商配置
 * 
 * @author 1553BTools
 * @date 2024
 */

#include "XmlConfigParser.h"
#include <QFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDebug>
#include <QCoreApplication>
#include "utils/Logger.h"

/**
 * @brief 构造函数，初始化默认配置值
 * 
 * 默认API超时时间：60秒
 * 默认重试次数：3次
 */
XmlConfigParser::XmlConfigParser()
    : m_apiTimeout(60)
    , m_retryCount(3)
    , m_reportFormat("html")
{
    m_databaseConfig.recordThreshold = 50000;
    LOG_DEBUG("XmlConfigParser", u8"XmlConfigParser实例创建");
}

/**
 * @brief 解析XML配置文件
 * @param filePath 配置文件路径
 * @return 解析成功返回true，失败返回false
 * 
 * 解析流程：
 * 1. 打开配置文件
 * 2. 逐个读取XML元素
 * 3. 根据元素名称分发到对应的解析函数
 * 4. 处理解析错误
 */
bool XmlConfigParser::parse(const QString& filePath)
{
    LOG_INFO("XmlConfigParser", QString::fromUtf8(u8"开始解析配置文件: %1").arg(filePath));
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_lastError = QCoreApplication::translate("XmlConfigParser", u8"无法打开配置文件: %1").arg(filePath);
        LOG_ERROR("XmlConfigParser", m_lastError);
        return false;
    }
    
    LOG_DEBUG("XmlConfigParser", u8"配置文件打开成功，开始读取XML内容");
    
    QXmlStreamReader xml(&file);
    
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        
        if (token == QXmlStreamReader::StartElement) {
            QString elementName = xml.name().toString();
            LOG_DEBUG("XmlConfigParser", QString::fromUtf8(u8"发现XML元素: %1").arg(elementName));
            
            if (elementName == "Parser") {
                if (!parseParserSection(xml)) {
                    LOG_WARNING("XmlConfigParser", QString::fromUtf8(u8"解析Parser节点失败"));
                }
            } else if (elementName == "GanttChart") {
                if (!parseGanttSection(xml)) {
                    LOG_WARNING("XmlConfigParser", QString::fromUtf8(u8"解析GanttChart节点失败"));
                }
            } else if (elementName == "Database") {
                if (!parseDatabaseSection(xml)) {
                    LOG_WARNING("XmlConfigParser", QString::fromUtf8(u8"解析Database节点失败"));
                }
            } else if (elementName == "Models") {
                if (!parseModelsSection(xml)) {
                    LOG_WARNING("XmlConfigParser", QString::fromUtf8(u8"解析Models节点失败"));
                }
            } else if (elementName == "Export") {
                if (!parseExportSection(xml)) {
                    LOG_WARNING("XmlConfigParser", QString::fromUtf8(u8"解析Export节点失败"));
                }
            } else if (elementName == "Speech") {
                if (!parseSpeechSection(xml)) {
                    LOG_WARNING("XmlConfigParser", QString::fromUtf8(u8"解析Speech节点失败"));
                }
            }
        }
    }
    
    if (xml.hasError()) {
        m_lastError = QCoreApplication::translate("XmlConfigParser", u8"XML解析错误: %1").arg(xml.errorString());
        LOG_ERROR("XmlConfigParser", QString::fromUtf8(u8"XML解析错误: %1, 行号: %2")
                  .arg(xml.errorString())
                  .arg(xml.lineNumber()));
        file.close();
        return false;
    }
    
    file.close();
    
    LOG_INFO("XmlConfigParser", QString::fromUtf8(u8"配置文件解析完成，共加载 %1 个模型提供商")
              .arg(m_modelProviders.size()));
    
    return true;
}

/**
 * @brief 解析Parser配置节点
 * @param xml XML流读取器引用
 * @return 解析成功返回true
 * 
 * 解析的配置项：
 * - ByteOrder: 字节序（little/big）
 * - Header1: 包头1标识（十六进制）
 * - Header2: 包头2标识（十六进制）
 * - DataHeader: 数据头标识（十六进制）
 * - TimestampUnit: 时间戳单位（微秒）
 * - MaxErrorTolerance: 最大错误容差
 */
bool XmlConfigParser::parseParserSection(QXmlStreamReader& xml)
{
    LOG_DEBUG("XmlConfigParser", u8"开始解析Parser配置节点");
    
    int parsedCount = 0;
    
    while (!(xml.tokenType() == QXmlStreamReader::EndElement && 
             xml.name().toString() == "Parser")) {
        xml.readNext();
        
        if (xml.tokenType() == QXmlStreamReader::StartElement) {
            QString name = xml.name().toString();
            QString text = xml.readElementText();
            
            LOG_DEBUG("XmlConfigParser", QString::fromUtf8(u8"解析配置项: %1 = %2").arg(name, text));
            
            if (name == "ByteOrder") {
                m_parserConfig.byteOrder = text;
                parsedCount++;
            } else if (name == "Header1") {
                m_parserConfig.header1 = text.toUShort(nullptr, 16);
                parsedCount++;
            } else if (name == "Header2") {
                m_parserConfig.header2 = text.toUShort(nullptr, 16);
                parsedCount++;
            } else if (name == "DataHeader") {
                m_parserConfig.dataHeader = text.toUShort(nullptr, 16);
                parsedCount++;
            } else if (name == "TimestampUnit") {
                m_parserConfig.timestampUnit = text.toInt();
                parsedCount++;
            } else if (name == "MaxErrorTolerance") {
                m_parserConfig.maxErrorTolerance = text.toInt();
                parsedCount++;
            }
        }
    }
    
    LOG_INFO("XmlConfigParser", QString::fromUtf8(u8"Parser配置解析完成，共解析 %1 个配置项").arg(parsedCount));
    return true;
}

/**
 * @brief 解析GanttChart配置节点
 * @param xml XML流读取器引用
 * @return 解析成功返回true
 * 
 * 解析的颜色配置：
 * - ColorBC2RT: BC→RT消息颜色
 * - ColorRT2BC: RT→BC消息颜色
 * - ColorRT2RT: RT→RT消息颜色
 * - ColorBroadcast: 广播消息颜色
 * - ColorError: 错误消息颜色
 */
bool XmlConfigParser::parseGanttSection(QXmlStreamReader& xml)
{
    LOG_DEBUG("XmlConfigParser", u8"开始解析GanttChart配置节点");
    
    int parsedCount = 0;
    
    while (!(xml.tokenType() == QXmlStreamReader::EndElement && 
             xml.name().toString() == "GanttChart")) {
        xml.readNext();
        
        if (xml.tokenType() == QXmlStreamReader::StartElement) {
            QString name = xml.name().toString();
            QString text = xml.readElementText();
            
            LOG_DEBUG("XmlConfigParser", QString::fromUtf8(u8"解析颜色配置: %1 = %2").arg(name, text));
            
            if (name == "ColorBC2RT") {
                m_ganttConfig.colorBC2RT = parseColor(text);
                parsedCount++;
            } else if (name == "ColorRT2BC") {
                m_ganttConfig.colorRT2BC = parseColor(text);
                parsedCount++;
            } else if (name == "ColorRT2RT") {
                m_ganttConfig.colorRT2RT = parseColor(text);
                parsedCount++;
            } else if (name == "ColorBroadcast") {
                m_ganttConfig.colorBroadcast = parseColor(text);
                parsedCount++;
            } else if (name == "ColorError") {
                m_ganttConfig.colorError = parseColor(text);
                parsedCount++;
            }
        }
    }
    
    LOG_INFO("XmlConfigParser", QString::fromUtf8(u8"GanttChart配置解析完成，共解析 %1 个颜色配置").arg(parsedCount));
    return true;
}

/**
 * @brief 解析Database配置节点
 * @param xml XML流读取器引用
 * @return 解析成功返回true
 * 
 * 解析的配置项：
 * - RecordThreshold: 使用数据库模式的记录数阈值
 */
bool XmlConfigParser::parseDatabaseSection(QXmlStreamReader& xml)
{
    LOG_DEBUG("XmlConfigParser", u8"开始解析Database配置节点");
    
    int parsedCount = 0;
    
    while (!(xml.tokenType() == QXmlStreamReader::EndElement && 
             xml.name().toString() == "Database")) {
        xml.readNext();
        
        if (xml.tokenType() == QXmlStreamReader::StartElement) {
            QString name = xml.name().toString();
            QString text = xml.readElementText();
            
            LOG_DEBUG("XmlConfigParser", QString::fromUtf8(u8"解析数据库配置: %1 = %2").arg(name, text));
            
            if (name == "RecordThreshold") {
                m_databaseConfig.recordThreshold = text.toInt();
                parsedCount++;
            }
        }
    }
    
    LOG_INFO("XmlConfigParser", QString::fromUtf8(u8"Database配置解析完成，记录阈值: %1").arg(m_databaseConfig.recordThreshold));
    return true;
}

/**
 * @brief 解析Models配置节点
 * @param xml XML流读取器引用
 * @return 解析成功返回true
 * 
 * 解析的配置项：
 * - DefaultModel: 默认使用的模型ID
 * - ApiTimeout: API超时时间（秒）
 * - RetryCount: 重试次数
 * - Provider: 模型提供商列表
 */
bool XmlConfigParser::parseModelsSection(QXmlStreamReader& xml)
{
    LOG_DEBUG("XmlConfigParser", u8"开始解析Models配置节点");
    
    int providerCount = 0;
    
    while (!(xml.tokenType() == QXmlStreamReader::EndElement && 
             xml.name().toString() == "Models")) {
        xml.readNext();
        
        if (xml.tokenType() == QXmlStreamReader::StartElement) {
            QString name = xml.name().toString();
            
            if (name == "DefaultModel") {
                QString text = xml.readElementText();
                m_defaultModel = text;
                LOG_DEBUG("XmlConfigParser", QString::fromUtf8(u8"默认模型: %1").arg(m_defaultModel));
            } else if (name == "ApiTimeout") {
                QString text = xml.readElementText();
                m_apiTimeout = text.toInt();
                LOG_DEBUG("XmlConfigParser", QString::fromUtf8(u8"API超时: %1秒").arg(m_apiTimeout));
            } else if (name == "RetryCount") {
                QString text = xml.readElementText();
                m_retryCount = text.toInt();
                LOG_DEBUG("XmlConfigParser", QString::fromUtf8(u8"重试次数: %1").arg(m_retryCount));
            } else if (name == "Provider") {
                ModelProvider provider;
                if (parseProvider(xml, provider)) {
                    m_modelProviders.append(provider);
                    providerCount++;
                    LOG_INFO("XmlConfigParser", QString::fromUtf8(u8"加载提供商: %1 (%2), 实例数: %3")
                              .arg(provider.name)
                              .arg(provider.id)
                              .arg(provider.instances.size()));
                }
            }
        }
    }
    
    LOG_INFO("XmlConfigParser", QString::fromUtf8(u8"Models配置解析完成，共加载 %1 个提供商").arg(providerCount));
    return true;
}

bool XmlConfigParser::parseExportSection(QXmlStreamReader& xml)
{
    LOG_DEBUG("XmlConfigParser", u8"开始解析Export配置节点");
    
    while (!(xml.tokenType() == QXmlStreamReader::EndElement && 
             xml.name().toString() == "Export")) {
        xml.readNext();
        
        if (xml.tokenType() == QXmlStreamReader::StartElement) {
            QString name = xml.name().toString();
            QString text = xml.readElementText();
            
            if (name == "ReportFormat") {
                m_reportFormat = text.toLower();
                if (m_reportFormat != "html" && m_reportFormat != "pdf" && m_reportFormat != "docx") {
                    m_reportFormat = "html";
                }
                LOG_DEBUG("XmlConfigParser", QString::fromUtf8(u8"报告格式: %1").arg(m_reportFormat));
            }
        }
    }
    
    LOG_INFO("XmlConfigParser", QString::fromUtf8(u8"Export配置解析完成，报告格式: %1").arg(m_reportFormat));
    return true;
}

/**
 * @brief 解析Speech配置节点
 * @param xml XML流读取器引用
 * @return 解析成功返回true
 *
 * 解析的配置项：
 * - Engine/ModelPath: 语音模型路径
 * - Engine/SampleRate: 采样率
 * - Engine/Enabled: 是否启用语音识别
 * - Capture/BufferDurationMs: 缓冲区大小
 * - Capture/ChannelCount: 通道数
 * - Capture/SampleSize: 采样位深
 * - Runtime/AutoStop: 是否自动停止
 * - Runtime/SilenceTimeoutMs: 静音超时时间
 * - Runtime/SilenceThreshold: 静音阈值
 */
bool XmlConfigParser::parseSpeechSection(QXmlStreamReader& xml)
{
    LOG_DEBUG("XmlConfigParser", u8"开始解析Speech配置节点");

    int parsedCount = 0;

    while (!(xml.tokenType() == QXmlStreamReader::EndElement &&
             xml.name().toString() == "Speech")) {
        xml.readNext();

        if (xml.tokenType() == QXmlStreamReader::StartElement) {
            QString name = xml.name().toString();

            if (name == "Engine") {
                while (!(xml.tokenType() == QXmlStreamReader::EndElement &&
                         xml.name().toString() == "Engine")) {
                    xml.readNext();

                    if (xml.tokenType() == QXmlStreamReader::StartElement) {
                        QString engineName = xml.name().toString();
                        QString text = xml.readElementText();

                        if (engineName == "ModelPath") {
                            m_speechConfig.engine.modelPath = text;
                            parsedCount++;
                        } else if (engineName == "SampleRate") {
                            m_speechConfig.engine.sampleRate = text.toInt();
                            parsedCount++;
                        } else if (engineName == "Enabled") {
                            m_speechConfig.engine.enabled = (text == "true");
                            parsedCount++;
                        }
                    }
                }
            } else if (name == "Capture") {
                while (!(xml.tokenType() == QXmlStreamReader::EndElement &&
                         xml.name().toString() == "Capture")) {
                    xml.readNext();

                    if (xml.tokenType() == QXmlStreamReader::StartElement) {
                        QString captureName = xml.name().toString();
                        QString text = xml.readElementText();

                        if (captureName == "BufferDurationMs") {
                            m_speechConfig.capture.bufferDurationMs = text.toInt();
                            parsedCount++;
                        } else if (captureName == "ChannelCount") {
                            m_speechConfig.capture.channelCount = text.toInt();
                            parsedCount++;
                        } else if (captureName == "SampleSize") {
                            m_speechConfig.capture.sampleSize = text.toInt();
                            parsedCount++;
                        }
                    }
                }
            } else if (name == "Runtime") {
                while (!(xml.tokenType() == QXmlStreamReader::EndElement &&
                         xml.name().toString() == "Runtime")) {
                    xml.readNext();

                    if (xml.tokenType() == QXmlStreamReader::StartElement) {
                        QString runtimeName = xml.name().toString();
                        QString text = xml.readElementText();

                        if (runtimeName == "AutoStop") {
                            m_speechConfig.runtime.autoStop = (text == "true");
                            parsedCount++;
                        } else if (runtimeName == "SilenceTimeoutMs") {
                            m_speechConfig.runtime.silenceTimeoutMs = text.toInt();
                            parsedCount++;
                        } else if (runtimeName == "SilenceThreshold") {
                            m_speechConfig.runtime.silenceThreshold = text.toDouble();
                            parsedCount++;
                        }
                    }
                }
            }
        }
    }

    LOG_INFO("XmlConfigParser", QString::fromUtf8(u8"Speech配置解析完成，共解析 %1 个配置项，模型: %2, 启用: %3")
              .arg(parsedCount)
              .arg(m_speechConfig.engine.modelPath)
              .arg(m_speechConfig.engine.enabled ? "true" : "false"));
    return true;
}

/**
 * @brief 解析Provider配置节点
 * @param xml XML流读取器引用
 * @param provider 输出的提供商配置结构
 * @return 解析成功返回true
 * 
 * 解析的属性：
 * - id: 提供商唯一标识
 * - name: 提供商显示名称
 * 
 * 子节点：
 * - Instance: 模型实例列表
 */
bool XmlConfigParser::parseProvider(QXmlStreamReader& xml, ModelProvider& provider)
{
    QXmlStreamAttributes attrs = xml.attributes();
    provider.id = attrs.value("id").toString();
    provider.name = attrs.value("name").toString();
    
    LOG_DEBUG("XmlConfigParser", QString::fromUtf8(u8"解析Provider: id=%1, name=%2").arg(provider.id, provider.name));
    
    int instanceCount = 0;
    
    while (!(xml.tokenType() == QXmlStreamReader::EndElement && 
             xml.name().toString() == "Provider")) {
        xml.readNext();
        
        if (xml.tokenType() == QXmlStreamReader::StartElement) {
            QString name = xml.name().toString();
            
            if (name == "Instance") {
                ModelInstance instance;
                if (parseInstance(xml, instance)) {
                    provider.instances.append(instance);
                    instanceCount++;
                    LOG_DEBUG("XmlConfigParser", QString::fromUtf8(u8"加载模型实例: %1 (enabled=%2)")
                              .arg(instance.name)
                              .arg(instance.enabled ? "true" : "false"));
                }
            }
        }
    }
    
    LOG_DEBUG("XmlConfigParser", QString::fromUtf8(u8"Provider解析完成，实例数: %1").arg(instanceCount));
    return true;
}

/**
 * @brief 解析Instance配置节点
 * @param xml XML流读取器引用
 * @param instance 输出的模型实例配置结构
 * @return 解析成功返回true
 * 
 * 解析的属性：
 * - name: 实例名称（模型名称）
 * - enabled: 是否启用
 * 
 * 子节点：
 * - ApiKey: API密钥
 * - ApiUrl: API地址
 * - SystemPrompt: 系统提示词
 */
bool XmlConfigParser::parseInstance(QXmlStreamReader& xml, ModelInstance& instance)
{
    QXmlStreamAttributes attrs = xml.attributes();
    instance.name = attrs.value("name").toString();
    instance.enabled = attrs.value("enabled").toString() == "true";
    
    LOG_DEBUG("XmlConfigParser", QString::fromUtf8(u8"解析Instance: name=%1, enabled=%2")
              .arg(instance.name)
              .arg(instance.enabled ? "true" : "false"));
    
    while (!(xml.tokenType() == QXmlStreamReader::EndElement && 
             xml.name().toString() == "Instance")) {
        xml.readNext();
        
        if (xml.tokenType() == QXmlStreamReader::StartElement) {
            QString name = xml.name().toString();
            QString text = xml.readElementText();
            
            if (name == "ApiKey") {
                instance.apiKey = text;
                LOG_DEBUG("XmlConfigParser", QString::fromUtf8(u8"ApiKey已设置，长度: %1").arg(text.length()));
            } else if (name == "ApiUrl") {
                instance.apiUrl = text;
                LOG_DEBUG("XmlConfigParser", QString("ApiUrl: %1").arg(text));
            } else if (name == "SystemPrompt") {
                instance.systemPrompt = text;
                LOG_DEBUG("XmlConfigParser", QString::fromUtf8(u8"SystemPrompt长度: %1").arg(text.length()));
            }
        }
    }
    
    return true;
}

/**
 * @brief 解析颜色字符串
 * @param colorStr 颜色字符串（支持#RRGGBB格式或颜色名称）
 * @return QColor对象
 */
QColor XmlConfigParser::parseColor(const QString& colorStr)
{
    QString color = colorStr.trimmed();
    if (color.startsWith("#")) {
        return QColor(color);
    }
    return QColor(color);
}

/**
 * @brief 将颜色转换为字符串
 * @param color QColor对象
 * @return 十六进制颜色字符串（#RRGGBB格式）
 */
QString XmlConfigParser::colorToString(const QColor& color)
{
    return color.name(QColor::HexRgb);
}

/**
 * @brief 保存配置到XML文件
 * @param filePath 目标文件路径
 * @param parserConfig 解析器配置
 * @param ganttConfig 甘特图配置
 * @param databaseConfig 数据库配置
 * @param providers 模型提供商列表
 * @param defaultModel 默认模型ID
 * @param apiTimeout API超时时间
 * @param retryCount 重试次数
 * @return 保存成功返回true，失败返回false
 */
bool XmlConfigParser::save(const QString& filePath,
                           const ParserConfig& parserConfig,
                           const GanttConfig& ganttConfig,
                           const DatabaseConfig& databaseConfig,
                           const QVector<ModelProvider>& providers,
                           const QString& defaultModel,
                           int apiTimeout,
                           int retryCount,
                           const QString& reportFormat,
                           const SpeechConfig& speechConfig)
{
    LOG_INFO("XmlConfigParser", QString::fromUtf8(u8"开始保存配置文件: %1").arg(filePath));
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        m_lastError = QCoreApplication::translate("XmlConfigParser", u8"无法写入配置文件: %1").arg(filePath);
        LOG_ERROR("XmlConfigParser", m_lastError);
        return false;
    }
    
    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    
    xml.writeStartElement("Config");
    xml.writeAttribute("version", "1.0");
    
    xml.writeStartElement("Parser");
    xml.writeTextElement("ByteOrder", parserConfig.byteOrder);
    xml.writeTextElement("Header1", QString("0x%1").arg(parserConfig.header1, 4, 16, QChar('0')));
    xml.writeTextElement("Header2", QString("0x%1").arg(parserConfig.header2, 4, 16, QChar('0')));
    xml.writeTextElement("DataHeader", QString("0x%1").arg(parserConfig.dataHeader, 4, 16, QChar('0')));
    xml.writeTextElement("TimestampUnit", QString::number(parserConfig.timestampUnit));
    xml.writeTextElement("MaxErrorTolerance", QString::number(parserConfig.maxErrorTolerance));
    xml.writeEndElement();
    
    xml.writeStartElement("GanttChart");
    xml.writeTextElement("ColorBC2RT", colorToString(ganttConfig.colorBC2RT));
    xml.writeTextElement("ColorRT2BC", colorToString(ganttConfig.colorRT2BC));
    xml.writeTextElement("ColorRT2RT", colorToString(ganttConfig.colorRT2RT));
    xml.writeTextElement("ColorBroadcast", colorToString(ganttConfig.colorBroadcast));
    xml.writeTextElement("ColorError", colorToString(ganttConfig.colorError));
    xml.writeEndElement();
    
    xml.writeStartElement("Database");
    xml.writeTextElement("RecordThreshold", QString::number(databaseConfig.recordThreshold));
    xml.writeEndElement();
    
    xml.writeStartElement("Models");
    xml.writeTextElement("DefaultModel", defaultModel);
    xml.writeTextElement("ApiTimeout", QString::number(apiTimeout));
    xml.writeTextElement("RetryCount", QString::number(retryCount));
    
    for (const auto& provider : providers) {
        xml.writeStartElement("Provider");
        xml.writeAttribute("id", provider.id);
        xml.writeAttribute("name", provider.name);
        
        for (const auto& instance : provider.instances) {
            xml.writeStartElement("Instance");
            xml.writeAttribute("name", instance.name);
            xml.writeAttribute("enabled", instance.enabled ? "true" : "false");
            xml.writeTextElement("ApiKey", instance.apiKey);
            xml.writeTextElement("ApiUrl", instance.apiUrl);
            xml.writeTextElement("SystemPrompt", instance.systemPrompt);
            xml.writeEndElement();
        }
        
        xml.writeEndElement();
    }
    
    xml.writeEndElement();
    
    xml.writeStartElement("Export");
    xml.writeTextElement("ReportFormat", reportFormat);
    xml.writeEndElement();

    xml.writeStartElement("Speech");

    xml.writeStartElement("Engine");
    xml.writeTextElement("ModelPath", speechConfig.engine.modelPath);
    xml.writeTextElement("SampleRate", QString::number(speechConfig.engine.sampleRate));
    xml.writeTextElement("Enabled", speechConfig.engine.enabled ? "true" : "false");
    xml.writeEndElement();

    xml.writeStartElement("Capture");
    xml.writeTextElement("BufferDurationMs", QString::number(speechConfig.capture.bufferDurationMs));
    xml.writeTextElement("ChannelCount", QString::number(speechConfig.capture.channelCount));
    xml.writeTextElement("SampleSize", QString::number(speechConfig.capture.sampleSize));
    xml.writeEndElement();

    xml.writeStartElement("Runtime");
    xml.writeTextElement("AutoStop", speechConfig.runtime.autoStop ? "true" : "false");
    xml.writeTextElement("SilenceTimeoutMs", QString::number(speechConfig.runtime.silenceTimeoutMs));
    xml.writeTextElement("SilenceThreshold", QString::number(speechConfig.runtime.silenceThreshold, 'f', 4));
    xml.writeEndElement();

    xml.writeEndElement();

    xml.writeEndElement();
    xml.writeEndDocument();
    
    file.close();
    
    LOG_INFO("XmlConfigParser", QString::fromUtf8(u8"配置文件保存成功，共保存 %1 个提供商").arg(providers.size()));
    
    return true;
}

ParserConfig XmlConfigParser::parserConfig() const
{
    return m_parserConfig;
}

GanttConfig XmlConfigParser::ganttConfig() const
{
    return m_ganttConfig;
}

DatabaseConfig XmlConfigParser::databaseConfig() const
{
    return m_databaseConfig;
}

QVector<ModelProvider> XmlConfigParser::modelProviders() const
{
    return m_modelProviders;
}

QString XmlConfigParser::defaultModel() const
{
    return m_defaultModel;
}

int XmlConfigParser::apiTimeout() const
{
    return m_apiTimeout;
}

int XmlConfigParser::retryCount() const
{
    return m_retryCount;
}

QString XmlConfigParser::reportFormat() const
{
    return m_reportFormat;
}

SpeechConfig XmlConfigParser::speechConfig() const
{
    return m_speechConfig;
}

QString XmlConfigParser::lastError() const
{
    return m_lastError;
}
