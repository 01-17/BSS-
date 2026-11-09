#include "ui/Dialogs.h"

#include "util/Validators.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace bbs {
namespace dialogs {

bool confirm(QWidget* parent, const QString& title, const QString& text,
             const QString& acceptText)
{
    QMessageBox box(parent);
    box.setWindowTitle(title);
    box.setText(text);
    box.setIcon(QMessageBox::Question);
    QPushButton* accept = box.addButton(acceptText, QMessageBox::AcceptRole);
    QPushButton* cancel = box.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
    box.setDefaultButton(cancel);   // 默认落在"取消"，避免误触
    box.setEscapeButton(cancel);
    box.exec();
    return box.clickedButton() == accept;
}

bool confirmDestructive(QWidget* parent, const QString& title, const QString& text,
                        const QString& acceptText)
{
    QMessageBox box(parent);
    box.setWindowTitle(title);
    box.setText(text);
    box.setIcon(QMessageBox::Warning);
    QPushButton* accept = box.addButton(acceptText, QMessageBox::DestructiveRole);
    QPushButton* cancel = box.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
    box.setDefaultButton(cancel);
    box.setEscapeButton(cancel);
    box.exec();
    return box.clickedButton() == accept;
}

bool promptReason(QWidget* parent, const QString& title, const QString& text,
                  const QString& reasonLabel, QString* reason)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.setMinimumWidth(420);

    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->setSpacing(10);

    QLabel* hint = new QLabel(text, &dialog);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    QLabel* label = new QLabel(reasonLabel, &dialog);
    layout->addWidget(label);

    QPlainTextEdit* edit = new QPlainTextEdit(&dialog);
    edit->setPlaceholderText(QStringLiteral("1-200 个字符，将记录到操作日志"));
    edit->setFixedHeight(90);
    if (reason)
        edit->setPlainText(*reason);
    layout->addWidget(edit);

    QLabel* error = new QLabel(&dialog);
    error->setObjectName(QStringLiteral("formError"));
    error->setWordWrap(true);
    layout->addWidget(error);

    QDialogButtonBox* buttons = new QDialogButtonBox(&dialog);
    QPushButton* submit = buttons->addButton(QStringLiteral("确认移除"), QDialogButtonBox::AcceptRole);
    QPushButton* cancel = buttons->addButton(QStringLiteral("取消"), QDialogButtonBox::RejectRole);
    cancel->setDefault(true);
    layout->addWidget(buttons);

    QObject::connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);

    bool accepted = false;
    QObject::connect(submit, &QPushButton::clicked, &dialog, [&]() {
        const validators::Check check = validators::reason(edit->toPlainText());
        if (!check.ok) {
            error->setText(check.error);
            return;
        }
        if (reason)
            *reason = check.normalized;
        accepted = true;
        dialog.accept();
    });

    dialog.exec();
    return accepted;
}

void info(QWidget* parent, const QString& title, const QString& text)
{
    QMessageBox::information(parent, title, text, QStringLiteral("知道了"));
}

void warn(QWidget* parent, const QString& title, const QString& text)
{
    QMessageBox::warning(parent, title, text, QStringLiteral("知道了"));
}

} // namespace dialogs
} // namespace bbs
