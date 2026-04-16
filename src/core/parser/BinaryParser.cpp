/**
 * @file BinaryParser.cpp
 * @brief 二进制文件解析器实现
 * 
 * 该文件实现了1553B二进制数据文件的解析功能，包括：
 * - 文件头识别和验证
 * - 数据包解析
 * - 命令字解析
 * - 数据长度计算
 * 
 * @author 1553BTools
 * @date 2024
 */

#include "BinaryParser.h"
#include "PacketStruct.h"
#include <QDataStream>
#include <QDebug>
#include <QFileInfo>
#include "utils/Logger.h"

/**
 * @brief 构造函数，初始化默认解析参数
 * @param parent 父对象指针
 */
BinaryParser::BinaryParser(QObject *parent)
    : QObject(parent)
    , m_littleEndian(true)
    , m_header1(0xA5A5)
    , m_header2(0xA5)
    , m_dataHeader(0xAABB)
    , m_maxErrorTolerance(0)
{
    LOG_DEBUG("BinaryParser", "BinaryParser实例创建");
}

/**
 * @brief 析构函数
 */
BinaryParser::~BinaryParser()
{
    LOG_DEBUG("BinaryParser", "BinaryParser实例销毁");
}

/**
 * @brief 设置字节序
 * @param littleEndian true为小端序，false为大端序
 */
void BinaryParser::setByteOrder(bool littleEndian)
{
    m_littleEndian = littleEndian;
    LOG_DEBUG("BinaryParser", QString("字节序设置为: %1").arg(littleEndian ? "小端" : "大端"));
}

/**
 * @brief 设置包头1标识
 * @param value 包头1的值
 */
void BinaryParser::setHeader1(quint16 value)
{
    m_header1 = value;
    LOG_DEBUG("BinaryParser", QString("包头1设置为: 0x%1").arg(value, 4, 16, QChar('0')));
}

/**
 * @brief 设置包头2标识
 * @param value 包头2的值（1字节）
 */
void BinaryParser::setHeader2(quint16 value)
{
    m_header2 = value & 0xFF;
    LOG_DEBUG("BinaryParser", QString("包头2设置为: 0x%1").arg(m_header2, 2, 16, QChar('0')));
}

/**
 * @brief 设置数据头标识
 * @param value 数据头的值
 */
void BinaryParser::setDataHeader(quint16 value)
{
    m_dataHeader = value;
    LOG_DEBUG("BinaryParser", QString("数据头设置为: 0x%1").arg(value, 4, 16, QChar('0')));
}

/**
 * @brief 设置最大错误容差
 * @param count 允许的最大连续解析错误数
 */
void BinaryParser::setMaxErrorTolerance(int count)
{
    m_maxErrorTolerance = count;
    LOG_DEBUG("BinaryParser", QString("最大错误容差设置为: %1").arg(count));
}

/**
 * @brief 解析二进制文件
 * @param filePath 文件路径
 * @return 解析成功返回true，失败返回false
 * 
 * 解析流程：
 * 1. 打开文件并设置字节序
 * 2. 循环读取包头
 * 3. 根据包长度读取数据
 * 4. 解析每条数据记录
 */
