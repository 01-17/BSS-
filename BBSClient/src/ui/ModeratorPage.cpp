#include "ui/ModeratorPage.h"

#include "model/JsonMapper.h"
#include "net/ApiClient.h"
#include "session/SessionManager.h"
#include "ui/Dialogs.h"
#include "ui/StateView.h"
#include "ui/Theme.h"
#include "util/TimeFormat.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QVBoxLayout>

#include <memory>

namespace bbs {

ModeratorPage::ModeratorPage(const PageContext& context, QWidget* parent)
    : Page(context, parent)
{
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 18);
    root->setSpacing(12);

    root->addWidget(theme::pageTitle(QStringLiteral("版主授权"), this));

    QFrame* formCard = new QFrame(this);
    formCard->setObjectName(QStringLiteral("card"));
    QVBoxLayout* formLayout = new QVBoxLayout(formCard);
    formLayout->setContentsMargins(18, 16, 18, 16);
    formLayout->setSpacing(10);
    formLayout->addWidget(theme::sectionTitle(QStringLiteral("授予版主"), formCard));

    QHBoxLayout* selectRow = new QHBoxLayout;
    selectRow->addWidget(theme::metaLabel(QStringLiteral("版块"), formCard));
    m_boardCombo = new QComboBox(formCard);
    m_boardCombo->setMinimumWidth(200);
    selectRow->addWidget(m_boardCombo, 1);

    selectRow->addWidget(theme::metaLabel(QStringLiteral("账号"), formCard));
    m_userCombo = new QComboBox(formCard);
    m_userCombo->setMinimumWidth(220);
    selectRow->addWidget(m_userCombo, 1);

    QPushButton* grantButton = theme::primaryButton(QStringLiteral("授予"), formCard);
    selectRow->addWidget(grantButton);
    formLayout->addLayout(selectRow);

    m_hintLabel = theme::metaLabel(
        QStringLiteral("仅启用状态的普通成员可被授权；管理员账号不接受版主授权。"), formCard);
    formLayout->addWidget(m_hintLabel);

    root->addWidget(formCard);

    m_assignmentSummary = theme::metaLabel(QString(), this);
    root->addWidget(m_assignmentSummary);

    m_state = new StateView(this);
    root->addWidget(m_state, 1);

    QScrollArea* scroll = new QScrollArea(m_state);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    m_assignmentContainer = new QWidget(scroll);
    m_assignmentLayout = new QVBoxLayout(m_assignmentContainer);
    m_assignmentLayout->setContentsMargins(0, 0, 0, 0);
    m_assignmentLayout->setSpacing(10);
    m_assignmentLayout->addStretch(1);
    scroll->setWidget(m_assignmentContainer);
    m_state->setContentWidget(scroll);

    connect(grantButton, &QPushButton::clicked, this, &ModeratorPage::grant);
    connect(m_boardCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { rebuildAssignmentList(); });
    connect(m_state, &StateView::retryRequested, this, &ModeratorPage::loadAll);
}

void ModeratorPage::onEnter()
{
    if (!m_context.session->isAdmin()) {
        m_state->showError(QStringLiteral("仅管理员可进行版主授权"));
        return;
    }
    loadAll();
}

