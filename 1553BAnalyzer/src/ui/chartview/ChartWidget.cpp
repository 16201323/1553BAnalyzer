/**
 * @file ChartWidget.cpp
 * @brief 统计图表组件实现
 *
 * 本文件实现了基于QCustomPlot的统计图表组件，支持：
 * - 饼图：消息类型分布
 * - 柱状图：终端消息统计
 * - 折线图：时间分布趋势
 *
 * 使用QCustomPlot库实现图表渲染，支持拖拽、缩放、选择等交互。
 * 数据源通过DataStore的信号自动更新。
 *
 * @author 1553BTools
 * @date 2024
 */

#include "ChartWidget.h"
#include "utils/Qt5Compat.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QToolTip>
#include <QMouseEvent>
#include <QShowEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QCoreApplication>
#include <algorithm>

#include "qcustomplot/qcustomplot.h"
#include "utils/Logger.h"

/**
 * @brief 构造函数，初始化图表组件
 * @param parent 父窗口指针
 *
 * 初始化图表类型为饼图，数据标记为未更新
 */
ChartWidget::ChartWidget(QWidget *parent)
    : QWidget(parent)
    , m_dataStore(nullptr)
    , m_chart(nullptr)
    , m_chartSubject(ChartSubject::MessageType)
    , m_chartType("pie")
    , m_dataDirty(false)
    , m_refreshTimer(nullptr)
{
    setupUI();
    
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setSingleShot(true);
    m_refreshTimer->setInterval(100);
    connect(m_refreshTimer, &QTimer::timeout, this, &ChartWidget::refreshChart);
}

ChartWidget::~ChartWidget()
{
}

/**
 * @brief 初始化界面布局
 *
 * 布局结构：
 * - 顶部工具栏：图表类型选择器 + 导出按钮
 * - 中间区域：QCustomPlot图表画布
 *
 * QCustomPlot交互设置：
 * - iRangeDrag：支持拖拽平移
 * - iRangeZoom：支持滚轮缩放
 * - iSelectPlottables：支持选择数据项
 */
void ChartWidget::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    QHBoxLayout* toolLayout = new QHBoxLayout();
    toolLayout->addWidget(new QLabel(tr(u8"图表类型:")));
    
    m_chartTypeCombo = new QComboBox(this);
    m_chartTypeCombo->addItem(tr(u8"饼图 - 消息类型分布"), 0);
    m_chartTypeCombo->addItem(tr(u8"柱状图 - 终端统计"), 1);
    m_chartTypeCombo->addItem(tr(u8"折线图 - 时间分布"), 2);
    toolLayout->addWidget(m_chartTypeCombo);
    
    toolLayout->addStretch();
    
    m_exportBtn = new QPushButton(tr(u8"导出图表"), this);
    toolLayout->addWidget(m_exportBtn);
    
    mainLayout->addLayout(toolLayout);
    
    m_chart = new QCustomPlot(this);
    m_chart->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
    // 不调用setOpenGl()，避免QCustomPlot打印OpenGL未定义警告
    // 如需启用OpenGL，需在CMakeLists.txt中添加QCUSTOMPLOT_USE_OPENGL定义
    mainLayout->addWidget(m_chart);
    
    connect(m_chartTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ChartWidget::onChartTypeChanged);
    connect(m_exportBtn, &QPushButton::clicked, this, &ChartWidget::onExportChart);
}

void ChartWidget::setDataStore(DataStore* store)
{
    m_dataStore = store;
    if (m_dataStore) {
        connect(m_dataStore, &DataStore::dataChanged, this, &ChartWidget::scheduleRefresh);
        connect(m_dataStore, &DataStore::filterChanged, this, &ChartWidget::scheduleRefresh);
        connect(m_dataStore, &DataStore::dataScopeChanged, this, &ChartWidget::scheduleRefresh);
        connect(m_dataStore, &DataStore::pageChanged, this, [this](int, int, int) {
            if (m_dataStore && m_dataStore->dataScope() == DataScope::CurrentPage) {
                scheduleRefresh();
            }
        });
        if (isVisible() && m_chart->width() > 0 && m_chart->height() > 0) {
            scheduleRefresh();
        }
    }
}

void ChartWidget::setChartSubject(ChartSubject subject)
{
    m_chartSubject = subject;
}

void ChartWidget::setChartType(const QString& type)
{
    m_chartType = type;
}

void ChartWidget::scheduleRefresh()
{
    if (m_refreshTimer) {
        m_refreshTimer->start(200);
    }
}

void ChartWidget::refreshChart()
{
    if (!isVisible() || m_chart->width() <= 0 || m_chart->height() <= 0) {
        m_dataDirty = true;
        return;
    }
    
    m_dataDirty = false;
    m_lastDrawSize = m_chart->size();
    
    QCoreApplication::processEvents();
    
    if (m_chartSubject == ChartSubject::Chstt) {
        drawChsttPieChart();
        emit updateFinished();
        return;
    }
    
    if (m_chartSubject == ChartSubject::Terminal) {
        drawBarChart();
        emit updateFinished();
        return;
    }
    
    if (m_chartSubject == ChartSubject::Time) {
        drawLineChart();
        emit updateFinished();
        return;
    }
    
    int type = m_chartTypeCombo->currentData().toInt();
    switch (type) {
    case 0:
        drawPieChart();
        break;
    case 1:
        drawBarChart();
        break;
    case 2:
        drawLineChart();
        break;
    }
    
    emit updateFinished();
}

