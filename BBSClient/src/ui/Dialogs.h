#pragma once

// 对话框工具：统一二次确认与"理由输入"交互。
// 关键约束（需求 IF-001 / NFR-003）：
//  - 删除、移除、停用、撤销版主等破坏性操作必须二次确认，且默认焦点在"取消"；
//  - 处置理由为必填（1-200 字符），取消则不发请求。

#include <QString>

class QWidget;

namespace bbs {
namespace dialogs {

// 普通确认：默认按钮为"取消"，返回 true 表示用户确认
bool confirm(QWidget* parent, const QString& title, const QString& text,
             const QString& acceptText = QStringLiteral("确认"));

// 破坏性操作确认（红色措辞 + 默认取消），text 需明确说明后果
bool confirmDestructive(QWidget* parent, const QString& title, const QString& text,
                        const QString& acceptText);

// 需要填写理由的确认：校验 1-200 字符，用户取消返回 false
bool promptReason(QWidget* parent, const QString& title, const QString& text,
                  const QString& reasonLabel, QString* reason);

// 信息提示
void info(QWidget* parent, const QString& title, const QString& text);
void warn(QWidget* parent, const QString& title, const QString& text);

} // namespace dialogs
} // namespace bbs
