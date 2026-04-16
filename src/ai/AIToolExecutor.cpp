/**
 * @file AIToolExecutor.cpp
 * @brief AI工具执行器实现
 * 
 * 本文件实现了AI工具的执行逻辑，将AI模型返回的工具调用
 * 转换为实际的数据操作和界面更新。
 * 
 * 支持的工具：
 * - query_data：数据查询，支持多条件筛选
 * - generate_chart：生成统计图表
 * - generate_gantt：生成甘特图
 * - get_statistics：获取统计信息
 * - clear_filter：清除筛选条件
 * 
 * 工作流程：
 * 1. AI分析用户请求，返回工具调用JSON
 * 2. MainWindow调用executeTool执行工具
 * 3. 工具通过信号通知MainWindow更新界面
 * 4. 执行结果返回给AI或直接显示给用户
 * 
 * @author 1553BTools
 * @date 2024
 */

#include "AIToolExecutor.h"
#include "utils/Logger.h"
#include "core/datastore/DatabaseManager.h"
#include <QSet>
#include <climits>

/**
 * @brief 构造函数
 * @param parent 父对象指针
 */
AIToolExecutor::AIToolExecutor(QObject *parent)
    : QObject(parent)
    , m_dataStore(nullptr)
{
}

/**
 * @brief 析构函数
 */
AIToolExecutor::~AIToolExecutor()
{
}

/**
 * @brief 设置数据存储对象
 * @param store DataStore对象指针
 * 
 * 设置数据源，工具执行时从此获取数据
 */
void AIToolExecutor::setDataStore(DataStore* store)
{
    m_dataStore = store;
}

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
ToolResult AIToolExecutor::executeTool(const QString& toolName, const QJsonObject& arguments)
{
    LOG_INFO("AIToolExecutor", QString("执行工具: %1").arg(toolName));
    
    ToolResult result;
    
    if (toolName == "query_data") {
        result = executeQueryData(arguments);
    } else if (toolName == "add_filter") {
        result = executeAddFilter(arguments);
    } else if (toolName == "apply_filters_batch") {
        result = executeApplyFiltersBatch(arguments);
    } else if (toolName == "preview_filter") {
        result = executePreviewFilter(arguments);
    } else if (toolName == "generate_chart") {
        result = executeGenerateChart(arguments);
    } else if (toolName == "generate_gantt") {
        result = executeGenerateGantt(arguments);
    } else if (toolName == "get_statistics") {
        result = executeGetStatistics(arguments);
    } else if (toolName == "clear_filter") {
        result = executeClearFilter(arguments);
    } else if (toolName == "get_anomalies") {
        result = executeGetAnomalies(arguments);
    } else if (toolName == "get_top_n") {
        result = executeGetTopN(arguments);
    } else if (toolName == "generate_report") {
        result = executeGenerateReport(arguments);
    } else {
        result.success = false;
        result.message = tr("未知的工具: %1").arg(toolName);
    }
    
    return result;
}

ToolResult AIToolExecutor::executeGetAnomalies(const QJsonObject& args)
{
    ToolResult result;
    result.actionType = "query";
    
    if (!m_dataStore) {
        result.success = false;
        result.message = tr("数据存储未初始化");
        return result;
    }
    
    QString anomalyType = args.contains("anomaly_type") ? args["anomaly_type"].toString() : "all";
    
    QJsonObject filterArgs;
    
    if (args.contains("terminal_address")) {
        filterArgs["terminal_address"] = args["terminal_address"].toArray();
    }
    
    if (anomalyType == "communication_failure") {
        filterArgs["chstt"] = false;
    } else if (anomalyType == "busy_flag") {
        filterArgs["status_bit"] = QJsonObject{{"field", 1}, {"bit", 1}, {"value", true}};
    } else if (anomalyType == "subsystem_fault") {
        filterArgs["status_bit"] = QJsonObject{{"field", 1}, {"bit", 2}, {"value", true}};
    } else if (anomalyType == "service_request") {
        filterArgs["status_bit"] = QJsonObject{{"field", 1}, {"bit", 4}, {"value", true}};
    } else if (anomalyType == "timeout") {
        filterArgs["error_flag"] = true;
    } else {
        filterArgs["error_flag"] = true;
    }
    
    applyFilterArgs(filterArgs, true);
    
    int filteredCount = m_dataStore->filteredCount();
    int totalCount = m_dataStore->totalCount();
    
    result.success = true;
    
    QString anomalyDesc;
    if (anomalyType == "communication_failure") anomalyDesc = tr("通信失败");
    else if (anomalyType == "busy_flag") anomalyDesc = tr("忙标志位(Busy)");
    else if (anomalyType == "subsystem_fault") anomalyDesc = tr("子系统故障");
    else if (anomalyType == "service_request") anomalyDesc = tr("服务请求");
    else if (anomalyType == "timeout") anomalyDesc = tr("超时错误");
    else anomalyDesc = tr("全部异常");
    
    if (filteredCount > 0) {
        result.message = tr("检测到 %1 条%2数据（占总数 %3%）")
            .arg(filteredCount)
            .arg(anomalyDesc)
            .arg(totalCount > 0 ? QString::number(filteredCount * 100.0 / totalCount, 'f', 1) : "0");
    } else {
        result.message = tr("未检测到%1数据").arg(anomalyDesc);
    }
    
    emit queryDataRequested(filterArgs);
    
    return result;
}