void ChartWidget::onChartTypeChanged(int index)
{
    Q_UNUSED(index)
    refreshChart();
}

void ChartWidget::onExportChart()
{
    QString filePath = QFileDialog::getSaveFileName(
        this, tr(u8"导出图表"), QString(),
        tr(u8"PNG图片 (*.png);;JPEG图片 (*.jpg);;PDF文件 (*.pdf)")
    );
    
    if (!filePath.isEmpty()) {
        if (filePath.endsWith(".pdf")) {
            m_chart->savePdf(filePath);
        } else {
            m_chart->savePng(filePath, 1200, 800);
        }
        QMessageBox::information(this, tr(u8"导出成功"), tr(u8"图表已保存到: %1").arg(filePath));
    }
}

void ChartWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    QTimer::singleShot(0, this, [this]() {
        QSize currentSize = m_chart->size();
        if ((m_dataDirty || currentSize != m_lastDrawSize) && currentSize.width() > 0 && currentSize.height() > 0) {
            m_lastDrawSize = currentSize;
            refreshChart();
        }
    });
}

void ChartWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (isVisible() && m_dataStore) {
        QSize currentSize = m_chart->size();
        if (currentSize != m_lastDrawSize && currentSize.width() > 0 && currentSize.height() > 0) {
            m_lastDrawSize = currentSize;
            QTimer::singleShot(0, this, [this]() {
                refreshChart();
            });
        }
    }
}

