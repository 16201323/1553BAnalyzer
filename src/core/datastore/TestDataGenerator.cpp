#include "TestDataGenerator.h"
#include "core/parser/PacketStruct.h"
#include "utils/Logger.h"

#include <QFile>
#include <QDataStream>
#include <QDateTime>
#include <QRandomGenerator>
#include <QFileInfo>
#include <cstring>

TestDataGenerator::TestDataGenerator(QObject* parent)
    : QObject(parent)
{
}

quint16 TestDataGenerator::generateCmd1(int terminalAddr, int subAddr, bool isSend, int dataCount)
{
    CMD cmd;
    cmd.zhongduandizhi = terminalAddr;
    cmd.zidizhi = subAddr;
    cmd.T_R = isSend ? 1 : 0;
    cmd.sjzjs_fsdm = dataCount;

    quint16 result;
    memcpy(&result, &cmd, sizeof(quint16));
    return result;
}

quint16 TestDataGenerator::generateCmd2(int terminalAddr, int subAddr, bool isSend, int dataCount)
{
    return generateCmd1(terminalAddr, subAddr, isSend, dataCount);
}

int TestDataGenerator::calculateDatasLength(quint16 cmd1Raw)
{
    CMD cmd1;
    memcpy(&cmd1, &cmd1Raw, sizeof(CMD));

    if (cmd1.zidizhi == 0 || cmd1.zidizhi == 31) {
        return 2;
    }
    if (cmd1.sjzjs_fsdm == 0) {
        return 64;
    }
    return cmd1.sjzjs_fsdm * 2;
}

