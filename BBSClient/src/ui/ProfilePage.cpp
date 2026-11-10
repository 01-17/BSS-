#include "ui/ProfilePage.h"

#include "model/JsonMapper.h"
#include "net/ApiClient.h"
#include "session/SessionManager.h"
#include "ui/Dialogs.h"
#include "ui/StateView.h"
#include "ui/Theme.h"
#include "util/TimeFormat.h"
#include "util/Validators.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace bbs {

ProfilePage::ProfilePage(const PageContext& context, QWidget* parent)
    : Page(context, parent)
{
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 18);
    root->setSpacing(12);

    root->addWidget(theme::pageTitle(QStringLiteral("个人中心"), this));

    m_state = new StateView(this);
    root->addWidget(m_state, 1);

    QScrollArea* scroll = new QScrollArea(m_state);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    m_content = new QWidget(scroll);
    QVBoxLayout* contentLayout = new QVBoxLayout(m_content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(12);

    // ---- 基本信息 ----
    QFrame* infoCard = new QFrame(m_content);
    infoCard->setObjectName(QStringLiteral("card"));
    QGridLayout* infoGrid = new QGridLayout(infoCard);
    infoGrid->setContentsMargins(18, 16, 18, 16);
    infoGrid->setHorizontalSpacing(14);
    infoGrid->setVerticalSpacing(8);

    infoGrid->addWidget(theme::sectionTitle(QStringLiteral("账号信息"), infoCard), 0, 0, 1, 2);

    infoGrid->addWidget(theme::metaLabel(QStringLiteral("用户名"), infoCard), 1, 0);
    m_usernameLabel = new QLabel(infoCard);
    m_usernameLabel->setTextFormat(Qt::PlainText);
    infoGrid->addWidget(m_usernameLabel, 1, 1);

    infoGrid->addWidget(theme::metaLabel(QStringLiteral("角色"), infoCard), 2, 0);
    m_roleLabel = new QLabel(infoCard);
    m_roleLabel->setTextFormat(Qt::PlainText);
    infoGrid->addWidget(m_roleLabel, 2, 1);

    infoGrid->addWidget(theme::metaLabel(QStringLiteral("状态"), infoCard), 3, 0);
    m_statusLabel = new QLabel(infoCard);
    m_statusLabel->setTextFormat(Qt::PlainText);
    infoGrid->addWidget(m_statusLabel, 3, 1);

    infoGrid->addWidget(theme::metaLabel(QStringLiteral("注册时间"), infoCard), 4, 0);
    m_registeredLabel = new QLabel(infoCard);
    m_registeredLabel->setTextFormat(Qt::PlainText);
    infoGrid->addWidget(m_registeredLabel, 4, 1);

    infoGrid->addWidget(theme::metaLabel(QStringLiteral("我管理的版块"), infoCard), 5, 0);
    m_boardsLabel = new QLabel(infoCard);
    m_boardsLabel->setTextFormat(Qt::PlainText);
    m_boardsLabel->setWordWrap(true);
    infoGrid->addWidget(m_boardsLabel, 5, 1);

    contentLayout->addWidget(infoCard);

    // ---- 修改昵称 ----
    QFrame* nicknameCard = new QFrame(m_content);
    nicknameCard->setObjectName(QStringLiteral("card"));
    QVBoxLayout* nicknameLayout = new QVBoxLayout(nicknameCard);
    nicknameLayout->setContentsMargins(18, 16, 18, 16);
    nicknameLayout->setSpacing(8);
    nicknameLayout->addWidget(theme::sectionTitle(QStringLiteral("修改昵称"), nicknameCard));

    QHBoxLayout* nicknameRow = new QHBoxLayout;
    m_nicknameEdit = new QLineEdit(nicknameCard);
    m_nicknameEdit->setPlaceholderText(QStringLiteral("1-30 个字符"));
    m_nicknameEdit->setMaxLength(64);
    nicknameRow->addWidget(m_nicknameEdit, 1);
    m_nicknameButton = theme::primaryButton(QStringLiteral("保存昵称"), nicknameCard);
    nicknameRow->addWidget(m_nicknameButton);
    nicknameLayout->addLayout(nicknameRow);

    m_nicknameError = theme::errorLabel(nicknameCard);
    m_nicknameError->setVisible(false);
    nicknameLayout->addWidget(m_nicknameError);
    contentLayout->addWidget(nicknameCard);

    // ---- 修改密码 ----
    QFrame* passwordCard = new QFrame(m_content);
    passwordCard->setObjectName(QStringLiteral("card"));
    QVBoxLayout* passwordLayout = new QVBoxLayout(passwordCard);
    passwordLayout->setContentsMargins(18, 16, 18, 16);
    passwordLayout->setSpacing(8);
    passwordLayout->addWidget(theme::sectionTitle(QStringLiteral("修改密码"), passwordCard));

    m_currentPasswordEdit = new QLineEdit(passwordCard);
    m_currentPasswordEdit->setEchoMode(QLineEdit::Password);
    m_currentPasswordEdit->setPlaceholderText(QStringLiteral("当前密码"));
    m_currentPasswordEdit->setMaxLength(64);
    passwordLayout->addWidget(m_currentPasswordEdit);

    m_newPasswordEdit = new QLineEdit(passwordCard);
    m_newPasswordEdit->setEchoMode(QLineEdit::Password);
    m_newPasswordEdit->setPlaceholderText(QStringLiteral("新密码（8-64 个字符）"));
    m_newPasswordEdit->setMaxLength(64);
    passwordLayout->addWidget(m_newPasswordEdit);

    m_confirmPasswordEdit = new QLineEdit(passwordCard);
    m_confirmPasswordEdit->setEchoMode(QLineEdit::Password);
    m_confirmPasswordEdit->setPlaceholderText(QStringLiteral("再次输入新密码"));
    m_confirmPasswordEdit->setMaxLength(64);
    passwordLayout->addWidget(m_confirmPasswordEdit);

    m_passwordError = theme::errorLabel(passwordCard);
    m_passwordError->setVisible(false);
    passwordLayout->addWidget(m_passwordError);

    QHBoxLayout* passwordRow = new QHBoxLayout;
    passwordRow->addStretch(1);
    m_passwordButton = theme::primaryButton(QStringLiteral("修改密码"), passwordCard);
    passwordRow->addWidget(m_passwordButton);
    passwordLayout->addLayout(passwordRow);
    contentLayout->addWidget(passwordCard);

    // ---- 退出登录 ----
    QFrame* logoutCard = new QFrame(m_content);
    logoutCard->setObjectName(QStringLiteral("card"));
    QHBoxLayout* logoutLayout = new QHBoxLayout(logoutCard);
    logoutLayout->setContentsMargins(18, 14, 18, 14);
    logoutLayout->addWidget(
        theme::metaLabel(QStringLiteral("退出后令牌立即从内存清除，需重新登录。"), logoutCard));
    logoutLayout->addStretch(1);
    QPushButton* logoutButton = theme::dangerButton(QStringLiteral("退出登录"), logoutCard);
    logoutLayout->addWidget(logoutButton);
    contentLayout->addWidget(logoutCard);

    contentLayout->addStretch(1);
    scroll->setWidget(m_content);
    m_state->setContentWidget(scroll);

    connect(m_state, &StateView::retryRequested, this, &ProfilePage::load);
    connect(m_nicknameButton, &QPushButton::clicked, this, &ProfilePage::submitNickname);
    connect(m_passwordButton, &QPushButton::clicked, this, &ProfilePage::submitPassword);
    connect(logoutButton, &QPushButton::clicked, this, &ProfilePage::logout);
}

