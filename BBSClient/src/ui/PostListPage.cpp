#include "ui/PostListPage.h"

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

PostListPage::PostListPage(const PageContext& context, QWidget* parent)
    : Page(context, parent)
{
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 18);
    root->setSpacing(12);

    QHBoxLayout* header = new QHBoxLayout;
    QPushButton* back = theme::normalButton(QStringLiteral("返回版块"), this);
    header->addWidget(back);
    m_titleLabel = theme::pageTitle(QStringLiteral("帖子列表"), this);
    header->addWidget(m_titleLabel);
    header->addStretch(1);
    m_postButton = theme::primaryButton(QStringLiteral("发帖"), this);
    m_refreshButton = theme::normalButton(QStringLiteral("刷新"), this);
    header->addWidget(m_postButton);
    header->addWidget(m_refreshButton);
    root->addLayout(header);

    m_hintLabel = theme::metaLabel(QString(), this);
    root->addWidget(m_hintLabel);

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

    connect(back, &QPushButton::clicked, this, [this]() {
        if (m_context.nav)
            m_context.nav->showBoardList();
    });
    connect(m_refreshButton, &QPushButton::clicked, this, &PostListPage::load);
    connect(m_state, &StateView::retryRequested, this, &PostListPage::load);
    connect(m_postButton, &QPushButton::clicked, this, [this]() {
        if (!m_context.nav)
            return;
        if (!m_boardOpen) {
            m_context.nav->notify(QStringLiteral("该版块已停用，暂不能发帖"));
            return;
        }
        m_context.nav->showPostEditor(m_boardId, 0);
    });
}

void PostListPage::setBoard(qint64 boardId, const QString& boardName)
{
    m_boardId = boardId;
    m_boardName = boardName;
    m_titleLabel->setText(boardName.isEmpty() ? QStringLiteral("帖子列表")
                                              : QStringLiteral("%1 · 帖子").arg(boardName));
}

void PostListPage::onEnter()
{
    if (m_boardId <= 0) {
        m_state->showError(QStringLiteral("未指定版块，请返回版块列表重新进入"));
        return;
    }
    loadBoardStatus();
    load();
}

void PostListPage::loadBoardStatus()
{
    // 只用于决定"发帖"按钮可用性与提示文案；失败不影响主流程（服务端仍会校验）
    m_context.api->listBoards(this, [this](const ApiResult& result) {
        if (!result.ok)
            return;
        QVector<BoardView> boards;
        QString error;
        if (!json::parseBoardList(result.data, &boards, &error))
            return;
        for (const BoardView& board : boards) {
            if (board.boardId == m_boardId) {
                m_boardOpen = board.isOpen();
                break;
            }
        }
        updatePostingAvailability();
    });
}

void PostListPage::updatePostingAvailability()
{
    m_postButton->setEnabled(m_boardOpen);
    m_postButton->setToolTip(m_boardOpen ? QString()
                                         : QStringLiteral("版块已停用，管理员启用后才能发帖"));
    m_hintLabel->setText(
        m_boardOpen
            ? QStringLiteral("共 %1 篇帖子；置顶帖排在最前，其余按更新时间倒序").arg(
                  m_listLayout->count() - 1)
            : QStringLiteral("该版块已停用：可以浏览，但不能发布新帖"));
}

void PostListPage::load()
{
    if (!m_context.isValid() || m_boardId <= 0)
        return;

    m_state->showLoading(QStringLiteral("正在加载帖子…"));
    const qint64 boardId = m_boardId;
    m_context.api->listBoardPosts(boardId, this, [this, boardId](const ApiResult& result) {
        if (boardId != m_boardId)
            return; // 页面已切换到其它版块，丢弃迟到响应
        if (!result.ok) {
            m_state->showError(result.displayMessage());
            return;
        }
        QVector<PostSummary> posts;
        QString error;
        if (!json::parsePostList(result.data, &posts, &error)) {
            m_state->showError(QStringLiteral("通信数据异常：%1").arg(error));
            return;
        }
        if (posts.isEmpty()) {
            m_state->showEmpty(QStringLiteral("该版块还没有帖子，点击右上角“发帖”发布第一篇。"));
            m_hintLabel->setText(QStringLiteral("共 0 篇帖子"));
            return;
        }
        rebuildList(posts);
        m_state->showContent();
        updatePostingAvailability();
    });
}

void PostListPage::rebuildList(const QVector<PostSummary>& posts)
{
    while (m_listLayout->count() > 1) {
        QLayoutItem* item = m_listLayout->takeAt(0);
        if (QWidget* widget = item->widget())
            widget->deleteLater();
        delete item;
    }

    for (const PostSummary& post : posts) {
        QFrame* card = new QFrame(m_listContainer);
        card->setObjectName(QStringLiteral("card"));
        QVBoxLayout* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(16, 12, 16, 12);
        cardLayout->setSpacing(6);

        QHBoxLayout* titleRow = new QHBoxLayout;
        if (post.isPinned) {
            titleRow->addWidget(theme::tagLabel(QStringLiteral("置顶"), QStringLiteral("#dc2626"),
                                               card));
        }
        if (post.isFeatured) {
            titleRow->addWidget(theme::tagLabel(QStringLiteral("精华"), QStringLiteral("#d97706"),
                                               card));
        }
        QLabel* title = new QLabel(post.title, card);
        title->setStyleSheet(QStringLiteral("font-size:15px; font-weight:600;"));
        title->setWordWrap(true);
        titleRow->addWidget(title, 1);

        QPushButton* open = theme::normalButton(QStringLiteral("查看"), card);
        open->setFixedWidth(70);
        titleRow->addWidget(open);
        cardLayout->addLayout(titleRow);

        cardLayout->addWidget(theme::metaLabel(
            QStringLiteral("%1 · 回复 %2 · 发布 %3 · 更新 %4")
                .arg(post.author.nickname)
                .arg(post.replyCount)
                .arg(timefmt::toLocalDisplay(post.createdAt))
                .arg(timefmt::toLocalDisplay(post.updatedAt)),
            card));

        const qint64 postId = post.postId;
        connect(open, &QPushButton::clicked, this, [this, postId]() {
            if (m_context.nav)
                m_context.nav->showPostDetail(postId);
        });

        m_listLayout->insertWidget(m_listLayout->count() - 1, card);
    }
}

} // namespace bbs
