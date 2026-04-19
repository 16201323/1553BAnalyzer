/**
 * @file AIToolDefinitions.h
 * @brief AI工具定义（Function Calling协议）
 *
 * 本文件定义了AI模型可调用的所有工具（Function Calling），
 * 遵循OpenAI Function Calling协议格式。
 *
 * 工作原理：
 * 1. 将工具定义随请求发送给AI模型
 * 2. AI分析用户意图后，返回要调用的工具名称和参数
 * 3. AIToolExecutor解析并执行工具调用
 * 4. 执行结果返回给AI或直接更新界面
 *
 * 当前支持的工具：
 * - query_data: 查询数据（清除现有筛选，设置新筛选条件）
 * - add_filter: 在现有筛选基础上增加条件
 * - apply_filters_batch: 一次性设置所有筛选条件
 * - preview_filter: 预览筛选结果数量
 * - generate_chart: 生成统计图表（饼图/柱状图/折线图）
 * - generate_gantt: 生成甘特图
 * - get_statistics: 获取统计信息
 * - clear_filter: 清除所有筛选条件
 * - generate_report: 生成智能分析报告
 * - get_anomalies: 获取异常数据
 * - get_top_n: 获取Top N数据
 *
 * 每个工具定义包含：
 * - name: 工具名称（唯一标识）
 * - description: 工具功能描述（AI根据此描述选择合适的工具）
 * - parameters: 工具参数的JSON Schema定义
 *
 * @note 本文件中的工具定义是AI与数据分析功能之间的桥梁，
 *       修改工具定义会影响AI的理解和调用行为。
 *
 * @author 1553BTools
 * @date 2024
 */

#ifndef AITOOLDEFINITIONS_H
#define AITOOLDEFINITIONS_H

#include <QJsonObject>
#include <QJsonArray>
#include <QString>

/**
 * @brief AI工具定义集合类
 *
 * 提供所有AI可调用工具的JSON Schema定义。
 * 所有方法均为静态方法，返回符合OpenAI Function Calling格式的JSON对象。
 *
 * 使用示例：
 * @code
 * QJsonArray tools = AIToolDefinitions::getTools();
 * // 将tools数组添加到AI请求的tools字段中
 * @endcode
 */
class AIToolDefinitions
{
public:
    /**
     * @brief 获取所有工具定义
     * @return 工具定义的JSON数组
     *
     * 返回所有可用工具的完整定义列表，
     * 可直接用于AI请求的tools参数
     */
    static QJsonArray getTools()
    {
        QJsonArray tools;
        
        tools.append(createQueryDataTool());
        tools.append(createAddFilterTool());
        tools.append(createApplyFiltersBatchTool());
        tools.append(createPreviewFilterTool());
        tools.append(createGenerateChartTool());
        tools.append(createGenerateGanttTool());
        tools.append(createStatisticsTool());
        tools.append(createClearFilterTool());
        tools.append(createGenerateReportTool());
        tools.append(createGetAnomaliesTool());
        tools.append(createGetTopNTool());
        tools.append(createAnalyzeTimeIntervalTool());
        
        return tools;
    }
    
