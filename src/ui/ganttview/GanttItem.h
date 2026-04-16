/**
 * @file GanttItem.h
 * @brief 甘特图条块图元类定义
 *
 * GanttItem类继承自QGraphicsRectItem，表示甘特图中的单个数据传输条块。
 * 每个条块对应一条1553B消息记录，在甘特图中按终端地址分行排列。
 *
 * 条块视觉属性：
 * - 颜色：根据消息类型（BC→RT/RT→BC/RT→RT/广播）自动着色
 * - 位置：x轴对应时间，y轴对应终端地址行
 * - 宽度：对应消息的持续时间
 * - 边框：悬停时高亮显示
 *
 * 交互行为：
 * - 鼠标悬停：显示工具提示（终端地址、子地址、时间等信息）
 * - 鼠标点击：发出信号，主窗口可据此定位到表格中对应行
 *
 * @author 1553BTools
 * @date 2024
 */

#ifndef GANTTITEM_H
#define GANTTITEM_H

#include <QGraphicsRectItem>
#include <QColor>
#include <QString>
#include "core/parser/PacketStruct.h"

/**
 * @brief 甘特图条块图元类
 *
 * 该类使用Qt Graphics View框架的图元系统，
 * 支持悬停高亮、点击选中和自定义绘制。
 */
class GanttItem : public QGraphicsRectItem
{
public:
    /**
     * @brief 自定义图元类型标识
     *
     * 用于qgraphicsitem_cast安全类型转换，
     * UserType + 1 确保不与Qt内置类型冲突
     */
    enum { Type = UserType + 1 };

    /**
     * @brief 构造函数
     * @param rowIndex 对应的数据行索引（用于点击时定位到表格行）
     * @param x 条块左上角x坐标（对应时间轴位置）
     * @param y 条块左上角y坐标（对应终端地址行位置）
     * @param width 条块宽度（对应消息持续时间）
     * @param height 条块高度（固定行高）
     * @param parent 父图元
     */
    GanttItem(int rowIndex, qreal x, qreal y, qreal width, qreal height,
              QGraphicsItem *parent = nullptr);
    ~GanttItem();

    /**
     * @brief 重写图元类型，支持安全类型转换
     */
    int type() const override { return Type; }

    /**
     * @brief 设置消息类型（决定条块颜色）
     * @param type 消息类型枚举值
     *
     * 消息类型与颜色映射：
     * - BC_TO_RT: 蓝色
     * - RT_TO_BC: 绿色
     * - RT_TO_RT: 橙色
     * - Broadcast: 紫色
     */
    void setMessageType(MessageType type);

    /**
     * @brief 设置传输状态
     * @param success true=成功（正常颜色），false=失败（红色）
     */
    void setSuccess(bool success);

    /**
     * @brief 设置工具提示文本
     * @param tooltip 提示内容（鼠标悬停时显示）
     */
    void setTooltip(const QString& tooltip);

    /**
     * @brief 设置选中状态
     * @param selected 是否选中（选中时显示高亮边框）
     */
    void setSelected(bool selected);

    /**
     * @brief 获取数据行索引
     * @return 对应表格中的行号
     */
    int rowIndex() const;

    /**
     * @brief 获取工具提示文本
     * @return 提示内容字符串
     */
    QString tooltip() const;

protected:
    /**
     * @brief 鼠标进入图元区域事件
     *
     * 设置悬停标志，触发重绘以显示高亮效果
     */
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;

    /**
     * @brief 鼠标离开图元区域事件
     *
     * 清除悬停标志，恢复普通显示
     */
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

    /**
     * @brief 鼠标按下事件
     *
     * 设置选中状态，触发重绘
     */
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

    /**
     * @brief 自定义绘制方法
     *
     * 根据当前状态（悬停/选中/正常）绘制不同样式的条块：
     * - 正常：填充颜色 + 细边框
     * - 悬停：填充颜色 + 粗边框
     * - 选中：填充颜色 + 高亮边框
     */
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget = nullptr) override;

private:
    /**
     * @brief 根据消息类型和状态更新颜色
     *
     * 综合考虑消息类型、传输成功/失败、悬停/选中状态
     * 计算最终的填充色和边框色
     */
    void updateColor();

    int m_rowIndex;              ///< 对应的数据行索引
    MessageType m_messageType;   ///< 消息类型（决定基础颜色）
    bool m_success;              ///< 传输是否成功（失败时变红）
    bool m_hovered;              ///< 鼠标是否悬停在条块上
    bool m_selected;             ///< 条块是否被选中
    QString m_tooltip;           ///< 工具提示文本
    QColor m_color;              ///< 当前填充颜色
    QColor m_borderColor;        ///< 当前边框颜色
};

#endif