ToolResult AIToolExecutor::executeGetTopN(const QJsonObject& args)
{
    ToolResult result;
    result.actionType = "statistics";
    
    if (!m_dataStore) {
        result.success = false;
        result.message = tr("数据存储未初始化");
        return result;
    }
    
    QString subject = args.contains("subject") ? args["subject"].toString() : "active_terminal";
    int topN = args.contains("top_n") ? args["top_n"].toInt() : 5;
    
    result.success = true;
    
    if (subject == "active_terminal") {
        QMap<int, int> terminalCounts = m_dataStore->getTerminalStatistics();
        QList<QPair<int, int>> sortedList;
        for (auto it = terminalCounts.begin(); it != terminalCounts.end(); ++it) {
            sortedList.append(qMakePair(it.key(), it.value()));
        }
        std::sort(sortedList.begin(), sortedList.end(), [](const auto& a, const auto& b) {
            return a.second > b.second;
        });
        
        QString msg = tr("最活跃的终端（前%1名）：\n").arg(topN);
        for (int i = 0; i < qMin(topN, sortedList.size()); ++i) {
            msg += tr("  %1. 终端 %2：%3 条消息\n").arg(i + 1).arg(sortedList[i].first).arg(sortedList[i].second);
        }
        result.message = msg;
    } else if (subject == "active_sub_address") {
        QMap<int, int> subAddrCounts = m_dataStore->getSubAddressStatistics();
        QList<QPair<int, int>> sortedList;
        for (auto it = subAddrCounts.begin(); it != subAddrCounts.end(); ++it) {
            sortedList.append(qMakePair(it.key(), it.value()));
        }
        std::sort(sortedList.begin(), sortedList.end(), [](const auto& a, const auto& b) {
            return a.second > b.second;
        });
        
        QString msg = tr("最频繁的子地址（前%1名）：\n").arg(topN);
        for (int i = 0; i < qMin(topN, sortedList.size()); ++i) {
            msg += tr("  %1. 子地址 %2：%3 条消息\n").arg(i + 1).arg(sortedList[i].first).arg(sortedList[i].second);
        }
        result.message = msg;
    } else if (subject == "error_terminal") {
        DataStore* store = m_dataStore;
        store->clearFilter();
        store->clearTerminalFilter();
        store->setChsttFilter(0);
        
        QMap<int, int> terminalCounts = store->getTerminalStatistics();
        QList<QPair<int, int>> sortedList;
        for (auto it = terminalCounts.begin(); it != terminalCounts.end(); ++it) {
            sortedList.append(qMakePair(it.key(), it.value()));
        }
        std::sort(sortedList.begin(), sortedList.end(), [](const auto& a, const auto& b) {
            return a.second > b.second;
        });
        
        store->clearChsttFilter();
        
        QString msg = tr("错误最多的终端（前%1名）：\n").arg(topN);
        for (int i = 0; i < qMin(topN, sortedList.size()); ++i) {
            msg += tr("  %1. 终端 %2：%3 条错误\n").arg(i + 1).arg(sortedList[i].first).arg(sortedList[i].second);
        }
        result.message = msg;
    } else if (subject == "frequent_message_type") {
        QMap<MessageType, int> typeCounts = m_dataStore->getMessageTypeStatistics();
        QList<QPair<MessageType, int>> sortedList;
        for (auto it = typeCounts.begin(); it != typeCounts.end(); ++it) {
            sortedList.append(qMakePair(it.key(), it.value()));
        }
        std::sort(sortedList.begin(), sortedList.end(), [](const auto& a, const auto& b) {
            return a.second > b.second;
        });
        
        auto typeToString = [](MessageType type) -> QString {
            switch (type) {
            case MessageType::BC_TO_RT: return "BC→RT";
            case MessageType::RT_TO_BC: return "RT→BC";
            case MessageType::RT_TO_RT: return "RT→RT";
            case MessageType::Broadcast: return "广播";
            default: return "未知";
            }
        };
        
        QString msg = tr("消息类型频率排名：\n");
        for (int i = 0; i < qMin(topN, sortedList.size()); ++i) {
            msg += tr("  %1. %2：%3 条\n").arg(i + 1).arg(typeToString(sortedList[i].first)).arg(sortedList[i].second);
        }
        result.message = msg;
    } else {
        result.success = false;
        result.message = tr("未知的排名主题：%1").arg(subject);
    }
    
    return result;
}