    static QJsonObject createQueryDataTool()
    {
        QJsonObject tool;
        tool["type"] = "function";
        
        QJsonObject function;
        function["name"] = "query_data";
        function["description"] = "查询1553B总线数据并显示在表格中。此工具会清除所有现有筛选条件并设置新的筛选条件。如果只想在现有筛选基础上增加条件，请使用add_filter工具。如果需要一次性设置多个筛选条件，请使用apply_filters_batch工具。";
        
        QJsonObject parameters;
        parameters["type"] = "object";
        
        QJsonObject properties;
        
        QJsonObject terminalAddress;
        terminalAddress["type"] = "array";
        QJsonObject items1;
        items1["type"] = "integer";
        items1["minimum"] = 0;
        items1["maximum"] = 31;
        terminalAddress["items"] = items1;
        terminalAddress["description"] = u8"终端地址列表，范围0-31，31表示广播地址";
        properties["terminal_address"] = terminalAddress;
        
        QJsonObject subAddress;
        subAddress["type"] = "array";
        QJsonObject items2;
        items2["type"] = "integer";
        items2["minimum"] = 0;
        items2["maximum"] = 31;
        subAddress["items"] = items2;
        subAddress["description"] = u8"子地址列表，范围0-31";
        properties["sub_address"] = subAddress;
        
        QJsonObject messageType;
        messageType["type"] = "array";
        QJsonObject items3;
        items3["type"] = "string";
        QJsonArray enumValues;
        enumValues.append("BC_TO_RT");
        enumValues.append("RT_TO_BC");
        enumValues.append("RT_TO_RT");
        enumValues.append("Broadcast");
        items3["enum"] = enumValues;
        messageType["items"] = items3;
        messageType["description"] = u8"消息类型：BC_TO_RT(BC到RT), RT_TO_BC(RT到BC), RT_TO_RT(RT到RT), Broadcast(广播)";
        properties["message_type"] = messageType;
        
        QJsonObject chstt;
        chstt["type"] = "boolean";
        chstt["description"] = u8"收发状态：true=成功, false=失败, 不指定则显示全部";
        properties["chstt"] = chstt;
        
        QJsonObject mpuId;
        mpuId["type"] = "integer";
        QJsonObject enumMpu;
        QJsonArray mpuValues;
        mpuValues.append(1);
        mpuValues.append(2);
        enumMpu["enum"] = mpuValues;
        mpuId["enum"] = enumMpu;
        mpuId["description"] = u8"任务机ID：1=MPU1, 2=MPU2";
        properties["mpu_id"] = mpuId;
        
        QJsonObject timeRange;
        timeRange["type"] = "object";
        QJsonObject timeProps;
        QJsonObject startTime;
        startTime["type"] = "string";
        startTime["description"] = u8"开始时间，格式：HH:MM:SS.mmm 或 HH:MM:SS";
        timeProps["start"] = startTime;
        QJsonObject endTime;
        endTime["type"] = "string";
        endTime["description"] = u8"结束时间，格式：HH:MM:SS.mmm 或 HH:MM:SS";
        timeProps["end"] = endTime;
        timeRange["properties"] = timeProps;
        timeRange["description"] = u8"时间范围筛选";
        properties["time_range"] = timeRange;
        
        QJsonObject dateRange;
        dateRange["type"] = "object";
        QJsonObject dateProps;
        QJsonObject startDate;
        startDate["type"] = "string";
        startDate["description"] = u8"开始日期，格式：YYYY-MM-DD";
        dateProps["start"] = startDate;
        QJsonObject endDate;
        endDate["type"] = "string";
        endDate["description"] = u8"结束日期，格式：YYYY-MM-DD";
        dateProps["end"] = endDate;
        dateRange["properties"] = dateProps;
        dateRange["description"] = u8"日期范围筛选，按年月日过滤数据";
        properties["date_range"] = dateRange;
        
        QJsonObject packetLength;
        packetLength["type"] = "object";
        QJsonObject lenProps;
        QJsonObject minLen;
        minLen["type"] = "integer";
        minLen["description"] = u8"最小包长度";
        lenProps["min"] = minLen;
        QJsonObject maxLen;
        maxLen["type"] = "integer";
        maxLen["description"] = u8"最大包长度";
        lenProps["max"] = maxLen;
        packetLength["properties"] = lenProps;
        packetLength["description"] = u8"包长度范围筛选";
        properties["packet_length"] = packetLength;
        
        QJsonObject wordCount;
        wordCount["type"] = "object";
        QJsonObject wcProps;
        QJsonObject minWc;
        minWc["type"] = "integer";
        minWc["description"] = u8"最小数据计数";
        wcProps["min"] = minWc;
        QJsonObject maxWc;
        maxWc["type"] = "integer";
        maxWc["description"] = u8"最大数据计数";
        wcProps["max"] = maxWc;
        wordCount["properties"] = wcProps;
        wordCount["description"] = u8"数据计数（字数）范围筛选";
        properties["word_count"] = wordCount;
        
        QJsonObject statusBit;
        statusBit["type"] = "object";
        QJsonObject sbProps;
        QJsonObject sbField;
        sbField["type"] = "integer";
        QJsonObject sbFieldEnum;
        QJsonArray sbFieldValues;
        sbFieldValues.append(1);
        sbFieldValues.append(2);
        sbFieldEnum["enum"] = sbFieldValues;
        sbField["enum"] = sbFieldEnum;
        sbField["description"] = u8"状态字字段：1=状态字1(states1), 2=状态字2(states2)";
        sbProps["field"] = sbField;
        QJsonObject sbBit;
        sbBit["type"] = "integer";
        sbBit["minimum"] = 0;
        sbBit["maximum"] = 15;
        sbBit["description"] = u8"位位置(0-15)。常见位：0=已响应,1=忙,2=子系统故障,3=动态总线控制接受,4=服务请求,10=测试标志,11=服务请求,15=广播命令接收";
        sbProps["bit"] = sbBit;
        QJsonObject sbValue;
        sbValue["type"] = "boolean";
        sbValue["description"] = u8"位值：true=该位为1, false=该位为0";
        sbProps["value"] = sbValue;
        statusBit["properties"] = sbProps;
        statusBit["description"] = u8"状态字位筛选，用于检查状态字的特定位是否为1或0。例如检查Busy标志位(bit1)是否为1";
        properties["status_bit"] = statusBit;
        
        QJsonObject excludeTerminal;
        excludeTerminal["type"] = "array";
        QJsonObject exItems1;
        exItems1["type"] = "integer";
        excludeTerminal["items"] = exItems1;
        excludeTerminal["description"] = u8"排除的终端地址列表，这些终端的数据不会显示";
        properties["exclude_terminal"] = excludeTerminal;
        
        QJsonObject excludeMessageType;
        excludeMessageType["type"] = "array";
        QJsonObject exItems2;
        exItems2["type"] = "string";
        QJsonArray exEnumValues;
        exEnumValues.append("BC_TO_RT");
        exEnumValues.append("RT_TO_BC");
        exEnumValues.append("RT_TO_RT");
        exEnumValues.append("Broadcast");
        exItems2["enum"] = exEnumValues;
        excludeMessageType["items"] = exItems2;
        excludeMessageType["description"] = u8"排除的消息类型列表";
        properties["exclude_message_type"] = excludeMessageType;
        
        QJsonObject errorFlag;
        errorFlag["type"] = "boolean";
        errorFlag["description"] = u8"错误标志筛选：true=有错误(chstt=0), false=无错误(chstt!=0)";
        properties["error_flag"] = errorFlag;
        
        QJsonObject limit;
        limit["type"] = "integer";
        limit["description"] = u8"返回数据条数限制，默认显示全部";
        limit["minimum"] = 1;
        properties["limit"] = limit;
        
        parameters["properties"] = properties;
        function["parameters"] = parameters;
        tool["function"] = function;
        
        return tool;
    }
    
    static QJsonObject createGenerateChartTool()
    {
        QJsonObject tool;
        tool["type"] = "function";
        
        QJsonObject function;
        function["name"] = "generate_chart";
        function["description"] = u8"生成统计图表并显示。可以生成饼图、柱状图、折线图等，展示消息类型分布、终端数据量、收发状态等统计信息。";
        
        QJsonObject parameters;
        parameters["type"] = "object";
        
        QJsonArray required;
        required.append("chart_type");
        required.append("subject");
        parameters["required"] = required;
        
        QJsonObject properties;
        
        QJsonObject chartType;
        chartType["type"] = "string";
        QJsonArray chartEnum;
        chartEnum.append("pie");
        chartEnum.append("bar");
        chartEnum.append("line");
        chartType["enum"] = chartEnum;
        chartType["description"] = u8"图表类型：pie(饼图), bar(柱状图), line(折线图)";
        properties["chart_type"] = chartType;
        
        QJsonObject subject;
        subject["type"] = "string";
        QJsonArray subjectEnum;
        subjectEnum.append("message_type");
        subjectEnum.append("terminal");
        subjectEnum.append("chstt");
        subjectEnum.append("time");
        subject["enum"] = subjectEnum;
        subject["description"] = u8"统计主题：message_type(消息类型分布), terminal(终端数据量), chstt(收发成功失败), time(时间趋势)";
        properties["subject"] = subject;
        
        QJsonObject title;
        title["type"] = "string";
        title["description"] = u8"图表标题，可选";
        properties["title"] = title;
        
        parameters["properties"] = properties;
        function["parameters"] = parameters;
        tool["function"] = function;
        
        return tool;
    }
    
