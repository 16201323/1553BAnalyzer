/**
 * @file CsvExporter.cpp
 * @brief CSV格式导出工具实现
 *
 * 提供CSV字段的转义和格式化功能：
 * - 包含逗号、双引号或换行符的字段需要用双引号包裹
 * - 字段内的双引号需要转义为两个双引号
 *
 * 符合RFC 4180 CSV标准。
 *
 * @author 1553BTools
 * @date 2024
 */

#include "CsvExporter.h"

/**
 * @brief 转义CSV字段
 * @param field 原始字段值
 * @return 转义后的字段值
 *
 * 转义规则（RFC 4180）：
 * - 如果字段包含逗号、双引号或换行符，用双引号包裹整个字段
 * - 字段内的双引号替换为两个双引号
 */
QString CsvExporter::escapeField(const QString& field)
{
    if (field.contains(',') || field.contains('"') || field.contains('\n')) {
        QString escaped = field;
        escaped.replace("\"", "\"\"");
        return QString("\"%1\"").arg(escaped);
    }
    return field;
}

QString CsvExporter::formatRow(const QStringList& fields)
{
    QStringList escapedFields;
    for (const QString& field : fields) {
        escapedFields.append(escapeField(field));
    }
    return escapedFields.join(',');
}