ToolResult AIToolExecutor::executeGenerateReport(const QJsonObject& args)
{
    ToolResult result;
    result.actionType = "report";
    
    QString format = args.contains("format") ? args["format"].toString() : "html";
    
    emit generateReportRequested(format);
    
    result.success = true;
    result.message = tr("正在生成%1格式智能分析报告...").arg(format.toUpper());
    
    return result;
}

void AIToolExecutor::applyFilterArgs(const QJsonObject& args, bool clearExisting)
{
    if (!m_dataStore) return;
    
    m_dataStore->beginBatchFilterUpdate();
    
    if (clearExisting) {
        m_dataStore->clearFilter();
        m_dataStore->clearTerminalFilter();
        m_dataStore->clearSubAddressFilter();
        m_dataStore->clearMessageTypeFilter();
        m_dataStore->clearChsttFilter();
        m_dataStore->clearMpuIdFilter();
        m_dataStore->clearTimeRange();
        m_dataStore->clearPacketLenFilter();
        m_dataStore->clearDateRangeFilter();
        m_dataStore->clearStatusBitFilter();
        m_dataStore->clearExcludeTerminalFilter();
        m_dataStore->clearExcludeMessageTypeFilter();
        m_dataStore->clearWordCountFilter();
        m_dataStore->clearErrorFlagFilter();
    }
    
    if (args.contains("terminal_address")) {
        QJsonArray arr = args["terminal_address"].toArray();
        if (clearExisting) {
            QSet<int> terminals;
            for (const auto& v : arr) terminals.insert(v.toInt());
            if (!terminals.isEmpty()) m_dataStore->setTerminalFilter(terminals);
        } else {
            QSet<int> existing = m_dataStore->terminalFilter();
            for (const auto& v : arr) existing.insert(v.toInt());
            if (!existing.isEmpty()) m_dataStore->setTerminalFilter(existing);
        }
    }
    
    if (args.contains("sub_address")) {
        QJsonArray arr = args["sub_address"].toArray();
        if (clearExisting) {
            QSet<int> subAddresses;
            for (const auto& v : arr) subAddresses.insert(v.toInt());
            if (!subAddresses.isEmpty()) m_dataStore->setSubAddressFilter(subAddresses);
        } else {
            QSet<int> existing = m_dataStore->subAddressFilter();
            for (const auto& v : arr) existing.insert(v.toInt());
            if (!existing.isEmpty()) m_dataStore->setSubAddressFilter(existing);
        }
    }
    
    if (args.contains("message_type")) {
        QJsonArray arr = args["message_type"].toArray();
        QSet<MessageType> types;
        for (const auto& v : arr) {
            QString typeStr = v.toString();
            if (typeStr == "BC_TO_RT") types.insert(MessageType::BC_TO_RT);
            else if (typeStr == "RT_TO_BC") types.insert(MessageType::RT_TO_BC);
            else if (typeStr == "RT_TO_RT") types.insert(MessageType::RT_TO_RT);
            else if (typeStr == "Broadcast") types.insert(MessageType::Broadcast);
        }
        if (clearExisting) {
            if (!types.isEmpty()) m_dataStore->setMessageTypeFilter(types);
        } else {
            QSet<MessageType> existing = m_dataStore->messageTypeFilter();
            for (const auto& t : types) existing.insert(t);
            if (!existing.isEmpty()) m_dataStore->setMessageTypeFilter(existing);
        }
    }
    
    if (args.contains("chstt")) {
        bool chstt = args["chstt"].toBool();
        m_dataStore->setChsttFilter(chstt ? 1 : 0);
    }
    
    if (args.contains("mpu_id")) {
        int mpuId = args["mpu_id"].toInt();
        m_dataStore->setMpuIdFilter(mpuId);
    }
    
    if (args.contains("time_range")) {
        QJsonObject timeRange = args["time_range"].toObject();
        if (timeRange.contains("start") || timeRange.contains("end")) {
            quint32 startMs = 0, endMs = 0xFFFFFFFF;
            if (timeRange.contains("start")) {
                QString startStr = timeRange["start"].toString();
                QStringList parts = startStr.split(':');
                if (parts.size() >= 3) {
                    int h = parts[0].toInt();
                    int m = parts[1].toInt();
                    QStringList secParts = parts[2].split('.');
                    int s = secParts[0].toInt();
                    int ms = secParts.size() > 1 ? secParts[1].left(3).toInt() : 0;
                    startMs = static_cast<quint32>((static_cast<quint64>(h) * 3600 + m * 60 + s) * 1000 + ms);
                }
            }
            if (timeRange.contains("end")) {
                QString endStr = timeRange["end"].toString();
                QStringList parts = endStr.split(':');
                if (parts.size() >= 3) {
                    int h = parts[0].toInt();
                    int m = parts[1].toInt();
                    QStringList secParts = parts[2].split('.');
                    int s = secParts[0].toInt();
                    int ms = secParts.size() > 1 ? secParts[1].left(3).toInt() : 0;
                    endMs = static_cast<quint32>((static_cast<quint64>(h) * 3600 + m * 60 + s) * 1000 + ms);
                }
            }
            m_dataStore->setTimeRange(startMs, endMs);
        }
    }
    
    if (args.contains("date_range")) {
        QJsonObject dateRange = args["date_range"].toObject();
        if (dateRange.contains("start") || dateRange.contains("end")) {
            int yStart = 0, mStart = 0, dStart = 0, yEnd = 0, mEnd = 0, dEnd = 0;
            if (dateRange.contains("start")) {
                QString startStr = dateRange["start"].toString();
                QStringList parts = startStr.split('-');
                if (parts.size() >= 3) {
                    yStart = parts[0].toInt();
                    mStart = parts[1].toInt();
                    dStart = parts[2].toInt();
                }
            }
            if (dateRange.contains("end")) {
                QString endStr = dateRange["end"].toString();
                QStringList parts = endStr.split('-');
                if (parts.size() >= 3) {
                    yEnd = parts[0].toInt();
                    mEnd = parts[1].toInt();
                    dEnd = parts[2].toInt();
                }
            }
            if (yStart > 0 && yEnd > 0) {
                m_dataStore->setDateRangeFilter(yStart, mStart, dStart, yEnd, mEnd, dEnd);
            } else if (yStart > 0) {
                m_dataStore->setDateRangeFilter(yStart, mStart, dStart);
            }
        }
    }
    
    if (args.contains("packet_length")) {
        QJsonObject lenRange = args["packet_length"].toObject();
        int minLen = lenRange.contains("min") ? lenRange["min"].toInt() : 0;
        int maxLen = lenRange.contains("max") ? lenRange["max"].toInt() : INT_MAX;
        m_dataStore->setPacketLenFilter(minLen, maxLen);
    }
    
    if (args.contains("word_count")) {
        QJsonObject wcRange = args["word_count"].toObject();
        int minWc = wcRange.contains("min") ? wcRange["min"].toInt() : 0;
        int maxWc = wcRange.contains("max") ? wcRange["max"].toInt() : 32;
        m_dataStore->setWordCountFilter(minWc, maxWc);
    }
    
    if (args.contains("status_bit")) {
        QJsonObject sb = args["status_bit"].toObject();
        int field = sb.contains("field") ? sb["field"].toInt() : 1;
        int bit = sb.contains("bit") ? sb["bit"].toInt() : 0;
        bool value = sb.contains("value") ? sb["value"].toBool() : true;
        m_dataStore->setStatusBitFilter(field, bit, value);
    }
    
    if (args.contains("exclude_terminal")) {
        QJsonArray arr = args["exclude_terminal"].toArray();
        QSet<int> excludeTerminals;
        for (const auto& v : arr) excludeTerminals.insert(v.toInt());
        if (!excludeTerminals.isEmpty()) m_dataStore->setExcludeTerminalFilter(excludeTerminals);
    }
    
    if (args.contains("exclude_message_type")) {
        QJsonArray arr = args["exclude_message_type"].toArray();
        QSet<MessageType> excludeTypes;
        for (const auto& v : arr) {
            QString typeStr = v.toString();
            if (typeStr == "BC_TO_RT") excludeTypes.insert(MessageType::BC_TO_RT);
            else if (typeStr == "RT_TO_BC") excludeTypes.insert(MessageType::RT_TO_BC);
            else if (typeStr == "RT_TO_RT") excludeTypes.insert(MessageType::RT_TO_RT);
            else if (typeStr == "Broadcast") excludeTypes.insert(MessageType::Broadcast);
        }
        if (!excludeTypes.isEmpty()) m_dataStore->setExcludeMessageTypeFilter(excludeTypes);
    }
    
    if (args.contains("error_flag")) {
        bool errorFlag = args["error_flag"].toBool();
        m_dataStore->setErrorFlagFilter(errorFlag ? 1 : 0);
    }
    
    m_dataStore->endBatchFilterUpdate();
}