void ChartWidget::drawPieChart()
{
    m_chart->clearItems();
    m_chart->clearPlottables();
    
    if (!m_dataStore) {
        LOG_WARNING("ChartWidget", "drawPieChart: m_dataStore is null");
        return;
    }
    
    QMap<MessageType, int> stats = m_dataStore->getMessageTypeStatisticsByScope(m_dataStore->dataScope());
    LOG_DEBUG("ChartWidget", QString("drawPieChart: stats count = %1").arg(stats.size()));
    
    if (stats.isEmpty()) {
        m_chart->replot();
        return;
    }
    
    QVector<double> values;
    QVector<QString> labels;
    QVector<MessageType> types;
    
    for (auto it = stats.begin(); it != stats.end(); ++it) {
        if (it.value() > 0) {
            values.append(it.value());
            labels.append(messageTypeToString(it.key()));
            types.append(it.key());
        }
    }
    
    double total = 0;
    for (double v : values) total += v;
    
    if (total == 0) {
        m_chart->replot();
        return;
    }
    
    LOG_DEBUG("ChartWidget", QString("drawPieChart: total=%1, slices=%2").arg(total).arg(values.size()));
    for (int i = 0; i < values.size(); ++i) {
        LOG_DEBUG("ChartWidget", QString("  slice %1: %2=%3 (%4%)")
            .arg(i).arg(labels[i]).arg(values[i]).arg(values[i]/total*100, 0, 'f', 1));
    }
    
    QMap<MessageType, QColor> typeColors;
    typeColors[MessageType::BC_TO_RT] = QColor("#3498DB");
    typeColors[MessageType::RT_TO_BC] = QColor("#2ECC71");
    typeColors[MessageType::RT_TO_RT] = QColor("#F39C12");
    typeColors[MessageType::Broadcast] = QColor("#9B59B6");
    typeColors[MessageType::Unknown] = QColor("#95A5A6");
    
    m_chart->xAxis->setVisible(false);
    m_chart->yAxis->setVisible(false);
    m_chart->axisRect()->setBackground(Qt::transparent);
    m_chart->axisRect()->setAutoMargins(QCP::msNone);
    m_chart->axisRect()->setMargins(QMargins(10, 10, 10, 10));
    
    int chartWidth = m_chart->width();
    int chartHeight = m_chart->height();
    
    qreal dpr = devicePixelRatioF();
    QPixmap pixmap(chartWidth * dpr, chartHeight * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    
    int margin = 20;
    int titleHeight = 40;
    int legendWidth = 200;
    
    QFont titleFont("Microsoft YaHei", 14, QFont::Bold);
    painter.setFont(titleFont);
    painter.setPen(QColor("#2c3e50"));
    painter.drawText(QRectF(0, 5, chartWidth, titleHeight), Qt::AlignCenter, tr(u8"消息类型分布"));
    
    int pieAreaWidth = chartWidth - margin * 2 - legendWidth;
    int pieAreaHeight = chartHeight - margin * 2 - titleHeight;
    int pieDiameter = qMin(pieAreaWidth, pieAreaHeight);
    if (pieDiameter > 400) pieDiameter = 400;
    if (pieDiameter < 150) pieDiameter = 150;
    
    int pieX = margin + (pieAreaWidth - pieDiameter) / 2;
    int pieY = margin + titleHeight + (pieAreaHeight - pieDiameter) / 2;
    QRectF pieRect(pieX, pieY, pieDiameter, pieDiameter);
    
    QPointF center = pieRect.center();
    double outerRadius = pieDiameter / 2.0;
    double innerRadius = outerRadius * 0.55;
    
    QRadialGradient shadowGradient(center, outerRadius + 8);
    shadowGradient.setColorAt(0.85, QColor(0, 0, 0, 30));
    shadowGradient.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter.setBrush(shadowGradient);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(pieRect.adjusted(-8, -8, 8, 8));
    
    double startAngle = 90.0;
    
    for (int i = 0; i < values.size(); ++i) {
        double sliceAngle = values[i] / total * 360.0;
        
        QColor color = typeColors.value(types[i], QColor("#95A5A6"));
        
        QRadialGradient sliceGradient(center, outerRadius);
        sliceGradient.setColorAt(0, color.lighter(130));
        sliceGradient.setColorAt(0.6, color);
        sliceGradient.setColorAt(1.0, color.darker(115));
        
        painter.setBrush(sliceGradient);
        painter.setPen(QPen(Qt::white, 2.5));
        
        QPainterPath slicePath;
        double startRad = qDegreesToRadians(startAngle);
        double endRad = qDegreesToRadians(startAngle - sliceAngle);
        slicePath.moveTo(center.x() + outerRadius * qCos(startRad),
                         center.y() - outerRadius * qSin(startRad));
        slicePath.arcTo(pieRect, startAngle, -sliceAngle);
        double endX = center.x() + outerRadius * qCos(endRad);
        double endY = center.y() - outerRadius * qSin(endRad);
        slicePath.lineTo(endX, endY);
        QRectF innerRect(center.x() - innerRadius, center.y() - innerRadius,
                         innerRadius * 2, innerRadius * 2);
        slicePath.arcTo(innerRect, startAngle - sliceAngle, sliceAngle);
        slicePath.closeSubpath();
        painter.drawPath(slicePath);
        
        double midAngle = startAngle - sliceAngle / 2;
        double midRad = qDegreesToRadians(midAngle);
        
        if (values[i] / total > 0.05) {
            double labelRadius = outerRadius + 18;
            double labelX = center.x() + labelRadius * qCos(midRad);
            double labelY = center.y() - labelRadius * qSin(midRad);
            
            QFont labelFont("Microsoft YaHei", 9);
            painter.setFont(labelFont);
            painter.setPen(QColor("#2c3e50"));
            QString percentText = QString("%1%").arg(values[i] / total * 100, 0, 'f', 1);
            QFontMetrics fm(labelFont);
            QRect textRect = fm.boundingRect(percentText);
            painter.drawText(QRectF(labelX - textRect.width()/2, labelY - textRect.height()/2,
                                     textRect.width() + 4, textRect.height()),
                            Qt::AlignCenter, percentText);
        }
        
        if (values[i] / total > 0.10) {
            double midLabelRadius = (outerRadius + innerRadius) / 2.0;
            double midX = center.x() + midLabelRadius * qCos(midRad);
            double midY = center.y() - midLabelRadius * qSin(midRad);
            
            QFont midFont("Microsoft YaHei", 8, QFont::Bold);
            painter.setFont(midFont);
            painter.setPen(Qt::white);
            QString midText = QString::number(static_cast<int>(values[i]));
            QFontMetrics fmMid(midFont);
            QRect midTextRect = fmMid.boundingRect(midText);
            painter.drawText(QRectF(midX - midTextRect.width()/2, midY - midTextRect.height()/2,
                                     midTextRect.width() + 2, midTextRect.height()),
                            Qt::AlignCenter, midText);
        }
        
        startAngle -= sliceAngle;
    }
    
    painter.setBrush(Qt::white);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QRectF(center.x() - innerRadius + 1, center.y() - innerRadius + 1,
                               (innerRadius - 1) * 2, (innerRadius - 1) * 2));
    
    QFont centerNumFont("Microsoft YaHei", 18, QFont::Bold);
    painter.setFont(centerNumFont);
    painter.setPen(QColor("#2c3e50"));
    QString totalText = QString::number(static_cast<int>(total));
    QFontMetrics fmTotal(centerNumFont);
    QRect totalRect = fmTotal.boundingRect(totalText);
    painter.drawText(QRectF(center.x() - totalRect.width()/2, center.y() - totalRect.height() - 2,
                             totalRect.width(), totalRect.height()),
                    Qt::AlignCenter, totalText);
    
    QFont centerLabelFont("Microsoft YaHei", 9);
    painter.setFont(centerLabelFont);
    painter.setPen(QColor("#7f8c8d"));
    QString totalLabel = tr(u8"总消息数");
    QFontMetrics fmLabel(centerLabelFont);
    QRect labelRect = fmLabel.boundingRect(totalLabel);
    painter.drawText(QRectF(center.x() - labelRect.width()/2, center.y() + 2,
                             labelRect.width(), labelRect.height()),
                    Qt::AlignCenter, totalLabel);
    
    int legendX = pieX + pieDiameter + 25;
    int legendY = pieY + 5;
    int legendItemHeight = 28;
    
    QFont legendFont("Microsoft YaHei", 10);
    painter.setFont(legendFont);
    
    for (int i = 0; i < values.size(); ++i) {
        QColor color = typeColors.value(types[i], QColor("#95A5A6"));
        int yPos = legendY + i * legendItemHeight;
        
        if (yPos + 20 > chartHeight - 10) break;
        
        QRectF colorRect(legendX, yPos + 5, 16, 16);
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(colorRect, 3, 3);
        
        QString legendText = QString("%1: %2 (%3%)")
            .arg(labels[i])
            .arg(static_cast<int>(values[i]))
            .arg(values[i] / total * 100, 0, 'f', 1);
        
        painter.setPen(QColor("#34495e"));
        QRectF textRect(legendX + 24, yPos, legendWidth - 34, legendItemHeight);
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, legendText);
    }
    
    painter.end();
    
    m_chart->xAxis->setRange(-2, 2);
    m_chart->yAxis->setRange(-1.5, 1.5);
    
    QCPItemPixmap *pixmapItem = new QCPItemPixmap(m_chart);
    pixmapItem->setPixmap(pixmap);
    pixmapItem->topLeft->setCoords(-2, 1.5);
    pixmapItem->bottomRight->setCoords(2, -1.5);
    pixmapItem->setScaled(true, Qt::IgnoreAspectRatio);
    
    m_chart->replot();
}

