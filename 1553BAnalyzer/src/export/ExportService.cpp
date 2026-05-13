/**
 * @file ExportService.cpp
 * @brief 数据导出服务实现
 *
 * 本文件实现了数据导出功能，支持以下格式：
 * - CSV：逗号分隔值文件，兼容Excel打开
 * - PDF：通过QTextDocument + QPrinter生成
 *
 * 导出流程：
 * 1. 从DataStore获取筛选后的数据记录
 * 2. 将每条记录的字段格式化为字符串
 * 3. 写入目标文件
 *
 * CSV格式说明：
 * - UTF-8编码（带BOM，确保Excel正确识别中文）
 * - 包含完整的包头、命令字、状态字、数据字段
 * - 十六进制字段使用0X前缀和大写字母
 *
 * @author 1553BTools
 * @date 2024
 */

#include "ExportService.h"
#include "core/parser/PacketStruct.h"
#include "core/config/ConfigManager.h"
#include <QFile>
#include <QTextStream>
#include <QPrinter>
#include <QTextDocument>
#include <QTableWidget>
#include <QDataStream>
#include <QDebug>
#include "utils/Qt5Compat.h"
#include <cstring>

/**
 * @brief 将时间戳转换为时分秒毫秒[纳秒]格式
 * @param timestamp 时间戳（单位40微秒）
 * @return 格式化的时间字符串，格式为 HH:MM:SS.mmm[ns]
 *
 * 时间戳单位说明：
 * - 时间戳原始单位为40微秒（即每个计数值代表40000纳秒）
 * - 转换：总纳秒 = timestamp * 40000
 * - 例：timestamp=90000 → 总纳秒=3600000000 → "00:00:03.600[000000]"
 */
QString formatTimestamp(quint32 timestamp)
{
    // 时间戳单位40微秒，转换为纳秒：timestamp * 40000
    quint64 totalNs = static_cast<quint64>(timestamp) * 40000ULL;
    quint64 totalMs = totalNs / 1000000ULL;
    int hours = static_cast<int>(totalMs / 3600000);
    int minutes = static_cast<int>((totalMs % 3600000) / 60000);
    int seconds = static_cast<int>((totalMs % 60000) / 1000);
    int milliseconds = static_cast<int>(totalMs % 1000);
    // 剩余纳秒 = 总纳秒超出毫秒的部分
    quint64 remainingNs = totalNs - totalMs * 1000000ULL;
    return QString("%1:%2:%3.%4[%5]").arg(hours, 2, 10, QChar('0'))
                                     .arg(minutes, 2, 10, QChar('0'))
                                     .arg(seconds, 2, 10, QChar('0'))
                                     .arg(milliseconds, 3, 10, QChar('0'))
                                     .arg(remainingNs, 6, 10, QChar('0'));
}

/**
 * @brief 将QByteArray转换为每2字节一组的十六进制字符串（大端序显示）
 * @param data 原始字节数据
 * @return 十六进制字符串，每2字节后跟一个空格，如 "A5A5 A501 0032"
 *
 * 使用手动nibble查表法，确保值为0x00的字节也能正确显示为"00"
 */
static QString toHexBE2(const QByteArray& data)
{
    if (data.isEmpty()) return QString();
    static const char hexChars[] = "0123456789ABCDEF";
    QString result;
    result.reserve(data.size() * 3);
    for (int i = 0; i < data.size(); i += 2) {
        if (i > 0) result += ' ';
        quint8 hi = static_cast<quint8>(data[i]);
        result += QChar(hexChars[hi >> 4]);
        result += QChar(hexChars[hi & 0x0F]);
        if (i + 1 < data.size()) {
            quint8 lo = static_cast<quint8>(data[i + 1]);
            result += QChar(hexChars[lo >> 4]);
            result += QChar(hexChars[lo & 0x0F]);
        }
    }
    return result.toUpper();
}

ExportService::ExportService(QObject *parent)
    : QObject(parent)
{
}

ExportService::~ExportService()
{
}

/**
 * @brief 导出数据为CSV格式
 * @param filePath 目标文件路径
 * @param store 数据存储对象
 * @return 导出成功返回true
 *
 * CSV文件包含完整的1553B数据包字段，每行一条记录。
 * 使用BOM(Byte Order Mark)确保Excel正确识别UTF-8编码。
 */
