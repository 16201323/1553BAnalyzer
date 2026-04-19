/**
 * @file BinaryParser.h
 * @brief 二进制文件解析器类定义
 * 
 * BinaryParser类负责解析1553B二进制数据文件，将二进制数据
 * 转换为SMbiMonPacketMsg结构数组供上层使用。
 * 
 * 主要功能：
 * - 支持大端/小端字节序配置
 * - 支持自定义包头标识
 * - 支持错误容差配置
 * - 提供解析进度反馈
 * 
 * 使用示例：
 * @code
 * BinaryParser parser;
 * parser.setByteOrder(false);  // 大端序
 * parser.setHeader1(0xA5A5);
 * parser.setHeader2(0xA5);
 * parser.setDataHeader(0xAABB);
 * 
 * if (parser.parseFile("data.bin")) {
 *     QVector<PacketParser::SMbiMonPacketMsg> data = parser.getParsedData();
 *     // 处理解析后的数据
 * }
 * @endcode
 * 
 * @author 1553BTools
 * @date 2024
 */

#ifndef BINARYPARSER_H
#define BINARYPARSER_H

#include <QObject>
#include <QFile>
#include <QByteArray>
#include <atomic>
#include "PacketStruct.h"

/**
 * @brief 二进制文件解析器类
 * 
 * 该类负责将1553B二进制数据文件解析为结构化的消息数组。
 * 支持可配置的字节序、包头标识等参数，并通过信号机制
 * 报告解析进度和结果。
 */