qint64 AIToolExecutor::countFilteredRecords(const QJsonObject& args)
{
    if (!m_dataStore || !m_dataStore->isDatabaseMode()) return -1;
    
    int fileId = m_dataStore->currentFileId();
    if (fileId <= 0) return -1;
    
    QSet<int> terminalFilter = m_dataStore->terminalFilter();
    QSet<int> subAddrFilter = m_dataStore->subAddressFilter();
    QSet<MessageType> typeFilter = m_dataStore->messageTypeFilter();
    int chsttFilter = m_dataStore->chsttFilter();
    int mpuIdFilter = m_dataStore->mpuIdFilter();
    qint64 startTime = m_dataStore->hasTimeRange() ? m_dataStore->startTime() : 0;
    qint64 endTime = m_dataStore->hasTimeRange() ? m_dataStore->endTime() : LLONG_MAX;
    
    if (args.contains("terminal_address")) {
        QJsonArray arr = args["terminal_address"].toArray();
        terminalFilter.clear();
        for (const auto& v : arr) terminalFilter.insert(v.toInt());
    }
    if (args.contains("sub_address")) {
        QJsonArray arr = args["sub_address"].toArray();
        subAddrFilter.clear();
        for (const auto& v : arr) subAddrFilter.insert(v.toInt());
    }
    if (args.contains("message_type")) {
        QJsonArray arr = args["message_type"].toArray();
        typeFilter.clear();
        for (const auto& v : arr) {
            QString typeStr = v.toString();
            if (typeStr == "BC_TO_RT") typeFilter.insert(MessageType::BC_TO_RT);
            else if (typeStr == "RT_TO_BC") typeFilter.insert(MessageType::RT_TO_BC);
            else if (typeStr == "RT_TO_RT") typeFilter.insert(MessageType::RT_TO_RT);
            else if (typeStr == "Broadcast") typeFilter.insert(MessageType::Broadcast);
        }
    }
    if (args.contains("chstt")) {
        chsttFilter = args["chstt"].toBool() ? 1 : 0;
    }
    if (args.contains("mpu_id")) {
        mpuIdFilter = args["mpu_id"].toInt();
    }
    if (args.contains("time_range")) {
        QJsonObject timeRange = args["time_range"].toObject();
        if (timeRange.contains("start")) {
            QString startStr = timeRange["start"].toString();
            QStringList parts = startStr.split(':');
            if (parts.size() >= 3) {
                int h = parts[0].toInt();
                int m = parts[1].toInt();
                QStringList secParts = parts[2].split('.');
                int s = secParts[0].toInt();
                int ms = secParts.size() > 1 ? secParts[1].left(3).toInt() : 0;
                startTime = (h * 3600 + m * 60 + s) * 1000 + ms;
            }
        }
        if (timeRange.contains("end")) {
            QString endStr = timeRange["end"].toString();
            QStringList parts = endStr.split(':');
            if (parts.size() >= 3) {
                int h = parts[0].toInt();
                int m = parts[1].toInt();
                QStringList secParts = parts[2].split('.');
                int s = secParts[0].toInt();
                int ms = secParts.size() > 1 ? secParts[1].left(3).toInt() : 0;
                endTime = (h * 3600 + m * 60 + s) * 1000 + ms;
            }
        }
    }
    
    return DatabaseManager::instance()->queryPacketCount(
        fileId, terminalFilter, subAddrFilter, typeFilter,
        startTime, endTime, chsttFilter, mpuIdFilter
    );
}

