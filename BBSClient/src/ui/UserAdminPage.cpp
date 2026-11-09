#include "ui/UserAdminPage.h"

#include "model/JsonMapper.h"
#include "net/ApiClient.h"
#include "session/SessionManager.h"
#include "ui/Dialogs.h"
#include "ui/StateView.h"
#include "ui/Theme.h"
#include "util/TimeFormat.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace bbs {

UserAdminPage::UserAdminPage(const PageContext& context, QWidget* parent)
    : Page(context, parent)
{
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 18);
    root->setSpacing(12);

    QHBoxLayout* header = new QHBoxLayout;
    header->addWidget(theme::pageTitle(QStringLiteral("账号管理"), this));
    header->addStretch(1);
    QPushButton* refresh = theme::normalButton(QStringLiteral("刷新"), this);
    header->addWidget(refresh);
    root->addLayout(header);

    QHBoxLayout* filterRow = new QHBoxLayout;
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(QStringLiteral("按用户名 / 昵称筛选（本地过滤）"));
    filterRow->addWidget(m_filterEdit, 1);
    root->addLayout(filterRow);

    m_summaryLabel = theme::metaLabel(QString(), this);
    root->addWidget(m_summaryLabel);

    m_state = new StateView(this);
    root->addWidget(m_state, 1);

    QScrollArea* scroll = new QScrollArea(m_state);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    m_listContainer = new QWidget(scroll);
    m_listLayout = new QVBoxLayout(m_listContainer);
    m_listLayout->setContentsMargins(0, 0, 0, 0);
    m_listLayout->setSpacing(10);
    m_listLayout->addStretch(1);
    scroll->setWidget(m_listContainer);
    m_state->setContentWidget(scroll);

    connect(refresh, &QPushButton::clicked, this, &UserAdminPage::load);
    connect(m_state, &StateView::retryRequested, this, &UserAdminPage::load);
    connect(m_filterEdit, &QLineEdit::textChanged, this, &UserAdminPage::rebuild);
}

void UserAdminPage::onEnter()
{
    load();
}

void UserAdminPage::load()
{
    if (!m_context.isValid())
        return;
    if (!m_context.session->isAdmin()) {
        m_state->showError(QStringLiteral("仅管理员可访问账号管理"));
        return;
    }

    m_state->showLoading(QStringLiteral("正在加载账号列表…"));
    m_context.api->adminListUsers(this, [this](const ApiResult& result) {
        if (!result.ok) {
            m_state->showError(result.displayMessage());
            return;
        }
        QVector<UserProfile> users;
        QString error;
        if (!json::parseUserList(result.data, &users, &error)) {
            m_state->showError(QStringLiteral("通信数据异常：%1").arg(error));
            return;
        }
        m_users = users;
        if (m_users.isEmpty()) {
            m_state->showEmpty(QStringLiteral("暂无账号数据"));
            return;
        }
        rebuild();
        m_state->showContent();
    });
}