void ModeratorPage::loadAll()
{
    if (!m_context.isValid())
        return;

    m_state->showLoading(QStringLiteral("正在加载版块与账号数据…"));

    // 两个请求互不依赖，任一失败都按失败态提示，避免半屏数据误导操作
    struct LoadState {
        bool boardsDone = false;
        bool usersDone = false;
        bool failed = false;
    };
    auto state = std::make_shared<LoadState>();

    m_context.api->listBoards(this, [this, state](const ApiResult& result) {
        if (state->failed)
            return;
        if (!result.ok) {
            state->failed = true;
            m_state->showError(result.displayMessage());
            return;
        }
        QVector<BoardView> boards;
        QString error;
        if (!json::parseBoardList(result.data, &boards, &error)) {
            state->failed = true;
            m_state->showError(QStringLiteral("通信数据异常：%1").arg(error));
            return;
        }
        m_boards = boards;
        state->boardsDone = true;
        if (state->usersDone) {
            rebuildBoardCombo();
            rebuildUserCombo();
            rebuildAssignmentList();
            m_state->showContent();
        }
    });

    m_context.api->adminListUsers(this, [this, state](const ApiResult& result) {
        if (state->failed)
            return;
        if (!result.ok) {
            state->failed = true;
            m_state->showError(result.displayMessage());
            return;
        }
        QVector<UserProfile> users;
        QString error;
        if (!json::parseUserList(result.data, &users, &error)) {
            state->failed = true;
            m_state->showError(QStringLiteral("通信数据异常：%1").arg(error));
            return;
        }
        m_users = users;
        state->usersDone = true;
        if (state->boardsDone) {
            rebuildBoardCombo();
            rebuildUserCombo();
            rebuildAssignmentList();
            m_state->showContent();
        }
    });
}

void ModeratorPage::rebuildBoardCombo()
{
    const qint64 previous = m_boardCombo->currentData().toLongLong();
    m_boardCombo->blockSignals(true);
    m_boardCombo->clear();
    for (const BoardView& board : m_boards) {
        m_boardCombo->addItem(board.isOpen()
                                  ? board.name
                                  : QStringLiteral("%1（已停用）").arg(board.name),
                              QVariant::fromValue<qint64>(board.boardId));
    }
    m_boardCombo->blockSignals(false);

    if (previous > 0) {
        const int index = m_boardCombo->findData(QVariant::fromValue<qint64>(previous));
        if (index >= 0)
            m_boardCombo->setCurrentIndex(index);
    }
}

void ModeratorPage::rebuildUserCombo()
{
    m_userCombo->clear();
    for (const UserProfile& user : m_users) {
        if (user.isAdmin())
            continue; // 管理员不接受版主授权
        QString label = QStringLiteral("%1（%2）").arg(user.nickname, user.username);
        if (!user.isActive())
            label += QStringLiteral(" · 已停用");
        m_userCombo->addItem(label, QVariant::fromValue<qint64>(user.userId));
        if (!user.isActive()) {
            // 停用账号保留在下拉中以便识别，但禁用选择
            const int index = m_userCombo->count() - 1;
            if (QStandardItemModel* model = qobject_cast<QStandardItemModel*>(m_userCombo->model())) {
                if (QStandardItem* item = model->item(index))
                    item->setEnabled(false);
            }
        }
    }
}