ToolResult AIToolExecutor::executeAddFilter(const QJsonObject& args)
{
    ToolResult result;
    result.actionType = "query";
    
    if (!m_dataStore) {
        result.success = false;
        result.message = tr("数据存储未初始化");
        return result;
    }
    
    applyFilterArgs(args, false);
    
    QJsonObject filters;
    if (args.contains("terminal_address")) filters["terminal_address"] = args["terminal_address"].toArray();
    if (args.contains("sub_address")) filters["sub_address"] = args["sub_address"].toArray();
    if (args.contains("message_type")) filters["message_type"] = args["message_type"].toArray();
    if (args.contains("chstt")) filters["chstt"] = args["chstt"];
    if (args.contains("mpu_id")) filters["mpu_id"] = args["mpu_id"];
    if (args.contains("time_range")) filters["time_range"] = args["time_range"].toObject();
    if (args.contains("date_range")) filters["date_range"] = args["date_range"].toObject();
    if (args.contains("packet_length")) filters["packet_length"] = args["packet_length"].toObject();
    if (args.contains("word_count")) filters["word_count"] = args["word_count"].toObject();
    if (args.contains("status_bit")) filters["status_bit"] = args["status_bit"].toObject();
    if (args.contains("exclude_terminal")) filters["exclude_terminal"] = args["exclude_terminal"].toArray();
    if (args.contains("exclude_message_type")) filters["exclude_message_type"] = args["exclude_message_type"].toArray();
    if (args.contains("error_flag")) filters["error_flag"] = args["error_flag"];
    
    emit queryDataRequested(filters);
    emit switchToTableTabRequested();
    
    int count = m_dataStore->filteredCount();
    result.success = true;
    result.message = tr("已追加筛选条件，当前显示 %1 条数据").arg(count);
    result.data = filters;
    
    return result;
}