/**
 * @brief 绘制时间间隔分析折线图
 * 
 * 展示指定RT和子地址的时间间隔变化趋势，
 * 用于观察周期数据是否稳定发送
 * 
 * @param analysis 时间间隔分析结果
 */
void ChartWidget::drawTimeIntervalChart(const TimeIntervalAnalysis& analysis)
{
    m_chart->clearItems();
    m_chart->clearPlottables();
    
    disconnect(m_chart, &QCustomPlot::mouseMove, this, nullptr);
    
    // 检查数据有效性
    if (analysis.recordCount < 2 || analysis.intervals.isEmpty()) {
        // 显示无数据提示
        QFont titleFont("Microsoft YaHei", 14, QFont::Bold);
        m_chart->xAxis->setVisible(false);
        m_chart->yAxis->setVisible(false);
        
        QCPItemText *textItem = new QCPItemText(m_chart);
        textItem->setPositionAlignment(Qt::AlignCenter);
        textItem->position->setType(QCPItemPosition::ptAxisRectRatio);
        textItem->position->setCoords(0.5, 0.5);
        textItem->setText(tr(u8"数据不足，无法生成时间间隔图表\n（需要至少2条记录）"));
        textItem->setFont(titleFont);
        textItem->setColor(QColor("#7f8c8d"));
        
        m_chart->replot();
        return;
    }
    
    // 准备数据
    QVector<double> keys;
    QVector<double> values;
    
    for (int i = 0; i < analysis.intervals.size(); ++i) {
        keys.append(i + 1);  // 序号从1开始
        values.append(analysis.intervals[i]);
    }
    
    // 设置坐标轴
    m_chart->xAxis->setVisible(true);
    m_chart->yAxis->setVisible(true);
    m_chart->xAxis->grid()->setVisible(true);
    m_chart->yAxis->grid()->setVisible(true);
    m_chart->xAxis->grid()->setPen(QPen(QColor(200, 200, 200), 1, Qt::DashLine));
    m_chart->yAxis->grid()->setPen(QPen(QColor(200, 200, 200), 1, Qt::DashLine));
    m_chart->xAxis->grid()->setZeroLinePen(QPen(QColor(150, 150, 150), 1, Qt::SolidLine));
    m_chart->yAxis->grid()->setZeroLinePen(QPen(QColor(150, 150, 150), 1, Qt::SolidLine));
    
    QFont axisFont("Microsoft YaHei", 10);
    QFont labelFont("Microsoft YaHei", 9);
    m_chart->xAxis->setTickLabelFont(labelFont);
    m_chart->yAxis->setTickLabelFont(axisFont);
    m_chart->xAxis->setLabelFont(axisFont);
    m_chart->yAxis->setLabelFont(axisFont);
    
    // 创建折线图
    QCPGraph *graph = m_chart->addGraph();
    graph->setData(keys, values);
    graph->setPen(QPen(QColor("#3498DB"), 2));
    graph->setBrush(QBrush(QColor(52, 152, 219, 50)));  // 半透明填充
    graph->setLineStyle(QCPGraph::lsLine);
    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, QColor("#3498DB"), QColor("#FFFFFF"), 6));
    
    // 添加平均线
    QCPGraph *avgLine = m_chart->addGraph();
    QVector<double> avgKeys, avgValues;
    avgKeys << 0 << analysis.intervals.size() + 1;
    avgValues << analysis.avgIntervalMs << analysis.avgIntervalMs;
    avgLine->setData(avgKeys, avgValues);
    avgLine->setPen(QPen(QColor("#E74C3C"), 2, Qt::DashLine));
    avgLine->setName(tr(u8"平均间隔: %1 ms").arg(analysis.avgIntervalMs, 0, 'f', 2));
    
    // 设置坐标轴范围和标签
    m_chart->xAxis->setRange(0, analysis.intervals.size() + 1);
    double maxVal = analysis.maxIntervalMs;
    double minVal = analysis.minIntervalMs;
    double range = maxVal - minVal;
    if (range < 1) range = 1;
    m_chart->yAxis->setRange(qMax(0.0, minVal - range * 0.1), maxVal + range * 0.1);
    
    m_chart->xAxis->setLabel(tr(u8"序号"));
    m_chart->yAxis->setLabel(tr(u8"时间间隔(ms)"));
    
    // 设置标题
    QString title = tr(u8"RT%1 子地址%2 时间间隔分析")
        .arg(analysis.terminalAddress)
        .arg(analysis.subAddress);
    
    QCPItemText *titleItem = new QCPItemText(m_chart);
    titleItem->setPositionAlignment(Qt::AlignTop | Qt::AlignHCenter);
    titleItem->position->setType(QCPItemPosition::ptAxisRectRatio);
    titleItem->position->setCoords(0.5, -0.08);
    titleItem->setText(title);
    titleItem->setFont(QFont("Microsoft YaHei", 12, QFont::Bold));
    titleItem->setColor(QColor("#2c3e50"));
    
    // 添加统计信息文本框
    QString statsText = tr(u8"记录数: %1 | 平均: %2 ms | 标准差: %3 ms | 抖动: %4%% | %5")
        .arg(analysis.recordCount)
        .arg(analysis.avgIntervalMs, 0, 'f', 2)
        .arg(analysis.stdDevMs, 0, 'f', 2)
        .arg(analysis.jitterPercent, 0, 'f', 1)
        .arg(analysis.stabilityAssessment);
    
    QCPItemText *statsItem = new QCPItemText(m_chart);
    statsItem->setPositionAlignment(Qt::AlignBottom | Qt::AlignHCenter);
    statsItem->position->setType(QCPItemPosition::ptAxisRectRatio);
    statsItem->position->setCoords(0.5, 1.05);
    statsItem->setText(statsText);
    statsItem->setFont(QFont("Microsoft YaHei", 9));
    statsItem->setColor(analysis.isStable ? QColor("#27AE60") : QColor("#E74C3C"));
    
    // 设置图例
    m_chart->legend->setVisible(true);
    m_chart->legend->setBrush(QColor(255, 255, 255, 230));
    m_chart->legend->setBorderPen(QPen(QColor(180, 180, 180), 1));
    m_chart->legend->setFont(QFont("Microsoft YaHei", 9));
    m_chart->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop | Qt::AlignRight);
    
    // 设置边距
    m_chart->axisRect()->setAutoMargins(QCP::msAll);
    m_chart->axisRect()->setMargins(QMargins(60, 50, 40, 60));
    
    // 设置交互
    m_chart->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    
    m_chart->replot();
    
    // 添加鼠标悬停提示
    connect(m_chart, &QCustomPlot::mouseMove, this, [this, graph, values](QMouseEvent *event) {
        double x = m_chart->xAxis->pixelToCoord(event->pos().x());
        double y = m_chart->yAxis->pixelToCoord(event->pos().y());
        
        int index = qRound(x) - 1;
        if (index >= 0 && index < values.size() && qAbs(x - (index + 1)) < 0.5) {
            QString tooltip = tr(u8"序号 %1: %2 ms").arg(index + 1).arg(values[index], 0, 'f', 2);
            QToolTip::showText(QT5COMPAT_MOUSE_GLOBAL_POS(event), tooltip, m_chart);
        } else {
            QToolTip::hideText();
        }
    });
    
    LOG_INFO("ChartWidget", QString::fromUtf8(u8"时间间隔图表绘制完成 - RT%1 SA%2, 记录数: %3")
             .arg(analysis.terminalAddress)
             .arg(analysis.subAddress)
             .arg(analysis.recordCount));
}