    static QJsonObject createGenerateGanttTool()
    {
        QJsonObject tool;
        tool["type"] = "function";
        
        QJsonObject function;
        function["name"] = "generate_gantt";
        function["description"] = u8"生成数据流动甘特图，展示数据传输的时间序列和终端间的通信关系。";
        
        QJsonObject parameters;
        parameters["type"] = "object";
        
        QJsonObject properties;
        
        QJsonObject terminalAddress;
        terminalAddress["type"] = "array";
        QJsonObject items;
        items["type"] = "integer";
        terminalAddress["items"] = items;
        terminalAddress["description"] = u8"要显示的终端地址列表，不指定则显示全部";
        properties["terminal_address"] = terminalAddress;
        
        QJsonObject timeRange;
        timeRange["type"] = "object";
        QJsonObject timeProps;
        QJsonObject startTime;
        startTime["type"] = "string";
        startTime["description"] = u8"开始时间";
        timeProps["start"] = startTime;
        QJsonObject endTime;
        endTime["type"] = "string";
        endTime["description"] = u8"结束时间";
        timeProps["end"] = endTime;
        timeRange["properties"] = timeProps;
        timeRange["description"] = u8"时间范围";
        properties["time_range"] = timeRange;
        
        QJsonObject messageType;
        messageType["type"] = "array";
        QJsonObject items2;
        items2["type"] = "string";
        messageType["items"] = items2;
        messageType["description"] = u8"消息类型筛选";
        properties["message_type"] = messageType;
        
        parameters["properties"] = properties;
        function["parameters"] = parameters;
        tool["function"] = function;
        
        return tool;
    }
    
    static QJsonObject createStatisticsTool()
    {
        QJsonObject tool;
        tool["type"] = "function";
        
        QJsonObject function;
        function["name"] = "get_statistics";
        function["description"] = u8"获取数据统计信息，如总数、成功率、各类型数量等。";
        
        QJsonObject parameters;
        parameters["type"] = "object";
        
        QJsonObject properties;
        
        QJsonObject statType;
        statType["type"] = "string";
        QJsonArray statEnum;
        statEnum.append("overview");
        statEnum.append("by_terminal");
        statEnum.append("by_type");
        statEnum.append("by_time");
        statType["enum"] = statEnum;
        statType["description"] = u8"统计类型：overview(概览), by_terminal(按终端), by_type(按类型), by_time(按时间)";
        properties["stat_type"] = statType;
        
        parameters["properties"] = properties;
        function["parameters"] = parameters;
        tool["function"] = function;
        
        return tool;
    }
    
    static QJsonObject createClearFilterTool()
    {
        QJsonObject tool;
        tool["type"] = "function";
        
        QJsonObject function;
        function["name"] = "clear_filter";
        function["description"] = u8"清除所有筛选条件，显示全部数据。";
        
        QJsonObject parameters;
        parameters["type"] = "object";
        parameters["properties"] = QJsonObject();
        function["parameters"] = parameters;
        tool["function"] = function;
        
        return tool;
    }
    
    static QJsonObject createAddFilterTool()
    {
        QJsonObject tool;
        tool["type"] = "function";
        
        QJsonObject function;
        function["name"] = "add_filter";
        function["description"] = u8"在现有筛选条件基础上增加新的筛选条件，不会清除已有的筛选条件。适合用于逐步缩小数据范围。支持所有筛选参数。";
        
        QJsonObject parameters = createCommonFilterProperties();
        function["parameters"] = parameters;
        tool["function"] = function;
        
        return tool;
    }
    
    static QJsonObject createApplyFiltersBatchTool()
    {
        QJsonObject tool;
        tool["type"] = "function";
        
        QJsonObject function;
        function["name"] = "apply_filters_batch";
        function["description"] = u8"一次性设置所有筛选条件，会清除现有筛选条件后重新设置。适合需要同时设置多个筛选条件的场景，比多次调用add_filter更高效。支持所有筛选参数。";
        
        QJsonObject parameters = createCommonFilterProperties();
        function["parameters"] = parameters;
        tool["function"] = function;
        
        return tool;
    }
    
    static QJsonObject createPreviewFilterTool()
    {
        QJsonObject tool;
        tool["type"] = "function";
        
        QJsonObject function;
        function["name"] = "preview_filter";
        function["description"] = u8"预览筛选条件的结果数量，不会实际应用筛选条件到界面。适合在应用筛选前先查看匹配的数据量。支持所有筛选参数。";
        
        QJsonObject parameters = createCommonFilterProperties();
        function["parameters"] = parameters;
        tool["function"] = function;
        
        return tool;
    }
    
