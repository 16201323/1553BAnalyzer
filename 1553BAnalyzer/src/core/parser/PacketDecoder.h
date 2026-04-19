/**
 * @file PacketDecoder.h
 * @brief 1553B数据包解码器
 *
 * PacketDecoder提供一组静态方法，用于将1553B协议的原始二进制数据
 * 解码为人类可读的字符串表示。
 *
 * 1553B协议核心概念：
 * - BC (Bus Controller): 总线控制器，发起所有通信
 * - RT (Remote Terminal): 远程终端，响应BC的命令
 * - 命令字: BC发出的控制指令，包含终端地址、子地址、数据计数等
 * - 状态字: RT返回的响应状态，包含终端地址和状态标志位
 *
 * 使用示例：
 * @code
 * CMD cmd;
 * memcpy(&cmd, &rawCmdWord, sizeof(CMD));
 *
 * // 解码命令字为可读字符串
 * QString cmdInfo = PacketDecoder::decodeCmd1(cmd);
 * // 输出: "RT5 SA3 RX WC=10" (终端5, 子地址3, 接收, 字计数10)
 *
 * // 获取特定字段
 * int rtAddr = PacketDecoder::getTerminalAddress(cmd);  // 5
 * int subAddr = PacketDecoder::getSubAddress(cmd);       // 3
 * @endcode
 *
 * @author 1553BTools
 * @date 2024
 */

#ifndef PACKETDECODER_H
#define PACKETDECODER_H

#include "PacketStruct.h"
#include <QString>

/**
 * @brief 1553B数据包解码工具类
 *
 * 所有方法均为静态方法，无需创建实例即可使用。
 * 主要提供两类功能：
 * 1. 解码为可读字符串（decodeXxx系列）
 * 2. 提取特定字段值（getXxx系列）
 */
class PacketDecoder
{
public:
    /**
     * @brief 解码命令字1（主命令字）
     * @param cmd CMD结构引用
     * @return 可读字符串，格式如 "RT5 SA3 RX WC=10"
     *
     * 输出格式说明：
     * - RTn: 终端地址 (0-31)
     * - SAn: 子地址 (0-31)
     * - TX/RX: 发送/接收方向
     * - WC=n: 数据字计数 (0-31, 0表示32个字)
     */
    static QString decodeCmd1(const CMD& cmd);

    /**
     * @brief 解码命令字2（副命令字，仅RT→RT模式使用）
     * @param cmd CMD结构引用
     * @return 可读字符串，格式同decodeCmd1
     */
    static QString decodeCmd2(const CMD& cmd);

    /**
     * @brief 解码状态字
     * @param status 16位状态字原始值
     * @return 可读字符串，包含各标志位状态
     *
     * 状态字标志位（bit位置）：
     * - bit0-4: 终端地址
     * - bit5-15: 各种错误/状态标志
     */
    static QString decodeStatusWord(quint16 status);

    /**
     * @brief 解码时间戳为可读时间字符串
     * @param timestamp 1553B时间戳（单位：40微秒）
     * @return 格式化的时间字符串，如 "01:23:45.678"
     */
    static QString decodeTimestamp(quint32 timestamp);

    /**
     * @brief 将数据内容转换为十六进制字符串
     * @param data 原始数据字节
     * @return 十六进制字符串，每字节用空格分隔，如 "A5 3C 00 FF"
     */
    static QString decodeDataToHex(const QByteArray& data);

    /**
     * @brief 将数据内容转换为十进制字符串
     * @param data 原始数据字节（按16位字解析）
     * @return 十进制字符串，每字用空格分隔，如 "165 60 0 255"
     */
    static QString decodeDataToDecimal(const QByteArray& data);

    /**
     * @brief 获取终端地址
     * @param cmd CMD结构引用
     * @return 终端地址 (0-31)，31为广播地址
     */
    static int getTerminalAddress(const CMD& cmd);

    /**
     * @brief 获取子地址
     * @param cmd CMD结构引用
     * @return 子地址 (0-31)，0和31为模式命令地址
     */
    static int getSubAddress(const CMD& cmd);

    /**
     * @brief 获取数据字计数
     * @param cmd CMD结构引用
     * @return 数据字计数 (0-31)，0表示32个字
     */
    static int getDataCount(const CMD& cmd);

    /**
     * @brief 判断是否为发送命令
     * @param cmd CMD结构引用
     * @return true=发送(TX), false=接收(RX)
     */
    static bool isTransmit(const CMD& cmd);

    /**
     * @brief 判断是否为模式命令
     * @param cmd CMD结构引用
     * @return true=模式命令（子地址为0或31），false=数据命令
     *
     * 模式命令用于BC与RT之间的控制交互，不传输数据
     */
    static bool isModeCode(const CMD& cmd);
};

#endif