void ChartWidget::drawBarChart()
{
    m_chart->clearItems();
    m_chart->clearPlottables();
    
    disconnect(m_chart, &QCustomPlot::mouseMove, this, nullptr);
    
    if (!m_dataStore) {
        LOG_WARNING("ChartWidget", "drawBarChart: m_dataStore is null");
        return;
    }
    
    QMap<int, int> stats = m_dataStore->getTerminalStatisticsByScope(m_dataStore->dataScope());
    LOG_DEBUG("ChartWidget", QString("drawBarChart: stats count = %1").arg(stats.size()));
    
    if (stats.isEmpty()) {
        m_chart->replot();
        return;
    }
    
    QVector<double> keys;
    QVector<double> values;
    QVector<QString> labels;
    
    int index = 0;
    for (auto it = stats.begin(); it != stats.end(); ++it, ++index) {
        keys.append(index);
        values.append(it.value());
        labels.append(QString("RT%1").arg(it.key()));
    }
    
    m_chart->xAxis->setVisible(true);
    m_chart->yAxis->setVisible(true);
    m_chart->xAxis->grid()->setVisible(true);
    m_chart->yAxis->grid()->setVisible(true);
    m_chart->xAxis->grid()->setPen(QPen(QColor(200, 200, 200), 1, Qt::DashLine));
    m_chart->yAxis->grid()->setPen(QPen(QColor(200, 200, 200), 1, Qt::DashLine));
    m_chart->xAxis->grid()->setZeroLinePen(QPen(QColor(150, 150, 150), 1, Qt::SolidLine));
    m_chart->yAxis->grid()->setZeroLinePen(QPen(QColor(150, 150, 150), 1, Qt::SolidLine));
    
    QFont axisFont("Microsoft YaHei", 10);
    QFont labelFont("Microsoft YaHei", 9);
    m_chart->xAxis->setTickLabelFont(labelFont);
    m_chart->yAxis->setTickLabelFont(axisFont);
    m_chart->xAxis->setLabelFont(axisFont);
    m_chart->yAxis->setLabelFont(axisFont);
    
    m_chart->xAxis->setTickLabelRotation(-45);
    m_chart->xAxis->setTickLabelPadding(8);
    m_chart->yAxis->setTickLabelPadding(5);
    
    QCPBars *bars = new QCPBars(m_chart->xAxis, m_chart->yAxis);
    bars->setData(keys, values);
    bars->setBrush(QColor("#3498DB"));
    bars->setPen(QPen(QColor("#2980B9"), 1));
    bars->setWidth(0.7);
    bars->setName(tr(u8"数据量"));
    
    QSharedPointer<QCPAxisTickerText> textTicker(new QCPAxisTickerText);
    for (int i = 0; i < labels.size(); ++i) {
        textTicker->addTick(i, labels[i]);
    }
    m_chart->xAxis->setTicker(textTicker);
    m_chart->xAxis->setLabel(tr(u8"终端地址"));
    m_chart->yAxis->setLabel(tr(u8"数据量"));
    
    m_chart->xAxis->setRange(-0.5, keys.size() - 0.5);
    double maxVal = values.isEmpty() ? 1 : *std::max_element(values.begin(), values.end());
    if (maxVal <= 0) maxVal = 1;
    m_chart->yAxis->setRange(0, maxVal * 1.2);
    
    m_chart->yAxis->setNumberPrecision(0);
    m_chart->yAxis->setNumberFormat("f");
    
    QSharedPointer<QCPAxisTicker> ticker(new QCPAxisTicker);
    m_chart->yAxis->setTicker(ticker);
    
    m_chart->legend->setVisible(true);
    m_chart->legend->setBrush(QColor(255, 255, 255, 230));
    m_chart->legend->setBorderPen(QPen(QColor(180, 180, 180), 1));
    m_chart->legend->setFont(QFont("Microsoft YaHei", 9));
    m_chart->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop | Qt::AlignRight);
    
    m_chart->axisRect()->setAutoMargins(QCP::msAll);
    m_chart->axisRect()->setMargins(QMargins(60, 40, 40, 80));
    
    m_chart->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    
    m_chart->replot();
    
    connect(m_chart, &QCustomPlot::mouseMove, this, [this, bars, values, labels](QMouseEvent *event) {
        double x = m_chart->xAxis->pixelToCoord(event->pos().x());
        double y = m_chart->yAxis->pixelToCoord(event->pos().y());
        
        int index = qRound(x);
        if (index >= 0 && index < values.size() && qAbs(x - index) < 0.4 && y >= 0 && y <= values[index] * 1.1) {
            QString tooltip = QString::fromUtf8(u8"%1: %2 条数据").arg(labels[index]).arg(static_cast<int>(values[index]));
            QToolTip::showText(QT5COMPAT_MOUSE_GLOBAL_POS(event), tooltip, m_chart);
        } else {
            QToolTip::hideText();
        }
    });
}

