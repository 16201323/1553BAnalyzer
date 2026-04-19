/**
 * @file GanttScene.cpp
 * @brief 甘特图场景类实现
 *
 * 本文件实现了QGraphicsScene的子类GanttScene，负责绘制：
 * - 时间轴：根据时间范围自动选择合适的刻度间隔（ms/s/min）
 * - 行标签：终端地址标签（RT0, RT1, ...）
 * - 网格背景
 *
 * 时间轴刻度策略：
 * - >1小时：每分钟一个刻度
 * - >1分钟：每10秒一个刻度
 * - >10秒：每秒一个刻度
 * - >1秒：每100ms一个刻度
 * - <=1秒：每10ms一个刻度
 *
 * @author 1553BTools
 * @date 2024
 */

#include "GanttScene.h"
#include <QGraphicsTextItem>
#include <QGraphicsLineItem>
#include <QDebug>
#include <QPainter>
#include <QFont>

GanttScene::GanttScene(QObject *parent)
    : QGraphicsScene(parent)
{
}

GanttScene::~GanttScene()
{
}

void GanttScene::drawTimeAxis(qint64 startTime, qint64 endTime, double pixelsPerUnit)
{
    qint64 timeRange = endTime - startTime;
    if (timeRange == 0) return;
    
    addRect(-80, 0, 3000, 25, QPen(QColor(180, 180, 180)), QBrush(QColor(240, 240, 240)));
    
    QFont font("Microsoft YaHei", 9, QFont::Normal);
    
    qint64 tickInterval = 1;
    
    if (timeRange > 3600000) {
        tickInterval = 300000;
    } else if (timeRange > 600000) {
        tickInterval = 60000;
    } else if (timeRange > 120000) {
        tickInterval = 30000;
    } else if (timeRange > 60000) {
        tickInterval = 10000;
    } else if (timeRange > 30000) {
        tickInterval = 5000;
    } else if (timeRange > 10000) {
        tickInterval = 2000;
    } else if (timeRange > 5000) {
        tickInterval = 1000;
    } else if (timeRange > 2000) {
        tickInterval = 500;
    } else if (timeRange > 1000) {
        tickInterval = 200;
    } else if (timeRange > 500) {
        tickInterval = 100;
    } else if (timeRange > 100) {
        tickInterval = 50;
    } else {
        tickInterval = 10;
    }
    
    int numTicks = static_cast<int>(timeRange / tickInterval) + 1;
    while (numTicks > 15) {
        tickInterval *= 2;
        numTicks = static_cast<int>(timeRange / tickInterval) + 1;
    }
    
    QPen majorPen(QColor(100, 100, 100), 1);
    
    QFont titleFont("Microsoft YaHei", 10, QFont::Bold);
    QGraphicsTextItem* title = addText(QString::fromUtf8(u8"时间轴"), titleFont);
    title->setDefaultTextColor(QColor(50, 50, 50));
    title->setPos(-75, 3);
    
    auto formatTime = [](qint64 ms) -> QString {
        qint64 totalSeconds = ms / 1000;
        int hours = static_cast<int>(totalSeconds / 3600);
        int minutes = static_cast<int>((totalSeconds % 3600) / 60);
        int seconds = static_cast<int>(totalSeconds % 60);
        int millis = static_cast<int>(ms % 1000);
        
        if (hours > 0) {
            return QString("%1:%2:%3")
                .arg(hours)
                .arg(minutes, 2, 10, QChar('0'))
                .arg(seconds, 2, 10, QChar('0'));
        } else {
            return QString("%1:%2:%3")
                .arg(hours)
                .arg(minutes, 2, 10, QChar('0'))
                .arg(seconds, 2, 10, QChar('0'));
        }
    };
    
    for (int i = 0; i <= numTicks && i <= 100; ++i) {
        qint64 time = startTime + i * tickInterval;
        double x = (time - startTime) * pixelsPerUnit;
        
        if (x > 50000) break;
        
        addLine(x, 0, x, 25, majorPen);
        
        QString timeLabel = formatTime(time);
        
        QGraphicsTextItem* label = addText(timeLabel, font);
        label->setDefaultTextColor(QColor(50, 50, 50));
        label->setPos(x - 25, 8);
    }
}

void GanttScene::drawRowLabels(const QList<int>& terminals, double rowHeight)
{
    addRect(-80, 25, 80, terminals.size() * rowHeight, QPen(QColor(180, 180, 180)), QBrush(QColor(245, 245, 245)));
    
    QFont titleFont("Microsoft YaHei", 9, QFont::Bold);
    QGraphicsTextItem* title = addText(u8"终端", titleFont);
    title->setDefaultTextColor(QColor(50, 50, 50));
    title->setPos(-70, 25);
    
    QFont labelFont("Microsoft YaHei", 8, QFont::Normal);
    for (int i = 0; i < terminals.size(); ++i) {
        QString label = QString("RT%1").arg(terminals[i]);
        QGraphicsTextItem* textItem = addText(label, labelFont);
        textItem->setDefaultTextColor(QColor(30, 30, 30));
        textItem->setPos(-55, i * rowHeight + 38);
    }
}
