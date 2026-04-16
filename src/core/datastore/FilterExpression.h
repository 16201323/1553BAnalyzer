/**
 * @file FilterExpression.h
 * @brief 筛选表达式解析和评估系统
 * 
 * 本文件定义了筛选表达式的解析和评估功能，支持：
 * - 简单条件：>1, <3, =8, >=5, <=10, !=0
 * - 逻辑运算：&& (AND), || (OR)
 * - 分号分隔多个条件：a;>1;<3&&>10
 * 
 * 表达式语法：
 * - 比较运算符：>、<、=、>=、<=、!=
 * - 逻辑运算符：&&（与）、||（或）
 * - 分隔符：;（分隔多个独立条件）
 * 
 * 示例：
 * - ">1" - 大于1
 * - "<3&&>1" - 小于3且大于1
 * - "<3||>10" - 小于3或大于10
 * - ">=5&&<=10" - 大于等于5且小于等于10
 * 
 * @author 1553BTools
 * @date 2024
 */

#ifndef FILTEREXPRESSION_H
#define FILTEREXPRESSION_H

#include <QString>
#include <QVariant>
#include <QVector>
#include <QRegularExpression>

/**
 * @brief 比较运算符枚举
 */
enum class CompareOperator {
    Greater,        ///< 大于 (>)
    Less,           ///< 小于 (<)
    Equal,          ///< 等于 (=)
    GreaterEqual,   ///< 大于等于 (>=)
    LessEqual,      ///< 小于等于 (<=)
    NotEqual        ///< 不等于 (!=)
};

/**
 * @brief 逻辑运算符枚举
 */
enum class LogicOperator {
    And,            ///< 逻辑与 (&&)
    Or              ///< 逻辑或 (||)
};

/**
 * @brief 单个条件项
 * 
 * 表示一个简单的比较条件，如 ">1" 或 "<=10"
 */
struct ConditionItem {
    CompareOperator op;     ///< 比较运算符
    QVariant value;         ///< 比较值
    bool isValid;           ///< 是否有效
    
    ConditionItem() : isValid(false) {}
};

/**
 * @brief 条件组
 * 
 * 表示由逻辑运算符连接的多个条件项
 * 例如：">1&&<3" 或 "<3||>10"
 */
struct ConditionGroup {
    QVector<ConditionItem> items;           ///< 条件项列表
    QVector<LogicOperator> operators;       ///< 条件项之间的逻辑运算符
    bool isValid;                           ///< 是否有效
    
    ConditionGroup() : isValid(false) {}
    
    /**
     * @brief 评估条件组
     * @param actualValue 实际值
     * @return 是否满足条件
     */
    bool evaluate(const QVariant& actualValue) const;
};

/**
 * @brief 筛选表达式类
 * 
 * 解析和评估筛选表达式，支持复杂的条件组合
 */
class FilterExpression
{
public:
    /**
     * @brief 构造函数
     */
    FilterExpression();
    
    /**
     * @brief 构造函数，直接解析表达式
     * @param expression 表达式字符串
     */
    explicit FilterExpression(const QString& expression);
    
    /**
     * @brief 设置表达式
     * @param expression 表达式字符串
     * @return 解析是否成功
     */
    bool setExpression(const QString& expression);
    
    /**
     * @brief 获取表达式字符串
     * @return 表达式字符串
     */
    QString getExpression() const { return m_expression; }
    
    /**
     * @brief 评估表达式
     * @param actualValue 实际值
     * @return 是否满足条件
     */
    bool evaluate(const QVariant& actualValue) const;
    
    /**
     * @brief 检查表达式是否有效
     * @return 表达式是否有效
     */
    bool isValid() const { return m_isValid; }
    
    /**
     * @brief 获取错误信息
     * @return 错误信息字符串
     */
    QString getError() const { return m_error; }
    
    /**
     * @brief 获取所有条件组
     * @return 条件组列表
     */
    const QVector<ConditionGroup>& getConditionGroups() const { return m_conditionGroups; }
    
private:
    /**
     * @brief 解析表达式
     * @return 解析是否成功
     */
    bool parseExpression();
    
    /**
     * @brief 解析单个条件组
     * @param groupStr 条件组字符串
     * @param group 输出的条件组
     * @return 解析是否成功
     */
    bool parseConditionGroup(const QString& groupStr, ConditionGroup& group);
    
    /**
     * @brief 解析单个条件项
     * @param itemStr 条件项字符串
     * @param item 输出的条件项
     * @return 解析是否成功
     */
    bool parseConditionItem(const QString& itemStr, ConditionItem& item);
    
    /**
     * @brief 解析比较运算符
     * @param opStr 运算符字符串
     * @param op 输出的运算符
     * @return 解析是否成功
     */
    bool parseCompareOperator(const QString& opStr, CompareOperator& op);
    
    /**
     * @brief 评估单个条件项
     * @param item 条件项
     * @param actualValue 实际值
     * @return 是否满足条件
     */
    bool evaluateConditionItem(const ConditionItem& item, const QVariant& actualValue) const;
    
    QString m_expression;                   ///< 原始表达式字符串
    QVector<ConditionGroup> m_conditionGroups;  ///< 条件组列表
    bool m_isValid;                         ///< 表达式是否有效
    QString m_error;                        ///< 错误信息
};

#endif // FILTEREXPRESSION_H
