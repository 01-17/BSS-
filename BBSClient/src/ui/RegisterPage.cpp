#include "ui/RegisterPage.h"

#include "model/JsonMapper.h"
#include "net/ApiClient.h"
#include "session/SessionManager.h"
#include "ui/Theme.h"
#include "util/Validators.h"

#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace bbs {

RegisterPage::RegisterPage(const PageContext& context, QWidget* parent)
    : Page(context, parent)
{
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addStretch(1);

    QHBoxLayout* center = new QHBoxLayout;
    center->addStretch(1);

    QFrame* card = new QFrame(this);
    card->setObjectName(QStringLiteral("card"));
    card->setFixedWidth(420);
    QVBoxLayout* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(28, 28, 28, 24);
    cardLayout->setSpacing(14);

    cardLayout->addWidget(theme::pageTitle(QStringLiteral("注册新账号"), card));
    cardLayout->addWidget(theme::metaLabel(
        QStringLiteral("注册后角色为普通成员（member），如需版主权限请联系管理员授权"), card));

    QFormLayout* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setSpacing(10);

    m_usernameEdit = new QLineEdit(card);
    m_usernameEdit->setPlaceholderText(QStringLiteral("4-20 位字母 / 数字 / 下划线"));
    m_usernameEdit->setMaxLength(32);
    form->addRow(QStringLiteral("用户名"), m_usernameEdit);

    m_nicknameEdit = new QLineEdit(card);
    m_nicknameEdit->setPlaceholderText(QStringLiteral("1-30 个字符，论坛内展示"));
    m_nicknameEdit->setMaxLength(64);
    form->addRow(QStringLiteral("昵称"), m_nicknameEdit);

    m_passwordEdit = new QLineEdit(card);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText(QStringLiteral("8-64 个字符"));
    m_passwordEdit->setMaxLength(64);
    form->addRow(QStringLiteral("密码"), m_passwordEdit);

    m_confirmEdit = new QLineEdit(card);
    m_confirmEdit->setEchoMode(QLineEdit::Password);
    m_confirmEdit->setPlaceholderText(QStringLiteral("再次输入密码"));
    m_confirmEdit->setMaxLength(64);
    form->addRow(QStringLiteral("确认密码"), m_confirmEdit);

    cardLayout->addLayout(form);

    m_errorLabel = theme::errorLabel(card);
    m_errorLabel->setVisible(false);
    cardLayout->addWidget(m_errorLabel);

    m_submitButton = theme::primaryButton(QStringLiteral("注册并登录"), card);
    cardLayout->addWidget(m_submitButton);

    QHBoxLayout* footer = new QHBoxLayout;
    footer->addStretch(1);
    m_backButton = new QPushButton(QStringLiteral("已有账号，返回登录"), card);
    m_backButton->setObjectName(QStringLiteral("linkButton"));
    m_backButton->setCursor(Qt::PointingHandCursor);
    footer->addWidget(m_backButton);
    footer->addStretch(1);
    cardLayout->addLayout(footer);

    center->addWidget(card, 0, Qt::AlignHCenter);
    center->addStretch(1);
    root->addLayout(center);
    root->addStretch(2);

    connect(m_submitButton, &QPushButton::clicked, this, &RegisterPage::submit);
    connect(m_confirmEdit, &QLineEdit::returnPressed, this, &RegisterPage::submit);
    connect(m_backButton, &QPushButton::clicked, this, [this]() {
        if (m_context.nav)
            m_context.nav->showLogin();
    });
}

void RegisterPage::onEnter()
{
    m_errorLabel->setVisible(false);
    setBusy(false);
}

void RegisterPage::showError(const QString& text)
{
    m_errorLabel->setText(text);
    m_errorLabel->setVisible(true);
}

void RegisterPage::setBusy(bool busy)
{
    m_submitButton->setEnabled(!busy);
    m_submitButton->setText(busy ? QStringLiteral("处理中…") : QStringLiteral("注册并登录"));
    m_backButton->setEnabled(!busy);
    m_usernameEdit->setEnabled(!busy);
    m_nicknameEdit->setEnabled(!busy);
    m_passwordEdit->setEnabled(!busy);
    m_confirmEdit->setEnabled(!busy);
}

void RegisterPage::submit()
{
    if (!m_context.isValid())
        return;

    const validators::Check username = validators::username(m_usernameEdit->text());
    if (!username.ok) {
        showError(username.error);
        return;
    }
    const validators::Check nickname = validators::nickname(m_nicknameEdit->text());
    if (!nickname.ok) {
        showError(nickname.error);
        return;
    }
    const validators::Check password = validators::password(m_passwordEdit->text(),
                                                           QStringLiteral("密码"));
    if (!password.ok) {
        showError(password.error);
        return;
    }
    if (m_passwordEdit->text() != m_confirmEdit->text()) {
        showError(QStringLiteral("两次输入的密码不一致"));
        return;
    }

    m_errorLabel->setVisible(false);
    setBusy(true);

    const QString usernameValue = username.normalized;
    const QString passwordValue = password.normalized;

    m_context.api->registerAccount(usernameValue, passwordValue, this,
                                   [this, usernameValue, passwordValue](const ApiResult& result) {
        if (!result.ok) {
            setBusy(false);
            showError(result.displayMessage());
            return; // 保留全部输入，由用户手动重试
        }

        // 注册成功：回读 RegisteredUser 以确认字段完整（结构不符按通信异常处理）
        RegisteredUser registered;
        QString error;
        if (!json::parseRegisteredUser(result.data, &registered, &error)) {
            setBusy(false);
            showError(QStringLiteral("通信数据异常：%1").arg(error));
            return;
        }

        m_submitButton->setText(QStringLiteral("注册成功，正在登录…"));
        m_context.api->login(usernameValue, passwordValue, this,
                             [this, usernameValue](const ApiResult& loginResult) {
            setBusy(false);
            if (!loginResult.ok) {
                // 注册已完成，仅自动登录失败：回登录页并预填用户名
                if (m_context.nav) {
                    m_context.nav->showLogin(QStringLiteral(
                        "账号已注册成功，请手动登录（自动登录未成功：%1）")
                                                 .arg(loginResult.displayMessage()));
                    m_context.nav->notify(QStringLiteral("账号已注册成功，请手动登录"));
                }
                return;
            }
            LoginResult parsed;
            QString parseError;
            if (!json::parseLoginResult(loginResult.data, &parsed, &parseError)) {
                if (m_context.nav) {
                    m_context.nav->showLogin(QStringLiteral("账号已注册成功，请手动登录"));
                }
                return;
            }
            m_context.session->applyLogin(parsed);
            if (m_context.nav)
                m_context.nav->showBoardList();
        });
    });
}

} // namespace bbs