void ChartWidget::drawLineChart()
{
    m_chart->clearItems();
    m_chart->clearPlottables();
    
    disconnect(m_chart, &QCustomPlot::mouseMove, this, nullptr);
    
    if (!m_dataStore) {
        LOG_WARNING("ChartWidget", "drawLineChart: m_dataStore is null");
        return;
    }
    
    QVector<DataRecord> records = m_dataStore->getRecordsByScope(m_dataStore->dataScope());
    LOG_DEBUG("ChartWidget", QString("drawLineChart: records count = %1").arg(records.size()));
    
    if (records.isEmpty()) {
        m_chart->replot();
        return;
    }
    
    qint64 minTimeMs = records.first().timestampMs;
    qint64 maxTimeMs = records.first().timestampMs;
    for (const DataRecord& r : records) {
        if (r.timestampMs < minTimeMs) minTimeMs = r.timestampMs;
        if (r.timestampMs > maxTimeMs) maxTimeMs = r.timestampMs;
    }
    qint64 timeRange = maxTimeMs - minTimeMs;
    
    qint64 bucketSize = 1000;
    if (timeRange > 3600000) {
        bucketSize = 60000;
    } else if (timeRange > 60000) {
        bucketSize = 10000;
    } else if (timeRange > 10000) {
        bucketSize = 5000;
    } else if (timeRange > 1000) {
        bucketSize = 100;
    }
    
    QMap<qint64, int> timeStats;
    for (const DataRecord& record : records) {
        qint64 timeBucket = (record.timestampMs - minTimeMs) / bucketSize;
        timeStats[timeBucket]++;
    }
    
    if (timeStats.isEmpty()) {
        m_chart->replot();
        return;
    }
    
    qint64 maxBucket = timeStats.lastKey();
    
    QVector<double> keys;
    QVector<double> values;
    keys.reserve(maxBucket + 1);
    values.reserve(maxBucket + 1);
    
    for (qint64 i = 0; i <= maxBucket; ++i) {
        keys.append(static_cast<double>(i));
        values.append(static_cast<double>(timeStats.value(i, 0)));
    }
    
    LOG_DEBUG("ChartWidget", QString("drawLineChart: keys.size=%1, values.size=%2")
        .arg(keys.size()).arg(values.size()));
    
    m_chart->xAxis->setVisible(true);
    m_chart->yAxis->setVisible(true);
    m_chart->xAxis->grid()->setVisible(true);
    m_chart->yAxis->grid()->setVisible(true);
    m_chart->xAxis->grid()->setPen(QPen(QColor(200, 200, 200), 1, Qt::DashLine));
    m_chart->yAxis->grid()->setPen(QPen(QColor(200, 200, 200), 1, Qt::DashLine));
    m_chart->xAxis->grid()->setZeroLinePen(QPen(QColor(150, 150, 150), 1, Qt::SolidLine));
    m_chart->yAxis->grid()->setZeroLinePen(QPen(QColor(150, 150, 150), 1, Qt::SolidLine));
    
    QFont axisFont("Microsoft YaHei", 10);
    QFont labelFont("Microsoft YaHei", 9);
    m_chart->xAxis->setTickLabelFont(labelFont);
    m_chart->yAxis->setTickLabelFont(axisFont);
    m_chart->xAxis->setLabelFont(axisFont);
    m_chart->yAxis->setLabelFont(axisFont);
    
    m_chart->xAxis->setTickLabelPadding(5);
    m_chart->yAxis->setTickLabelPadding(5);
    m_chart->xAxis->setTickLabelRotation(-30);
    
    m_chart->addGraph();
    m_chart->graph(0)->setData(keys, values);
    m_chart->graph(0)->setPen(QPen(QColor("#E74C3C"), 2));
    m_chart->graph(0)->setBrush(QBrush(QColor(231, 76, 60, 50)));
    m_chart->graph(0)->setName(tr(u8"数据量"));
    
    if (keys.size() < 100) {
        m_chart->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, QColor("#E74C3C"), QColor("#E74C3C"), 5));
    }
    
    m_chart->yAxis->setLabel(tr(u8"数据量"));
    
    double maxVal = values.isEmpty() ? 1 : *std::max_element(values.begin(), values.end());
    if (maxVal <= 0) maxVal = 1;
    
    m_chart->xAxis->setRange(0, maxBucket);
    m_chart->yAxis->setRange(0, maxVal * 1.2);
    
    QSharedPointer<QCPAxisTickerText> textTicker(new QCPAxisTickerText);
    int tickCount = qMin(10, static_cast<int>(maxBucket) + 1);
    int tickStep = qMax(1, static_cast<int>(maxBucket) / tickCount);
    
    for (int i = 0; i <= maxBucket; i += tickStep) {
        qint64 totalMs = minTimeMs + i * bucketSize;
        int totalSeconds = static_cast<int>(totalMs / 1000);
        int hours = totalSeconds / 3600;
        int minutes = (totalSeconds % 3600) / 60;
        int seconds = totalSeconds % 60;
        QString label = QString("%1:%2:%3")
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
        textTicker->addTick(i, label);
    }
    m_chart->xAxis->setTicker(textTicker);
    
    m_chart->xAxis->setLabel(tr(u8"时间"));
    
    QSharedPointer<QCPAxisTicker> yTicker(new QCPAxisTicker);
    yTicker->setTickCount(6);
    m_chart->yAxis->setTicker(yTicker);
    m_chart->yAxis->setNumberPrecision(0);
    m_chart->yAxis->setNumberFormat("f");
    
    m_chart->legend->setVisible(true);
    m_chart->legend->setBrush(QColor(255, 255, 255, 230));
    m_chart->legend->setBorderPen(QPen(QColor(180, 180, 180), 1));
    m_chart->legend->setFont(QFont("Microsoft YaHei", 9));
    m_chart->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop | Qt::AlignRight);
    
    m_chart->axisRect()->setAutoMargins(QCP::msAll);
    m_chart->axisRect()->setMargins(QMargins(70, 40, 40, 70));
    
    m_chart->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    m_chart->setNoAntialiasingOnDrag(true);
    
    m_chart->replot();
    
    connect(m_chart, &QCustomPlot::mouseMove, this, [this, keys, values, bucketSize](QMouseEvent *event) {
        double x = m_chart->xAxis->pixelToCoord(event->pos().x());
        
        if (keys.isEmpty() || x < 0 || x >= keys.size()) {
            QToolTip::hideText();
            return;
        }
        
        int idx = static_cast<int>(x + 0.5);
        if (idx >= 0 && idx < values.size()) {
            double timeMs = idx * bucketSize;
            QString timeStr;
            if (bucketSize >= 60000) {
                timeStr = QString::number(timeMs / 60000.0, 'f', 2) + tr(u8" 分钟");
            } else if (bucketSize >= 1000) {
                timeStr = QString::number(timeMs / 1000.0, 'f', 2) + tr(u8" 秒");
            } else {
                timeStr = QString::number(timeMs) + tr(u8" 毫秒");
            }
            QString tooltip = QString("%1: %2 %3")
                .arg(timeStr)
                .arg(static_cast<int>(values[idx]))
                .arg(tr(u8"条数据"));
            QToolTip::showText(QT5COMPAT_MOUSE_GLOBAL_POS(event), tooltip, m_chart);
        } else {
            QToolTip::hideText();
        }
    });
}

