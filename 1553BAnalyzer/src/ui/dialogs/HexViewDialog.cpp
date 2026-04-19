/**
 * @file HexViewDialog.cpp
 * @brief 十六进制查看对话框实现
 *
 * 本文件实现了二进制文件的十六进制查看功能，类似专业Hex编辑器的显示方式。
 *
 * 显示格式：每行16字节，左侧偏移量 + 中间十六进制 + 右侧ASCII
 * 支持大文件分页加载，避免一次性加载导致内存溢出。
 *
 * 技术要点：
 * - 使用QFile按需读取文件内容，而非一次性加载到内存
 * - 分页机制：每次加载100行（1600字节），滚动到底部自动加载更多
 * - 十六进制和ASCII双列显示，非可打印字符显示为点号
 *
 * @author 1553BTools
 * @date 2024
 */

#include "HexViewDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QApplication>
#include <QFileInfo>
#include <QDebug>

HexViewDialog::HexViewDialog(const QString& filePath, QWidget *parent)
    : QDialog(parent)
    , m_hexView(new QPlainTextEdit(this))
    , m_file(new QFile(filePath, this))
    , m_fileSize(0)
    , m_currentOffset(0)
    , m_bytesPerLine(16)
    , m_linesPerPage(100)
    , m_loadingMore(false)
{
    setupUI();
    loadFile(filePath);
}

HexViewDialog::~HexViewDialog()
{
    if (m_file && m_file->isOpen()) {
        m_file->close();
    }
}

void HexViewDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    QFileInfo fileInfo(m_file->fileName());
    QLabel* infoLabel = new QLabel(tr(u8"文件: %1").arg(fileInfo.fileName()));
    mainLayout->addWidget(infoLabel);
    
    m_hexView->setReadOnly(true);
    m_hexView->setFont(QFont("Courier New", 9));
    m_hexView->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_hexView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_hexView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    mainLayout->addWidget(m_hexView);
    
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    QPushButton* loadMoreBtn = new QPushButton(tr(u8"加载更多"));
    connect(loadMoreBtn, &QPushButton::clicked, this, &HexViewDialog::onLoadMore);
    buttonLayout->addWidget(loadMoreBtn);
    
    QPushButton* closeBtn = new QPushButton(tr(u8"关闭"));
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeBtn);
    
    mainLayout->addLayout(buttonLayout);
    
    setWindowTitle(tr(u8"16进制查看器"));
    setMinimumSize(900, 600);
    resize(1000, 700);
}

void HexViewDialog::loadFile(const QString& filePath)
{
    if (!m_file->open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, tr(u8"错误"), tr(u8"无法打开文件: %1").arg(filePath));
        return;
    }
    
    m_fileSize = m_file->size();
    m_currentOffset = 0;
    
    m_hexView->clear();
    
    QString header = QString("%1  %2  %3\n")
        .arg(u8"地址", -10)
        .arg("00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F", -48)
        .arg("ASCII");
    m_hexView->appendPlainText(header);
    m_hexView->appendPlainText(QString(80, '-'));
    
    loadChunk(0, m_linesPerPage);
}

void HexViewDialog::loadChunk(qint64 offset, int lines)
{
    if (!m_file->isOpen() || m_loadingMore) {
        return;
    }
    
    m_loadingMore = true;
    m_file->seek(offset);
    
    QString content;
    for (int i = 0; i < lines && !m_file->atEnd(); i++) {
        QByteArray data = m_file->read(m_bytesPerLine);
        if (data.isEmpty()) {
            break;
        }
        content += formatHexLine(offset + i * m_bytesPerLine, data) + "\n";
    }
    
    m_currentOffset = m_file->pos();
    
    if (!content.isEmpty()) {
        m_hexView->appendPlainText(content);
    }
    
    m_loadingMore = false;
    
    QScrollBar* vScrollBar = m_hexView->verticalScrollBar();
    if (m_currentOffset < m_fileSize) {
        vScrollBar->setRange(0, static_cast<int>(m_fileSize / m_bytesPerLine));
    }
}

QString HexViewDialog::formatHexLine(qint64 offset, const QByteArray& data)
{
    QString hexPart;
    QString asciiPart;
    
    for (int i = 0; i < data.size(); i++) {
        unsigned char byte = static_cast<unsigned char>(data[i]);
        hexPart += QString("%1 ").arg(byte, 2, 16, QChar('0')).toUpper();
        
        if (byte >= 32 && byte <= 126) {
            asciiPart += QChar(byte);
        } else {
            asciiPart += '.';
        }
    }
    
    for (int i = data.size(); i < m_bytesPerLine; i++) {
        hexPart += "   ";
    }
    
    return QString("%1  %2 %3")
        .arg(offset, 8, 16, QChar('0')).toUpper()
        .arg(hexPart, -48)
        .arg(asciiPart);
}

void HexViewDialog::onScrollChanged(int value)
{
    Q_UNUSED(value);
}

void HexViewDialog::onLoadMore()
{
    if (m_currentOffset < m_fileSize) {
        loadChunk(m_currentOffset, m_linesPerPage);
    } else {
        QMessageBox::information(this, tr(u8"提示"), tr(u8"已到达文件末尾"));
    }
}