    static QJsonObject createCommonFilterProperties()
    {
        QJsonObject parameters;
        parameters["type"] = "object";
        
        QJsonObject properties;
        
        QJsonObject terminalAddress;
        terminalAddress["type"] = "array";
        QJsonObject items1;
        items1["type"] = "integer";
        items1["minimum"] = 0;
        items1["maximum"] = 31;
        terminalAddress["items"] = items1;
        terminalAddress["description"] = u8"终端地址列表，范围0-31，31表示广播地址";
        properties["terminal_address"] = terminalAddress;
        
        QJsonObject subAddress;
        subAddress["type"] = "array";
        QJsonObject items2;
        items2["type"] = "integer";
        items2["minimum"] = 0;
        items2["maximum"] = 31;
        subAddress["items"] = items2;
        subAddress["description"] = u8"子地址列表，范围0-31";
        properties["sub_address"] = subAddress;
        
        QJsonObject messageType;
        messageType["type"] = "array";
        QJsonObject items3;
        items3["type"] = "string";
        QJsonArray enumValues;
        enumValues.append("BC_TO_RT");
        enumValues.append("RT_TO_BC");
        enumValues.append("RT_TO_RT");
        enumValues.append("Broadcast");
        items3["enum"] = enumValues;
        messageType["items"] = items3;
        messageType["description"] = u8"消息类型：BC_TO_RT(BC到RT), RT_TO_BC(RT到BC), RT_TO_RT(RT到RT), Broadcast(广播)";
        properties["message_type"] = messageType;
        
        QJsonObject chstt;
        chstt["type"] = "boolean";
        chstt["description"] = u8"收发状态：true=成功, false=失败";
        properties["chstt"] = chstt;
        
        QJsonObject mpuId;
        mpuId["type"] = "integer";
        QJsonObject enumMpu;
        QJsonArray mpuValues;
        mpuValues.append(1);
        mpuValues.append(2);
        enumMpu["enum"] = mpuValues;
        mpuId["enum"] = enumMpu;
        mpuId["description"] = u8"任务机ID：1=MPU1, 2=MPU2";
        properties["mpu_id"] = mpuId;
        
        QJsonObject timeRange;
        timeRange["type"] = "object";
        QJsonObject timeProps;
        QJsonObject startTime;
        startTime["type"] = "string";
        startTime["description"] = u8"开始时间，格式：HH:MM:SS.mmm 或 HH:MM:SS";
        timeProps["start"] = startTime;
        QJsonObject endTime;
        endTime["type"] = "string";
        endTime["description"] = u8"结束时间，格式：HH:MM:SS.mmm 或 HH:MM:SS";
        timeProps["end"] = endTime;
        timeRange["properties"] = timeProps;
        timeRange["description"] = u8"时间范围筛选";
        properties["time_range"] = timeRange;
        
        QJsonObject dateRange;
        dateRange["type"] = "object";
        QJsonObject dateProps;
        QJsonObject startDate;
        startDate["type"] = "string";
        startDate["description"] = u8"开始日期，格式：YYYY-MM-DD";
        dateProps["start"] = startDate;
        QJsonObject endDate;
        endDate["type"] = "string";
        endDate["description"] = u8"结束日期，格式：YYYY-MM-DD";
        dateProps["end"] = endDate;
        dateRange["properties"] = dateProps;
        dateRange["description"] = u8"日期范围筛选";
        properties["date_range"] = dateRange;
        
        QJsonObject packetLength;
        packetLength["type"] = "object";
        QJsonObject lenProps;
        QJsonObject minLen;
        minLen["type"] = "integer";
        minLen["description"] = u8"最小包长度";
        lenProps["min"] = minLen;
        QJsonObject maxLen;
        maxLen["type"] = "integer";
        maxLen["description"] = u8"最大包长度";
        lenProps["max"] = maxLen;
        packetLength["properties"] = lenProps;
        packetLength["description"] = u8"包长度范围筛选";
        properties["packet_length"] = packetLength;
        
        QJsonObject wordCount;
        wordCount["type"] = "object";
        QJsonObject wcProps;
        QJsonObject minWc;
        minWc["type"] = "integer";
        minWc["description"] = u8"最小数据计数";
        wcProps["min"] = minWc;
        QJsonObject maxWc;
        maxWc["type"] = "integer";
        maxWc["description"] = u8"最大数据计数";
        wcProps["max"] = maxWc;
        wordCount["properties"] = wcProps;
        wordCount["description"] = u8"数据计数（字数）范围筛选";
        properties["word_count"] = wordCount;
        
        QJsonObject statusBit;
        statusBit["type"] = "object";
        QJsonObject sbProps;
        QJsonObject sbField;
        sbField["type"] = "integer";
        QJsonObject sbFieldEnum;
        QJsonArray sbFieldValues;
        sbFieldValues.append(1);
        sbFieldValues.append(2);
        sbFieldEnum["enum"] = sbFieldValues;
        sbField["enum"] = sbFieldEnum;
        sbField["description"] = u8"状态字字段：1=状态字1, 2=状态字2";
        sbProps["field"] = sbField;
        QJsonObject sbBit;
        sbBit["type"] = "integer";
        sbBit["minimum"] = 0;
        sbBit["maximum"] = 15;
        sbBit["description"] = u8"位位置(0-15)。常见位：0=已响应,1=忙,2=子系统故障,4=服务请求,10=测试标志,15=广播命令接收";
        sbProps["bit"] = sbBit;
        QJsonObject sbValue;
        sbValue["type"] = "boolean";
        sbValue["description"] = u8"位值：true=该位为1, false=该位为0";
        sbProps["value"] = sbValue;
        statusBit["properties"] = sbProps;
        statusBit["description"] = u8"状态字位筛选";
        properties["status_bit"] = statusBit;
        
        QJsonObject excludeTerminal;
        excludeTerminal["type"] = "array";
        QJsonObject exItems1;
        exItems1["type"] = "integer";
        excludeTerminal["items"] = exItems1;
        excludeTerminal["description"] = u8"排除的终端地址列表";
        properties["exclude_terminal"] = excludeTerminal;
        
        QJsonObject excludeMessageType;
        excludeMessageType["type"] = "array";
        QJsonObject exItems2;
        exItems2["type"] = "string";
        QJsonArray exEnumValues;
        exEnumValues.append("BC_TO_RT");
        exEnumValues.append("RT_TO_BC");
        exEnumValues.append("RT_TO_RT");
        exEnumValues.append("Broadcast");
        exItems2["enum"] = exEnumValues;
        excludeMessageType["items"] = exItems2;
        excludeMessageType["description"] = u8"排除的消息类型列表";
        properties["exclude_message_type"] = excludeMessageType;
        
        QJsonObject errorFlag;
        errorFlag["type"] = "boolean";
        errorFlag["description"] = u8"错误标志：true=有错误, false=无错误";
        properties["error_flag"] = errorFlag;
        
        parameters["properties"] = properties;
        return parameters;
    }
    
