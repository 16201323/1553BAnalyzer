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
#include <QFile>
#include <QTextStream>
#include <QPrinter>
#include <QTextDocument>
#include <QTableWidget>
#include <QDebug>
#include "utils/Qt5Compat.h"
#include <cstring>

/**
 * @brief 将时间戳转换为时分秒毫秒格式
 * @param timestamp 时间戳（毫秒）
 * @return 格式化的时间字符串，格式为 HH:MM:SS.mmm
 */
QString formatTimestamp(quint32 timestamp)
{
    int totalSeconds = timestamp / 1000;
    int milliseconds = timestamp % 1000;
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;
    return QString("%1:%2:%3.%4").arg(hours, 2, 10, QChar('0'))
                                .arg(minutes, 2, 10, QChar('0'))
                                .arg(seconds, 2, 10, QChar('0'))
                                .arg(milliseconds, 3, 10, QChar('0'));
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
    
    // 使用UTF-8编码的标题行
    out << QString::fromUtf8(u8"包头1,包头2,MPU标识,包长度,年,月,日,头时间,")
        << QString::fromUtf8(u8"块头,CMD1,终端地址1,T_R1,子地址1,计数/码1,")
        << QString::fromUtf8(u8"CMD2,终端地址2,T_R2,子地址2,计数/码2,")
        << QString::fromUtf8(u8"状态1,状态2,结果,块时间,")
        << QString::fromUtf8(u8"数据,数据包,完整数据\n");
    
    QVector<DataRecord> records = store->getRecordsByScope(scope);
    int total = records.size();
    
    for (int i = 0; i < total; ++i) {
        const DataRecord& record = records[i];
        
        CMD cmd1;
        memcpy(&cmd1, &record.packetData.cmd1, sizeof(CMD));
        
        CMD cmd2;
        memcpy(&cmd2, &record.packetData.cmd2, sizeof(CMD));
        
        out << QString("0X%1,").arg(record.packetHeader.header1, 4, 16, QChar('0')).toUpper();
        out << QString("0X%1,").arg(record.packetHeader.header2, 2, 16, QChar('0')).toUpper();
        out << QString("%1,").arg(record.packetHeader.mpuProduceId);
        out << QString("%1,").arg(record.packetHeader.packetLen);
        out << QString("%1,").arg(record.packetHeader.year);
        out << QString("%1,").arg(record.packetHeader.month);
        out << QString("%1,").arg(record.packetHeader.day);
        out << QString("%1,").arg(formatTimestamp(record.packetHeader.timestamp));
        
        out << QString("0X%1,").arg(record.packetData.header, 4, 16, QChar('0')).toUpper();
        out << QString("0X%1,").arg(record.packetData.cmd1, 4, 16, QChar('0')).toUpper();
        out << QString("%1,").arg(cmd1.zhongduandizhi);
        out << QString("%1,").arg(cmd1.T_R ? u8"RT→BC" : u8"BC→RT");
        out << QString("%1,").arg(cmd1.zidizhi);
        out << QString("%1,").arg(cmd1.sjzjs_fsdm);
        
        out << QString("0X%1,").arg(record.packetData.cmd2, 4, 16, QChar('0')).toUpper();
        out << QString("%1,").arg(cmd2.zhongduandizhi);
        out << QString("%1,").arg(cmd2.T_R ? u8"RT→BC" : u8"BC→RT");
        out << QString("%1,").arg(cmd2.zidizhi);
        out << QString("%1,").arg(cmd2.sjzjs_fsdm);
        
        out << QString("0X%1,").arg(record.packetData.states1, 4, 16, QChar('0')).toUpper();
        out << QString("0X%1,").arg(record.packetData.states2, 4, 16, QChar('0')).toUpper();
        out << QString("%1,").arg(record.packetData.chstt ? u8"成功" : u8"失败");
        out << QString("%1,").arg(formatTimestamp(record.packetData.timestamp));
        
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
        out << QString("\"%1\",").arg(dataWith0X);
        
        QByteArray packetRaw;
        packetRaw.append(reinterpret_cast<const char*>(&record.packetData.header), sizeof(record.packetData.header));
        packetRaw.append(reinterpret_cast<const char*>(&record.packetData.cmd1), sizeof(record.packetData.cmd1));
        packetRaw.append(reinterpret_cast<const char*>(&record.packetData.cmd2), sizeof(record.packetData.cmd2));
        packetRaw.append(reinterpret_cast<const char*>(&record.packetData.states1), sizeof(record.packetData.states1));
        packetRaw.append(reinterpret_cast<const char*>(&record.packetData.states2), sizeof(record.packetData.states2));
        packetRaw.append(reinterpret_cast<const char*>(&record.packetData.chstt), sizeof(record.packetData.chstt));
        packetRaw.append(reinterpret_cast<const char*>(&record.packetData.timestamp), sizeof(record.packetData.timestamp));
        packetRaw.append(record.packetData.datas);
        out << QString("\"%1\",").arg(QString(packetRaw.toHex(' ')).toUpper());
        
        QByteArray fullRaw;
        fullRaw.append(reinterpret_cast<const char*>(&record.packetHeader), sizeof(record.packetHeader));
        fullRaw.append(packetRaw);
        out << QString("\"%1\"\n").arg(QString(fullRaw.toHex(' ')).toUpper());
        
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
