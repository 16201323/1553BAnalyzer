/**
 * @file FilterExpression.cpp
 * @brief 筛选表达式解析和评估系统实现
 * 
 * 本文件实现了筛选表达式的解析和评估功能，包括：
 * - 表达式词法分析
 * - 条件项解析
 * - 逻辑运算处理
 * - 表达式评估
 * 
 * @author 1553BTools
 * @date 2024
 */

#include "FilterExpression.h"
#include <QDebug>
#include <QRegularExpression>
#include <cmath>

/**
 * @brief 构造函数
 */
FilterExpression::FilterExpression()
    : m_isValid(false)
{
}

/**
 * @brief 构造函数，直接解析表达式
 * @param expression 表达式字符串
 */
FilterExpression::FilterExpression(const QString& expression)
    : m_isValid(false)
{
    setExpression(expression);
}

/**
 * @brief 设置表达式
 * @param expression 表达式字符串
 * @return 解析是否成功
 */
bool FilterExpression::setExpression(const QString& expression)
{
    m_expression = expression.trimmed();
    m_error.clear();
    m_conditionGroups.clear();
    m_isValid = false;
    
    // 空表达式视为无效
    if (m_expression.isEmpty()) {
        m_error = "表达式不能为空";
        return false;
    }
    
    // 解析表达式
    return parseExpression();
}

/**
 * @brief 解析表达式
 * @return 解析是否成功
 */
bool FilterExpression::parseExpression()
{
    // 按分号分割多个条件组
    QStringList groupStrs = m_expression.split(';', Qt::SkipEmptyParts);
    
    if (groupStrs.isEmpty()) {
        m_error = "未找到有效的条件";
        return false;
    }
    
    // 解析每个条件组
    for (const QString& groupStr : groupStrs) {
        ConditionGroup group;
        if (!parseConditionGroup(groupStr.trimmed(), group)) {
            return false;
        }
        m_conditionGroups.append(group);
    }
    
    m_isValid = true;
    return true;
}

/**
 * @brief 解析单个条件组
 * @param groupStr 条件组字符串
 * @param group 输出的条件组
 * @return 解析是否成功
 */
bool FilterExpression::parseConditionGroup(const QString& groupStr, ConditionGroup& group)
{
    // 使用正则表达式分割条件项和逻辑运算符
    // 匹配模式：(条件项)(逻辑运算符)(条件项)...
    QRegularExpression itemPattern(R"((>=|<=|!=|>|<|=)\s*([^\s&|]+))");
    
    int pos = 0;
    QRegularExpressionMatch match;
    bool firstItem = true;
    
    // 查找所有条件项
    while ((match = itemPattern.match(groupStr, pos)).hasMatch()) {
        // 检查条件项之间的逻辑运算符
        if (!firstItem) {
            // 提取前一个条件项和当前条件项之间的逻辑运算符
            QString between = groupStr.mid(pos, match.capturedStart() - pos).trimmed();
            if (between == "&&") {
                group.operators.append(LogicOperator::And);
            } else if (between == "||") {
                group.operators.append(LogicOperator::Or);
            } else if (between.isEmpty()) {
                // 默认使用AND连接
                group.operators.append(LogicOperator::And);
            } else {
                m_error = QString("无效的逻辑运算符: %1").arg(between);
                return false;
            }
        }
        
        // 解析条件项
        ConditionItem item;
        QString opStr = match.captured(1);
        QString valueStr = match.captured(2);
        
        if (!parseCompareOperator(opStr, item.op)) {
            return false;
        }
        
        // 尝试转换为数值
        bool ok;
        double numValue = valueStr.toDouble(&ok);
        if (ok) {
            item.value = numValue;
        } else {
            // 作为字符串处理
            item.value = valueStr;
        }
        
        item.isValid = true;
        group.items.append(item);
        
        pos = match.capturedEnd();
        firstItem = false;
    }
    
    if (group.items.isEmpty()) {
        m_error = QString("条件组中未找到有效的条件: %1").arg(groupStr);
        return false;
    }
    
    group.isValid = true;
    return true;
}

