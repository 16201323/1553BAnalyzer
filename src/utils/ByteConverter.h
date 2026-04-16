/**
 * @file ByteConverter.h
 * @brief 字节转换工具类
 *
 * ByteConverter类提供二进制数据与可读格式之间的转换功能，
 * 包括十六进制显示、字节序交换和文件大小格式化。
 *
 * 所有方法均为静态方法，无需创建实例。
 *
 * 使用示例：
 * @code
 * QByteArray data = "\xA5\x3C\x00\xFF";
 * QString hex = ByteConverter::toHexString(data);
 * // 输出: "A5 3C 00 FF"
 *
 * quint16 swapped = ByteConverter::swapBytes(0x1234);
 * // 输出: 0x3412
 *
 * QString size = ByteConverter::formatSize(1536);
 * // 输出: "1.5 KB"
 * @endcode
 *
 * @author 1553BTools
 * @date 2024
 */

#ifndef BYTECONVERTER_H
#define BYTECONVERTER_H

#include <QByteArray>
#include <QString>

/**
 * @brief 字节转换工具类
 *
 * 提供常用的字节操作和格式化静态方法，
 * 主要用于1553B数据的显示和调试。
 */
class ByteConverter
{
public:
    /**
     * @brief 将字节数组转换为十六进制字符串
     * @param data 原始字节数组
     * @param sep 字节之间的分隔符（默认空格）
     * @return 十六进制字符串，如 "A5 3C 00 FF"
     */
    static QString toHexString(const QByteArray& data, char sep = ' ');

    /**
     * @brief 将十六进制字符串转换为字节数组
     * @param hexStr 十六进制字符串（支持空格分隔或连续格式）
     * @return 解析后的字节数组
     */
    static QByteArray fromHexString(const QString& hexStr);

    /**
     * @brief 交换16位整数的字节序（大端↔小端）
     * @param value 原始16位值
     * @return 字节序交换后的值
     *
     * 例如：0x1234 → 0x3412
     */
    static quint16 swapBytes(quint16 value);

    /**
     * @brief 交换32位整数的字节序（大端↔小端）
     * @param value 原始32位值
     * @return 字节序交换后的值
     *
     * 例如：0x12345678 → 0x78563412
     */
    static quint32 swapBytes(quint32 value);

    /**
     * @brief 格式化文件大小为人类可读字符串
     * @param bytes 字节数
     * @return 格式化后的大小字符串，如 "1.5 KB"、"2.3 MB"
     *
     * 自动选择合适的单位：B、KB、MB、GB、TB
     */
    static QString formatSize(qint64 bytes);
};

#endif
