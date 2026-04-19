/**
 * @file GanttView.cpp
 * @brief 甘特图视图类实现
 * 
 * 本文件实现了GanttView类的所有方法，包括：
 * - 视图初始化和配置
 * - 数据绑定和更新
 * - 缩放和平移操作
 * - 鼠标交互处理
 * - 导出功能实现
 * 
 * @author 1553BTools
 * @date 2024
 */

#include "GanttView.h"
#include "GanttScene.h"
#include "GanttItem.h"
#include "core/parser/PacketStruct.h"
#include "utils/Logger.h"
#include "utils/Qt5Compat.h"
#include <QWheelEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QGraphicsView>
#include <QDebug>
#include <cstring>
#include <QPainter>
#include <QImage>
#include <QPrinter>

GanttView::GanttView(QWidget *parent)
    : QGraphicsView(parent)
    , m_scene(new GanttScene(this))
    , m_dataStore(nullptr)
    , m_zoomLevel(1.0)
    , m_timeStart(0)
    , m_timeEnd(0)
    , m_hasTimeRange(false)
    , m_updateTimer(nullptr)
    , m_updating(false)
{
    setupScene();
    
    m_updateTimer = new QTimer(this);
    m_updateTimer->setSingleShot(true);
    m_updateTimer->setInterval(100);
    connect(m_updateTimer, &QTimer::timeout, this, &GanttView::updateView);
}

GanttView::~GanttView()
{
}

void GanttView::setupScene()
{
    setScene(m_scene);
    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setBackgroundBrush(QColor(245, 245, 245));
}

void GanttView::setDataStore(DataStore* store)
{
    if (m_dataStore) {
        disconnect(m_dataStore, nullptr, this, nullptr);
    }
    
    m_dataStore = store;
    
    if (m_dataStore) {
        connect(m_dataStore, &DataStore::dataChanged, this, &GanttView::scheduleUpdate);
        connect(m_dataStore, &DataStore::filterChanged, this, &GanttView::scheduleUpdate);
        connect(m_dataStore, &DataStore::dataScopeChanged, this, &GanttView::scheduleUpdate);
        connect(m_dataStore, &DataStore::pageChanged, this, [this](int, int, int) {
            if (m_dataStore && m_dataStore->dataScope() == DataScope::CurrentPage) {
                scheduleUpdate();
            }
        });
        scheduleUpdate();
    }
}

void GanttView::setTimeRange(quint32 start, quint32 end)
{
    m_timeStart = start;
    m_timeEnd = end;
    m_hasTimeRange = true;
    scheduleUpdate();
}

void GanttView::zoomIn()
{
    m_zoomLevel *= 1.2;
    if (m_zoomLevel > 10.0) m_zoomLevel = 10.0;
    scheduleUpdate();
}

void GanttView::zoomOut()
{
    m_zoomLevel /= 1.2;
    if (m_zoomLevel < 0.1) m_zoomLevel = 0.1;
    scheduleUpdate();
}

void GanttView::zoomToFit()
{
    if (!scene() || scene()->items().isEmpty()) return;
    fitInView(scene()->sceneRect(), Qt::KeepAspectRatio);
    m_zoomLevel = transform().m11();
}

void GanttView::resetZoom()
{
    m_zoomLevel = 1.0;
    resetTransform();
    scheduleUpdate();
}

void GanttView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->angleDelta().y() > 0) {
            zoomIn();
        } else {
            zoomOut();
        }
        event->accept();
    } else {
        QGraphicsView::wheelEvent(event);
    }
}

void GanttView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QGraphicsItem* item = itemAt(event->pos());
        if (item && item->type() == GanttItem::Type) {
            GanttItem* ganttItem = static_cast<GanttItem*>(item);
            emit itemClicked(ganttItem->rowIndex());
        }
    }
    QGraphicsView::mousePressEvent(event);
}

void GanttView::mouseMoveEvent(QMouseEvent *event)
{
    QGraphicsItem* item = itemAt(event->pos());
    if (item && item->type() == GanttItem::Type) {
        GanttItem* ganttItem = static_cast<GanttItem*>(item);
        emit itemHovered(ganttItem->tooltip());
    }
    QGraphicsView::mouseMoveEvent(event);
}