void ModeratorPage::rebuildAssignmentList()
{
    while (m_assignmentLayout->count() > 1) {
        QLayoutItem* item = m_assignmentLayout->takeAt(0);
        if (QWidget* widget = item->widget())
            widget->deleteLater();
        delete item;
    }

    const qint64 boardId = m_boardCombo->currentData().toLongLong();
    if (boardId <= 0) {
        m_assignmentSummary->setText(QStringLiteral("请先选择版块"));
        m_state->showEmpty(QStringLiteral("暂无可管理的版块"));
        return;
    }

    QString boardName;
    for (const BoardView& board : m_boards) {
        if (board.boardId == boardId) {
            boardName = board.name;
            break;
        }
    }

    int count = 0;
    for (const UserProfile& user : m_users) {
        if (!user.moderates(boardId))
            continue;
        ++count;

        QFrame* card = new QFrame(m_assignmentContainer);
        card->setObjectName(QStringLiteral("card"));
        QHBoxLayout* cardLayout = new QHBoxLayout(card);
        cardLayout->setContentsMargins(16, 12, 16, 12);
        cardLayout->setSpacing(12);

        QVBoxLayout* infoLayout = new QVBoxLayout;
        infoLayout->setSpacing(4);
        QLabel* name = new QLabel(user.nickname, card);
        name->setTextFormat(Qt::PlainText);
        name->setStyleSheet(QStringLiteral("font-size:15px; font-weight:600;"));
        infoLayout->addWidget(name);

        QString assignedAt;
        for (const ModeratorAssignment& item : user.assignments) {
            if (item.boardId == boardId)
                assignedAt = item.assignedAt;
        }
        infoLayout->addWidget(theme::metaLabel(
            QStringLiteral("用户名 %1 · 授权时间 %2")
                .arg(user.username, timefmt::toLocalDisplay(assignedAt)),
            card));
        cardLayout->addLayout(infoLayout, 1);

        QPushButton* revokeButton = theme::dangerButton(QStringLiteral("撤销"), card);
        revokeButton->setFixedWidth(70);
        const qint64 userId = user.userId;
        const QString nickname = user.nickname;
        connect(revokeButton, &QPushButton::clicked, this,
                [this, userId, nickname]() { revoke(userId, nickname); });
        cardLayout->addWidget(revokeButton);

        m_assignmentLayout->insertWidget(m_assignmentLayout->count() - 1, card);
    }

    m_assignmentSummary->setText(
        QStringLiteral("版块“%1”当前版主 %2 名").arg(boardName).arg(count));

    if (count == 0) {
        QLabel* empty = theme::metaLabel(
            QStringLiteral("该版块暂无版主，可在上方选择账号后点击“授予”。"), m_assignmentContainer);
        m_assignmentLayout->insertWidget(0, empty);
    }
}

void ModeratorPage::grant()
{
    if (!m_context.isValid())
        return;

    const qint64 boardId = m_boardCombo->currentData().toLongLong();
    const qint64 userId = m_userCombo->currentData().toLongLong();
    if (boardId <= 0 || userId <= 0) {
        dialogs::warn(this, QStringLiteral("无法授予"), QStringLiteral("请先选择版块与账号。"));
        return;
    }

    QString boardName;
    for (const BoardView& board : m_boards) {
        if (board.boardId == boardId)
            boardName = board.name;
    }
    const QString userName = m_userCombo->currentText();

    if (!dialogs::confirm(this, QStringLiteral("授予版主"),
                          QStringLiteral("确认授予“%1”版块“%2”的版主权限？\n授予后其可处置该版块内容。")
                              .arg(boardName, userName),
                          QStringLiteral("授予")))
        return;

    m_context.api->adminGrantModerator(boardId, userId, this, [this](const ApiResult& result) {
        if (!result.ok) {
            dialogs::warn(this, QStringLiteral("授予失败"), result.displayMessage());
            return;
        }
        GrantResult parsed;
        QString error;
        if (!json::parseGrant(result.data, &parsed, &error)) {
            dialogs::warn(this, QStringLiteral("通信数据异常"), error);
            return;
        }
        if (m_context.nav)
            m_context.nav->notify(QStringLiteral("版主授权成功"));
        loadAll();
    });
}

void ModeratorPage::revoke(qint64 userId, const QString& nickname)
{
    if (!m_context.isValid())
        return;

    const qint64 boardId = m_boardCombo->currentData().toLongLong();
    if (boardId <= 0)
        return;

    if (!dialogs::confirmDestructive(
            this, QStringLiteral("撤销版主"),
            QStringLiteral("确认撤销“%1”在该版块的版主权限？撤销后其将无法处置该版块内容。")
                .arg(nickname),
            QStringLiteral("撤销")))
        return;

    m_context.api->adminRevokeModerator(boardId, userId, this, [this](const ApiResult& result) {
        if (!result.ok) {
            dialogs::warn(this, QStringLiteral("撤销失败"), result.displayMessage());
            return;
        }
        RevokeResult parsed;
        QString error;
        if (!json::parseRevoke(result.data, &parsed, &error)) {
            dialogs::warn(this, QStringLiteral("通信数据异常"), error);
            return;
        }
        if (m_context.nav)
            m_context.nav->notify(QStringLiteral("已撤销版主权限"));
        loadAll();
    });
}

} // namespace bbs
