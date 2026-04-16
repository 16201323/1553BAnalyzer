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

    bool generateTestDataFile(const QString& filename, int recordCount, const QString& byteOrder,
                              const QDate& startDate, const QDate& endDate,
                              const QTime& startTime, const QTime& endTime);

    static quint16 generateCmd1(int terminalAddr, int subAddr, bool isSend, int dataCount);
    static quint16 generateCmd2(int terminalAddr, int subAddr, bool isSend, int dataCount);
    static int calculateDatasLength(quint16 cmd1Raw);
};

#endif