bool BinaryParser::parseFile(const QString& filePath)
{
    LOG_INFO("BinaryParser", QString("开始解析文件: %1").arg(filePath));
    
    m_parsedData.clear();
    m_lastError.clear();
    
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        m_lastError = tr("文件不存在: %1").arg(filePath);
        LOG_ERROR("BinaryParser", m_lastError);
        emit parseError(m_lastError);
        emit parseFinished(false, 0);
        return false;
    }
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_lastError = tr("无法打开文件: %1").arg(filePath);
        LOG_ERROR("BinaryParser", m_lastError);
        emit parseError(m_lastError);
        emit parseFinished(false, 0);
        return false;
    }
    
    LOG_DEBUG("BinaryParser", QString("文件大小: %1 字节").arg(file.size()));
    
    QDataStream stream(&file);
    if (m_littleEndian) {
        stream.setByteOrder(QDataStream::LittleEndian);
    } else {
        stream.setByteOrder(QDataStream::BigEndian);
    }
    
    qint64 fileSize = file.size();
    int parsedCount = 0;
    int errorCount = 0;
    int totalDataCount = 0;
    qint64 lastProgressPos = 0;
    
    while (!stream.atEnd()) {
        qint64 currentPos = file.pos();
        
        if (currentPos - lastProgressPos > fileSize / 100) {
            emit parseProgress(static_cast<int>(currentPos), static_cast<int>(fileSize));
            lastProgressPos = currentPos;
        }
        
        SMbiMonPacketMsg msg;
        
        if (!parseHeader(stream, msg.header)) {
            errorCount++;
            
            if (errorCount > m_maxErrorTolerance) {
                m_lastError = tr("解析错误超过容差限制，已解析: %1 条消息").arg(parsedCount);
                LOG_WARNING("BinaryParser", m_lastError);
                break;
            }
            continue;
        }
        
        int dataLen = msg.header.packetLen - sizeof(SMbiMonPacketHeader);
        if (dataLen <= 0) {
            LOG_WARNING("BinaryParser", QString("无效的数据长度: %1, packetLen: %2")
                        .arg(dataLen)
                        .arg(msg.header.packetLen));
            errorCount++;
            continue;
        }
        
        int estimatedCount = dataLen / SMbiMonPacketDataFixedPartSize;
        if (estimatedCount == 0) {
            estimatedCount = 1;
        }
        
        // LOG_DEBUG("BinaryParser", QString("解析消息 #%1, 数据长度: %2, 预估数据条数: %3")
        //           .arg(parsedCount + 1)
        //           .arg(dataLen)
        //           .arg(estimatedCount));
        
        int bytesRead = 0;
        while (bytesRead < dataLen) {
            SMbiMonPacketData pktData;
            int pktSize = 0;
            if (parsePacketData(stream, pktData, dataLen - bytesRead, pktSize)) {
                msg.packetDatas.append(pktData);
                totalDataCount++;
                bytesRead += pktSize;
            } else {
                LOG_WARNING("BinaryParser", QString("数据包解析失败，已读取 %1 字节，跳过剩余 %2 字节")
                            .arg(bytesRead).arg(dataLen - bytesRead));
                stream.skipRawData(dataLen - bytesRead);
                bytesRead = dataLen;
                break;
            }
        }
        
        if (!msg.packetDatas.isEmpty()) {
            m_parsedData.append(msg);
            parsedCount++;
        }
        
        errorCount = 0;
    }
    
    file.close();
    
    LOG_INFO("BinaryParser", QString("文件解析完成，共解析 %1 条消息，%2 条数据记录")
              .arg(parsedCount)
              .arg(totalDataCount));
    
    emit parseProgress(static_cast<int>(fileSize), static_cast<int>(fileSize));
    emit parseFinished(true, parsedCount);
    return true;
}

/**
 * @brief 解析包头
 * @param stream 数据流
 * @param header 输出的包头结构
 * @return 解析成功返回true，失败返回false
 * 
 * 包头结构（14字节）：
 * - header1: 2字节
 * - header2: 1字节
 * - mpuProduceId: 1字节
 * - packetLen: 2字节
 * - year: 2字节
 * - month: 1字节
 * - day: 1字节
 * - timestamp: 4字节
 */
