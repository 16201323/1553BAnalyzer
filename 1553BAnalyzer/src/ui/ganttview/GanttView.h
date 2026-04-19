/**
 * @file GanttView.h
 * @brief 甘特图视图类定义
 * 
 * GanttView类继承自QGraphicsView，用于显示1553B数据的时序甘特图。
 * 
 * 主要功能：
 * - 按终端地址分行显示数据传输
 * - 支持缩放和平移操作
 * - 支持导出为PNG图片和PDF文档
 * - 显示数据传输状态和类型
 * 
 * 甘特图布局：
 * - 横轴：时间轴，显示数据传输的时间顺序
 * - 纵轴：终端地址，每个终端一行
 * - 条块：表示单次数据传输，颜色表示消息类型
 * 
 * 颜色映射：
 * - 蓝色：BC→RT
 * - 绿色：RT→BC
 * - 橙色：RT→RT
 * - 紫色：广播
 * 
 * @author 1553BTools
 * @date 2024
 */

#ifndef GANTTVIEW_H
#define GANTTVIEW_H

#include <QGraphicsView>
#include <QSet>
#include <QTimer>
#include <QtConcurrent>
#include <atomic>
#include "core/datastore/DataStore.h"

// 前向声明
class GanttScene;
class QMenu;

/**
 * @brief 甘特图视图类
 * 
 * 该类使用Qt的Graphics View框架实现甘特图显示，
 * 支持交互式缩放、平移和导出功能。
 */
class GanttView : public QGraphicsView
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针
     */
    explicit GanttView(QWidget *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~GanttView();
    
    /**
     * @brief 设置数据存储对象
     * @param store DataStore对象指针
     * 
     * 设置数据源并建立信号连接，自动更新视图
     */
    void setDataStore(DataStore* store);
    
    /**
     * @brief 设置时间范围
     * @param start 起始时间戳
     * @param end 结束时间戳
     * 
     * 限制甘特图显示的时间范围
     */
    void setTimeRange(quint32 start, quint32 end);
    
    /**
     * @brief 放大视图
     * 
     * 放大倍数增加20%，最大10倍
     */
    void zoomIn();
    
    /**
     * @brief 缩小视图
     * 
     * 放大倍数减少20%，最小0.1倍
     */
    void zoomOut();
    
    /**
     * @brief 缩放以适应窗口
     * 
     * 自动调整缩放级别使整个甘特图适应视图区域
     */
    void zoomToFit();
    
    /**
     * @brief 重置缩放
     * 
     * 恢复默认缩放级别（1.0倍）
     */
    void resetZoom();
    
    /**
     * @brief 导出为图片
     * @param filePath 输出文件路径
     * @return 导出成功返回true，失败返回false
     * 
     * 将当前甘特图导出为PNG格式图片
     */
    bool exportToImage(const QString& filePath);
    
    /**
     * @brief 导出为PDF
     * @param filePath 输出文件路径
     * @return 导出成功返回true，失败返回false
     * 
     * 将当前甘特图导出为PDF文档
     */
    bool exportToPdf(const QString& filePath);

public slots:
    void updateView();
    
    void scheduleUpdate();
    
signals:
    /**
     * @brief 条块点击信号
     * @param rowIndex 被点击的数据行索引
     */
    void itemClicked(int rowIndex);
    
    /**
     * @brief 条块悬停信号
     * @param info 悬停提示信息
     */
    void itemHovered(const QString& info);
    
    /**
     * @brief 视图更新完成信号
     * 
     * 当甘特图视图更新完成时发出此信号，
     * 用于通知外部组件加载已完成。
     */
    void updateFinished();

protected:
    /**
     * @brief 鼠标滚轮事件
     * @param event 滚轮事件对象
     * 
     * Ctrl+滚轮：缩放视图
     * 普通滚轮：滚动视图
     */
    void wheelEvent(QWheelEvent *event) override;
    
    /**
     * @brief 鼠标按下事件
     * @param event 鼠标事件对象
     * 
     * 左键点击条块时发出itemClicked信号
     */
    void mousePressEvent(QMouseEvent *event) override;
    
    /**
     * @brief 鼠标移动事件
     * @param event 鼠标事件对象
     * 
     * 悬停在条块上时发出itemHovered信号
     */
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    /**
     * @brief 设置场景
     * 
     * 配置QGraphicsView的渲染和交互属性
     */
    void setupScene();
    
    GanttScene* m_scene;       // 甘特图场景对象
    DataStore* m_dataStore;    // 数据存储对象
    double m_zoomLevel;        // 当前缩放级别
    quint32 m_timeStart;       // 时间范围起始值
    quint32 m_timeEnd;         // 时间范围结束值
    bool m_hasTimeRange;       // 是否设置了时间范围
    QTimer* m_updateTimer;     // 防抖更新定时器
    std::atomic<bool> m_updating; // 是否正在更新中
};

#endif
