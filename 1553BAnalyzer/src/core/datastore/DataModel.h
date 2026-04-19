/**
 * @file DataModel.h
 * @brief 数据表格模型类定义
 * 
 * DataModel类继承自QAbstractTableModel，为数据表格视图提供数据模型。
 * 该类作为DataStore和TableView之间的桥梁，负责：
 * - 将DataStore中的数据转换为表格显示格式
 * - 提供列定义和表头信息
 * - 支持数据排序功能
 * - 提供数据变化通知机制
 * 
 * 使用示例：
 * @code
 * DataModel* model = new DataModel(this);
 * model->setDataStore(dataStore);
 * tableView->setModel(model);
 * @endcode
 * 
 * @author 1553BTools
 * @date 2024
 */

#ifndef DATAMODEL_H
#define DATAMODEL_H

#include <QAbstractTableModel>
#include <QVariant>
#include "DataStore.h"

/**
 * @brief 数据表格模型类
 * 
 * 该类实现了Qt的Model/View架构中的模型部分，
 * 将DataStore中的1553B数据以表格形式展示。
 */
class DataModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    /**
     * @brief 表格列枚举定义
     * 
     * 定义了表格中显示的所有列，顺序与显示顺序一致
     */
    enum Column {
        ColRowIndex = 0,    // 行序号（从1开始显示）
        ColMpuId,           // MPU任务机ID
        ColPacketLen,       // 数据包长度
        ColDate,            // 日期（年-月-日）
        ColTimestamp,       // 时间戳（时:分:秒.毫秒）
        ColMessageType,     // 消息类型（BC→RT、RT→BC等）
        ColTerminalAddr,    // 终端地址
        ColSubAddr,         // 子地址
        ColTR,              // 收发状态（BC→RT或RT→BC）
        ColDataCount,       // 数据计数/发送码
        ColStatus,          // 传输状态（成功/失败）
        ColDataHex,         // 数据内容（十六进制显示）
        ColCount            // 列总数，用于遍历
    };

    /**
     * @brief 构造函数
     * @param parent 父对象指针
     */
    explicit DataModel(QObject *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~DataModel();
    
    /**
     * @brief 设置数据存储对象
     * @param store DataStore对象指针
     * 
     * 设置数据源，并建立信号连接：
     * - 数据变化时自动刷新表格
     * - 筛选条件变化时更新显示
     */
    void setDataStore(DataStore* store);
    
    /**
     * @brief 获取行数
     * @param parent 父索引（表格模型中通常无效）
     * @return 数据总行数
     * 
     * 重写QAbstractTableModel方法
     */
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    
    /**
     * @brief 获取列数
     * @param parent 父索引（表格模型中通常无效）
     * @return 列总数（ColCount）
     * 
     * 重写QAbstractTableModel方法
     */
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    
    /**
     * @brief 获取单元格数据
     * @param index 单元格索引
     * @param role 数据角色（显示、背景色、对齐方式等）
     * @return 单元格数据
     * 
     * 重写QAbstractTableModel方法
     * 支持的角色：
     * - Qt::DisplayRole: 显示文本
     * - Qt::BackgroundRole: 背景色（根据状态和类型着色）
     * - Qt::TextAlignmentRole: 文本对齐方式
     * - Qt::ToolTipRole: 工具提示（完整数据显示）
     */
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    
    /**
     * @brief 获取表头数据
     * @param section 节（行号或列号）
     * @param orientation 方向（水平或垂直）
     * @param role 数据角色
     * @return 表头文本
     * 
     * 重写QAbstractTableModel方法
     */
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    
    /**
     * @brief 排序数据
     * @param column 排序列
     * @param order 排序顺序（升序或降序）
     * 
     * 重写QAbstractTableModel方法
     * 支持按时间戳、终端地址、消息类型排序
     */
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;
    
    /**
     * @brief 获取指定行的数据记录
     * @param row 行号
     * @return 数据记录，无效行返回空记录
     */
    DataRecord getRecord(int row) const;
    
    /**
     * @brief 从模型索引获取行号
     * @param index 模型索引
     * @return 行号，无效索引返回-1
     */
    int getRowIndex(const QModelIndex& index) const;
    
    /**
     * @brief 刷新模型
     * 
     * 触发模型重置，通知视图更新所有数据
     */
    void refresh();
    
signals:
    /**
     * @brief 数据加载完成信号
     * @param count 加载的数据条数
     * 
     * 当数据发生变化并完成模型更新后发送
     */
    void dataLoaded(int count);

private slots:
    /**
     * @brief 数据变化槽函数
     * 
     * 响应DataStore::dataChanged信号，重置模型
     */
    void onDataChanged();
    
    /**
     * @brief 筛选条件变化槽函数
     * 
     * 响应DataStore::filterChanged信号，重置模型
     */
    void onFilterChanged();

private:
    DataStore* m_dataStore;           // 数据存储对象指针
    Qt::SortOrder m_currentSortOrder; // 当前排序顺序
    int m_currentSortColumn;          // 当前排序列
};

#endif