bool BinaryParser::parseHeader(QDataStream& stream, SMbiMonPacketHeader& header)
{
    if (stream.atEnd()) {
        return false;
    }
    
    qint64 startPos = stream.device()->pos();
    
    header.header1 = readUint16(stream);
    if (header.header1 != m_header1) {
        stream.device()->seek(startPos + 1);
        return false;
    }
    
    if (stream.atEnd()) {
        stream.device()->seek(startPos + 2);
        return false;
    }
    
    header.header2 = readUint8(stream);
    if (header.header2 != m_header2) {
        stream.device()->seek(startPos + 1);
        return false;
    }
    
    header.mpuProduceId = readUint8(stream);
    header.packetLen = readUint16(stream);
    header.year = readUint16(stream);
    header.month = readUint8(stream);
    header.day = readUint8(stream);
    header.timestamp = readUint32(stream);
    
    return true;
}

/**
 * @brief 解析数据包
 * @param stream 数据流
 * @param data 输出的数据结构
 * @param remainingLen 剩余可读长度
 * @param bytesRead 输出参数，实际读取的字节数
 * @return 解析成功返回true，失败返回false
 * 
 * 数据包固定部分结构（16字节）：
 * - header: 2字节
 * - cmd1: 2字节
 * - cmd2: 2字节
 * - states1: 2字节
 * - states2: 2字节
 * - chstt: 2字节
 * - timestamp: 4字节
 * - datas: 变长
 */
bool BinaryParser::parsePacketData(QDataStream& stream, SMbiMonPacketData& data, int remainingLen, int& bytesRead)
{
    bytesRead = 0;
    
    if (stream.atEnd() || remainingLen < SMbiMonPacketDataFixedPartSize) {
        return false;
    }
    
    data.header = readUint16(stream);
    bytesRead += 2;
    
    if (data.header != m_dataHeader) {
        return false;
    }
    
    data.cmd1 = readUint16(stream);
    data.cmd2 = readUint16(stream);
    bytesRead += 4;
    
    data.states1 = readUint16(stream);
    data.states2 = readUint16(stream);
    bytesRead += 4;
    
    data.chstt = readUint16(stream);
    bytesRead += 2;
    
    data.timestamp = readUint32(stream);
    bytesRead += 4;
    
    int datasLen = calculateDatasLength(data.cmd1);
    
    if (remainingLen - bytesRead < datasLen) {
        LOG_WARNING("BinaryParser", QString("剩余数据不足: 需要datas %1字节, 剩余%2字节")
                    .arg(datasLen)
                    .arg(remainingLen - bytesRead));
        return false;
    }
    
    data.datas.resize(datasLen);
    
    int readResult = stream.readRawData(data.datas.data(), datasLen);
    if (readResult != datasLen) {
        LOG_WARNING("BinaryParser", QString("数据读取不完整: 期望%1字节, 实际读取%2字节")
                    .arg(datasLen)
                    .arg(readResult));
        return false;
    }
    
    bytesRead += datasLen;
    
    return true;
}

/**
 * @brief 读取16位无符号整数
 * @param stream 数据流
 * @return 读取的值
 */
quint16 BinaryParser::readUint16(QDataStream& stream)
{
    quint16 value;
    stream >> value;
    return value;
}

/**
 * @brief 读取32位无符号整数
 * @param stream 数据流
 * @return 读取的值
 */
quint32 BinaryParser::readUint32(QDataStream& stream)
{
    quint32 value;
    stream >> value;
    return value;
}

/**
 * @brief 读取8位无符号整数
 * @param stream 数据流
 * @return 读取的值
 */
quint8 BinaryParser::readUint8(QDataStream& stream)
{
    quint8 value;
    stream >> value;
    return value;
}

/**
 * @brief 获取解析后的数据
 * @return 解析后的消息列表
 */
QVector<SMbiMonPacketMsg> BinaryParser::getParsedData() const
{
    return m_parsedData;
}

/**
 * @brief 获取最后的错误信息
 * @return 错误信息字符串
 */
QString BinaryParser::getLastError() const
{
    return m_lastError;
}

/**
 * @brief 获取解析的消息数量
 * @return 消息数量
 */
int BinaryParser::getParsedCount() const
{
    return m_parsedData.size();
}