    static QJsonObject createGenerateReportTool()
    {
        QJsonObject tool;
        tool["type"] = "function";
        
        QJsonObject function;
        function["name"] = "generate_report";
        function["description"] = u8"生成智能分析报告，自动收集数据概览、生成图表、进行AI深度分析（错误率是否超标、负载是否均衡等），并导出报告。";
        
        QJsonObject parameters;
        parameters["type"] = "object";
        
        QJsonObject properties;
        
        QJsonObject format;
        format["type"] = "string";
        QJsonArray formatEnum;
        formatEnum.append("html");
        formatEnum.append("pdf");
        formatEnum.append("docx");
        format["enum"] = formatEnum;
        format["description"] = u8"报告格式：html、pdf或docx，默认html";
        properties["format"] = format;
        
        parameters["properties"] = properties;
        function["parameters"] = parameters;
        tool["function"] = function;
        
        return tool;
    }
    
    static QJsonObject createGetAnomaliesTool()
    {
        QJsonObject tool;
        tool["type"] = "function";
        
        QJsonObject function;
        function["name"] = "get_anomalies";
        function["description"] = u8"检测数据中的异常情况，包括通信失败、超时错误、状态字异常位（如忙标志、子系统故障等）。返回异常数据列表和统计信息。";
        
        QJsonObject parameters;
        parameters["type"] = "object";
        
        QJsonObject properties;
        
        QJsonObject anomalyType;
        anomalyType["type"] = "string";
        QJsonArray anomalyEnum;
        anomalyEnum.append("all");
        anomalyEnum.append("communication_failure");
        anomalyEnum.append("busy_flag");
        anomalyEnum.append("subsystem_fault");
        anomalyEnum.append("service_request");
        anomalyEnum.append("timeout");
        anomalyType["enum"] = anomalyEnum;
        anomalyType["description"] = u8"异常类型：all(全部异常), communication_failure(通信失败/chstt=0), busy_flag(忙标志位), subsystem_fault(子系统故障), service_request(服务请求), timeout(超时)";
        properties["anomaly_type"] = anomalyType;
        
        QJsonObject terminalAddress;
        terminalAddress["type"] = "array";
        QJsonObject items;
        items["type"] = "integer";
        terminalAddress["items"] = items;
        terminalAddress["description"] = u8"限定检测的终端地址列表，不指定则检测全部";
        properties["terminal_address"] = terminalAddress;
        
        parameters["properties"] = properties;
        function["parameters"] = parameters;
        tool["function"] = function;
        
        return tool;
    }
    
    static QJsonObject createGetTopNTool()
    {
        QJsonObject tool;
        tool["type"] = "function";
        
        QJsonObject function;
        function["name"] = "get_top_n";
        function["description"] = u8"获取排名前N的统计数据，如最活跃的终端、最频繁的子地址、错误最多的终端等。";
        
        QJsonObject parameters;
        parameters["type"] = "object";
        
        QJsonArray required;
        required.append("subject");
        parameters["required"] = required;
        
        QJsonObject properties;
        
        QJsonObject subject;
        subject["type"] = "string";
        QJsonArray subjectEnum;
        subjectEnum.append("active_terminal");
        subjectEnum.append("active_sub_address");
        subjectEnum.append("error_terminal");
        subjectEnum.append("frequent_message_type");
        subject["enum"] = subjectEnum;
        subject["description"] = u8"排名主题：active_terminal(最活跃终端), active_sub_address(最频繁子地址), error_terminal(错误最多终端), frequent_message_type(最频繁消息类型)";
        properties["subject"] = subject;
        
        QJsonObject topN;
        topN["type"] = "integer";
        topN["minimum"] = 1;
        topN["maximum"] = 32;
        topN["description"] = u8"返回前N名，默认5";
        properties["top_n"] = topN;
        
        parameters["properties"] = properties;
        function["parameters"] = parameters;
        tool["function"] = function;
        
        return tool;
    }
    
    /**
     * @brief 创建时间间隔分析工具定义
     * 
     * 用于分析指定RT和子地址的时间间隔，
     * 生成折线图展示时间间隔变化趋势，
     * 判断周期数据发送是否稳定
     */
    static QJsonObject createAnalyzeTimeIntervalTool()
    {
        QJsonObject tool;
        tool["type"] = "function";
        
        QJsonObject function;
        function["name"] = "analyze_time_interval";
        function["description"] = u8"分析指定RT和子地址的时间间隔，生成折线图展示时间间隔变化趋势，用于观察周期数据是否稳定发送。返回统计数据包括平均间隔、标准差、抖动百分比和稳定性评估。";
        
        QJsonObject parameters;
        parameters["type"] = "object";
        
        QJsonArray required;
        required.append("terminal_address");
        required.append("sub_address");
        parameters["required"] = required;
        
        QJsonObject properties;
        
        QJsonObject terminalAddress;
        terminalAddress["type"] = "integer";
        terminalAddress["minimum"] = 0;
        terminalAddress["maximum"] = 31;
        terminalAddress["description"] = u8"终端地址（RT地址），范围0-31";
        properties["terminal_address"] = terminalAddress;
        
        QJsonObject subAddress;
        subAddress["type"] = "integer";
        subAddress["minimum"] = 0;
        subAddress["maximum"] = 31;
        subAddress["description"] = u8"子地址，范围0-31";
        properties["sub_address"] = subAddress;
        
        QJsonObject timeRange;
        timeRange["type"] = "object";
        QJsonObject timeProps;
        QJsonObject startTime;
        startTime["type"] = "string";
        startTime["description"] = u8"开始时间，格式：HH:MM:SS.mmm 或 HH:MM:SS，可选";
        timeProps["start"] = startTime;
        QJsonObject endTime;
        endTime["type"] = "string";
        endTime["description"] = u8"结束时间，格式：HH:MM:SS.mmm 或 HH:MM:SS，可选";
        timeProps["end"] = endTime;
        timeRange["properties"] = timeProps;
        timeRange["description"] = u8"时间范围筛选，可选";
        properties["time_range"] = timeRange;
        
        QJsonObject exportCsv;
        exportCsv["type"] = "boolean";
        exportCsv["description"] = u8"是否同时导出CSV文件，默认false";
        properties["export_csv"] = exportCsv;
        
        parameters["properties"] = properties;
        function["parameters"] = parameters;
        tool["function"] = function;
        
        return tool;
    }
    