class BinaryParser : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象指针
     * 
     * 初始化默认解析参数：
     * - 字节序：小端序
     * - 包头1：0xA5A5
     * - 包头2：0xA5
     * - 数据头：0xAABB
     */
    explicit BinaryParser(QObject *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~BinaryParser();
    
    /**
     * @brief 设置字节序
     * @param littleEndian true为小端序，false为大端序
     * 
     * 字节序决定了多字节数据的存储顺序：
     * - 小端序：低字节在前，高字节在后
     * - 大端序：高字节在前，低字节在后
     */
    Q_INVOKABLE void setByteOrder(bool littleEndian);
    
    /**
     * @brief 设置包头1标识
     * @param value 包头1的值（2字节）
     * 
     * 包头1用于识别数据包的起始位置
     */
    Q_INVOKABLE void setHeader1(quint16 value);
    
    /**
     * @brief 设置包头2标识
     * @param value 包头2的值（实际使用1字节）
     * 
     * 包头2是包头1之后的第二个标识字节
     */
    Q_INVOKABLE void setHeader2(quint16 value);
    
    /**
     * @brief 设置数据头标识
     * @param value 数据头的值（2字节）
     * 
     * 数据头用于识别每条数据记录的起始位置
     */
    Q_INVOKABLE void setDataHeader(quint16 value);
    
    /**
     * @brief 设置最大错误容差
     * @param count 允许的最大连续解析错误数
     * 
     * 当连续解析错误次数超过此值时，解析将终止。
     * 设置为0表示遇到任何错误立即终止。
     */
    Q_INVOKABLE void setMaxErrorTolerance(int count);
    
    /**
     * @brief 解析二进制文件
     * @param filePath 要解析的文件路径
     * @return 解析成功返回true，失败返回false
     * 
     * 解析流程：
     * 1. 打开文件并验证
     * 2. 设置数据流字节序
     * 3. 循环读取包头和数据
     * 4. 发送进度信号
     * 5. 完成后发送完成信号
     * 
     * @see parseProgress  解析进度信号
     * @see parseFinished  解析完成信号
     * @see parseError     解析错误信号
     */
    Q_INVOKABLE bool parseFile(const QString& filePath);
    
    /**
     * @brief 获取解析后的数据
     * @return 解析后的消息列表
     * 
     * 返回所有成功解析的SMbiMonPacketMsg消息
     */
    Q_INVOKABLE QVector<PacketParser::SMbiMonPacketMsg> getParsedData() const;
    
    /**
     * @brief 获取最后的错误信息
     * @return 错误信息字符串，无错误时返回空字符串
     */
    Q_INVOKABLE QString getLastError() const;
    
    /**
     * @brief 获取解析的消息数量
     * @return 成功解析的消息数量
     */
    int getParsedCount() const;
    
    /**
     * @brief 取消解析操作
     * 
     * 设置取消标志，解析循环会在下一次迭代时检测并退出。
     * 这是一个线程安全的操作。
     */
    Q_INVOKABLE void cancel();
    
    /**
     * @brief 检查是否已取消
     * @return true表示已取消，false表示未取消
     */
    bool isCanceled() const;
    
signals:
    /**
     * @brief 解析进度信号
     * @param current 当前已解析的字节数
     * @param total 文件总字节数
     * 
     * 在解析过程中定期发送，用于更新进度条
     */
    void parseProgress(int current, int total);
    
    /**
     * @brief 解析完成信号
     * @param success 是否成功
     * @param count 解析的消息数量
     * 
     * 解析结束后发送，无论成功或失败
     */
    void parseFinished(bool success, int count);
    
    /**
     * @brief 解析错误信号
     * @param error 错误描述信息
     * 
     * 发生严重错误时发送
     */
    void parseError(const QString& error);

private:
    /**
     * @brief 解析包头
     * @param stream 数据流
     * @param header 输出的包头结构引用
     * @return 解析成功返回true，失败返回false
     * 
     * 包头结构（14字节）：
     * - header1: 2字节，包头标识1
     * - header2: 1字节，包头标识2
     * - mpuProduceId: 1字节，MPU生产ID
     * - packetLen: 2字节，数据包总长度
     * - year: 2字节，年份
     * - month: 1字节，月份
     * - day: 1字节，日期
     * - timestamp: 4字节，时间戳
     */
    bool parseHeader(QDataStream& stream, PacketParser::SMbiMonPacketHeader& header);
    
    /**
     * @brief 解析数据包
     * @param stream 数据流
     * @param data 输出的数据结构引用
     * @param remainingLen 剩余可读长度
     * @param bytesRead 输出参数，实际读取的字节数
     * @return 解析成功返回true，失败返回false
     * 
     * 数据包结构：
     * - header: 2字节，数据头标识
     * - cmd1: 2字节，命令字1
     * - cmd2: 2字节，命令字2
     * - states1: 2字节，状态字1
     * - states2: 2字节，状态字2
     * - chstt: 2字节，通道状态
     * - timestamp: 4字节，时间戳
     * - datas: 变长，数据内容
     */
    bool parsePacketData(QDataStream& stream, PacketParser::SMbiMonPacketData& data, int remainingLen, int& bytesRead);
    
    /**
     * @brief 读取16位无符号整数
     * @param stream 数据流
     * @return 读取的值
     * 
     * 根据当前字节序设置读取数据
     */
    quint16 readUint16(QDataStream& stream);
    
    /**
     * @brief 读取32位无符号整数
     * @param stream 数据流
     * @return 读取的值
     * 
     * 根据当前字节序设置读取数据
     */
    quint32 readUint32(QDataStream& stream);
    
    /**
     * @brief 读取8位无符号整数
     * @param stream 数据流
     * @return 读取的值
     */
    quint8 readUint8(QDataStream& stream);
    
    bool m_littleEndian;           // 字节序标志，true为小端序，false为大端序
    quint16 m_header1;             // 包头1标识值
    quint8 m_header2;              // 包头2标识值
    quint16 m_dataHeader;          // 数据头标识值
    int m_maxErrorTolerance;       // 最大错误容差次数
    
    std::atomic<bool> m_canceled;  // 取消标志，用于中断解析
    
    QVector<PacketParser::SMbiMonPacketMsg> m_parsedData;  // 解析后的消息数组
    QString m_lastError;           // 最后的错误信息
};

Q_DECLARE_METATYPE(QVector<PacketParser::SMbiMonPacketMsg>)

#endif
