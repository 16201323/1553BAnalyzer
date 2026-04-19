#ifndef QT5COMPAT_H
#define QT5COMPAT_H

#include <QtGlobal>
#include <QFontMetrics>
#include <QThread>
#include <QTime>

#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)

#include <cstdlib>
#include <QTextCodec>
#include <QTextStream>
#include <QPrinter>
#include <QTimer>
#include <QEventLoop>

namespace Qt5Compat {

/**
 * @brief 生成指定范围内的随机整数
 * @param min 最小值（包含）
 * @param max 最大值（不包含）
 * @return 范围内的随机整数
 */
inline int randomInt(int min, int max)
{
    return min + (qrand() % (max - min));
}

/**
 * @brief 设置随机数种子
 * @param seed 种子值，0表示使用当前时间作为种子
 */
inline void setRandomSeed(quint32 seed)
{
    if (seed == 0) {
        qsrand(static_cast<uint>(QTime::currentTime().msec()));
    } else {
        qsrand(seed);
    }
}

inline void setUtf8Encoding(QTextStream& stream)
{
    QTextCodec* codec = QTextCodec::codecForName("UTF-8");
    if (codec) {
        stream.setCodec(codec);
    }
}

inline void setPrinterPageSizeA4(QPrinter& printer)
{
    printer.setPageSize(QPrinter::A4);
}

inline void setPrinterPageSizePoints(QPrinter& printer, const QSizeF& size)
{
    Q_UNUSED(size);
    printer.setPageSize(QPrinter::A4);
}

inline void setPrinterPageMargins(QPrinter& printer, qreal left, qreal top, qreal right, qreal bottom, QPrinter::Unit unit)
{
    printer.setPageMargins(left, top, right, bottom, unit);
}

inline void setPrinterOrientationLandscape(QPrinter& printer)
{
    printer.setOrientation(QPrinter::Landscape);
}

inline int fontHorizontalAdvance(const QFontMetrics& fm, const QString& text)
{
    return fm.width(text);
}

}

template<typename Functor>
void qt5InvokeMethod(QObject* context, Functor functor)
{
    QTimer::singleShot(0, context, functor);
}

template<typename Functor>
void qt5InvokeMethodBlocking(QObject* context, Functor functor)
{
    if (context->thread() == QThread::currentThread()) {
        functor();
        return;
    }
    QEventLoop loop;
    QTimer::singleShot(0, context, [&functor, &loop]() {
        functor();
        loop.quit();
    });
    loop.exec();
}

#define QT5COMPAT_RANDOM_INT(min, max) Qt5Compat::randomInt(min, max)
#define QT5COMPAT_SET_RANDOM_SEED(seed) Qt5Compat::setRandomSeed(seed)
#define QT5COMPAT_SET_UTF8(stream) Qt5Compat::setUtf8Encoding(stream)
#define QT5COMPAT_PRINTER_A4(printer) Qt5Compat::setPrinterPageSizeA4(printer)
#define QT5COMPAT_PRINTER_PAGE_SIZE(printer, size) Qt5Compat::setPrinterPageSizePoints(printer, size)
#define QT5COMPAT_PRINTER_MARGINS(printer, l, t, r, b, u) Qt5Compat::setPrinterPageMargins(printer, l, t, r, b, u)
#define QT5COMPAT_PRINTER_LANDSCAPE(printer) Qt5Compat::setPrinterOrientationLandscape(printer)
#define QT5COMPAT_FONT_HADVANCE(fm, text) Qt5Compat::fontHorizontalAdvance(fm, text)
#define QT5COMPAT_SKIP_EMPTY QString::SkipEmptyParts
#define QT5COMPAT_MOUSE_GLOBAL_POS(event) ((event)->globalPos())

#else

#include <QRandomGenerator>
#include <QPageSize>
#include <QPageLayout>

template<typename Functor>
void qt5InvokeMethod(QObject* context, Functor functor)
{
    QMetaObject::invokeMethod(context, functor);
}

template<typename Functor>
void qt5InvokeMethodBlocking(QObject* context, Functor functor)
{
    QMetaObject::invokeMethod(context, functor, Qt::BlockingQueuedConnection);
}

inline int qt5FontHorizontalAdvance(const QFontMetrics& fm, const QString& text)
{
    return fm.horizontalAdvance(text);
}

#define QT5COMPAT_RANDOM_INT(min, max) QRandomGenerator::global()->bounded(min, max)
#define QT5COMPAT_SET_RANDOM_SEED(seed) QRandomGenerator::global()->seed(seed)
#define QT5COMPAT_SET_UTF8(stream) stream.setEncoding(QStringConverter::Utf8)
#define QT5COMPAT_PRINTER_A4(printer) printer.setPageSize(QPageSize(QPageSize::A4))
#define QT5COMPAT_PRINTER_PAGE_SIZE(printer, size) printer.setPageSize(QPageSize(size, QPageSize::Point))
#define QT5COMPAT_PRINTER_MARGINS(printer, l, t, r, b, u) printer.setPageMargins(QMarginsF(l, t, r, b), QPageLayout::Millimeter)
#define QT5COMPAT_PRINTER_LANDSCAPE(printer) printer.setPageOrientation(QPageLayout::Landscape)
#define QT5COMPAT_FONT_HADVANCE(fm, text) fm.horizontalAdvance(text)
#define QT5COMPAT_SKIP_EMPTY Qt::SkipEmptyParts
#define QT5COMPAT_MOUSE_GLOBAL_POS(event) ((event)->globalPosition().toPoint())

#endif

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QTextCodec>
#else
#include <QStringConverter>
#endif

#endif