ToolResult AIToolExecutor::executeApplyFiltersBatch(const QJsonObject& args)
{
    ToolResult result;
    result.actionType = "query";
    
    if (!m_dataStore) {
        result.success = false;
        result.message = tr("数据存储未初始化");
        return result;
    }
    
    applyFilterArgs(args, true);
    
    QJsonObject filters;
    if (args.contains("terminal_address")) filters["terminal_address"] = args["terminal_address"].toArray();
    if (args.contains("sub_address")) filters["sub_address"] = args["sub_address"].toArray();
    if (args.contains("message_type")) filters["message_type"] = args["message_type"].toArray();
    if (args.contains("chstt")) filters["chstt"] = args["chstt"];
    if (args.contains("mpu_id")) filters["mpu_id"] = args["mpu_id"];
    if (args.contains("time_range")) filters["time_range"] = args["time_range"].toObject();
    if (args.contains("date_range")) filters["date_range"] = args["date_range"].toObject();
    if (args.contains("packet_length")) filters["packet_length"] = args["packet_length"].toObject();
    if (args.contains("word_count")) filters["word_count"] = args["word_count"].toObject();
    if (args.contains("status_bit")) filters["status_bit"] = args["status_bit"].toObject();
    if (args.contains("exclude_terminal")) filters["exclude_terminal"] = args["exclude_terminal"].toArray();
    if (args.contains("exclude_message_type")) filters["exclude_message_type"] = args["exclude_message_type"].toArray();
    if (args.contains("error_flag")) filters["error_flag"] = args["error_flag"];
    
    emit queryDataRequested(filters);
    emit switchToTableTabRequested();
    
    int count = m_dataStore->filteredCount();
    result.success = true;
    result.message = tr("已批量设置筛选条件，当前显示 %1 条数据").arg(count);
    result.data = filters;
    
    return result;
}

ToolResult AIToolExecutor::executePreviewFilter(const QJsonObject& args)
{
    ToolResult result;
    result.actionType = "preview";
    
    if (!m_dataStore) {
        result.success = false;
        result.message = tr("数据存储未初始化");
        return result;
    }
    
    qint64 previewCount = countFilteredRecords(args);
    
    if (previewCount >= 0) {
        int totalCount = m_dataStore->totalCount();
        result.success = true;
        double percent = totalCount > 0 ? (previewCount * 100.0 / totalCount) : 0.0;
        result.message = tr("预览：满足条件的数据约 %1 条（占总数 %2%，共 %3 条）")
            .arg(previewCount)
            .arg(QString::number(percent, 'f', 1))
            .arg(totalCount);
        result.data["preview_count"] = static_cast<qint64>(previewCount);
        result.data["total_count"] = totalCount;
    } else {
        result.success = true;
        result.message = tr("预览功能仅支持数据库模式，当前为内存模式");
    }
    
    return result;
}

/**
 * @brief 执行数据查询工具
 * @param args 查询参数
 * @return 执行结果
 * 
 * 支持的参数：
 * - terminal_address: 终端地址数组
 * - sub_address: 子地址数组
 * - message_type: 消息类型数组（BC_TO_RT/RT_TO_BC/RT_TO_RT/Broadcast）
 * - chstt: 通道状态（true/false）
 * - mpu_id: 任务机ID
 * - time_range: 时间范围对象（start/end）
 * 
 * 执行流程：
 * 1. 清除所有现有筛选条件
 * 2. 根据参数设置新的筛选条件
 * 3. 发出信号通知界面更新
 * 4. 返回筛选结果统计
 */
