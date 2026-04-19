/**
 * @file CsvExporter.h
 * @brief CSV格式导出工具类
 *
 * CsvExporter类提供CSV格式的字段转义和行格式化功能。
 * 所有方法均为静态方法，无需创建实例。
 *
 * CSV格式规范（RFC 4180）：
 * - 字段之间用逗号分隔
 * - 包含逗号、双引号或换行符的字段需要用双引号包围
 * - 字段内的双引号需要转义为两个双引号
 *
 * 使用示例：
 * @code
 * QStringList fields = {"RT5", "SA3", "数据内容,含逗号"};
 * QString row = CsvExporter::formatRow(fields);
 * // 输出: "RT5,SA3,\"数据内容,含逗号\""
 * @endcode
 *
 * @author 1553BTools
 * @date 2024
 */

#ifndef CSVEXPORTER_H
#define CSVEXPORTER_H

#include <QObject>
#include <QString>

/**
 * @brief CSV格式导出工具类
 *
 * 提供CSV字段转义和行格式化的静态工具方法，
 * 确保导出的CSV文件符合标准格式。
 */
class CsvExporter
{
public:
    /**
     * @brief 转义CSV字段
     * @param field 原始字段字符串
     * @return 转义后的字段字符串
     *
     * 转义规则：
     * - 纯数字和简单字符串：不转义
     * - 包含逗号、双引号或换行符：用双引号包围
     * - 字段内的双引号：替换为两个双引号
     */
    static QString escapeField(const QString& field);

    /**
     * @brief 格式化CSV行
     * @param fields 字段列表
     * @return 格式化后的CSV行字符串（不含行尾换行符）
     *
     * 对每个字段调用escapeField转义后用逗号连接
     */
    static QString formatRow(const QStringList& fields);
};

#endif