bool ExportService::exportToCsv(const QString& filePath, DataStore* store, DataScope scope)
{
    if (!store) {
        m_lastError = tr(u8"数据存储为空");
        return false;
    }
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_lastError = tr(u8"无法打开文件: %1").arg(filePath);
        return false;
    }
    
    QTextStream out(&file);
    QT5COMPAT_SET_UTF8(out);
    out.setGenerateByteOrderMark(true);
    
    // CSV文本单元格包装器：将所有单元格格式化为="值"，
    // 强制Excel将单元格识别为文本模式，防止前导0被隐藏或数字被科学计数法表示
    auto csvText = [](const QString& val) -> QString {
        QString escaped = val;
        escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        return QStringLiteral("=\"%1\"").arg(escaped);
    };
    
    // CSV表头行：每个列标题包裹为="标题"，强制Excel以文本模式显示
    out << csvText(QString::fromUtf8(u8"包头1")) << QLatin1Char(',')
        << csvText(QString::fromUtf8(u8"包头2")) << QLatin1Char(',')
        << csvText(QString::fromUtf8(u8"MPU标识")) << QLatin1Char(',')
        << csvText(QString::fromUtf8(u8"包长度")) << QLatin1Char(',')
        << csvText(QString::fromUtf8(u8"年")) << QLatin1Char(',')
        << csvText(QString::fromUtf8(u8"月")) << QLatin1Char(',')
        << csvText(QString::fromUtf8(u8"日")) << QLatin1Char(',')
        << csvText(QString::fromUtf8(u8"头时间")) << QLatin1Char(',')
        << csvText(QString::fromUtf8(u8"块头")) << QLatin1Char(',')
        << csvText(QString::fromUtf8(u8"CMD1")) << QLatin1Char(',')
        << csvText(QString::fromUtf8(u8"终端地址1")) << QLatin1Char(',')
        << csvText(QString::fromUtf8(u8"T_R1")) << QLatin1Char(',')
        << csvText(QString::fromUtf8(u8"子地址1")) << QLatin1Char(',')
        << csvText(QString::fromUtf8(u8"计数/码1")) << QLatin1Char(',')
        << csvText(QString::fromUtf8(u8"CMD2")) << QLatin1Char(',')
        << csvText(QString::fromUtf8(u8"终端地址2")) << QLatin1Char(',')
        << csvText(QString::fromUtf8(u8"T_R2")) << QLatin1Char(',')
        << csvText(QString::fromUtf8(u8"子地址2")) << QLatin1Char(',')
        << csvText(QString::fromUtf8(u8"计数/码2")) << QLatin1Char(',')
        << csvText(QString::fromUtf8(u8"状态1")) << QLatin1Char(',')
        << csvText(QString::fromUtf8(u8"状态2")) << QLatin1Char(',')
        << csvText(QString::fromUtf8(u8"结果")) << QLatin1Char(',')
        << csvText(QString::fromUtf8(u8"块时间")) << QLatin1Char(',')
        << csvText(QString::fromUtf8(u8"数据")) << QLatin1Char(',')
        << csvText(QString::fromUtf8(u8"数据块")) << QLatin1Char(',')
        << csvText(QString::fromUtf8(u8"数据包")) << QLatin1Char('\n');
    
    // 获取字节序设置，用于hex显示
    bool isLittleEndian = (ConfigManager::instance()->getParserConfig().byteOrder == "little");
    
    QVector<DataRecord> records = store->getRecordsByScope(scope);
    int total = records.size();
    
    for (int i = 0; i < total; ++i) {
        const DataRecord& record = records[i];
        
        CMD cmd1;
        memcpy(&cmd1, &record.packetData.cmd1, sizeof(CMD));
        
        CMD cmd2;
        memcpy(&cmd2, &record.packetData.cmd2, sizeof(CMD));
        
        // 所有单元格输出均使用csvText()包裹为="值"格式，强制Excel识别为文本
        out << csvText(QString("0X%1").arg(record.packetHeader.header1, 4, 16, QChar('0')).toUpper()) << QLatin1Char(',');
        out << csvText(QString("0X%1").arg(record.packetHeader.header2, 2, 16, QChar('0')).toUpper()) << QLatin1Char(',');
        out << csvText(QString::number(record.packetHeader.mpuProduceId)) << QLatin1Char(',');
        out << csvText(QString::number(record.packetHeader.packetLen)) << QLatin1Char(',');
        out << csvText(QString::number(record.packetHeader.year)) << QLatin1Char(',');
        out << csvText(QString::number(record.packetHeader.month)) << QLatin1Char(',');
        out << csvText(QString::number(record.packetHeader.day)) << QLatin1Char(',');
        out << csvText(formatTimestamp(record.packetHeader.timestamp)) << QLatin1Char(',');
        
        out << csvText(QString("0X%1").arg(record.packetData.header, 4, 16, QChar('0')).toUpper()) << QLatin1Char(',');
        out << csvText(QString("0X%1").arg(record.packetData.cmd1, 4, 16, QChar('0')).toUpper()) << QLatin1Char(',');
        out << csvText(QString::number(cmd1.zhongduandizhi)) << QLatin1Char(',');
        out << csvText(cmd1.T_R ? QString::fromUtf8(u8"RT→BC") : QString::fromUtf8(u8"BC→RT")) << QLatin1Char(',');
        out << csvText(QString::number(cmd1.zidizhi)) << QLatin1Char(',');
        out << csvText(QString::number(cmd1.sjzjs_fsdm)) << QLatin1Char(',');
        
        out << csvText(QString("0X%1").arg(record.packetData.cmd2, 4, 16, QChar('0')).toUpper()) << QLatin1Char(',');
        out << csvText(QString::number(cmd2.zhongduandizhi)) << QLatin1Char(',');
        out << csvText(cmd2.T_R ? QString::fromUtf8(u8"RT→BC") : QString::fromUtf8(u8"BC→RT")) << QLatin1Char(',');
        out << csvText(QString::number(cmd2.zidizhi)) << QLatin1Char(',');
        out << csvText(QString::number(cmd2.sjzjs_fsdm)) << QLatin1Char(',');
        
        out << csvText(QString("0X%1").arg(record.packetData.states1, 4, 16, QChar('0')).toUpper()) << QLatin1Char(',');
        out << csvText(QString("0X%1").arg(record.packetData.states2, 4, 16, QChar('0')).toUpper()) << QLatin1Char(',');
        out << csvText(record.packetData.chstt ? QString::fromUtf8(u8"成功") : QString::fromUtf8(u8"失败")) << QLatin1Char(',');
        out << csvText(formatTimestamp(record.packetData.timestamp)) << QLatin1Char(',');
        
        QString dataHex = toHexBE2(record.packetData.datas);
        out << csvText(dataHex) << QLatin1Char(',');
        
        // 构建数据块hex：按设置的字节序排列每个多字节字段
        QByteArray blockRaw;
        // helper：按字节序添加quint16
        auto appendU16 = [&](QByteArray& arr, quint16 v) {
            if (isLittleEndian) {
                arr.append(static_cast<char>(v & 0xFF));
                arr.append(static_cast<char>((v >> 8) & 0xFF));
            } else {
                arr.append(static_cast<char>((v >> 8) & 0xFF));
                arr.append(static_cast<char>(v & 0xFF));
            }
        };
        // helper：按字节序添加quint32
        auto appendU32 = [&](QByteArray& arr, quint32 v) {
            if (isLittleEndian) {
                arr.append(static_cast<char>(v & 0xFF));
                arr.append(static_cast<char>((v >> 8) & 0xFF));
                arr.append(static_cast<char>((v >> 16) & 0xFF));
                arr.append(static_cast<char>((v >> 24) & 0xFF));
            } else {
                arr.append(static_cast<char>((v >> 24) & 0xFF));
                arr.append(static_cast<char>((v >> 16) & 0xFF));
                arr.append(static_cast<char>((v >> 8) & 0xFF));
                arr.append(static_cast<char>(v & 0xFF));
            }
        };
        appendU16(blockRaw, record.packetData.header);
        appendU16(blockRaw, record.packetData.cmd1);
        appendU16(blockRaw, record.packetData.cmd2);
        appendU16(blockRaw, record.packetData.states1);
        appendU16(blockRaw, record.packetData.states2);
        appendU16(blockRaw, record.packetData.chstt);
        appendU32(blockRaw, record.packetData.timestamp);
        blockRaw.append(record.packetData.datas);
        out << csvText(toHexBE2(blockRaw)) << QLatin1Char(',');
        
        // =====================================================================
        // 构建数据包hex：完全复刻数据详细弹框"源码"标签页的构造逻辑
        //   1) 通过store->getMessage()获取完整消息
        //   2) 用QDataStream BigEndian构造包头字节数组
        //   3) 遍历消息中所有数据块，用QDataStream BigEndian构造数据块字节数组
        //   4) 拼接包头+所有数据块 = 整包源码
        //   5) toHexBE2()格式化（每2字节空格，与formatHexWithSpace效果相同）
        const PacketParser::SMbiMonPacketMsg& msg = store->getMessage(record.msgIndex);
        const PacketParser::SMbiMonPacketHeader& hdr = msg.header;
        
        // 构建包头源码（14字节，大端序），与MainWindow.cpp源码标签页完全一致
        QByteArray headerSource;
        QDataStream headerStream(&headerSource, QIODevice::WriteOnly);
        headerStream.setByteOrder(QDataStream::BigEndian);
        headerStream << hdr.header1;
        headerStream << hdr.header2;
        headerStream << hdr.mpuProduceId;
        headerStream << hdr.packetLen;
        headerStream << hdr.year;
        headerStream << hdr.month;
        headerStream << hdr.day;
        headerStream << hdr.timestamp;
        
        // 构建所有数据块源码（大端序），遍历消息中全部数据块
        QByteArray allDataSource;
        for (const auto& pkt : msg.packetDatas) {
            QDataStream dataStream(&allDataSource, QIODevice::WriteOnly | QIODevice::Append);
            dataStream.setByteOrder(QDataStream::BigEndian);
            dataStream << pkt.header;
            dataStream << pkt.cmd1;
            dataStream << pkt.cmd2;
            dataStream << pkt.states1;
            dataStream << pkt.states2;
            dataStream << pkt.chstt;
            dataStream << pkt.timestamp;
            dataStream.writeRawData(pkt.datas.constData(), pkt.datas.size());
        }
        
        // 拼接包头+所有数据块，与源码标签页的 fullSource = headerSource + allDataSource 一致
        QByteArray fullSource = headerSource + allDataSource;
        out << csvText(toHexBE2(fullSource)) << QLatin1Char('\n');
        
        if (i % 100 == 0) {
            emit exportProgress(i, total);
        }
    }
    
    file.close();
    emit exportFinished(true);
    return true;
}