    static QString getSystemPromptForAnalysis()
    {
        return QString(
            u8"你是一个1553B总线数据分析助手。用户会用自然语言向你提问关于数据的问题，"
            u8"你需要理解用户的意图，并调用相应的工具来完成操作。\n\n"
            u8"## 可用工具\n\n"
            u8"### 1. query_data - 查询并筛选数据（清除现有筛选后重新设置）\n"
            u8"参数：\n"
            u8"- terminal_address: 终端地址数组，范围0-31（如 [5, 10, 31]）\n"
            u8"- sub_address: 子地址数组，范围0-31（如 [7, 21]）\n"
            u8"- message_type: 消息类型数组，可选值：BC_TO_RT, RT_TO_BC, RT_TO_RT, Broadcast\n"
            u8"- chstt: 收发状态，true=成功，false=失败\n"
            u8"- mpu_id: 任务机ID，1或2\n"
            u8"- time_range: 时间范围，{\"start\": \"HH:MM:SS\", \"end\": \"HH:MM:SS\"}\n"
            u8"- date_range: 日期范围，{\"start\": \"YYYY-MM-DD\", \"end\": \"YYYY-MM-DD\"}\n"
            u8"- packet_length: 包长度范围，{\"min\": 最小值, \"max\": 最大值}\n"
            u8"- word_count: 数据计数范围，{\"min\": 最小值, \"max\": 最大值}\n"
            u8"- status_bit: 状态字位筛选，{\"field\": 1或2, \"bit\": 0-15, \"value\": true/false}\n"
            u8"  常见位：bit0=已响应, bit1=忙(Busy), bit2=子系统故障, bit4=服务请求, bit10=测试标志, bit15=广播命令接收\n"
            u8"- exclude_terminal: 排除的终端地址列表\n"
            u8"- exclude_message_type: 排除的消息类型列表\n"
            u8"- error_flag: 错误标志，true=有错误(chstt=0), false=无错误\n\n"
            u8"### 2. add_filter - 增量筛选（在现有条件上追加，不清除已有条件）\n"
            u8"参数与query_data相同。适合\"再筛选\"、\"加上\"、\"进一步\"等场景。\n\n"
            u8"### 3. apply_filters_batch - 批量设置筛选条件\n"
            u8"参数与query_data相同，清除现有条件后一次性设置。比query_data更高效。\n\n"
            u8"### 4. preview_filter - 预览筛选结果数量\n"
            u8"参数与query_data相同，只返回匹配数据条数，不实际应用。\n\n"
            u8"### 5. generate_chart - 生成统计图表\n"
            u8"参数：\n"
            u8"- chart_type: 图表类型，pie/bar/line\n"
            u8"- subject: 统计主题，message_type/terminal/chstt/time\n"
            u8"- title: 图表标题（可选）\n\n"
            u8"### 6. generate_gantt - 生成甘特图\n"
            u8"参数：\n"
            u8"- terminal_address: 终端地址数组（可选）\n"
            u8"- message_type: 消息类型数组（可选）\n"
            u8"- time_range: 时间范围（可选）\n\n"
            u8"### 7. get_statistics - 获取统计信息\n"
            u8"参数：\n"
            u8"- stat_type: 统计类型，overview/by_terminal/by_type/by_time\n\n"
            u8"### 8. get_anomalies - 检测异常数据\n"
            u8"参数：\n"
            u8"- anomaly_type: 异常类型，all/communication_failure/busy_flag/subsystem_fault/service_request/timeout\n"
            u8"- terminal_address: 限定检测的终端地址（可选）\n\n"
            u8"### 9. get_top_n - 获取排名统计\n"
            u8"参数：\n"
            u8"- subject: 排名主题，active_terminal/active_sub_address/error_terminal/frequent_message_type\n"
            u8"- top_n: 返回前N名，默认5\n\n"
            u8"### 10. generate_report - 生成智能分析报告\n"
            u8"参数：\n"
            u8"- format: 报告格式，html或pdf\n\n"
            u8"### 11. clear_filter - 清除筛选条件\n"
            u8"无参数\n\n"
            u8"### 12. analyze_time_interval - 时间间隔分析\n"
            u8"分析指定RT和子地址的时间间隔，判断周期数据发送是否稳定。\n"
            u8"参数：\n"
            u8"- terminal_address: 终端地址（必填，0-31）\n"
            u8"- sub_address: 子地址（必填，0-31）\n"
            u8"- time_range: 时间范围（可选）\n"
            u8"- export_csv: 是否导出CSV文件（可选，默认false）\n\n"
            u8"## 使用场景与示例\n\n"
            u8"### 场景一：故障排查与异常定位\n"
            u8"用户：帮我找一下所有状态字里包含忙标志位的数据\n"
            u8"→ {\"name\": \"query_data\", \"arguments\": {\"status_bit\": {\"field\": 1, \"bit\": 1, \"value\": true}}}\n\n"
            u8"用户：显示所有获取状态为失败的数据帧\n"
            u8"→ {\"name\": \"query_data\", \"arguments\": {\"chstt\": false}}\n\n"
            u8"用户：RT地址为5的终端有没有出现超时错误\n"
            u8"→ {\"name\": \"get_anomalies\", \"arguments\": {\"anomaly_type\": \"timeout\", \"terminal_address\": [5]}}\n\n"
            u8"用户：查看所有广播消息，检查有没有发送失败的\n"
            u8"→ {\"name\": \"query_data\", \"arguments\": {\"message_type\": [\"Broadcast\"], \"chstt\": false}}\n\n"
            u8"用户：有没有异常数据\n"
            u8"→ {\"name\": \"get_anomalies\", \"arguments\": {\"anomaly_type\": \"all\"}}\n\n"
            u8"### 场景二：通信流量与性能统计\n"
            u8"用户：画一个饼图，展示各个终端地址的消息数量占比\n"
            u8"→ {\"name\": \"generate_chart\", \"arguments\": {\"chart_type\": \"pie\", \"subject\": \"terminal\"}}\n\n"
            u8"用户：统计一下哪个子地址通信最频繁\n"
            u8"→ {\"name\": \"get_top_n\", \"arguments\": {\"subject\": \"active_sub_address\"}}\n\n"
            u8"用户：生成一个条形图，对比BC→RT和RT→BC的数量\n"
            u8"→ {\"name\": \"generate_chart\", \"arguments\": {\"chart_type\": \"bar\", \"subject\": \"message_type\"}}\n\n"
            u8"用户：显示数据量随时间变化的折线图\n"
            u8"→ {\"name\": \"generate_chart\", \"arguments\": {\"chart_type\": \"line\", \"subject\": \"time\"}}\n\n"
            u8"用户：哪个终端最活跃\n"
            u8"→ {\"name\": \"get_top_n\", \"arguments\": {\"subject\": \"active_terminal\"}}\n\n"
            u8"用户：哪个终端错误最多\n"
            u8"→ {\"name\": \"get_top_n\", \"arguments\": {\"subject\": \"error_terminal\"}}\n\n"
            u8"### 场景三：深度协议分析与甘特图\n"
            u8"用户：生成甘特图，我要看RT3和RT5之间的数据交互情况\n"
            u8"→ {\"name\": \"generate_gantt\", \"arguments\": {\"terminal_address\": [3, 5]}}\n\n"
            u8"用户：把时间轴放大到第10秒到第15秒，只看BC发给RT的消息\n"
            u8"→ {\"name\": \"generate_gantt\", \"arguments\": {\"time_range\": {\"start\": \"00:00:10\", \"end\": \"00:00:15\"}, \"message_type\": [\"BC_TO_RT\"]}}\n\n"
            u8"用户：筛选出所有RT12发送给RT15的数据\n"
            u8"→ {\"name\": \"query_data\", \"arguments\": {\"message_type\": [\"RT_TO_RT\"], \"terminal_address\": [12, 15]}}\n\n"
            u8"### 场景四：数据筛选与多条件组合\n"
            u8"用户：帮我找包长度大于100的数据\n"
            u8"→ {\"name\": \"query_data\", \"arguments\": {\"packet_length\": {\"min\": 101}}}\n\n"
            u8"用户：显示2024年3月1日的数据\n"
            u8"→ {\"name\": \"query_data\", \"arguments\": {\"date_range\": {\"start\": \"2024-03-01\", \"end\": \"2024-03-01\"}}}\n\n"
            u8"用户：排除广播消息，只看点对点通信\n"
            u8"→ {\"name\": \"query_data\", \"arguments\": {\"exclude_message_type\": [\"Broadcast\"]}}\n\n"
            u8"用户：数据计数为0的消息有哪些\n"
            u8"→ {\"name\": \"query_data\", \"arguments\": {\"word_count\": {\"min\": 0, \"max\": 0}}}\n\n"
            u8"用户：除了RT5和RT10，其他终端的数据都显示\n"
            u8"→ {\"name\": \"query_data\", \"arguments\": {\"exclude_terminal\": [5, 10]}}\n\n"
            u8"用户：MPU1的数据中有没有出错的\n"
            u8"→ {\"name\": \"query_data\", \"arguments\": {\"mpu_id\": 1, \"error_flag\": true}}\n\n"
            u8"### 场景五：自动化报告生成\n"
            u8"用户：生成一份智能分析报告\n"
            u8"→ {\"name\": \"generate_report\", \"arguments\": {\"format\": \"html\"}}\n\n"
            u8"用户：帮我导出PDF报告\n"
            u8"→ {\"name\": \"generate_report\", \"arguments\": {\"format\": \"pdf\"}}\n\n"
            u8"用户：生成一份Word文档报告\n"
            u8"→ {\"name\": \"generate_report\", \"arguments\": {\"format\": \"docx\"}}\n\n"
            u8"### 场景六：时间间隔分析（周期稳定性检测）\n"
            u8"用户：分析RT5子地址10的时间间隔\n"
            u8"→ {\"name\": \"analyze_time_interval\", \"arguments\": {\"terminal_address\": 5, \"sub_address\": 10}}\n\n"
            u8"用户：看看终端3子地址0的发送周期是否稳定\n"
            u8"→ {\"name\": \"analyze_time_interval\", \"arguments\": {\"terminal_address\": 3, \"sub_address\": 0}}\n\n"
            u8"用户：导出RT1子地址15的时间间隔数据\n"
            u8"→ {\"name\": \"analyze_time_interval\", \"arguments\": {\"terminal_address\": 1, \"sub_address\": 15, \"export_csv\": true}}\n\n"
            u8"用户：分析RT7子地址5在10秒到20秒之间的时间间隔\n"
            u8"→ {\"name\": \"analyze_time_interval\", \"arguments\": {\"terminal_address\": 7, \"sub_address\": 5, \"time_range\": {\"start\": \"00:00:10\", \"end\": \"00:00:20\"}}}\n\n"
            u8"## 口语化表达映射\n"
            u8"- \"忙\"、\"忙碌\" → status_bit.bit=1 (Busy标志)\n"
            u8"- \"失败\"、\"出错\"、\"错误\" → chstt=false 或 error_flag=true\n"
            u8"- \"成功\"、\"正常\" → chstt=true 或 error_flag=false\n"
            u8"- \"超时\" → get_anomalies anomaly_type=timeout\n"
            u8"- \"异常\"、\"问题\" → get_anomalies anomaly_type=all\n"
            u8"- \"最多\"、\"最频繁\"、\"最活跃\" → get_top_n\n"
            u8"- \"占比\"、\"比例\"、\"分布\" → generate_chart chart_type=pie\n"
            u8"- \"趋势\"、\"变化\"、\"波动\" → generate_chart chart_type=line subject=time\n"
            u8"- \"对比\"、\"比较\" → generate_chart chart_type=bar\n"
            u8"- \"交互\"、\"流向\"、\"时序\" → generate_gantt\n"
            u8"- \"除了\"、\"不要\"、\"排除\" → exclude_terminal 或 exclude_message_type\n"
            u8"- \"加上\"、\"再筛选\"、\"进一步\" → add_filter\n"
            u8"- \"全部\"、\"所有\"、\"恢复\" → clear_filter\n"
            u8"- \"报告\"、\"分析报告\" → generate_report\n"
            u8"- \"Word\"、\"文档\"、\"DOCX\" → generate_report format=docx\n"
            u8"- \"PDF\" → generate_report format=pdf\n"
            u8"- \"时间间隔\"、\"周期\"、\"发送间隔\"、\"稳定性\" → analyze_time_interval\n\n"
            u8"## 输出格式要求\n\n"
            u8"你必须严格按照以下JSON格式输出工具调用，不要输出其他任何文字：\n\n"
            "```json\n"
            u8"{\"name\": \"工具名称\", \"arguments\": {参数对象}}\n"
            "```\n\n"
            u8"## 多条件组合查询规则\n\n"
            u8"### AND逻辑（同时满足多个条件）\n"
            u8"当用户说\"且\"、\"并且\"、\"同时\"时，多个条件放在同一个参数对象中：\n"
            u8"用户：显示终端地址为31且子地址为7的数据\n"
            u8"输出：{\"name\": \"query_data\", \"arguments\": {\"terminal_address\": [31], \"sub_address\": [7]}}\n\n"
            u8"### OR逻辑（满足任一条件）\n"
            u8"当用户说\"或\"、\"或者\"时，将多个值放在同一个数组中：\n"
            u8"用户：显示终端地址为5或10的数据\n"
            u8"输出：{\"name\": \"query_data\", \"arguments\": {\"terminal_address\": [5, 10]}}\n\n"
            u8"### 范围查询\n"
            u8"当用户说\"X到Y\"、\"X至Y\"时，需要展开为包含所有值的数组：\n"
            u8"用户：显示子地址为7到10的数据\n"
            u8"输出：{\"name\": \"query_data\", \"arguments\": {\"sub_address\": [7, 8, 9, 10]}}\n\n"
            u8"## 注意事项\n"
            u8"- 只输出JSON，不要输出任何解释性文字\n"
            u8"- terminal_address和sub_address必须是数组格式\n"
            u8"- 消息类型必须是BC_TO_RT、RT_TO_BC、RT_TO_RT、Broadcast之一\n"
            u8"- 范围查询时必须展开为完整数组，不要使用范围表达式\n"
            u8"- 一次只调用一个工具\n"
            u8"- 用户说\"排除\"、\"不要\"时使用exclude_terminal或exclude_message_type\n"
            u8"- 用户说\"忙\"、\"Busy\"时使用status_bit参数，bit=1\n"
            u8"- 用户说\"失败\"时使用chstt=false\n"
            u8"- 用户说\"超时\"时使用get_anomalies工具\n"
        );
    }
    
