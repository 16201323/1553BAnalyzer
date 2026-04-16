QT += core gui widgets network xml printsupport concurrent sql

#QMAKE_PROJECT_DEPTH 是 qmake 的内置变量，用于限制 qmake 解析项目文件时向上递归查找父项目（.pro 文件）的深度，设置为 0 时会完全禁用这种递归查找行为。
QMAKE_PROJECT_DEPTH = 0

CONFIG += c++17
CONFIG += qt

TARGET = 1553BAnalyzer
TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
    src/main.cpp \
    src/core/datastore/TestDataGenerator.cpp \
    src/core/parser/BinaryParser.cpp \
    src/core/datastore/DataStore.cpp \
    src/core/datastore/DataModel.cpp \
    src/core/datastore/FilterExpression.cpp \
    src/core/datastore/DatabaseManager.cpp \
    src/core/config/ConfigManager.cpp \
    src/core/config/XmlConfigParser.cpp \
    src/ui/mainwindow/MainWindow.cpp \
    src/ui/tableview/TableView.cpp \
    src/ui/ganttview/GanttView.cpp \
    src/ui/ganttview/GanttScene.cpp \
    src/ui/ganttview/GanttItem.cpp \
    src/ui/chartview/ChartWidget.cpp \
    src/ui/dialogs/SettingsDialog.cpp \
    src/ui/dialogs/HexViewDialog.cpp \
    src/ui/dialogs/ProgressDialog.cpp \
    src/ui/widgets/FileListPanel.cpp \
    src/ui/widgets/AIQueryPanel.cpp \
    src/ui/widgets/PaginationWidget.cpp \
    src/model/ModelAdapter.cpp \
    src/model/ModelManager.cpp \
    src/model/providers/QwenProvider.cpp \
    src/model/providers/DeepSeekProvider.cpp \
    src/model/providers/KimiProvider.cpp \
    src/model/providers/OllamaProvider.cpp \
    src/ai/AIToolExecutor.cpp \
    src/export/ExportService.cpp \
    src/export/CsvExporter.cpp \
    src/report/ReportGenerator.cpp \
    src/utils/Logger.cpp \
    libs/qcustomplot/qcustomplot.cpp

HEADERS += \
    src/core/datastore/TestDataGenerator.h \
    src/core/parser/PacketStruct.h \
    src/core/parser/BinaryParser.h \
    src/core/parser/PacketDecoder.h \
    src/core/datastore/DataStore.h \
    src/core/datastore/DataModel.h \
    src/core/datastore/FilterExpression.h \
    src/core/datastore/DatabaseManager.h \
    src/core/config/ConfigManager.h \
    src/core/config/XmlConfigParser.h \
    src/ui/mainwindow/MainWindow.h \
    src/ui/tableview/TableView.h \
    src/ui/tableview/TableModel.h \
    src/ui/ganttview/GanttView.h \
    src/ui/ganttview/GanttScene.h \
    src/ui/ganttview/GanttItem.h \
    src/ui/chartview/ChartWidget.h \
    src/ui/dialogs/SettingsDialog.h \
    src/ui/dialogs/HexViewDialog.h \
    src/ui/dialogs/ProgressDialog.h \
    src/ui/widgets/FileListPanel.h \
    src/ui/widgets/AIQueryPanel.h \
    src/ui/widgets/PaginationWidget.h \
    src/model/ModelAdapter.h \
    src/model/ModelManager.h \
    src/model/providers/QwenProvider.h \
    src/model/providers/DeepSeekProvider.h \
    src/model/providers/KimiProvider.h \
    src/model/providers/OllamaProvider.h \
    src/ai/AIToolDefinitions.h \
    src/ai/AIToolExecutor.h \
    src/export/ExportService.h \
    src/export/CsvExporter.h \
    src/report/ReportGenerator.h \
    src/utils/ByteConverter.h \
    src/utils/TimeConverter.h \
    src/utils/Logger.h \
    libs/qcustomplot/qcustomplot.h

INCLUDEPATH += \
    src \
    libs

RESOURCES += \
    resources/resources.qrc

DESTDIR = bin
OBJECTS_DIR = build/obj
MOC_DIR = build/moc
RCC_DIR = build/rcc
UI_DIR = build/ui

win32 {
    QMAKE_TARGET_COMPANY = "1553BTools"
    QMAKE_TARGET_PRODUCT = "1553B Analyzer"
    QMAKE_TARGET_DESCRIPTION = "1553B Data Intelligent Analysis Tool"
    QMAKE_TARGET_COPYRIGHT = "Copyright 2024"
}