void ProfilePage::onEnter()
{
    m_nicknameError->setVisible(false);
    m_passwordError->setVisible(false);
    m_currentPasswordEdit->clear();
    m_newPasswordEdit->clear();
    m_confirmPasswordEdit->clear();
    load();
}

void ProfilePage::load()
{
    if (!m_context.isValid())
        return;

    m_state->showLoading(QStringLiteral("正在加载个人资料…"));
    m_context.api->fetchMe(this, [this](const ApiResult& result) {
        if (!result.ok) {
            m_state->showError(result.displayMessage());
            return;
        }
        UserProfile profile;
        QString error;
        if (!json::parseUserProfile(result.data, &profile, &error)) {
            m_state->showError(QStringLiteral("通信数据异常：%1").arg(error));
            return;
        }
        m_context.session->updateProfile(profile);
        render();
        m_state->showContent();
    });
}

void ProfilePage::render()
{
    const UserProfile& profile = m_context.session->profile();

    m_usernameLabel->setText(profile.username);
    m_roleLabel->setText(profile.isAdmin() ? QStringLiteral("管理员（admin）")
                                           : QStringLiteral("普通成员（member）"));
    m_statusLabel->setText(profile.isActive() ? QStringLiteral("正常") : QStringLiteral("已停用"));
    m_registeredLabel->setText(timefmt::toLocalDisplay(profile.registeredAt));

    if (profile.assignments.isEmpty()) {
        m_boardsLabel->setText(QStringLiteral("无（当前不是任何版块的版主）"));
    } else {
        QStringList names;
        for (const ModeratorAssignment& item : profile.assignments)
            names << QStringLiteral("版块 ID %1（授权于 %2）")
                         .arg(item.boardId)
                         .arg(timefmt::toLocalDisplay(item.assignedAt));
        m_boardsLabel->setText(names.join(QStringLiteral("；")));
    }

    m_nicknameEdit->setText(profile.nickname);
}

