/**
 * @file PacketStruct.h
 * @brief 1553B数据包结构定义
 * 
 * 本文件定义了1553B总线数据包的所有数据结构，包括：
 * - CMD命令字结构（位域定义）
 * - 数据包头部结构
 * - 数据包数据部分结构
 * - 完整消息结构
 * 
 * 数据结构说明：
 * - 所有结构使用#pragma pack(1)确保字节对齐
 * - 支持大端和小端字节序
 * - 时间戳单位为40微秒
 * 
 * 数据包格式：
 * @code
 * +----------------+----------------+------------------+
 * | 包头 (14字节)  | 数据包1        | 数据包2 ...      |
 * +----------------+----------------+------------------+
 * 
 * 包头结构：
 * +--------+--------+------------+----------+------+-----+-----+-----------+
 * | header1| header2| mpuProduceId| packetLen| year |month| day | timestamp |
 * | 2字节  | 1字节  | 1字节      | 2字节    | 2字节|1字节|1字节| 4字节     |
 * +--------+--------+------------+----------+------+-----+-----+-----------+
 * 
 * 数据包结构：
 * +--------+------+------+--------+--------+-------+-----------+-----------+
 * | header | cmd1 | cmd2 | states1| states2| chstt | timestamp | datas     |
 * | 2字节  | 2字节 | 2字节| 2字节  | 2字节  | 2字节 | 4字节     | 变长      |
 * +--------+------+------+--------+--------+-------+-----------+-----------+
 * @endcode
 * 
 * @author 1553BTools
 * @date 2024
 */

#ifndef PACKETSTRUCT_H
#define PACKETSTRUCT_H

#include <QtGlobal>
#include <QVector>
#include <QByteArray>

/**
 * @brief 使用1字节对齐
 * 
 * 确保结构体在内存中紧凑排列，不进行填充
 */
#pragma pack(push, 1)

/**
 * @brief CMD命令字结构（位域定义）
 * 
 * 1553B命令字的位域解析，用于提取命令字中的各个字段。
 * 命令字为16位，各字段定义如下：
 * 
 * | 位范围 | 字段名 | 说明 |
 * |--------|--------|------|
 * | 4-0 | sjzjs_fsdm | 数据计数/发送码，范围0-31 |
 * | 9-5 | zidizhi | 子地址，范围0-31 |
 * | 10 | T_R | 收发状态，0=接收，1=发送 |
 * | 15-11 | zhongduandizhi | 终端地址，范围0-31 |
 * 
 * 使用示例：
 * @code
 * quint16 cmdWord = 0x1843;
 * CMD cmd;
 * memcpy(&cmd, &cmdWord, sizeof(CMD));
 * int terminal = cmd.zhongduandizhi;  // 终端地址
 * int subAddr = cmd.zidizhi;          // 子地址
 * int dataCount = cmd.sjzjs_fsdm;     // 数据计数
 * bool isSend = cmd.T_R;              // 是否发送
 * @endcode
 */
struct CMD {
    quint16 sjzjs_fsdm : 5;      ///< 数据计数/发送码（bit4-0），范围0-31
    quint16 zidizhi : 5;         ///< 子地址（bit9-5），范围0-31，0和31为特殊地址
    quint16 T_R : 1;             ///< 收发状态（bit10），0=BC→RT，1=RT→BC
    quint16 zhongduandizhi : 5;  ///< 终端地址（bit15-11），范围0-31，31为广播地址
};

/**
 * @brief 1553B数据包头部结构
 * 
 * 每个数据包的固定头部，包含标识、长度和时间信息。
 * 总大小：14字节
 * 
 * 字段说明：
 * - header1：固定标识，用于识别数据包起始位置
 * - header2：辅助标识，与header1共同验证数据包有效性
 * - mpuProduceId：任务机标识，区分不同的数据来源
 * - packetLen：整个数据包的长度（含头部）
 * - year/month/day：数据采集日期
 * - timestamp：数据采集时间，单位为40微秒
 */
struct SMbiMonPacketHeader {
    quint16 header1;        ///< 固定值：0xA5A5（2字节），包头标识1
    quint8 header2;         ///< 固定值：0xA5（1字节），包头标识2
    quint8 mpuProduceId;    ///< 任务机标识（1字节）：1=MPU1, 2=MPU2
    quint16 packetLen;      ///< 包总长度（2字节），包含头部和数据
    quint16 year;           ///< 年份（2字节）
    quint8 month;           ///< 月份（1字节），范围1-12
    quint8 day;             ///< 日期（1字节），范围1-31
    quint32 timestamp;      ///< 时间戳（4字节），单位40微秒
};

/**
 * @brief 1553B数据包数据部分结构
 * 
 * 存储单条1553B消息的所有信息，包括命令字、状态字和数据内容。
 * 固定部分大小：16字节（不含datas变长字段）
 * 
 * 字段说明：
 * - header：数据头标识，用于识别数据记录起始
 * - cmd1/cmd2：命令字，包含终端地址、子地址等信息
 * - states1/states2：状态字，包含RT响应状态
 * - chstt：通道状态，指示传输成功或失败
 * - timestamp：时间戳，单位为40微秒
 * - datas：数据内容，长度由cmd1计算得出
 */