ToolResult AIToolExecutor::executeQueryData(const QJsonObject& args)
{
    ToolResult result;
    result.actionType = "query";
    
    if (!m_dataStore) {
        result.success = false;
        result.message = tr("数据存储未初始化");
        return result;
    }
    
    applyFilterArgs(args, true);
    
    QJsonObject filters;
    if (args.contains("terminal_address")) filters["terminal_address"] = args["terminal_address"].toArray();
    if (args.contains("sub_address")) filters["sub_address"] = args["sub_address"].toArray();
    if (args.contains("message_type")) filters["message_type"] = args["message_type"].toArray();
    if (args.contains("chstt")) filters["chstt"] = args["chstt"];
    if (args.contains("mpu_id")) filters["mpu_id"] = args["mpu_id"];
    if (args.contains("time_range")) filters["time_range"] = args["time_range"].toObject();
    
    emit queryDataRequested(filters);
    emit switchToTableTabRequested();
    
    int count = m_dataStore->filteredCount();
    result.success = true;
    result.message = tr("已应用筛选条件，当前显示 %1 条数据").arg(count);
    result.data = filters;
    
    return result;
}

/**
 * @brief 执行生成图表工具
 * @param args 图表参数
 * @return 执行结果
 * 
 * 支持的参数：
 * - chart_type: 图表类型（pie/bar/line）
 * - subject: 统计主题（message_type/terminal/chstt/time）
 * - title: 图表标题（可选）
 * 
 * 执行流程：
 * 1. 解析图表类型和统计主题
 * 2. 发出信号请求生成图表
 * 3. 发出信号切换到图表选项卡
 * 4. 返回生成结果描述
 */
ToolResult AIToolExecutor::executeGenerateChart(const QJsonObject& args)
{
    ToolResult result;
    result.actionType = "chart";
    
    QString chartType = args["chart_type"].toString("pie");
    QString subject = args["subject"].toString("message_type");
    QString title = args["title"].toString();
    
    emit generateChartRequested(chartType, subject, title);
    emit switchToChartTabRequested();
    
    QString subjectName;
    if (subject == "message_type") subjectName = tr("消息类型分布");
    else if (subject == "terminal") subjectName = tr("终端数据量");
    else if (subject == "chstt") subjectName = tr("收发状态");
    else if (subject == "time") subjectName = tr("时间趋势");
    else subjectName = subject;
    
    QString chartTypeName;
    if (chartType == "pie") chartTypeName = tr("饼图");
    else if (chartType == "bar") chartTypeName = tr("柱状图");
    else if (chartType == "line") chartTypeName = tr("折线图");
    else chartTypeName = chartType;
    
    result.success = true;
    result.message = tr("已生成%1：%2").arg(chartTypeName, subjectName);
    
    return result;
}

/**
 * @brief 执行生成甘特图工具
 * @param args 甘特图参数
 * @return 执行结果
 * 
 * 支持的参数：
 * - terminal_address: 终端地址数组（可选）
 * - message_type: 消息类型数组（可选）
 * - time_range: 时间范围对象（可选）
 * 
 * 执行流程：
 * 1. 收集筛选条件
 * 2. 发出信号请求生成甘特图
 * 3. 发出信号切换到甘特图选项卡
 * 4. 返回生成结果描述
 */
ToolResult AIToolExecutor::executeGenerateGantt(const QJsonObject& args)
{
    ToolResult result;
    result.actionType = "gantt";
    
    QJsonObject filters;
    
    if (args.contains("terminal_address")) {
        filters["terminal_address"] = args["terminal_address"].toArray();
    }
    if (args.contains("message_type")) {
        filters["message_type"] = args["message_type"].toArray();
    }
    if (args.contains("time_range")) {
        filters["time_range"] = args["time_range"].toObject();
    }
    
    emit generateGanttRequested(filters);
    emit switchToGanttTabRequested();
    
    result.success = true;
    result.message = tr("已生成甘特图");
    result.data = filters;
    
    return result;
}

/**
 * @brief 执行获取统计信息工具
 * @param args 统计参数
 * @return 执行结果
 * 
 * 支持的参数：
 * - stat_type: 统计类型（overview/by_type/by_terminal）
 * 
 * 返回的统计信息：
 * - total: 总数据量
 * - success/fail: 成功/失败数量
 * - success_rate: 成功率
 * - message_types: 各消息类型数量
 * - terminals: 各终端数据量
 */
