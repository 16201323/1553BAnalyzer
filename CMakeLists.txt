# =============================================================================
# 1553B数据智能分析工具 - CMake构建配置
# =============================================================================
#
# 项目简介:
#   本项目是一个基于Qt6的1553B MIL-STD-1553B总线数据分析工具，
#   支持二进制数据文件解析、数据可视化、AI智能分析、报告生成等功能。
#
# 项目架构:
#   src/
#   ├── core/           核心业务逻辑层
#   │   ├── parser/     二进制文件解析（1553B协议解码）
#   │   ├── datastore/  数据存储与管理（内存/数据库双模式）
#   │   └── config/     配置管理（XML格式配置文件读写）
#   ├── ui/             用户界面层
#   │   ├── mainwindow/ 主窗口（应用核心框架）
#   │   ├── tableview/  数据表格视图
#   │   ├── ganttview/  甘特图视图（消息时序可视化）
#   │   ├── chartview/  统计图表视图
#   │   ├── dialogs/    对话框（设置、进度、十六进制查看）
#   │   └── widgets/    自定义控件（文件列表、AI查询、分页）
#   ├── model/          AI模型接入层
#   │   └── providers/  各AI服务商适配器（通义千问/DeepSeek/Kimi/Ollama）
#   ├── ai/             AI工具定义与执行器
#   ├── export/         数据导出服务（CSV等格式）
#   ├── report/         智能分析报告生成（HTML/PDF/DOCX）
#   └── utils/          通用工具（日志、字节转换、时间转换）
#   libs/               第三方库（QCustomPlot绘图库）
#   resources/          资源文件（图标、配置模板等）
#
# 构建要求:
#   - CMake >= 3.16
#   - Qt6 (Core, Gui, Widgets, Network, Xml, PrintSupport, Svg, Sql, Concurrent)
#   - C++17 兼容编译器 (MSVC 2022 / GCC 9+ / Clang 10+)
#
# 构建命令示例 (MSVC + Ninja):
#   cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=<Qt6路径> -DCMAKE_BUILD_TYPE=Debug
#   cmake --build build
# =============================================================================

cmake_minimum_required(VERSION 3.16)

project(1553BAnalyzer VERSION 1.0.0 LANGUAGES CXX)

# C++17标准（需要std::optional, std::variant, 结构化绑定等特性）
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt自动代码生成工具开关
# AUTOMOC: 自动处理Q_OBJECT宏的moc文件生成
# AUTORCC: 自动编译.qrc资源文件
# AUTOUIC: 自动处理.ui界面文件
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 可执行文件输出到 build/bin 目录，便于调试和部署
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Qt6依赖组件说明:
#   Core       - 核心非GUI功能（事件循环、对象模型、文件IO）
#   Gui        - GUI基础（窗口系统、图像、字体）
#   Widgets    - 经典桌面UI控件
#   Network    - 网络通信（HTTP客户端，用于AI API调用）
#   Xml        - XML读写（配置文件解析）
#   PrintSupport - 打印和PDF导出支持
#   Svg        - SVG图像渲染
#   Sql        - SQLite数据库驱动
#   Concurrent - 多线程并发（QtConcurrent::run等）
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    Xml
    PrintSupport
    Svg
    Sql
    Concurrent
)