void ChartWidget::drawChsttPieChart()
{
    m_chart->clearItems();
    m_chart->clearPlottables();
    
    if (!m_dataStore) return;
    
    int successCount = 0;
    int failCount = 0;
    
    QVector<DataRecord> records = m_dataStore->getRecordsByScope(m_dataStore->dataScope());
    for (const DataRecord& record : records) {
        if (record.packetData.chstt) {
            successCount++;
        } else {
            failCount++;
        }
    }
    
    LOG_DEBUG("ChartWidget", QString("drawChsttPieChart: success=%1, fail=%2").arg(successCount).arg(failCount));
    
    if (successCount == 0 && failCount == 0) {
        m_chart->replot();
        return;
    }
    
    int total = successCount + failCount;
    
    m_chart->xAxis->setVisible(false);
    m_chart->yAxis->setVisible(false);
    m_chart->axisRect()->setBackground(Qt::transparent);
    m_chart->axisRect()->setAutoMargins(QCP::msNone);
    m_chart->axisRect()->setMargins(QMargins(10, 10, 10, 10));
    
    int chartWidth = m_chart->width();
    int chartHeight = m_chart->height();
    
    qreal dpr = devicePixelRatioF();
    QPixmap pixmap(chartWidth * dpr, chartHeight * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    
    int margin = 20;
    int titleHeight = 40;
    int legendWidth = 160;
    
    QFont titleFont("Microsoft YaHei", 14, QFont::Bold);
    painter.setFont(titleFont);
    painter.setPen(QColor("#2c3e50"));
    painter.drawText(QRectF(0, 5, chartWidth, titleHeight), Qt::AlignCenter, tr(u8"收发状态统计"));
    
    int pieAreaWidth = chartWidth - margin * 2 - legendWidth;
    int pieAreaHeight = chartHeight - margin * 2 - titleHeight;
    int pieDiameter = qMin(pieAreaWidth, pieAreaHeight);
    if (pieDiameter > 320) pieDiameter = 320;
    if (pieDiameter < 120) pieDiameter = 120;
    
    int pieX = margin + (pieAreaWidth - pieDiameter) / 2;
    int pieY = margin + titleHeight + (pieAreaHeight - pieDiameter) / 2;
    QRectF pieRect(pieX, pieY, pieDiameter, pieDiameter);
    
    QVector<int> pieValues;
    QVector<QString> pieLabels;
    QVector<QColor> pieColors;
    
    pieValues.append(successCount);
    pieLabels.append(tr(u8"成功"));
    pieColors.append(QColor("#2ECC71"));
    
    if (failCount > 0) {
        pieValues.append(failCount);
        pieLabels.append(tr(u8"失败"));
        pieColors.append(QColor("#E74C3C"));
    }
    
    double startAngle = 90.0;
    
    for (int i = 0; i < pieValues.size(); ++i) {
        if (pieValues[i] == 0) continue;
        
        double sliceAngle = pieValues[i] / static_cast<double>(total) * 360.0;
        double spanAngle16 = -sliceAngle * 16;
        double startAngle16 = startAngle * 16;
        
        QLinearGradient sliceGradient(
            pieRect.center().x() + pieDiameter/2 * qCos(qDegreesToRadians(startAngle - sliceAngle/2)),
            pieRect.center().y() - pieDiameter/2 * qSin(qDegreesToRadians(startAngle - sliceAngle/2)),
            pieRect.center().x(),
            pieRect.center().y()
        );
        sliceGradient.setColorAt(0, pieColors[i].lighter(110));
        sliceGradient.setColorAt(1, pieColors[i]);
        
        painter.setBrush(sliceGradient);
        painter.setPen(QPen(Qt::white, 2));
        painter.drawPie(pieRect, static_cast<int>(startAngle16), static_cast<int>(spanAngle16));
        
        double midAngle = startAngle - sliceAngle / 2;
        double midRad = qDegreesToRadians(midAngle);
        
        double labelRadius = pieDiameter / 2 + 15;
        double labelX = pieRect.center().x() + labelRadius * qCos(midRad);
        double labelY = pieRect.center().y() - labelRadius * qSin(midRad);
        
        QFont labelFont("Microsoft YaHei", 8);
        painter.setFont(labelFont);
        painter.setPen(QColor("#2c3e50"));
        QString percentText = QString("%1%").arg(pieValues[i] / static_cast<double>(total) * 100, 0, 'f', 1);
        QFontMetrics fm(labelFont);
        QRect textRect = fm.boundingRect(percentText);
        painter.drawText(QRectF(labelX - textRect.width()/2, labelY - textRect.height()/2,
                                 textRect.width() + 4, textRect.height()),
                        Qt::AlignCenter, percentText);
        
        double innerRadius = pieDiameter * 0.35;
        double innerX = pieRect.center().x() + innerRadius * qCos(midRad);
        double innerY = pieRect.center().y() - innerRadius * qSin(midRad);
        
        QFont innerFont("Microsoft YaHei", 7);
        painter.setFont(innerFont);
        painter.setPen(Qt::white);
        QString innerText = QString::number(pieValues[i]);
        QFontMetrics fmInner(innerFont);
        QRect innerTextRect = fmInner.boundingRect(innerText);
        painter.drawText(QRectF(innerX - innerTextRect.width()/2, innerY - innerTextRect.height()/2,
                                 innerTextRect.width() + 2, innerTextRect.height()),
                        Qt::AlignCenter, innerText);
        
        startAngle -= sliceAngle;
    }
    
    int legendX = pieX + pieDiameter + 20;
    int legendY = pieY + 5;
    int legendItemHeight = 24;
    
    QFont legendFont("Microsoft YaHei", 10);
    painter.setFont(legendFont);
    
    for (int i = 0; i < pieValues.size(); ++i) {
        int yPos = legendY + i * legendItemHeight;
        
        QRectF colorRect(legendX, yPos + 4, 14, 14);
        painter.setBrush(pieColors[i]);
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(colorRect, 3, 3);
        
        QString legendText = QString("%1: %2 (%3%)")
            .arg(pieLabels[i])
            .arg(pieValues[i])
            .arg(pieValues[i] / static_cast<double>(total) * 100, 0, 'f', 1);
        
        painter.setPen(QColor("#34495e"));
        QRectF textRect(legendX + 20, yPos, legendWidth - 30, legendItemHeight);
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, legendText);
    }
    
    painter.end();
    
    m_chart->xAxis->setRange(-2, 2);
    m_chart->yAxis->setRange(-1.5, 1.5);
    
    QCPItemPixmap *pixmapItem = new QCPItemPixmap(m_chart);
    pixmapItem->setPixmap(pixmap);
    pixmapItem->topLeft->setCoords(-2, 1.5);
    pixmapItem->bottomRight->setCoords(2, -1.5);
    pixmapItem->setScaled(true, Qt::KeepAspectRatio);
    
    m_chart->replot();
}
