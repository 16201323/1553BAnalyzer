/**
 * @file HexViewDialog.h
 * @brief 十六进制查看对话框类定义
 *
 * HexViewDialog类提供二进制文件的十六进制查看功能，
 * 以经典的"偏移量 | 十六进制数据 | ASCII字符"三栏格式显示文件内容。
 *
 * 主要功能：
 * - 分块加载：避免一次性加载大文件导致内存溢出
 * - 滚动加载：滚动到底部时自动加载更多数据
 * - 格式化显示：每行显示16字节，包含偏移量、十六进制值和ASCII字符
 *
 * 显示格式示例：
 * @code
 * 00000000 | A5 A5 A5 01 00 3C 07 E8 01 0F 00 00 00 00 BB AA | ..............
 * 00000010 | 18 43 00 00 00 00 00 01 00 00 00 00 3C 56 78 9A | .C..........<Vx.
 * @endcode
 *
 * @author 1553BTools
 * @date 2024
 */

#ifndef HEXVIEWDIALOG_H
#define HEXVIEWDIALOG_H

#include <QDialog>
#include <QPlainTextEdit>
#include <QFile>
#include <QScrollBar>

/**
 * @brief 十六进制查看对话框类
 *
 * 该对话框以只读方式显示二进制文件内容，
 * 支持大文件的分块加载和滚动浏览。
 */
class HexViewDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param filePath 要查看的文件路径
     * @param parent 父窗口指针
     */
    explicit HexViewDialog(const QString& filePath, QWidget *parent = nullptr);
    ~HexViewDialog();

    /**
     * @brief 加载文件内容
     * @param filePath 文件路径
     *
     * 打开文件并加载第一页数据，后续数据通过滚动触发加载
     */
    void loadFile(const QString& filePath);

private slots:
    /**
     * @brief 滚动条位置变化槽
     * @param value 当前滚动位置
     *
     * 当滚动到底部附近时，触发加载更多数据
     */
    void onScrollChanged(int value);

    /**
     * @brief 加载更多数据槽
     *
     * 从当前偏移位置加载下一块数据并追加到显示区域
     */
    void onLoadMore();

private:
    void setupUI();

    /**
     * @brief 加载指定偏移位置的数据块
     * @param offset 文件偏移量（字节）
     * @param lines 要加载的行数
     *
     * 从文件中读取数据并格式化为十六进制显示
     */
    void loadChunk(qint64 offset, int lines);

    /**
     * @brief 格式化单行十六进制数据
     * @param offset 行起始偏移量
     * @param data 原始字节数据
     * @return 格式化后的字符串（偏移量 | 十六进制 | ASCII）
     */
    QString formatHexLine(qint64 offset, const QByteArray& data);

    QPlainTextEdit* m_hexView;    ///< 十六进制显示文本框
    QFile* m_file;                ///< 当前打开的文件对象
    qint64 m_fileSize;            ///< 文件总大小（字节）
    qint64 m_currentOffset;       ///< 当前已加载到的文件偏移量
    int m_bytesPerLine;           ///< 每行显示的字节数（默认16）
    int m_linesPerPage;           ///< 每页加载的行数
    bool m_loadingMore;           ///< 是否正在加载更多数据（防止重复触发）
};

#endif