/**
 * @brief 解析单个条件项
 * @param itemStr 条件项字符串
 * @param item 输出的条件项
 * @return 解析是否成功
 */
bool FilterExpression::parseConditionItem(const QString& itemStr, ConditionItem& item)
{
    // 匹配比较运算符和值
    QRegularExpression pattern(R"((>=|<=|!=|>|<|=)\s*(.+))");
    QRegularExpressionMatch match = pattern.match(itemStr.trimmed());
    
    if (!match.hasMatch()) {
        m_error = QString("无效的条件项: %1").arg(itemStr);
        return false;
    }
    
    QString opStr = match.captured(1);
    QString valueStr = match.captured(2).trimmed();
    
    if (!parseCompareOperator(opStr, item.op)) {
        return false;
    }
    
    // 尝试转换为数值
    bool ok;
    double numValue = valueStr.toDouble(&ok);
    if (ok) {
        item.value = numValue;
    } else {
        // 作为字符串处理
        item.value = valueStr;
    }
    
    item.isValid = true;
    return true;
}

/**
 * @brief 解析比较运算符
 * @param opStr 运算符字符串
 * @param op 输出的运算符
 * @return 解析是否成功
 */
bool FilterExpression::parseCompareOperator(const QString& opStr, CompareOperator& op)
{
    if (opStr == ">") {
        op = CompareOperator::Greater;
    } else if (opStr == "<") {
        op = CompareOperator::Less;
    } else if (opStr == "=") {
        op = CompareOperator::Equal;
    } else if (opStr == ">=") {
        op = CompareOperator::GreaterEqual;
    } else if (opStr == "<=") {
        op = CompareOperator::LessEqual;
    } else if (opStr == "!=") {
        op = CompareOperator::NotEqual;
    } else {
        m_error = QString("无效的比较运算符: %1").arg(opStr);
        return false;
    }
    return true;
}

/**
 * @brief 评估表达式
 * @param actualValue 实际值
 * @return 是否满足条件
 */
bool FilterExpression::evaluate(const QVariant& actualValue) const
{
    if (!m_isValid || m_conditionGroups.isEmpty()) {
        return false;
    }
    
    // 多个条件组之间是OR关系
    for (const ConditionGroup& group : m_conditionGroups) {
        if (group.evaluate(actualValue)) {
            return true;
        }
    }
    
    return false;
}

/**
 * @brief 评估单个条件项
 * @param item 条件项
 * @param actualValue 实际值
 * @return 是否满足条件
 */
bool FilterExpression::evaluateConditionItem(const ConditionItem& item, const QVariant& actualValue) const
{
    // 尝试数值比较
    bool actualOk, itemOk;
    double actualNum = actualValue.toDouble(&actualOk);
    double itemNum = item.value.toDouble(&itemOk);
    
    if (actualOk && itemOk) {
        // 数值比较
        switch (item.op) {
        case CompareOperator::Greater:
            return actualNum > itemNum;
        case CompareOperator::Less:
            return actualNum < itemNum;
        case CompareOperator::Equal:
            return std::abs(actualNum - itemNum) < 1e-9;
        case CompareOperator::GreaterEqual:
            return actualNum >= itemNum;
        case CompareOperator::LessEqual:
            return actualNum <= itemNum;
        case CompareOperator::NotEqual:
            return std::abs(actualNum - itemNum) >= 1e-9;
        }
    } else {
        // 字符串比较
        QString actualStr = actualValue.toString();
        QString itemStr = item.value.toString();
        
        switch (item.op) {
        case CompareOperator::Greater:
            return actualStr > itemStr;
        case CompareOperator::Less:
            return actualStr < itemStr;
        case CompareOperator::Equal:
            return actualStr == itemStr;
        case CompareOperator::GreaterEqual:
            return actualStr >= itemStr;
        case CompareOperator::LessEqual:
            return actualStr <= itemStr;
        case CompareOperator::NotEqual:
            return actualStr != itemStr;
        }
    }
    
    return false;
}