void GanttView::scheduleUpdate()
{
    if (m_updateTimer) {
        m_updateTimer->start();
    }
}

void GanttView::updateView()
{
    LOG_INFO("GanttView", QString::fromUtf8(u8"========== 开始更新甘特图 =========="));
    
    if (!m_dataStore) {
        LOG_WARNING("GanttView", "updateView: m_dataStore is null");
        return;
    }
    
    if (m_updating.load()) {
        scheduleUpdate();
        return;
    }
    
    m_updating.store(true);
    m_scene->clear();
    
    DataScope scope = m_dataStore->dataScope();
    DataStore* store = m_dataStore;
    bool hasTimeRange = m_hasTimeRange;
    quint32 timeStart = m_timeStart;
    quint32 timeEnd = m_timeEnd;
    double zoomLevel = m_zoomLevel;
    
    (void)QtConcurrent::run([this, store, scope, hasTimeRange, timeStart, timeEnd, zoomLevel]() {
        QVector<DataRecord> scopedRecords;
        
        if (scope == DataScope::CurrentPage) {
            scopedRecords = store->getCurrentPageRecords();
        } else if (scope == DataScope::AllData) {
            scopedRecords = store->getRecordsByScope(DataScope::AllData);
        } else if (scope == DataScope::FilteredData) {
            scopedRecords = store->getFilteredRecords();
        }
        
        qint64 minTime, maxTime;
        if (!scopedRecords.isEmpty()) {
            minTime = scopedRecords.first().timestampMs;
            maxTime = scopedRecords.first().timestampMs;
            for (const DataRecord& r : scopedRecords) {
                if (r.timestampMs < minTime) minTime = r.timestampMs;
                if (r.timestampMs > maxTime) maxTime = r.timestampMs;
            }
        } else {
            minTime = store->getMinTimestampMs();
            maxTime = store->getMaxTimestampMs();
        }
        
        if (hasTimeRange) {
            minTime = timeStart;
            maxTime = timeEnd;
        }
        
        double timeRange = maxTime - minTime;
        if (timeRange <= 0) {
            timeRange = 1;
        }
        
        double targetWidth = 2000.0 * zoomLevel;
        double pixelsPerUnit = targetWidth / timeRange;
        double sceneWidth = timeRange * pixelsPerUnit;
        
        double minWidth = 800.0;
        if (sceneWidth < minWidth) {
            pixelsPerUnit = minWidth / timeRange;
            sceneWidth = minWidth;
        }
        
        double maxWidth = 50000.0;
        if (sceneWidth > maxWidth) {
            pixelsPerUnit = maxWidth / timeRange;
            sceneWidth = maxWidth;
        }
        
        QSet<int> terminals;
        if (scope == DataScope::FilteredData && !scopedRecords.isEmpty()) {
            for (const DataRecord& r : scopedRecords) {
                terminals.insert(r.terminalAddr);
            }
        } else {
            terminals = store->getAllTerminals();
        }
        QList<int> sortedTerminals = terminals.values();
        std::sort(sortedTerminals.begin(), sortedTerminals.end());
        
        QMap<int, int> terminalToRow;
        for (int i = 0; i < sortedTerminals.size(); ++i) {
            terminalToRow[sortedTerminals[i]] = i;
        }
        
        int dataCount = scopedRecords.size();
        const int maxDisplayItems = 5000;
        int step = qMax(1, dataCount / maxDisplayItems);
        bool needsSampling = dataCount > maxDisplayItems;
        
        struct GanttItemData {
            int rowIndex;
            double x, y, width, height;
            MessageType messageType;
            bool success;
            QString tooltip;
        };
        QVector<GanttItemData> itemDataList;
        itemDataList.reserve(qMin(dataCount, maxDisplayItems));
        
        int skippedCount = 0;
        
        for (int idx = 0; idx < scopedRecords.size(); idx++) {
            if (needsSampling && idx % step != 0) continue;
            
            const DataRecord& record = scopedRecords[idx];
            int terminal = record.terminalAddr;
            
            if (!terminalToRow.contains(terminal)) {
                skippedCount++;
                continue;
            }
            
            int row = terminalToRow[terminal];
            double rowHeight = 30.0;
            double x = (record.timestampMs - minTime) * pixelsPerUnit;
            double y = row * rowHeight + 37;
            double width = qMax(2.0, qMin(10.0, sceneWidth / 500.0));
            
            GanttItemData itemData;
            itemData.rowIndex = record.rowIndex;
            itemData.x = x;
            itemData.y = y;
            itemData.width = width;
            itemData.height = rowHeight - 4;
            itemData.messageType = record.messageType;
            itemData.success = record.packetData.chstt == 1;
            itemData.tooltip = QString::fromUtf8(u8"时间: %1 ms\n类型: %2\n终端: %3\n行号: %4")
                .arg(record.timestampMs, 0, 'f', 3)
                .arg(messageTypeToString(record.messageType))
                .arg(terminal)
                .arg(record.rowIndex + 1);
            itemDataList.append(itemData);
        }
        
        qt5InvokeMethod(this, [this, minTime, maxTime, pixelsPerUnit, sceneWidth,
            sortedTerminals, itemDataList, dataCount, step, needsSampling, skippedCount]() {
            m_scene->clear();
            
            double rowHeight = 30.0;
            double sceneHeight = sortedTerminals.size() * rowHeight + 60;
            
            m_scene->setSceneRect(-80, 0, sceneWidth + 80, sceneHeight);
            m_scene->setBackgroundBrush(QColor(250, 250, 250));
            m_scene->addRect(-80, 0, sceneWidth + 80, sceneHeight, QPen(QColor(180, 180, 180)), QBrush(QColor(250, 250, 250)));
            
            m_scene->drawTimeAxis(minTime, maxTime, pixelsPerUnit);
            
            QPen gridPen(QColor(220, 220, 220), 1);
            for (int i = 0; i <= sortedTerminals.size(); ++i) {
                double y = i * rowHeight + 35;
                m_scene->addLine(-80, y, sceneWidth, y, gridPen);
            }
            
            m_scene->drawRowLabels(sortedTerminals, rowHeight);
            
            int itemCount = 0;
            for (const auto& itemData : itemDataList) {
                GanttItem* item = new GanttItem(itemData.rowIndex, itemData.x, itemData.y, itemData.width, itemData.height);
                item->setMessageType(itemData.messageType);
                item->setSuccess(itemData.success);
                item->setTooltip(itemData.tooltip);
                m_scene->addItem(item);
                itemCount++;
            }
            
            if (needsSampling) {
                QFont infoFont("Microsoft YaHei", 8, QFont::Normal);
                QGraphicsTextItem* info = m_scene->addText(
                    QString::fromUtf8(u8"采样显示 %1/%2 条数据 (每%3条取1条)").arg(itemCount).arg(dataCount).arg(step),
                    infoFont);
                info->setDefaultTextColor(QColor(150, 150, 150));
                info->setPos(0, sceneHeight - 20);
            }
            
            LOG_INFO("GanttView", QString::fromUtf8(u8"创建的甘特图条块数: %1, 跳过数: %2").arg(itemCount).arg(skippedCount));
            LOG_INFO("GanttView", QString::fromUtf8(u8"========== 甘特图更新完成 =========="));
            
            m_updating.store(false);
            emit updateFinished();
        });
    });
}

bool GanttView::exportToImage(const QString& filePath)
{
    if (!m_scene || m_scene->items().isEmpty()) {
        return false;
    }
    
    QRectF sceneRect = m_scene->sceneRect();
    QImage image(sceneRect.size().toSize(), QImage::Format_ARGB32);
    image.fill(Qt::white);
    
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    m_scene->render(&painter);
    painter.end();
    
    return image.save(filePath);
}

bool GanttView::exportToPdf(const QString& filePath)
{
    if (!m_scene || m_scene->items().isEmpty()) {
        return false;
    }
    
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(filePath);
    
    QRectF sceneRect = m_scene->sceneRect();
    QT5COMPAT_PRINTER_PAGE_SIZE(printer, sceneRect.size());
    
    QPainter painter(&printer);
    painter.setRenderHint(QPainter::Antialiasing);
    m_scene->render(&painter);
    painter.end();
    
    return true;
}