struct SMbiMonPacketData {
    quint16 header;         ///< 固定值：0xAABB（2字节），数据头标识
    quint16 cmd1;           ///< 命令字1（2字节），包含终端地址、子地址等
    quint16 cmd2;           ///< 命令字2（2字节），RT→RT时使用
    quint16 states1;        ///< 状态字1（2字节），RT响应状态
    quint16 states2;        ///< 状态字2（2字节），RT响应状态
    quint16 chstt;          ///< 通道状态（2字节），非0表示成功，0表示失败
    quint32 timestamp;      ///< 时间戳（4字节），单位40微秒
    QByteArray datas;       ///< 数据内容（变长），长度由calculateDatasLength计算
};

/**
 * @brief 1553B完整消息结构
 * 
 * 一个完整的1553B消息，包含一个包头和多个数据包。
 * 通常一个消息对应一次数据采集周期的所有数据。
 */
struct SMbiMonPacketMsg {
    SMbiMonPacketHeader header;                 ///< 消息头部
    QVector<SMbiMonPacketData> packetDatas;     ///< 数据包数组
};

/**
 * @brief 恢复默认字节对齐
 */
#pragma pack(pop)

/**
 * @brief SMbiMonPacketData固定部分大小（不含datas变长字段）
 * 
 * 用于计算数据包在文件中的最小长度
 */
constexpr int SMbiMonPacketDataFixedPartSize = 16;

/**
 * @brief 消息类型枚举
 * 
 * 定义1553B总线的四种基本消息类型
 */
enum class MessageType {
    BC_TO_RT,    ///< BC到RT传输：总线控制器向远程终端发送数据
    RT_TO_BC,    ///< RT到BC传输：远程终端向总线控制器发送数据
    RT_TO_RT,    ///< RT到RT传输：远程终端之间的数据传输
    Broadcast,   ///< 广播消息：总线控制器向所有终端广播数据
    Unknown      ///< 未知类型：无法识别的消息类型
};

/**
 * @brief 检测消息类型
 * 
 * 根据命令字分析消息类型，判断数据传输方向。
 * 
 * 判断规则：
 * 1. 终端地址为31时，为广播消息
 * 2. cmd1.T_R=0且cmd2无效时，为BC→RT
 * 3. cmd1.T_R=1时，为RT→BC
 * 4. cmd1和cmd2都有效且终端地址不同时，为RT→RT
 * 
 * @param data 数据包引用
 * @return 消息类型枚举值
 */
inline MessageType detectMessageType(const SMbiMonPacketData& data) {
    CMD cmd1;
    CMD cmd2;
    memcpy(&cmd1, &data.cmd1, sizeof(CMD));
    memcpy(&cmd2, &data.cmd2, sizeof(CMD));
    
    if (cmd1.zhongduandizhi == 31 && cmd1.T_R == 0) {
        return MessageType::Broadcast;
    }
    
    bool cmd1Valid = (cmd1.zhongduandizhi != 0 || cmd1.zidizhi != 0);
    bool cmd2Valid = (cmd2.zhongduandizhi != 0 || cmd2.zidizhi != 0);
    
    if (cmd1.T_R == 0 && !cmd2Valid) {
        return MessageType::BC_TO_RT;
    }
    
    if (cmd1.T_R == 1) {
        return MessageType::RT_TO_BC;
    }
    
    if (cmd1Valid && cmd2Valid && 
        cmd1.zhongduandizhi != cmd2.zhongduandizhi) {
        return MessageType::RT_TO_RT;
    }
    
    return MessageType::Unknown;
}

/**
 * @brief 消息类型转字符串
 * 
 * 将消息类型枚举转换为可读的字符串表示
 * 
 * @param type 消息类型枚举值
 * @return 类型字符串，如"BC→RT"、"RT→BC"等
 */
inline QString messageTypeToString(MessageType type) {
    switch (type) {
    case MessageType::BC_TO_RT: return QStringLiteral("BC→RT");
    case MessageType::RT_TO_BC: return QStringLiteral("RT→BC");
    case MessageType::RT_TO_RT: return QStringLiteral("RT→RT");
    case MessageType::Broadcast: return QStringLiteral("Broadcast");
    default: return QStringLiteral("Unknown");
    }
}

/**
 * @brief 计算datas字段长度
 * 
 * 根据cmd1命令字计算数据内容的字节数。
 * 
 * 计算规则：
 * - 当子地址为0或31（模式命令/广播）时，长度固定为2字节
 * - 当数据计数为0时，长度固定为64字节
 * - 其他情况下，长度为数据计数×2字节
 * 
 * @param cmd1Raw cmd1原始值（16位无符号整数）
 * @return datas长度（字节数）
 * 
 * @note 1553B总线每个数据字为16位（2字节）
 */
inline int calculateDatasLength(quint16 cmd1Raw) {
    quint8 dataCount = cmd1Raw & 0x1F;
    quint8 subAddr = (cmd1Raw >> 5) & 0x1F;
    
    if (subAddr == 0 || subAddr == 31) {
        return 2;
    }
    if (dataCount == 0) {
        return 64;
    }
    return dataCount * 2;
}

inline quint8 cmdTerminalAddr(quint16 cmd1Raw) { return (cmd1Raw >> 11) & 0x1F; }  ///< 从cmd1原始值提取终端地址（位11-15）
inline quint8 cmdSubAddr(quint16 cmd1Raw) { return (cmd1Raw >> 5) & 0x1F; }       ///< 从cmd1原始值提取子地址（位5-9）
inline quint8 cmdTR(quint16 cmd1Raw) { return (cmd1Raw >> 10) & 0x1; }            ///< 从cmd1原始值提取收发标志（位10），0=接收，1=发送
inline quint8 cmdDataCount(quint16 cmd1Raw) { return cmd1Raw & 0x1F; }            ///< 从cmd1原始值提取数据计数/发送码（位0-4）

#endif