bool TestDataGenerator::generateTestDataFile(const QString& filename, int recordCount, const QString& byteOrder,
                                              const QDate& startDate, const QDate& endDate,
                                              const QTime& startTime, const QTime& endTime)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_ERROR("TestDataGenerator", QString("无法打开文件: %1").arg(filename));
        return false;
    }

    QDataStream out(&file);
    LOG_INFO("TestDataGenerator", "生成测试文件的大小端:" + byteOrder);
    out.setByteOrder(byteOrder == "big" ? QDataStream::BigEndian : QDataStream::LittleEndian);

    QDate useStartDate = startDate.isValid() ? startDate : QDate::currentDate();
    QDate useEndDate = endDate.isValid() ? endDate : useStartDate.addDays(1);
    QTime useStartTime = startTime.isValid() ? startTime : QTime(0, 0, 0);
    QTime useEndTime = endTime.isValid() ? endTime : QTime(23, 59, 59);

    QDateTime genStartDateTime(useStartDate, useStartTime);
    QDateTime genEndDateTime(useEndDate, useEndTime);
    qint64 totalMsRange = genStartDateTime.msecsTo(genEndDateTime);
    if (totalMsRange <= 0) {
        totalMsRange = 86400000;
    }

    int msgTypeCount[5] = {0, 0, 0, 0, 0};
    int statusTypeCount[4] = {0, 0, 0, 0};
    int totalPacketCount = 0;

    for (int i = 0; i < recordCount; i++) {
        SMbiMonPacketHeader header;
        header.header1 = 0xA5A5;
        header.header2 = 0xA5;
        header.mpuProduceId = (i % 2 == 0) ? 1 : 2;

        qint64 offsetMs = (totalMsRange * i) / recordCount;
        QDateTime recordTime = genStartDateTime.addMSecs(offsetMs);
        header.year = static_cast<quint16>(recordTime.date().year());
        header.month = static_cast<quint8>(recordTime.date().month());
        header.day = static_cast<quint8>(recordTime.date().day());

        qint64 totalMsOfDay = recordTime.time().msecsSinceStartOfDay();
        header.timestamp = static_cast<quint32>(totalMsOfDay * 25);

        int packetsInThisMsg = QRandomGenerator::global()->bounded(1, 6);
        QByteArray allPacketData;

        for (int p = 0; p < packetsInThisMsg; p++) {
            int msgType = (i + p) % 5;
            int terminalAddr1, terminalAddr2, subAddr1, subAddr2;
            bool isSend1;
            int dataCount1;
            quint16 cmd1, cmd2;
            bool hasCmd2 = false;

            terminalAddr2 = 0;
            subAddr2 = 0;

            switch (msgType) {
            case 0:
                msgTypeCount[0]++;
                terminalAddr1 = QRandomGenerator::global()->bounded(0, 31);
                subAddr1 = QRandomGenerator::global()->bounded(1, 31);
                isSend1 = false;
                dataCount1 = QRandomGenerator::global()->bounded(1, 32);
                cmd1 = generateCmd1(terminalAddr1, subAddr1, isSend1, dataCount1);
                cmd2 = 0;
                break;

            case 1:
                msgTypeCount[1]++;
                terminalAddr1 = QRandomGenerator::global()->bounded(0, 31);
                subAddr1 = QRandomGenerator::global()->bounded(1, 31);
                isSend1 = true;
                dataCount1 = QRandomGenerator::global()->bounded(1, 32);
                cmd1 = generateCmd1(terminalAddr1, subAddr1, isSend1, dataCount1);
                cmd2 = 0;
                break;

            case 2:
                msgTypeCount[2]++;
                terminalAddr1 = QRandomGenerator::global()->bounded(0, 31);
                terminalAddr2 = QRandomGenerator::global()->bounded(0, 31);
                while (terminalAddr2 == terminalAddr1) {
                    terminalAddr2 = QRandomGenerator::global()->bounded(0, 31);
                }
                subAddr1 = QRandomGenerator::global()->bounded(1, 31);
                subAddr2 = QRandomGenerator::global()->bounded(1, 31);
                dataCount1 = QRandomGenerator::global()->bounded(1, 32);
                cmd1 = generateCmd1(terminalAddr1, subAddr1, false, dataCount1);
                cmd2 = generateCmd2(terminalAddr2, subAddr2, true, dataCount1);
                hasCmd2 = true;
                break;

            case 3:
                msgTypeCount[3]++;
                terminalAddr1 = 31;
                subAddr1 = QRandomGenerator::global()->bounded(1, 31);
                isSend1 = false;
                dataCount1 = QRandomGenerator::global()->bounded(1, 32);
                cmd1 = generateCmd1(terminalAddr1, subAddr1, isSend1, dataCount1);
                cmd2 = 0;
                break;

            case 4:
                msgTypeCount[4]++;
                terminalAddr1 = QRandomGenerator::global()->bounded(0, 31);
                subAddr1 = 0;
                isSend1 = QRandomGenerator::global()->bounded(0, 2) == 1;
                dataCount1 = QRandomGenerator::global()->bounded(0, 32);
                cmd1 = generateCmd1(terminalAddr1, subAddr1, isSend1, dataCount1);
                cmd2 = 0;
                break;
            }

            SMbiMonPacketData data;
            data.header = 0xAABB;
            data.cmd1 = cmd1;
            data.cmd2 = cmd2;

            int statusType = QRandomGenerator::global()->bounded(100);
            if (statusType < 70) {
                statusTypeCount[0]++;
                data.states1 = (terminalAddr1 << 11) | 0x0001;
                if (hasCmd2) {
                    data.states2 = (terminalAddr2 << 11) | 0x0001;
                } else {
                    data.states2 = 0;
                }
                data.chstt = 0x0001;
            } else if (statusType < 80) {
                statusTypeCount[1]++;
                data.states1 = 0;
                data.states2 = 0;
                data.chstt = 0;
            } else if (statusType < 90) {
                statusTypeCount[2]++;
                data.states1 = (terminalAddr1 << 11) | 0x0019;
                if (hasCmd2) {
                    data.states2 = (terminalAddr2 << 11) | 0x0019;
                } else {
                    data.states2 = 0;
                }
                data.chstt = 0x0001;
            } else {
                statusTypeCount[3]++;
                data.states1 = (terminalAddr1 << 11) | 0x0001;
                if (hasCmd2) {
                    data.states2 = 0;
                } else {
                    data.states2 = 0;
                }
                data.chstt = 0;
            }

            data.timestamp = header.timestamp;

            int datasLen = calculateDatasLength(cmd1);
            QByteArray datas(datasLen, 0);
            for (int j = 0; j < datasLen; j++) {
                datas[j] = static_cast<char>(QRandomGenerator::global()->bounded(256));
            }

            QByteArray packetBytes;
            QDataStream packetStream(&packetBytes, QIODevice::WriteOnly);
            packetStream.setByteOrder(byteOrder == "big" ? QDataStream::BigEndian : QDataStream::LittleEndian);

            packetStream << data.header;
            packetStream << data.cmd1;
            packetStream << data.cmd2;
            packetStream << data.states1;
            packetStream << data.states2;
            packetStream << data.chstt;
            packetStream << data.timestamp;
            packetStream.writeRawData(datas.constData(), datasLen);

            allPacketData.append(packetBytes);
            totalPacketCount++;
        }

        header.packetLen = sizeof(SMbiMonPacketHeader) + allPacketData.size();

        out << header.header1;
        out << header.header2;
        out << header.mpuProduceId;
        out << header.packetLen;
        out << header.year;
        out << header.month;
        out << header.day;
        out << header.timestamp;

        out.writeRawData(allPacketData.constData(), allPacketData.size());
    }

    file.close();

    qint64 fileSize = QFileInfo(filename).size();
    LOG_INFO("TestDataGenerator", QString("生成测试数据文件: %1").arg(filename));
    LOG_INFO("TestDataGenerator", QString("  字节序: %1").arg(byteOrder == "big" ? "大端" : "小端"));
    LOG_INFO("TestDataGenerator", QString("  总消息数: %1").arg(recordCount));
    LOG_INFO("TestDataGenerator", QString("  总数据包数: %1").arg(totalPacketCount));
    LOG_INFO("TestDataGenerator", QString("  BC→RT: %1").arg(msgTypeCount[0]));
    LOG_INFO("TestDataGenerator", QString("  RT→BC: %1").arg(msgTypeCount[1]));
    LOG_INFO("TestDataGenerator", QString("  RT→RT: %1").arg(msgTypeCount[2]));
    LOG_INFO("TestDataGenerator", QString("  广播: %1").arg(msgTypeCount[3]));
    LOG_INFO("TestDataGenerator", QString("  模式命令: %1").arg(msgTypeCount[4]));
    LOG_INFO("TestDataGenerator", QString("  状态-成功: %1").arg(statusTypeCount[0]));
    LOG_INFO("TestDataGenerator", QString("  状态-失败: %1").arg(statusTypeCount[1]));
    LOG_INFO("TestDataGenerator", QString("  状态-错误标志: %1").arg(statusTypeCount[2]));
    LOG_INFO("TestDataGenerator", QString("  状态-部分失败: %1").arg(statusTypeCount[3]));
    LOG_INFO("TestDataGenerator", QString("  文件大小: %1 MB").arg(fileSize / 1024.0 / 1024.0, 0, 'f', 2));

    return true;
}
