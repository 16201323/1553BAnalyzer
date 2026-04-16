/**
 * @file TimeConverter.h
 * @brief 时间转换工具类
 *
 * TimeConverter类提供1553B时间戳与可读时间格式之间的转换功能。
 *
 * 1553B时间戳说明：
 * - 原始时间戳为32位无符号整数
 * - 默认单位为40微秒（1553B总线标准时间分辨率）
 * - 可配置为其他时间单位
 * - 最大表示时间：40us × 2^32 ≈ 47.7小时
 *
 * 所有方法均为静态方法，无需创建实例。
 *
 * 使用示例：
 * @code
 * quint32 rawTs = 125000;  // 原始时间戳
 * QString time = TimeConverter::timestampToTimeString(rawTs);
 * // 输出: "00:00:05.000" (125000 × 40us = 5秒)
 *
 * double ms = TimeConverter::timestampToMilliseconds(rawTs);
 * // 输出: 5000.0
 * @endcode
 *
 * @author 1553BTools
 * @date 2024
 */

#ifndef TIMECONVERTER_H
#define TIMECONVERTER_H

#include <QString>
#include <QDateTime>

/**
 * @brief 时间转换工具类
 *
 * 提供时间戳转换和格式化的静态方法，
 * 专门处理1553B协议的时间戳格式。
 */
class TimeConverter
{
public:
    /**
     * @brief 将原始时间戳转换为可读时间字符串
     * @param timestamp 原始时间戳值
     * @param unit 时间戳单位（微秒），默认40us
     * @return 格式化的时间字符串，如 "01:23:45.678"
     *
     * 输出格式：HH:MM:SS.mmm（时:分:秒.毫秒）
     */
    static QString timestampToTimeString(quint32 timestamp, int unit = 40);

    /**
     * @brief 将原始时间戳转换为毫秒数
     * @param timestamp 原始时间戳值
     * @param unit 时间戳单位（微秒），默认40us
     * @return 对应的毫秒数
     *
     * 计算公式：timestamp × unit / 1000
     */
    static double timestampToMilliseconds(quint32 timestamp, int unit = 40);

    /**
     * @brief 格式化两个时间戳之间的持续时间
     * @param startTs 起始时间戳
     * @param endTs 结束时间戳
     * @param unit 时间戳单位（微秒），默认40us
     * @return 格式化的持续时间字符串，如 "1分23秒"
     *
     * 自动选择合适的时间单位（秒/分/时）
     */
    static QString formatDuration(quint32 startTs, quint32 endTs, int unit = 40);
};

#endif