/**
 * @brief 评估条件组
 * @param actualValue 实际值
 * @return 是否满足条件
 */
bool ConditionGroup::evaluate(const QVariant& actualValue) const
{
    if (!isValid || items.isEmpty()) {
        return false;
    }
    
    // 第一个条件项
    bool result = false;
    if (items.size() > 0) {
        // 需要访问FilterExpression的私有方法，这里简化实现
        // 直接在这里实现评估逻辑
        
        const ConditionItem& item = items[0];
        bool actualOk, itemOk;
        double actualNum = actualValue.toDouble(&actualOk);
        double itemNum = item.value.toDouble(&itemOk);
        
        if (actualOk && itemOk) {
            switch (item.op) {
            case CompareOperator::Greater:
                result = actualNum > itemNum;
                break;
            case CompareOperator::Less:
                result = actualNum < itemNum;
                break;
            case CompareOperator::Equal:
                result = std::abs(actualNum - itemNum) < 1e-9;
                break;
            case CompareOperator::GreaterEqual:
                result = actualNum >= itemNum;
                break;
            case CompareOperator::LessEqual:
                result = actualNum <= itemNum;
                break;
            case CompareOperator::NotEqual:
                result = std::abs(actualNum - itemNum) >= 1e-9;
                break;
            }
        } else {
            QString actualStr = actualValue.toString();
            QString itemStr = item.value.toString();
            
            switch (item.op) {
            case CompareOperator::Greater:
                result = actualStr > itemStr;
                break;
            case CompareOperator::Less:
                result = actualStr < itemStr;
                break;
            case CompareOperator::Equal:
                result = actualStr == itemStr;
                break;
            case CompareOperator::GreaterEqual:
                result = actualStr >= itemStr;
                break;
            case CompareOperator::LessEqual:
                result = actualStr <= itemStr;
                break;
            case CompareOperator::NotEqual:
                result = actualStr != itemStr;
                break;
            }
        }
    }
    
    // 后续条件项，根据逻辑运算符组合
    for (int i = 1; i < items.size(); i++) {
        const ConditionItem& item = items[i];
        LogicOperator logicOp = operators[i - 1];
        
        bool itemResult = false;
        bool actualOk, itemOk;
        double actualNum = actualValue.toDouble(&actualOk);
        double itemNum = item.value.toDouble(&itemOk);
        
        if (actualOk && itemOk) {
            switch (item.op) {
            case CompareOperator::Greater:
                itemResult = actualNum > itemNum;
                break;
            case CompareOperator::Less:
                itemResult = actualNum < itemNum;
                break;
            case CompareOperator::Equal:
                itemResult = std::abs(actualNum - itemNum) < 1e-9;
                break;
            case CompareOperator::GreaterEqual:
                itemResult = actualNum >= itemNum;
                break;
            case CompareOperator::LessEqual:
                itemResult = actualNum <= itemNum;
                break;
            case CompareOperator::NotEqual:
                itemResult = std::abs(actualNum - itemNum) >= 1e-9;
                break;
            }
        } else {
            QString actualStr = actualValue.toString();
            QString itemStr = item.value.toString();
            
            switch (item.op) {
            case CompareOperator::Greater:
                itemResult = actualStr > itemStr;
                break;
            case CompareOperator::Less:
                itemResult = actualStr < itemStr;
                break;
            case CompareOperator::Equal:
                itemResult = actualStr == itemStr;
                break;
            case CompareOperator::GreaterEqual:
                itemResult = actualStr >= itemStr;
                break;
            case CompareOperator::LessEqual:
                itemResult = actualStr <= itemStr;
                break;
            case CompareOperator::NotEqual:
                itemResult = actualStr != itemStr;
                break;
            }
        }
        
        // 应用逻辑运算符
        if (logicOp == LogicOperator::And) {
            result = result && itemResult;
        } else {
            result = result || itemResult;
        }
    }
    
    return result;
}
