#ifndef TESTDATAGENERATOR_H
#define TESTDATAGENERATOR_H

#include <QObject>
#include <QString>
#include <QDate>
#include <QTime>

class TestDataGenerator : public QObject
{
    Q_OBJECT

public:
    explicit TestDataGenerator(QObject* parent = nullptr);

    /**
     * @brief 生成测试数据文件
     * @param filename 输出文件名
     * @param recordCount 记录数量
     * @param byteOrder 字节序（"big"或"little"）
     * @param startDate 开始日期
     * @param endDate 结束日期
     * @param startTime 开始时间
     * @param endTime 结束时间
     * @param seed 随机数种子，0表示使用当前时间作为种子（默认值）
     * @return 成功返回true，失败返回false
     *
     * 使用相同的种子值可以生成完全相同的测试数据，
     * 这对于测试的可重复性非常重要。
     */
    bool generateTestDataFile(const QString& filename, int recordCount, const QString& byteOrder,
                              const QDate& startDate, const QDate& endDate,
                              const QTime& startTime, const QTime& endTime,
                              quint32 seed = 0);

    static quint16 generateCmd1(int terminalAddr, int subAddr, bool isSend, int dataCount);
    static quint16 generateCmd2(int terminalAddr, int subAddr, bool isSend, int dataCount);
    static int calculateDatasLength(quint16 cmd1Raw);
};

#endif
