/**
 * @file main.cpp
 * @brief 1553B数据智能分析工具程序入口
 * 
 * 该文件是应用程序的主入口点，负责：
 * - 初始化Qt应用程序
 * - 初始化日志系统
 * - 加载配置文件
 * - 创建并显示主窗口
 * 
 * @author 1553BTools
 * @date 2024
 */

#include <QApplication>
#include <QFile>
#include <QDir>
#include <QMessageBox>
#include <QDateTime>
#include <QIcon>
#include <QPixmap>
#include <QImage>
#include "ui/mainwindow/MainWindow.h"
#include "core/config/ConfigManager.h"
#include "core/datastore/DatabaseManager.h"
#include "model/ModelManager.h"
#include "utils/Logger.h"

static QtMessageHandler originalHandler = nullptr;

static void customMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    if (msg.contains("QWindowsWindow::setGeometry: Unable to set geometry")) {
        return;
    }
    if (originalHandler) {
        originalHandler(type, context, msg);
    }
}

/**
 * @brief 初始化日志系统
 * 
 * 日志文件路径：程序目录/logs/1553BAnalyzer_yyyyMMdd.log
 * 日志级别：DEBUG及以上
 * 最大文件大小：10MB
 * 最大备份数量：5个
 */
void initLogger()
{
    QString appDir = QApplication::applicationDirPath();
    QString logDir = appDir + "/logs";
    
    QDir dir(logDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    QString logFileName = QString("1553BAnalyzer_%1.log")
                          .arg(QDateTime::currentDateTime().toString("yyyyMMdd"));
    QString logFilePath = logDir + "/" + logFileName;
    
    Logger::instance()->setLogFile(logFilePath);
    Logger::instance()->setLogLevel(LogLevel::Debug);
    Logger::instance()->setMaxFileSize(10);
    Logger::instance()->setMaxBackupCount(5);
    
    LOG_INFO("Main", QString("日志系统初始化完成，日志文件: %1").arg(logFilePath));
}

/**
 * @brief 程序主入口
 * @param argc 命令行参数个数
 * @param argv 命令行参数数组
 * @return 应用程序退出码
 */
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    originalHandler = qInstallMessageHandler(customMessageHandler);
    
    QApplication::setApplicationName("1553BAnalyzer");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setOrganizationName("1553BTools");
    
    QString iconPath = ":/icons/pandas.png";
    QPixmap pixmap;
    if (pixmap.load(iconPath)) {
        QIcon appIcon(pixmap);
        QApplication::setWindowIcon(appIcon);
    } else {
        QImage image(iconPath);
        if (!image.isNull()) {
            QIcon appIcon(QPixmap::fromImage(image));
            QApplication::setWindowIcon(appIcon);
        }
    }
    
    initLogger();
    
    LOG_INFO("Main", QString("应用程序启动，版本: %1").arg("1.0.0"));
    LOG_INFO("Main", QString("Qt版本: %1").arg(qVersion()));
    LOG_INFO("Main", QString("工作目录: %1").arg(QDir::currentPath()));
    
    LOG_INFO("Main", "开始加载配置文件");
    if (!ConfigManager::instance()->loadConfig()) {
        QString error = ConfigManager::instance()->getLastError();
        LOG_WARNING("Main", QString("配置文件加载失败: %1").arg(error));
        QMessageBox::warning(nullptr, QObject::tr("配置加载"),
            QObject::tr("配置文件加载失败，使用默认配置。\n%1").arg(error));
    } else {
        LOG_INFO("Main", "配置文件加载成功");
    }
    
    LOG_INFO("Main", "初始化模型管理器");
    ModelManager::instance()->initialize();
    
    LOG_INFO("Main", "初始化数据库管理器");
    if (!DatabaseManager::instance()->initialize()) {
        QString error = DatabaseManager::instance()->lastError();
        LOG_WARNING("Main", QString("数据库初始化失败: %1").arg(error));
    } else {
        LOG_INFO("Main", "数据库初始化成功");
    }
    
    LOG_INFO("Main", "创建主窗口");
    MainWindow mainWindow;
    mainWindow.show();
    LOG_INFO("Main", "主窗口显示完成");
    
    if (argc > 1) {
        QString filePath = QString::fromLocal8Bit(argv[1]);
        LOG_INFO("Main", QString("检测到命令行参数，尝试加载文件: %1").arg(filePath));
        if (QFile::exists(filePath)) {
            mainWindow.loadFile(filePath);
        } else {
            LOG_WARNING("Main", QString("命令行指定的文件不存在: %1").arg(filePath));
        }
    }
    
    LOG_INFO("Main", "进入主事件循环");
    int result = app.exec();
    
    LOG_INFO("Main", QString("应用程序退出，退出码: %1").arg(result));
    
    return result;
}