ToolResult AIToolExecutor::executeGetStatistics(const QJsonObject& args)
{
    ToolResult result;
    result.actionType = "statistics";
    
    if (!m_dataStore) {
        result.success = false;
        result.message = tr("数据存储未初始化");
        return result;
    }
    
    QString statType = args["stat_type"].toString("overview");
    
    QJsonObject stats;
    
    if (statType == "overview" || statType == "by_type") {
        QMap<MessageType, int> typeStats = m_dataStore->getMessageTypeStatistics();
        QJsonObject typeObj;
        for (auto it = typeStats.begin(); it != typeStats.end(); ++it) {
            QString typeName;
            switch (it.key()) {
                case MessageType::BC_TO_RT: typeName = "BC_TO_RT"; break;
                case MessageType::RT_TO_BC: typeName = "RT_TO_BC"; break;
                case MessageType::RT_TO_RT: typeName = "RT_TO_RT"; break;
                case MessageType::Broadcast: typeName = "Broadcast"; break;
                default: typeName = "Unknown"; break;
            }
            typeObj[typeName] = it.value();
        }
        stats["message_types"] = typeObj;
    }
    
    if (statType == "overview" || statType == "by_terminal") {
        QMap<int, int> terminalStats = m_dataStore->getTerminalStatistics();
        QJsonObject termObj;
        for (auto it = terminalStats.begin(); it != terminalStats.end(); ++it) {
            termObj[QString::number(it.key())] = it.value();
        }
        stats["terminals"] = termObj;
    }
    
    if (statType == "overview") {
        int totalCount = m_dataStore->totalCount();
        int successCount = 0, failCount = 0;
        
        for (int i = 0; i < totalCount; ++i) {
            DataRecord record = m_dataStore->getRecord(i);
            if (record.packetData.chstt) successCount++;
            else failCount++;
        }
        
        stats["total"] = totalCount;
        stats["success"] = successCount;
        stats["fail"] = failCount;
        stats["success_rate"] = totalCount > 0 ? QString::number(successCount * 100.0 / totalCount, 'f', 2) + "%" : "0%";
    }
    
    result.success = true;
    result.message = formatStatisticsResult(stats);
    result.data = stats;
    
    return result;
}

/**
 * @brief 执行清除筛选条件工具
 * @param args 无参数
 * @return 执行结果
 * 
 * 清除所有筛选条件，恢复显示全部数据
 */
ToolResult AIToolExecutor::executeClearFilter(const QJsonObject& args)
{
    Q_UNUSED(args);
    
    ToolResult result;
    result.actionType = "clear";
    
    if (m_dataStore) {
        m_dataStore->clearFilter();
        m_dataStore->clearTerminalFilter();
        m_dataStore->clearSubAddressFilter();
        m_dataStore->clearMessageTypeFilter();
        m_dataStore->clearChsttFilter();
        m_dataStore->clearMpuIdFilter();
        m_dataStore->clearTimeRange();
        m_dataStore->clearPacketLenFilter();
        m_dataStore->clearDateRangeFilter();
        m_dataStore->clearStatusBitFilter();
        m_dataStore->clearExcludeTerminalFilter();
        m_dataStore->clearExcludeMessageTypeFilter();
        m_dataStore->clearWordCountFilter();
        m_dataStore->clearErrorFlagFilter();
    }
    
    emit clearFilterRequested();
    emit switchToTableTabRequested();
    
    result.success = true;
    result.message = tr("已清除所有筛选条件，显示全部数据");
    
    return result;
}

/**
 * @brief 格式化统计结果
 * @param stats 统计数据JSON对象
 * @return 格式化后的文本描述
 * 
 * 将统计数据转换为人类可读的文本格式，包括：
 * - 数据概览（总量、成功/失败、成功率）
 * - 消息类型分布
 * - 终端数据量（前10个）
 */
QString AIToolExecutor::formatStatisticsResult(const QJsonObject& stats)
{
    QString result;
    
    if (stats.contains("total")) {
        result += tr("数据概览：\n");
        result += tr("  总数据量：%1 条\n").arg(stats["total"].toInt());
        result += tr("  成功：%1 条\n").arg(stats["success"].toInt());
        result += tr("  失败：%1 条\n").arg(stats["fail"].toInt());
        result += tr("  成功率：%1\n\n").arg(stats["success_rate"].toString());
    }
    
    if (stats.contains("message_types")) {
        result += tr("消息类型分布：\n");
        QJsonObject types = stats["message_types"].toObject();
        for (auto it = types.begin(); it != types.end(); ++it) {
            result += tr("  %1：%2 条\n").arg(it.key()).arg(it.value().toInt());
        }
        result += "\n";
    }
    
    if (stats.contains("terminals")) {
        result += tr("终端数据量（前10个）：\n");
        QJsonObject terminals = stats["terminals"].toObject();
        QStringList sortedTerminals;
        for (auto it = terminals.begin(); it != terminals.end(); ++it) {
            sortedTerminals.append(QString("%1:%2").arg(it.key(), -3).arg(it.value().toInt()));
        }
        std::sort(sortedTerminals.begin(), sortedTerminals.end(), [](const QString& a, const QString& b) {
            return a.split(":").last().toInt() > b.split(":").last().toInt();
        });
        int count = 0;
        for (const QString& t : sortedTerminals) {
            if (count++ >= 10) break;
            QStringList parts = t.split(":");
            result += tr("  终端 %1：%2 条\n").arg(parts[0].trimmed()).arg(parts[1]);
        }
    }
    
    return result;
}
