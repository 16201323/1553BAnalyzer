/**
 * @file GanttScene.h
 * @brief 甘特图场景类定义
 * 
 * GanttScene类继承自QGraphicsScene，用于管理甘特图的图形元素。
 * 
 * 主要功能：
 * - 绘制时间轴（横轴）
 * - 绘制行标签（纵轴，终端地址）
 * - 管理甘特图条块项
 * 
 * 场景布局：
 * - 左侧：终端地址标签
 * - 顶部：时间轴
 * - 主区域：数据传输条块
 * 
 * @author 1553BTools
 * @date 2024
 */

#ifndef GANTTSCENE_H
#define GANTTSCENE_H

#include <QGraphicsScene>

/**
 * @brief 甘特图场景类
 * 
 * 该类使用Qt的Graphics View框架管理甘特图的图形元素，
 * 提供时间轴和行标签的绘制功能。
 */
class GanttScene : public QGraphicsScene
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象指针
     */
    explicit GanttScene(QObject *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~GanttScene();
    
    /**
     * @brief 绘制时间轴
     * @param startTime 起始时间戳（毫秒）
     * @param endTime 结束时间戳（毫秒）
     * @param pixelsPerUnit 每时间单位的像素数
     * 
     * 在场景顶部绘制时间刻度和标签
     */
    void drawTimeAxis(qint64 startTime, qint64 endTime, double pixelsPerUnit);
    
    /**
     * @brief 绘制行标签
     * @param terminals 终端地址列表
     * @param rowHeight 每行的高度（像素）
     * 
     * 在场景左侧绘制终端地址标签
     */
    void drawRowLabels(const QList<int>& terminals, double rowHeight);
};

#endif
