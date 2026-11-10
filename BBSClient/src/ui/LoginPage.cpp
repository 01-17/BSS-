#include "ui/LoginPage.h"

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

LoginPage::LoginPage(const PageContext& context, QWidget* parent)
    : Page(context, parent)
{
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addStretch(1);

    QHBoxLayout* center = new QHBoxLayout;
    center->addStretch(1);

    QFrame* card = new QFrame(this);
    card->setObjectName(QStringLiteral("card"));
    card->setFixedWidth(400);
    QVBoxLayout* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(28, 28, 28, 24);
    cardLayout->setSpacing(14);

    cardLayout->addWidget(theme::pageTitle(QStringLiteral("BBS 论坛登录"), card));
    m_hintLabel = theme::metaLabel(QStringLiteral("请使用演示账号登录（服务端需已启动）"), card);
    cardLayout->addWidget(m_hintLabel);

    QFormLayout* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setSpacing(10);

    m_usernameEdit = new QLineEdit(card);
    m_usernameEdit->setPlaceholderText(QStringLiteral("4-20 位字母 / 数字 / 下划线"));
    m_usernameEdit->setMaxLength(32);
    form->addRow(QStringLiteral("用户名"), m_usernameEdit);

    m_passwordEdit = new QLineEdit(card);
    m_passwordEdit->setEchoMode(QLineEdit::Password); // 密码遮蔽
    m_passwordEdit->setPlaceholderText(QStringLiteral("请输入密码"));
    m_passwordEdit->setMaxLength(64);
    form->addRow(QStringLiteral("密码"), m_passwordEdit);

    cardLayout->addLayout(form);

    m_errorLabel = theme::errorLabel(card);
    m_errorLabel->setVisible(false);
    cardLayout->addWidget(m_errorLabel);

    m_loginButton = theme::primaryButton(QStringLiteral("登录"), card);
    cardLayout->addWidget(m_loginButton);

    QHBoxLayout* footer = new QHBoxLayout;
    footer->addStretch(1);
    footer->addWidget(theme::metaLabel(QStringLiteral("还没有账号？"), card));
    m_registerButton = new QPushButton(QStringLiteral("注册新账号"), card);
    m_registerButton->setObjectName(QStringLiteral("linkButton"));
    m_registerButton->setCursor(Qt::PointingHandCursor);
    footer->addWidget(m_registerButton);
    footer->addStretch(1);
    cardLayout->addLayout(footer);

    center->addWidget(card, 0, Qt::AlignHCenter);
    center->addStretch(1);
    root->addLayout(center);
    root->addStretch(2);

    connect(m_loginButton, &QPushButton::clicked, this, &LoginPage::submit);
    connect(m_usernameEdit, &QLineEdit::returnPressed, m_passwordEdit,
            qOverload<>(&QWidget::setFocus));
    connect(m_passwordEdit, &QLineEdit::returnPressed, this, &LoginPage::submit);
    connect(m_registerButton, &QPushButton::clicked, this, [this]() {
        if (m_context.nav)
            m_context.nav->showRegister();
    });
}

void LoginPage::onEnter()
{
    m_errorLabel->setVisible(false);
    m_passwordEdit->clear();
    setBusy(false);
}

void LoginPage::presetUsername(const QString& username)
{
    m_usernameEdit->setText(username);
}

void LoginPage::showHint(const QString& hint)
{
    m_hintLabel->setText(hint);
}

void LoginPage::setBusy(bool busy)
{
    m_loginButton->setEnabled(!busy);
    m_loginButton->setText(busy ? QStringLiteral("登录中…") : QStringLiteral("登录"));
    m_registerButton->setEnabled(!busy);
    m_usernameEdit->setEnabled(!busy);
    m_passwordEdit->setEnabled(!busy);
}

void LoginPage::submit()
{
    if (!m_context.isValid())
        return;

    const validators::Check user = validators::username(m_usernameEdit->text());
    if (!user.ok) {
        m_errorLabel->setText(user.error);
        m_errorLabel->setVisible(true);
        return;
    }
    const validators::Check pwd = validators::loginPassword(m_passwordEdit->text());
    if (!pwd.ok) {
        m_errorLabel->setText(pwd.error);
        m_errorLabel->setVisible(true);
        return;
    }

    m_errorLabel->setVisible(false);
    setBusy(true);

    // 输入值原样抄出：超时/失败后输入不丢，用户可自行再次点击登录（不自动重发）
    const QString username = user.normalized;
    const QString password = pwd.normalized;

    m_context.api->login(username, password, this, [this, password](const ApiResult& result) {
        setBusy(false);
        if (!result.ok) {
            // 失败保留输入，仅提示原因。
            // 401 文案以服务端 message 为准（"账号或密码错误" / "账号已停用" 由服务端区分）；
            // 仅在服务端未给出 message 时使用登录页专用兜底，避免出现"会话已失效"这类通用措辞。
            QString text = result.message;
            if (text.isEmpty()) {
                text = (result.httpStatus == 401)
                           ? QStringLiteral("账号或密码错误，或该账号已停用")
                           : result.displayMessage();
            }
            m_errorLabel->setText(text);
            m_errorLabel->setVisible(true);
            m_passwordEdit->selectAll();
            return;
        }

        LoginResult loginResult;
        QString error;
        if (!json::parseLoginResult(result.data, &loginResult, &error)) {
            m_errorLabel->setText(QStringLiteral("通信数据异常：%1").arg(error));
            m_errorLabel->setVisible(true);
            return;
        }

        m_context.session->applyLogin(loginResult);
        m_passwordEdit->clear(); // 登录成功后不保留密码
        m_hintLabel->setText(QStringLiteral("请使用演示账号登录（服务端需已启动）"));
        if (m_context.nav)
            m_context.nav->showBoardList();
    });
}

} // namespace bbs