bool ExportService::exportToExcel(const QString& filePath, DataStore* store, DataScope scope)
{
    m_lastError = tr(u8"Excel导出功能开发中，请使用CSV格式导出");
    return false;
}

bool ExportService::exportToPdf(const QString& filePath, DataStore* store, DataScope scope)
{
    if (!store) {
        m_lastError = tr(u8"数据存储为空");
        return false;
    }
    
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(filePath);
    QT5COMPAT_PRINTER_A4(printer);
    QT5COMPAT_PRINTER_LANDSCAPE(printer);
    
    QTextDocument doc;
    QString html = QString::fromUtf8("<html><head><meta charset=\"UTF-8\"><style>")
                   + QString::fromUtf8("table { border-collapse: collapse; width: 100%; font-size: 10px; }")
                   + QString::fromUtf8("th, td { border: 1px solid black; padding: 2px; text-align: center; }")
                   + QString::fromUtf8("th { background-color: #f0f0f0; }")
                   + QString::fromUtf8("</style></head><body>");
    html += QString::fromUtf8(u8"<h2>1553B数据导出报告</h2>");
    html += QString::fromUtf8("<table><tr>")
            + QString::fromUtf8(u8"<th>序号</th><th>时间戳</th><th>类型</th><th>终端</th><th>子地址</th>")
            + QString::fromUtf8(u8"<th>T/R</th><th>计数/码</th><th>状态1</th><th>状态2</th>")
            + QString::fromUtf8(u8"<th>结果</th><th>数据</th>")
            + QString::fromUtf8("</tr>");
    
    QVector<DataRecord> records = store->getRecordsByScope(scope);
    int total = records.size();
    
    for (int i = 0; i < total; ++i) {
        const DataRecord& record = records[i];
        
        CMD cmd1;
        memcpy(&cmd1, &record.packetData.cmd1, sizeof(CMD));
        
        QString dataHex = record.packetData.datas.toHex(' ').toUpper();
        QStringList dataBytes = dataHex.split(' ');
        QString dataWith0X;
        for (int j = 0; j < dataBytes.size(); ++j) {
            const QString& byte = dataBytes[j];
            if (!dataWith0X.isEmpty()) dataWith0X += " ";
            if (j == 0 && !byte.isEmpty()) {
                dataWith0X += "0X" + byte;
            } else {
                dataWith0X += byte;
            }
        }
        if (dataWith0X.length() > 60) {
            dataWith0X = dataWith0X.left(60) + "...";
        }
        
        html += QString("<tr>"
                         "<td>%1</td><td>%2</td><td>%3</td><td>%4</td><td>%5</td>"
                         "<td>%6</td><td>%7</td><td>%8</td><td>%9</td>"
                         "<td>%10</td><td>%11</td>"
                         "</tr>")
            .arg(i + 1)
            .arg(record.timestampMs, 0, 'f', 3)
            .arg(messageTypeToString(record.messageType))
            .arg(cmd1.zhongduandizhi)
            .arg(cmd1.zidizhi)
            .arg(cmd1.T_R ? u8"RT→BC" : u8"BC→RT")
            .arg(cmd1.sjzjs_fsdm)
            .arg(QString("0X%1").arg(record.packetData.states1, 4, 16, QChar('0')).toUpper())
            .arg(QString("0X%1").arg(record.packetData.states2, 4, 16, QChar('0')).toUpper())
            .arg(record.packetData.chstt ? u8"成功" : u8"失败")
            .arg(dataWith0X);
        
        if (i % 100 == 0) {
            emit exportProgress(i, total);
        }
    }
    
    html += "</table></body></html>";
    
    doc.setHtml(html);
    doc.print(&printer);
    
    emit exportFinished(true);
    return true;
}

QString ExportService::lastError() const
{
    return m_lastError;
}