    static QString getSystemPromptForChat()
    {
        return QString(
            u8"你是一个专业的1553B总线数据分析助手，可以帮助用户理解1553B协议、"
            u8"解释数据结构、分析数据特征、提供技术建议等。\n\n"
            u8"## 1553B协议知识库\n\n"
            u8"### 命令字格式（16位）\n"
            u8"| 位 | 含义 |\n"
            "|---|---|\n"
            u8"| 15-11 | 终端地址(0-31) |\n"
            u8"| 10 | T/R位(0=接收,1=发送) |\n"
            u8"| 9-5 | 子地址(0-31) |\n"
            u8"| 4-0 | 数据字计数/方式码(0-31) |\n\n"
            u8"### 状态字格式（16位）\n"
            u8"| 位 | 含义 |\n"
            "|---|---|\n"
            u8"| 15-11 | 终端地址(0-31) |\n"
            u8"| 10 | 报文错误 |\n"
            u8"| 9 | 测试标志 |\n"
            u8"| 8 | 服务请求 |\n"
            u8"| 7 | 保留 |\n"
            u8"| 6 | 广播命令接收 |\n"
            u8"| 5 | 忙(Busy) |\n"
            u8"| 4 | 子系统故障 |\n"
            u8"| 3 | 动态总线控制接受 |\n"
            u8"| 2 | 终端标志 |\n"
            u8"| 1-0 | 保留 |\n\n"
            u8"### 消息类型\n"
            u8"- BC→RT: 总线控制器发送到远程终端\n"
            u8"- RT→BC: 远程终端发送到总线控制器\n"
            u8"- RT→RT: 远程终端之间直接通信\n"
            u8"- Broadcast: 广播消息，地址31\n\n"
            u8"### 数据结构\n"
            u8"- SMbiMonPacketHeader: 包头(标识、任务机ID、包长度、日期、时间戳)\n"
            u8"- SMbiMonPacketData: 数据(命令字1/2、状态字1/2、通道状态、时间戳、数据内容)\n"
            u8"- CMD: 命令字解析(终端地址、子地址、T/R位、数据计数)\n\n"
            u8"### 常见子地址含义\n"
            u8"- SA0/SA31: 方式命令\n"
            u8"- SA1-SA30: 数据子地址，具体含义取决于系统ICD文件\n"
            u8"- SA19: 在某些系统中常用于传感器数据\n\n"
            u8"你可以回答关于：\n"
            u8"- 1553B协议规范和技术细节\n"
            u8"- 数据结构解释（SMbiMonPacketHeader、SMbiMonPacketData、CMD等）\n"
            u8"- 数据分析方法和技术\n"
            u8"- 常见问题和解决方案\n\n"
            u8"请注意：\n"
            u8"- 如果用户想要查询具体数据或生成图表，请建议用户切换到'智能分析模式'\n"
            u8"- 用专业但易懂的语言回答问题\n"
            u8"- 可以提供位定义图和代码片段帮助理解\n"
        );
    }
};

#endif
