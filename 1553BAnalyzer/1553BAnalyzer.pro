QT += core gui widgets network xml printsupport concurrent sql multimedia

# 限制 qmake 递归查找深度
QMAKE_PROJECT_DEPTH = 0

CONFIG += c++14
CONFIG += qt

TARGET = 1553BAnalyzer
TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS

# MSVC编译器配置
msvc {
    # 禁用可能导致冲突的 Windows 宏
    DEFINES += NOMINMAX
    # 允许使用关键字宏，防止 xkeycheck.h 重新定义 static_assert
    DEFINES += _ALLOW_KEYWORD_MACROS

    # MSVC2015 编译器标志
    # 注意：/Zc:__cplusplus 仅 MSVC2017+ 支持，MSVC2015 不支持
    # 注意：/source-charset 和 /execution-charset 仅 VS2015 Update 2+ (19.00.23918+) 支持
    # 当前 MSVC 版本为 19.00.23026 (VS2015 RTM)，不支持上述标志
    # 源文件使用 UTF-8 BOM 编码，编译器可自动识别，无需额外指定字符集标志

    # 优化编译速度（多处理器编译）
    QMAKE_CXXFLAGS += /MP
}

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
    src/core/analysis/TimeIntervalAnalyzer.cpp \
    src/ui/mainwindow/MainWindow.cpp \
    src/ui/tableview/TableView.cpp \
    src/ui/ganttview/GanttView.cpp \
    src/ui/ganttview/GanttScene.cpp \
    src/ui/ganttview/GanttItem.cpp \
    src/ui/chartview/ChartWidget.cpp \
    src/ui/dialogs/SettingsDialog.cpp \
    src/ui/dialogs/HexViewDialog.cpp \
    src/ui/dialogs/ProgressDialog.cpp \
    src/ui/dialogs/LoadingDialog.cpp \
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
    libs/qcustomplot/qcustomplot.cpp \
    src/speech/VoskEngine.cpp \
    src/speech/AudioCapture.cpp \
    src/speech/SpeechRecognizer.cpp

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
    src/core/analysis/TimeIntervalAnalyzer.h \
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
    src/ui/dialogs/LoadingDialog.h \
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
    src/utils/Qt5Compat.h \
    libs/qcustomplot/qcustomplot.h \
    src/speech/VoskEngine.h \
    src/speech/AudioCapture.h \
    src/speech/SpeechRecognizer.h

INCLUDEPATH += \
    src \
    libs \
    libs/vosk/include

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

    # Vosk语音识别库链接配置
    msvc {
        LIBS += $$PWD/libs/vosk/lib/libvosk.lib
    } else {
        LIBS += -L$$PWD/libs/vosk/lib -llibvosk
    }

    # 部署 DLL 依赖
    VOSK_DLLS = $$PWD/libs/vosk/lib/libvosk.dll $$PWD/libs/vosk/lib/libgcc_s_seh-1.dll $$PWD/libs/vosk/lib/libstdc++-6.dll $$PWD/libs/vosk/lib/libwinpthread-1.dll
    for(dll, VOSK_DLLS) {
        QMAKE_POST_LINK += $$escape_expand(\\n\\t)-if exist $$shell_path($$dll) copy /y \"$$shell_path($$dll)\" \"$$shell_path($$OUT_PWD/bin)\"
    }
}
