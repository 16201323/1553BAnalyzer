/**
 * @file GanttItem.cpp
 * @brief 甘特图数据项类实现
 *
 * 本文件实现了甘特图中单个消息数据项的绘制和交互功能：
 * - 根据消息类型（BC→RT/RT→BC/RT→RT/广播）显示不同颜色
 * - 错误消息显示为红色
 * - 鼠标悬停时高亮显示
 * - 点击选中效果
 * - 悬停时显示工具提示（tooltip）
 *
 * 颜色映射：
 * - BC→RT: 蓝色 (#3498DD)
 * - RT→BC: 绿色 (#2ECC71)
 * - RT→RT: 橙色 (#F39C12)
 * - Broadcast: 紫色 (#9B59B6)
 * - 错误: 红色 (#E74C3C)
 *
 * @author 1553BTools
 * @date 2024
 */

#include "GanttItem.h"
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QDebug>

GanttItem::GanttItem(int rowIndex, qreal x, qreal y, qreal width, qreal height,
                     QGraphicsItem *parent)
    : QGraphicsRectItem(x, y, width, height, parent)
    , m_rowIndex(rowIndex)
    , m_messageType(MessageType::Unknown)
    , m_success(true)
    , m_hovered(false)
    , m_selected(false)
{
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    
    updateColor();
}

GanttItem::~GanttItem()
{
}

void GanttItem::setMessageType(MessageType type)
{
    m_messageType = type;
    updateColor();
}

void GanttItem::setSuccess(bool success)
{
    m_success = success;
    updateColor();
}

void GanttItem::setTooltip(const QString& tooltip)
{
    m_tooltip = tooltip;
    setToolTip(tooltip);
}

void GanttItem::setSelected(bool selected)
{
    m_selected = selected;
    update();
}

int GanttItem::rowIndex() const
{
    return m_rowIndex;
}

QString GanttItem::tooltip() const
{
    return m_tooltip;
}

void GanttItem::updateColor()
{
    if (!m_success) {
        m_color = QColor("#E74C3C");
    } else {
        switch (m_messageType) {
        case MessageType::BC_TO_RT:
            m_color = QColor("#3498DB");
            break;
        case MessageType::RT_TO_BC:
            m_color = QColor("#2ECC71");
            break;
        case MessageType::RT_TO_RT:
            m_color = QColor("#F39C12");
            break;
        case MessageType::Broadcast:
            m_color = QColor("#9B59B6");
            break;
        default:
            m_color = QColor("#95A5A6");
            break;
        }
    }
    
    m_borderColor = m_color.darker(120);
    update();
}

void GanttItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)
    m_hovered = true;
    setZValue(100);
    update();
}

void GanttItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)
    m_hovered = false;
    setZValue(0);
    update();
}

void GanttItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event)
    m_selected = true;
    update();
}

void GanttItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)
    
    QRectF rect = boundingRect();
    
    QColor fillColor = m_color;
    QColor borderColor = m_borderColor;
    
    if (m_hovered) {
        fillColor = fillColor.lighter(120);
        borderColor = Qt::yellow;
    }
    
    if (m_selected) {
        borderColor = Qt::red;
    }
    
    painter->setPen(QPen(borderColor, m_hovered ? 2 : 1));
    painter->setBrush(QBrush(fillColor));
    painter->drawRoundedRect(rect, 3, 3);
    
    if (m_hovered && rect.width() > 30) {
        painter->setPen(Qt::white);
        QFont font = painter->font();
        font.setPointSize(8);
        painter->setFont(font);
        
        QString shortText = QString::number(m_rowIndex + 1);
        painter->drawText(rect, Qt::AlignCenter, shortText);
    }
}
