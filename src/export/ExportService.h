/**
 * @file ExportService.h
 * @brief 数据导出服务类定义
 *
 * ExportService类提供统一的数据导出接口，支持多种导出格式。
 * 作为导出功能的入口类，封装了不同格式导出器的调用细节。
 *
 * 支持的导出格式：
 * - CSV：逗号分隔值文件，通用表格数据交换格式
 * - Excel：Microsoft Excel格式（.xlsx）
 * - PDF：便携式文档格式
 *
 * 使用示例：
 * @code
 * ExportService exportService;
 * exportService.exportToCsv("/path/to/output.csv", dataStore);
 * @endcode
 *
 * @author 1553BTools
 * @date 2024
 */

#ifndef EXPORTSERVICE_H
#define EXPORTSERVICE_H

#include <QObject>
#include <QString>
#include "core/datastore/DataStore.h"

/**
 * @brief 数据导出服务类
 *
 * 该类提供异步数据导出功能，通过信号报告导出进度和结果。
 * 导出操作在后台线程执行，不阻塞UI。
 */
class ExportService : public QObject
{
    Q_OBJECT

public:
    explicit ExportService(QObject *parent = nullptr);
    ~ExportService();

    /**
     * @brief 导出为CSV格式
     * @param filePath 输出文件路径
     * @param store 数据存储对象指针
     * @return 导出成功返回true，失败返回false
     *
     * CSV格式特点：
     * - 使用UTF-8 BOM编码（确保Excel正确识别中文）
     * - 逗号分隔字段
     * - 包含表头行
     */
    bool exportToCsv(const QString& filePath, DataStore* store);

    /**
     * @brief 导出为Excel格式
     * @param filePath 输出文件路径
     * @param store 数据存储对象指针
     * @return 导出成功返回true，失败返回false
     */
    bool exportToExcel(const QString& filePath, DataStore* store);

    /**
     * @brief 导出为PDF格式
     * @param filePath 输出文件路径
     * @param store 数据存储对象指针
     * @return 导出成功返回true，失败返回false
     */
    bool exportToPdf(const QString& filePath, DataStore* store);

    /**
     * @brief 获取最后的错误信息
     * @return 错误描述字符串，无错误返回空字符串
     */
    QString lastError() const;

signals:
    /**
     * @brief 导出进度信号
     * @param current 当前已导出条数
     * @param total 总条数
     */
    void exportProgress(int current, int total);

    /**
     * @brief 导出完成信号
     * @param success 是否成功
     */
    void exportFinished(bool success);

private:
    QString m_lastError;    ///< 最后的错误信息
};

#endif
