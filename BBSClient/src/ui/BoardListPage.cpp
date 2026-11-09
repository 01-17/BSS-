#include "ui/BoardListPage.h"

#include "model/JsonMapper.h"
#include "net/ApiClient.h"
#include "session/SessionManager.h"
#include "ui/StateView.h"
#include "ui/Theme.h"
#include "util/TimeFormat.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace bbs {

BoardListPage::BoardListPage(const PageContext& context, QWidget* parent)
    : Page(context, parent)
{
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 18);
    root->setSpacing(14);

    QHBoxLayout* header = new QHBoxLayout;
    header->addWidget(theme::pageTitle(QStringLiteral("版块列表"), this));
    header->addStretch(1);
    m_adminButton = theme::normalButton(QStringLiteral("版块管理"), this);
    m_adminButton->setVisible(false);
    header->addWidget(m_adminButton);
    QPushButton* refresh = theme::normalButton(QStringLiteral("刷新"), this);
    header->addWidget(refresh);
    root->addLayout(header);

    root->addWidget(theme::metaLabel(
        QStringLiteral("全部未删除版块（含已停用）。已停用版块可浏览，但不能发帖。"), this));

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

    connect(m_state, &StateView::retryRequested, this, &BoardListPage::load);
    connect(refresh, &QPushButton::clicked, this, &BoardListPage::load);
    connect(m_adminButton, &QPushButton::clicked, this, [this]() {
        if (m_context.nav)
            m_context.nav->showBoardAdmin();
    });
}

void BoardListPage::onEnter()
{
    updateAdminEntry();
    load();
}

void BoardListPage::updateAdminEntry()
{
    const bool isAdmin = m_context.session && m_context.session->isAdmin();
    m_adminButton->setVisible(isAdmin);
}

void BoardListPage::load()
{
    if (!m_context.isValid())
        return;

    m_state->showLoading(QStringLiteral("正在加载版块列表…"));
    m_context.api->listBoards(this, [this](const ApiResult& result) {
        if (!result.ok) {
            m_state->showError(result.displayMessage());
            return;
        }
        QVector<BoardView> boards;
        QString error;
        if (!json::parseBoardList(result.data, &boards, &error)) {
            m_state->showError(QStringLiteral("通信数据异常：%1").arg(error));
            return;
        }
        if (boards.isEmpty()) {
            m_state->showEmpty(QStringLiteral("暂无版块。管理员可在“版块管理”中创建。"));
            return;
        }
        rebuildCards(boards);
        m_state->showContent();
    });
}

void BoardListPage::rebuildCards(const QVector<BoardView>& boards)
{
    // 清空旧卡片（保留末尾 stretch）
    while (m_listLayout->count() > 1) {
        QLayoutItem* item = m_listLayout->takeAt(0);
        if (QWidget* widget = item->widget())
            widget->deleteLater();
        delete item;
    }

    for (const BoardView& board : boards) {
        QFrame* card = new QFrame(m_listContainer);
        card->setObjectName(QStringLiteral("card"));
        QVBoxLayout* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(16, 14, 16, 14);
        cardLayout->setSpacing(8);

        QHBoxLayout* titleRow = new QHBoxLayout;
        QLabel* name = new QLabel(board.name, card);
        name->setStyleSheet(QStringLiteral("font-size:15px; font-weight:600;"));
        titleRow->addWidget(name);
        if (!board.isOpen()) {
            titleRow->addWidget(theme::tagLabel(QStringLiteral("已停用"), QStringLiteral("#9ca3af"),
                                               card));
        }
        titleRow->addStretch(1);

        QPushButton* enter = theme::primaryButton(QStringLiteral("进入版块"), card);
        enter->setFixedWidth(96);
        titleRow->addWidget(enter);
        cardLayout->addLayout(titleRow);

        QLabel* description = new QLabel(
            board.description.isEmpty() ? QStringLiteral("（无描述）") : board.description, card);
        description->setWordWrap(true);
        description->setStyleSheet(QStringLiteral("color:#4b5563;"));
        cardLayout->addWidget(description);

        cardLayout->addWidget(theme::metaLabel(
            QStringLiteral("版块 ID %1 · 创建 %2 · 更新 %3")
                .arg(board.boardId)
                .arg(timefmt::toLocalDisplay(board.createdAt))
                .arg(timefmt::toLocalDisplay(board.updatedAt)),
            card));

        const qint64 boardId = board.boardId;
        const QString boardName = board.name;
        connect(enter, &QPushButton::clicked, this, [this, boardId, boardName]() {
            if (m_context.nav)
                m_context.nav->showPostList(boardId, boardName);
        });

        m_listLayout->insertWidget(m_listLayout->count() - 1, card);
    }
}

} // namespace bbs