# ---- 源文件列表 ----
# 按模块分组，便于维护
set(SOURCES
    # 入口
    src/main.cpp

    # 核心层 - 二进制解析模块
    src/core/parser/PacketStruct.h        # 1553B协议数据结构定义
    src/core/parser/BinaryParser.h        # 二进制文件解析器头文件
    src/core/parser/BinaryParser.cpp      # 二进制文件解析器实现
    src/core/parser/PacketDecoder.h       # 数据包解码器（协议字段提取）

    # 核心层 - 数据存储模块
    src/core/datastore/DataStore.h        # 数据存储中心（内存/数据库双模式）
    src/core/datastore/DataStore.cpp
    src/core/datastore/DataModel.h        # Qt数据模型（为表格视图提供数据接口）
    src/core/datastore/DataModel.cpp
    src/core/datastore/DatabaseManager.h  # SQLite数据库管理器（连接池、CRUD）
    src/core/datastore/DatabaseManager.cpp
    src/core/datastore/FilterExpression.h # 筛选表达式解析器
    src/core/datastore/FilterExpression.cpp
    src/core/datastore/TestDataGenerator.h
    src/core/datastore/TestDataGenerator.cpp

    # 核心层 - 配置管理模块
    src/core/config/ConfigManager.h       # 配置管理器（单例，统一配置读写）
    src/core/config/ConfigManager.cpp
    src/core/config/XmlConfigParser.h     # XML配置文件解析器
    src/core/config/XmlConfigParser.cpp

    # UI层 - 主窗口
    src/ui/mainwindow/MainWindow.h        # 主窗口（应用框架，菜单/工具栏/停靠窗口）
    src/ui/mainwindow/MainWindow.cpp

    # UI层 - 表格视图
    src/ui/tableview/TableView.h          # 数据表格视图（支持排序、筛选、分页）
    src/ui/tableview/TableView.cpp
    src/ui/tableview/TableModel.h         # 表格数据模型
    src/ui/tableview/TableModel.cpp

    # UI层 - 甘特图视图
    src/ui/ganttview/GanttView.h          # 甘特图视图（消息时序可视化）
    src/ui/ganttview/GanttView.cpp
    src/ui/ganttview/GanttScene.h         # 甘特图场景（管理图元布局）
    src/ui/ganttview/GanttScene.cpp
    src/ui/ganttview/GanttItem.h          # 甘特图图元（单条消息的可视化项）
    src/ui/ganttview/GanttItem.cpp

    # UI层 - 图表视图
    src/ui/chartview/ChartWidget.h        # 统计图表控件（QCustomPlot封装）
    src/ui/chartview/ChartWidget.cpp

    # UI层 - 对话框
    src/ui/dialogs/SettingsDialog.h       # 设置对话框（解析参数、AI模型配置）
    src/ui/dialogs/SettingsDialog.cpp
    src/ui/dialogs/ProgressDialog.h       # 进度对话框（支持取消、耗时显示）
    src/ui/dialogs/ProgressDialog.cpp
    src/ui/dialogs/HexViewDialog.h        # 十六进制查看对话框（原始数据浏览）
    src/ui/dialogs/HexViewDialog.cpp

    # UI层 - 自定义控件
    src/ui/widgets/FileListPanel.h        # 文件列表面板（最近打开的文件）
    src/ui/widgets/FileListPanel.cpp
    src/ui/widgets/AIQueryPanel.h         # AI查询面板（自然语言数据查询）
    src/ui/widgets/AIQueryPanel.cpp
    src/ui/widgets/PaginationWidget.h     # 分页控件（大数据量分页浏览）
    src/ui/widgets/PaginationWidget.cpp

    # AI模型接入层
    src/model/ModelAdapter.h              # 模型适配器接口（统一AI调用接口）
    src/model/ModelAdapter.cpp
    src/model/ModelManager.h              # 模型管理器（单例，管理多个AI提供商）
    src/model/ModelManager.cpp
    src/model/providers/QwenProvider.h    # 通义千问提供商
    src/model/providers/QwenProvider.cpp
    src/model/providers/DeepSeekProvider.h # DeepSeek提供商
    src/model/providers/DeepSeekProvider.cpp
    src/model/providers/KimiProvider.h    # Kimi（月之暗面）提供商
    src/model/providers/KimiProvider.cpp
    src/model/providers/OllamaProvider.h  # Ollama本地模型提供商
    src/model/providers/OllamaProvider.cpp

    # AI工具层
    src/ai/AIToolDefinitions.h            # AI工具定义（Function Calling协议定义）
    src/ai/AIToolExecutor.h               # AI工具执行器（解析并执行AI返回的工具调用）
    src/ai/AIToolExecutor.cpp

    # 数据导出层
    src/export/ExportService.h            # 导出服务（统一导出入口）
    src/export/ExportService.cpp
    src/export/CsvExporter.h              # CSV格式导出器
    src/export/CsvExporter.cpp

    # 报告生成层
    src/report/ReportGenerator.h          # 智能分析报告生成器（HTML/PDF/DOCX）
    src/report/ReportGenerator.cpp

    # 工具层
    src/utils/ByteConverter.h             # 字节转换工具（大小端、十六进制等）
    src/utils/TimeConverter.h             # 时间转换工具（1553B时间戳转换）
    src/utils/Logger.h                    # 日志系统（分级日志、文件轮转）
    src/utils/Logger.cpp

    # 第三方库
    libs/qcustomplot/qcustomplot.h        # QCustomPlot绘图库（开源Qt图表库）
    libs/qcustomplot/qcustomplot.cpp

    # 资源文件
    resources/resources.qrc               # Qt资源集合（图标、配置模板等）
)

# 头文件列表（用于IDE项目和moc处理）
set(HEADERS
    src/core/parser/PacketStruct.h
    src/core/parser/BinaryParser.h
    src/core/parser/PacketDecoder.h
    src/core/datastore/DataStore.h
    src/core/datastore/DataModel.h
    src/core/config/ConfigManager.h
    src/core/config/XmlConfigParser.h
    src/ui/mainwindow/MainWindow.h
    src/ui/tableview/TableView.h
    src/ui/tableview/TableModel.h
    src/ui/ganttview/GanttView.h
    src/ui/ganttview/GanttScene.h
    src/ui/ganttview/GanttItem.h
    src/ui/chartview/ChartWidget.h
    src/ui/dialogs/SettingsDialog.h
    src/ui/widgets/FileListPanel.h
    src/ui/widgets/AIQueryPanel.h
    src/model/ModelAdapter.h
    src/model/ModelManager.h
    src/model/providers/QwenProvider.h
    src/model/providers/DeepSeekProvider.h
    src/model/providers/KimiProvider.h
    src/model/providers/OllamaProvider.h
    src/ai/AIToolDefinitions.h
    src/ai/AIToolExecutor.h
    src/core/datastore/DatabaseManager.h
    src/core/datastore/FilterExpression.h
    src/ui/widgets/PaginationWidget.h
    src/ui/dialogs/ProgressDialog.h
    src/export/ExportService.h
    src/export/CsvExporter.h
    src/report/ReportGenerator.h
    src/utils/ByteConverter.h
    src/utils/TimeConverter.h
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# 头文件搜索路径
# src/  - 项目源码头文件
# libs/ - 第三方库头文件
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/libs
)

# Qt模块链接
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Xml
    Qt6::PrintSupport
    Qt6::Svg
    Qt6::Sql
    Qt6::Concurrent
)

# Windows平台特定配置
# WIN32_EXECUTABLE: 生成Windows GUI应用（不显示控制台窗口）
# RC_FILE: 应用程序图标和版本信息资源文件
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
        RC_FILE ${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc
    )
endif()

# 安装规则（make install时复制可执行文件到bin目录）
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)