void UserAdminPage::rebuild()
{
    while (m_listLayout->count() > 1) {
        QLayoutItem* item = m_listLayout->takeAt(0);
        if (QWidget* widget = item->widget())
            widget->deleteLater();
        delete item;
    }

    const QString keyword = m_filterEdit->text().trimmed();
    int shown = 0;

    for (const UserProfile& user : m_users) {
        if (!keyword.isEmpty() && !user.username.contains(keyword, Qt::CaseInsensitive)
            && !user.nickname.contains(keyword, Qt::CaseInsensitive))
            continue;
        ++shown;

        QFrame* card = new QFrame(m_listContainer);
        card->setObjectName(QStringLiteral("card"));
        QHBoxLayout* cardLayout = new QHBoxLayout(card);
        cardLayout->setContentsMargins(16, 12, 16, 12);
        cardLayout->setSpacing(12);

        QVBoxLayout* infoLayout = new QVBoxLayout;
        infoLayout->setSpacing(4);

        QHBoxLayout* nameRow = new QHBoxLayout;
        QLabel* nickname = new QLabel(user.nickname, card);
        nickname->setStyleSheet(QStringLiteral("font-size:15px; font-weight:600;"));
        nameRow->addWidget(nickname);
        if (user.isAdmin())
            nameRow->addWidget(
                theme::tagLabel(QStringLiteral("管理员"), QStringLiteral("#2563eb"), card));
        if (!user.isActive())
            nameRow->addWidget(
                theme::tagLabel(QStringLiteral("已停用"), QStringLiteral("#9ca3af"), card));
        nameRow->addStretch(1);
        infoLayout->addLayout(nameRow);

        QStringList details;
        details << QStringLiteral("用户名 %1").arg(user.username);
        details << QStringLiteral("用户 ID %1").arg(user.userId);
        details << QStringLiteral("注册 %1").arg(timefmt::toLocalDisplay(user.registeredAt));
        if (!user.assignments.isEmpty())
            details << QStringLiteral("授权版块 %1 个").arg(user.assignments.size());
        infoLayout->addWidget(theme::metaLabel(details.join(QStringLiteral(" · ")), card));

        cardLayout->addLayout(infoLayout, 1);

        const bool targetIsAdmin = user.isAdmin();
        const qint64 userId = user.userId;
        const QString userNickname = user.nickname;

        if (user.isActive()) {
            QPushButton* disable = theme::dangerButton(QStringLiteral("停用"), card);
            if (targetIsAdmin) {
                disable->setEnabled(false);
                disable->setToolTip(QStringLiteral("服务端禁止管理管理员账号"));
            }
            connect(disable, &QPushButton::clicked, this, [this, userId, userNickname]() {
                setStatus(userId, 0, userNickname);
            });
            cardLayout->addWidget(disable);
        } else {
            QPushButton* enable = theme::primaryButton(QStringLiteral("启用"), card);
            if (targetIsAdmin) {
                enable->setEnabled(false);
                enable->setToolTip(QStringLiteral("服务端禁止管理管理员账号"));
            }
            connect(enable, &QPushButton::clicked, this, [this, userId, userNickname]() {
                setStatus(userId, 1, userNickname);
            });
            cardLayout->addWidget(enable);
        }

        m_listLayout->insertWidget(m_listLayout->count() - 1, card);
    }

    if (shown == 0) {
        QLabel* empty = theme::metaLabel(QStringLiteral("没有匹配的账号"), m_listContainer);
        m_listLayout->insertWidget(0, empty);
    }
    m_summaryLabel->setText(QStringLiteral("共 %1 个账号，当前显示 %2 个")
                                .arg(m_users.size())
                                .arg(shown));
}

void UserAdminPage::setStatus(qint64 userId, int status, const QString& nickname)
{
    if (!m_context.isValid())
        return;

    const QString action = status == 0 ? QStringLiteral("停用") : QStringLiteral("启用");
    if (!dialogs::confirmDestructive(
            this, QStringLiteral("%1账号").arg(action),
            QStringLiteral("确认%1账号“%2”？%3")
                .arg(action, nickname,
                     status == 0
                         ? QStringLiteral("停用后该用户将无法登录，已有令牌立即失效。")
                         : QStringLiteral("启用后该用户可重新登录。")),
            action))
        return;

    m_context.api->adminSetUserStatus(userId, status, this, [this](const ApiResult& result) {
        if (!result.ok) {
            dialogs::warn(this, QStringLiteral("操作未生效"), result.displayMessage());
            return;
        }
        AccountStatusResult parsed;
        QString error;
        if (!json::parseAccountStatus(result.data, &parsed, &error)) {
            dialogs::warn(this, QStringLiteral("通信数据异常"), error);
            return;
        }
        if (m_context.nav) {
            m_context.nav->notify(parsed.changed ? QStringLiteral("账号状态已更新")
                                                 : QStringLiteral("账号状态未发生变化"));
        }
        load();
    });
}

} // namespace bbs
