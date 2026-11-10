#pragma once

// 统一视觉：集中 QSS 与少量构建辅助，保证 P01-P13 的控件命名与样式一致。
// 颜色/间距仅在此处调整，页面代码不写内联样式。

#include <QString>

class QLabel;
class QPushButton;
class QWidget;

namespace bbs {
namespace theme {

QString styleSheet();

// 页面大标题（如"版块列表"）
QLabel* pageTitle(const QString& text, QWidget* parent);
// 卡片内小节标题
QLabel* sectionTitle(const QString& text, QWidget* parent);
// 次要说明文字（时间、作者、计数等）
QLabel* metaLabel(const QString& text, QWidget* parent);
// 表单错误提示（红字，默认隐藏由调用方控制）
QLabel* errorLabel(QWidget* parent);

// 按钮构造：primary（主操作）/ danger（破坏性）/ normal（次要）
QPushButton* primaryButton(const QString& text, QWidget* parent);
QPushButton* dangerButton(const QString& text, QWidget* parent);
QPushButton* normalButton(const QString& text, QWidget* parent);
// 顶部导航条内的扁平文字按钮（白色导航条底色）
QPushButton* navButton(const QString& text, QWidget* parent);

// 标记（置顶 / 精华 / 已停用）
QLabel* tagLabel(const QString& text, const QString& colorName, QWidget* parent);

} // namespace theme
} // namespace bbs