void ProfilePage::submitNickname()
{
    if (!m_context.isValid())
        return;

    const validators::Check check = validators::nickname(m_nicknameEdit->text());
    if (!check.ok) {
        m_nicknameError->setText(check.error);
        m_nicknameError->setVisible(true);
        return;
    }
    m_nicknameError->setVisible(false);
    m_nicknameButton->setEnabled(false);
    m_nicknameButton->setText(QStringLiteral("保存中…"));

    m_context.api->updateNickname(check.normalized, this, [this](const ApiResult& result) {
        m_nicknameButton->setEnabled(true);
        m_nicknameButton->setText(QStringLiteral("保存昵称"));
        if (!result.ok) {
            m_nicknameError->setText(result.displayMessage());
            m_nicknameError->setVisible(true);
            return;
        }
        UserProfile profile;
        QString error;
        if (!json::parseUserProfile(result.data, &profile, &error)) {
            m_nicknameError->setText(QStringLiteral("通信数据异常：%1").arg(error));
            m_nicknameError->setVisible(true);
            return;
        }
        m_context.session->updateProfile(profile);
        render();
        if (m_context.nav)
            m_context.nav->notify(QStringLiteral("昵称已更新"));
    });
}

void ProfilePage::submitPassword()
{
    if (!m_context.isValid())
        return;

    if (m_currentPasswordEdit->text().isEmpty()) {
        m_passwordError->setText(QStringLiteral("请输入当前密码"));
        m_passwordError->setVisible(true);
        return;
    }
    const validators::Check check =
        validators::password(m_newPasswordEdit->text(), QStringLiteral("新密码"));
    if (!check.ok) {
        m_passwordError->setText(check.error);
        m_passwordError->setVisible(true);
        return;
    }
    if (m_newPasswordEdit->text() != m_confirmPasswordEdit->text()) {
        m_passwordError->setText(QStringLiteral("两次输入的新密码不一致"));
        m_passwordError->setVisible(true);
        return;
    }

    m_passwordError->setVisible(false);
    m_passwordButton->setEnabled(false);
    m_passwordButton->setText(QStringLiteral("提交中…"));

    m_context.api->changePassword(m_currentPasswordEdit->text(), check.normalized, this,
                                  [this](const ApiResult& result) {
        m_passwordButton->setEnabled(true);
        m_passwordButton->setText(QStringLiteral("修改密码"));
        if (!result.ok) {
            m_passwordError->setText(result.displayMessage());
            m_passwordError->setVisible(true);
            return;
        }
        m_currentPasswordEdit->clear();
        m_newPasswordEdit->clear();
        m_confirmPasswordEdit->clear();
        if (m_context.nav)
            m_context.nav->notify(QStringLiteral("密码已修改，请使用新密码重新登录"));
    });
}

void ProfilePage::logout()
{
    if (!m_context.isValid())
        return;
    if (!dialogs::confirm(this, QStringLiteral("退出登录"),
                          QStringLiteral("确认退出当前账号？"), QStringLiteral("退出")))
        return;

    m_context.api->logout(this, [this](const ApiResult&) {
        // 无论服务端返回什么，本地都必须清空令牌（避免残留会话）
        m_context.session->clear();
    });
}

} // namespace bbs
