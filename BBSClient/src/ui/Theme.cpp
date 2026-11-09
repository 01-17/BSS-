#include "ui/Theme.h"

#include <QFont>
#include <QLabel>
#include <QPushButton>
#include <QWidget>

namespace bbs {
namespace theme {

QString styleSheet()
{
    // 说明：仅使用 Qt 原生支持的 QSS 子集，避免在不同 Qt 版本/平台上失效。
    return QStringLiteral(R"QSS(
QWidget {
    font-family: "Microsoft YaHei UI", "Microsoft YaHei", "PingFang SC", sans-serif;
    font-size: 13px;
    color: #1f2328;
}
QMainWindow, QDialog {
    background: #f5f6f8;
}
QLabel#pageTitle {
    font-size: 20px;
    font-weight: 600;
    color: #111827;
}
QLabel#sectionTitle {
    font-size: 15px;
    font-weight: 600;
    color: #111827;
}
QLabel#metaText {
    color: #6b7280;
    font-size: 12px;
}
QLabel#stateHint {
    color: #6b7280;
    font-size: 14px;
}
QLabel#formError {
    color: #dc2626;
    font-size: 12px;
}
QFrame#card {
    background: #ffffff;
    border: 1px solid #e5e7eb;
    border-radius: 8px;
}
QFrame#navBar {
    background: #ffffff;
    border-bottom: 1px solid #e5e7eb;
}
QFrame#topBar {
    background: #111827;
    border: none;
}
QLabel#topBarTitle {
    color: #ffffff;
    font-size: 15px;
    font-weight: 600;
}
QLabel#topBarUser {
    color: #d1d5db;
    font-size: 12px;
}
QPushButton {
    background: #ffffff;
    border: 1px solid #d1d5db;
    border-radius: 6px;
    padding: 6px 14px;
    color: #1f2328;
}
QPushButton:hover {
    background: #f3f4f6;
}
QPushButton:disabled {
    color: #9ca3af;
    background: #f3f4f6;
    border-color: #e5e7eb;
}
QPushButton#primaryButton {
    background: #2563eb;
    border: 1px solid #2563eb;
    color: #ffffff;
    font-weight: 600;
}
QPushButton#primaryButton:hover {
    background: #1d4ed8;
}
QPushButton#primaryButton:disabled {
    background: #93b4f5;
    border-color: #93b4f5;
    color: #ffffff;
}
QPushButton#dangerButton {
    background: #ffffff;
    border: 1px solid #dc2626;
    color: #dc2626;
}
QPushButton#dangerButton:hover {
    background: #fef2f2;
}
QPushButton#linkButton {
    background: transparent;
    border: none;
    color: #2563eb;
    padding: 2px 4px;
}
QLabel#brandLabel {
    font-size: 16px;
    font-weight: 700;
    color: #111827;
}
QFrame#statusBar {
    background: #ffffff;
    border-top: 1px solid #e5e7eb;
}
QLabel#statusText {
    color: #6b7280;
    font-size: 12px;
}
QPushButton#navButton {
    background: transparent;
    border: none;
    border-radius: 4px;
    padding: 6px 12px;
    color: #374151;
}
QPushButton#navButton:hover {
    background: #f3f4f6;
}
QPushButton#navButton:checked {
    background: #e5e7eb;
    color: #111827;
}
QLineEdit, QPlainTextEdit, QTextEdit, QComboBox, QDateEdit, QSpinBox {
    background: #ffffff;
    border: 1px solid #d1d5db;
    border-radius: 6px;
    padding: 6px 8px;
    selection-background-color: #bfdbfe;
}
QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus, QComboBox:focus {
    border-color: #2563eb;
}
QLineEdit:disabled, QPlainTextEdit:disabled {
    background: #f3f4f6;
    color: #6b7280;
}
QListWidget, QTreeWidget, QTableWidget {
    background: #ffffff;
    border: 1px solid #e5e7eb;
    border-radius: 8px;
    outline: none;
}
QListWidget::item {
    padding: 10px 12px;
    border-bottom: 1px solid #f1f2f4;
}
QListWidget::item:selected {
    background: #eff6ff;
    color: #111827;
}
QHeaderView::section {
    background: #f9fafb;
    border: none;
    border-bottom: 1px solid #e5e7eb;
    padding: 8px;
    font-weight: 600;
}
QProgressBar {
    border: none;
    background: #e5e7eb;
    border-radius: 3px;
    max-height: 6px;
}
QProgressBar::chunk {
    background: #2563eb;
    border-radius: 3px;
}
QStatusBar {
    background: #ffffff;
    border-top: 1px solid #e5e7eb;
    color: #4b5563;
}
)QSS");
}

QLabel* pageTitle(const QString& text, QWidget* parent)
{
    QLabel* label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("pageTitle"));
    return label;
}

QLabel* sectionTitle(const QString& text, QWidget* parent)
{
    QLabel* label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("sectionTitle"));
    return label;
}

QLabel* metaLabel(const QString& text, QWidget* parent)
{
    QLabel* label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("metaText"));
    return label;
}

QLabel* errorLabel(QWidget* parent)
{
    QLabel* label = new QLabel(parent);
    label->setObjectName(QStringLiteral("formError"));
    label->setWordWrap(true);
    label->clear();
    return label;
}

QPushButton* primaryButton(const QString& text, QWidget* parent)
{
    QPushButton* button = new QPushButton(text, parent);
    button->setObjectName(QStringLiteral("primaryButton"));
    button->setCursor(Qt::PointingHandCursor);
    button->setMinimumHeight(32);
    return button;
}

QPushButton* dangerButton(const QString& text, QWidget* parent)
{
    QPushButton* button = new QPushButton(text, parent);
    button->setObjectName(QStringLiteral("dangerButton"));
    button->setCursor(Qt::PointingHandCursor);
    button->setMinimumHeight(30);
    return button;
}

QPushButton* normalButton(const QString& text, QWidget* parent)
{
    QPushButton* button = new QPushButton(text, parent);
    button->setCursor(Qt::PointingHandCursor);
    button->setMinimumHeight(30);
    return button;
}

QPushButton* navButton(const QString& text, QWidget* parent)
{
    QPushButton* button = new QPushButton(text, parent);
    button->setObjectName(QStringLiteral("navButton"));
    button->setCursor(Qt::PointingHandCursor);
    button->setMinimumHeight(30);
    return button;
}

QLabel* tagLabel(const QString& text, const QString& colorName, QWidget* parent)
{
    QLabel* label = new QLabel(text, parent);
    label->setStyleSheet(QStringLiteral(
                             "background:%1; color:#ffffff; border-radius:3px;"
                             "padding:1px 6px; font-size:11px;")
                             .arg(colorName));
    return label;
}

} // namespace theme
} // namespace bbs
