#include "ui/ModerationPage.h"

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
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace bbs {

ModerationPage::ModerationPage(const PageContext& context, QWidget* parent)
    : Page(context, parent)
{
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 18);
    root->setSpacing(12);

    QHBoxLayout* header = new QHBoxLayout;
    QPushButton* back = theme::normalButton(QStringLiteral("返回"), this);
    header->addWidget(back);
    header->addWidget(theme::pageTitle(QStringLiteral("内容处置"), this));
    header->addStretch(1);
    m_pinButton = theme::normalButton(QStringLiteral("置顶"), this);
    m_featureButton = theme::normalButton(QStringLiteral("设为精华"), this);
    m_removeButton = theme::dangerButton(QStringLiteral("移除帖子"), this);
    header->addWidget(m_pinButton);
    header->addWidget(m_featureButton);
    header->addWidget(m_removeButton);
    root->addLayout(header);

    m_stepLabel = theme::metaLabel(
        QStringLiteral("处置操作均需二次确认；移除必须填写理由（1-200 字符），"
                       "移除后内容对普通用户不可见。"),
        this);
    m_stepLabel->setWordWrap(true);
    root->addWidget(m_stepLabel);

    m_state = new StateView(this);
    root->addWidget(m_state, 1);

    QScrollArea* scroll = new QScrollArea(m_state);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    m_content = new QWidget(scroll);
    m_contentLayout = new QVBoxLayout(m_content);
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(12);
    scroll->setWidget(m_content);
    m_state->setContentWidget(scroll);

    connect(back, &QPushButton::clicked, this, [this]() {
        if (m_context.nav)
            m_context.nav->goBack();
    });
    connect(m_state, &StateView::retryRequested, this, &ModerationPage::load);
}

void ModerationPage::setPostId(qint64 postId)
{
    m_postId = postId;
}

void ModerationPage::onEnter()
{
    load();
}

void ModerationPage::load()
{
    if (!m_context.isValid())
        return;
    if (m_postId <= 0) {
        m_state->showError(QStringLiteral("未指定要处置的帖子"));
        return;
    }

    const qint64 postId = m_postId;
    m_state->showLoading(QStringLiteral("正在加载帖子内容…"));
    m_context.api->fetchPost(postId, this, [this, postId](const ApiResult& result) {
        if (postId != m_postId)
            return;
        if (!result.ok) {
            m_state->showError(result.displayMessage());
            return;
        }
        PostDetail detail;
        QString error;
        if (!json::parsePostDetail(result.data, &detail, &error)) {
            m_state->showError(QStringLiteral("通信数据异常：%1").arg(error));
            return;
        }
        render(detail);
        m_state->showContent();
    });
}

void ModerationPage::render(const PostDetail& detail)
{
    while (QLayoutItem* item = m_contentLayout->takeAt(0)) {
        if (QWidget* widget = item->widget())
            widget->deleteLater();
        delete item;
    }

    const bool canModerate = m_context.session->canModerate(detail.boardId);

    // ---- 帖子主体 ----
    QFrame* postCard = new QFrame(m_content);
    postCard->setObjectName(QStringLiteral("card"));
    QVBoxLayout* postLayout = new QVBoxLayout(postCard);
    postLayout->setContentsMargins(18, 16, 18, 16);
    postLayout->setSpacing(8);

    QHBoxLayout* titleRow = new QHBoxLayout;
    if (detail.isPinned)
        titleRow->addWidget(
            theme::tagLabel(QStringLiteral("已置顶"), QStringLiteral("#dc2626"), postCard));
    if (detail.isFeatured)
        titleRow->addWidget(
            theme::tagLabel(QStringLiteral("精华"), QStringLiteral("#d97706"), postCard));
    QLabel* title = new QLabel(detail.title, postCard);
    title->setTextFormat(Qt::PlainText);
    title->setWordWrap(true);
    title->setStyleSheet(QStringLiteral("font-size:16px; font-weight:700;"));
    titleRow->addWidget(title, 1);
    postLayout->addLayout(titleRow);

    postLayout->addWidget(theme::metaLabel(
        QStringLiteral("作者 %1 · 版块 ID %2 · 发布 %3 · 回复 %4")
            .arg(detail.author.nickname)
            .arg(detail.boardId)
            .arg(timefmt::toLocalDisplay(detail.createdAt))
            .arg(detail.replyCount),
        postCard));

    QLabel* body = new QLabel(detail.content, postCard);
    body->setTextFormat(Qt::PlainText);
    body->setWordWrap(true);
    body->setTextInteractionFlags(Qt::TextSelectableByMouse);
    postLayout->addWidget(body);

    m_contentLayout->addWidget(postCard);

    // ---- 回复列表 ----
    QLabel* replyTitle = theme::sectionTitle(
        QStringLiteral("回复（%1 条，可单独移除）").arg(detail.replies.size()), m_content);
    m_contentLayout->addWidget(replyTitle);

    if (detail.replies.isEmpty()) {
        m_contentLayout->addWidget(
            theme::metaLabel(QStringLiteral("本帖暂无回复。"), m_content));
    } else {
        for (const ReplyView& reply : detail.replies) {
            QFrame* replyCard = new QFrame(m_content);
            replyCard->setObjectName(QStringLiteral("card"));
            QHBoxLayout* replyLayout = new QHBoxLayout(replyCard);
            replyLayout->setContentsMargins(16, 12, 16, 12);
            replyLayout->setSpacing(12);

            QVBoxLayout* infoLayout = new QVBoxLayout;
            infoLayout->setSpacing(4);
            infoLayout->addWidget(theme::metaLabel(
                QStringLiteral("#%1 · %2 · %3")
                    .arg(reply.replyId)
                    .arg(reply.author.nickname, timefmt::toLocalDisplay(reply.createdAt)),
                replyCard));
            QLabel* text = new QLabel(reply.content, replyCard);
            text->setTextFormat(Qt::PlainText);
            text->setWordWrap(true);
            infoLayout->addWidget(text);
            replyLayout->addLayout(infoLayout, 1);

            QPushButton* removeReplyButton =
                theme::dangerButton(QStringLiteral("移除回复"), replyCard);
            const ReplyView snapshot = reply;
            connect(removeReplyButton, &QPushButton::clicked, this,
                    [this, snapshot]() { removeReply(snapshot); });
            replyLayout->addWidget(removeReplyButton);

            m_contentLayout->addWidget(replyCard);
        }
    }

    m_contentLayout->addStretch(1);

    // ---- 顶部按钮状态 ----
    m_pinButton->setText(detail.isPinned ? QStringLiteral("取消置顶") : QStringLiteral("置顶"));
    m_featureButton->setText(detail.isFeatured ? QStringLiteral("取消精华")
                                               : QStringLiteral("设为精华"));
    m_pinButton->setEnabled(canModerate);
    m_featureButton->setEnabled(canModerate);
    m_removeButton->setEnabled(canModerate);
    if (!canModerate) {
        m_stepLabel->setText(
            QStringLiteral("当前账号不是该版块版主或管理员，仅可查看内容，处置按钮已禁用。"));
    } else {
        m_stepLabel->setText(
            QStringLiteral("处置操作均需二次确认；移除必须填写理由（1-200 字符），"
                           "移除后内容对普通用户不可见。"));
    }

    disconnect(m_pinButton, static_cast<const char*>(nullptr), this, nullptr);
    disconnect(m_featureButton, static_cast<const char*>(nullptr), this, nullptr);
    disconnect(m_removeButton, static_cast<const char*>(nullptr), this, nullptr);

    const PostDetail snapshot = detail;
    connect(m_pinButton, &QPushButton::clicked, this,
            [this, snapshot]() { togglePinned(snapshot); });
    connect(m_featureButton, &QPushButton::clicked, this,
            [this, snapshot]() { toggleFeatured(snapshot); });
    connect(m_removeButton, &QPushButton::clicked, this,
            [this, snapshot]() { removePost(snapshot); });
}

void ModerationPage::removePost(const PostDetail& detail)
{
    if (!m_context.isValid())
        return;

    QString reason;
    if (!dialogs::promptReason(
            this, QStringLiteral("移除帖子"),
            QStringLiteral("将移除帖子“%1”。移除后普通用户不可见，且不可自助恢复。")
                .arg(detail.title),
            QStringLiteral("移除理由"), &reason))
        return;

    m_removeButton->setEnabled(false);
    const qint64 postId = detail.postId;
    m_context.api->moderateRemovePost(postId, reason, this, [this, postId](const ApiResult& result) {
        m_removeButton->setEnabled(true);
        if (!result.ok) {
            dialogs::warn(this, QStringLiteral("移除失败"), result.displayMessage());
            return;
        }
        PostStatusResult parsed;
        QString error;
        if (!json::parsePostStatus(result.data, &parsed, &error)) {
            dialogs::warn(this, QStringLiteral("通信数据异常"), error);
            return;
        }
        if (m_context.nav) {
            m_context.nav->notify(QStringLiteral("帖子已移除"));
            m_context.nav->showBoardList();
        }
        Q_UNUSED(postId)
    });
}

void ModerationPage::removeReply(const ReplyView& reply)
{
    if (!m_context.isValid())
        return;

    QString reason;
    if (!dialogs::promptReason(
            this, QStringLiteral("移除回复"),
            QStringLiteral("将移除 %1 的这条回复。移除后普通用户不可见。").arg(reply.author.nickname),
            QStringLiteral("移除理由"), &reason))
        return;

    m_context.api->moderateRemoveReply(reply.replyId, reason, this,
                                       [this](const ApiResult& result) {
        if (!result.ok) {
            dialogs::warn(this, QStringLiteral("移除失败"), result.displayMessage());
            return;
        }
        ReplyStatusResult parsed;
        QString error;
        if (!json::parseReplyStatus(result.data, &parsed, &error)) {
            dialogs::warn(this, QStringLiteral("通信数据异常"), error);
            return;
        }
        if (m_context.nav)
            m_context.nav->notify(QStringLiteral("回复已移除"));
        load();
    });
}

void ModerationPage::togglePinned(const PostDetail& detail)
{
    if (!m_context.isValid())
        return;

    const bool target = !detail.isPinned;
    if (!dialogs::confirm(this, target ? QStringLiteral("置顶帖子") : QStringLiteral("取消置顶"),
                          target ? QStringLiteral("确认将该帖置顶到版块列表顶部？")
                                 : QStringLiteral("确认取消该帖的置顶状态？"),
                          target ? QStringLiteral("置顶") : QStringLiteral("取消置顶")))
        return;

    m_pinButton->setEnabled(false);
    m_context.api->moderateSetPinned(detail.postId, target, this,
                                     [this](const ApiResult& result) {
        m_pinButton->setEnabled(true);
        if (!result.ok) {
            dialogs::warn(this, QStringLiteral("操作未生效"), result.displayMessage());
            return;
        }
        PostFlagResult parsed;
        QString error;
        if (!json::parsePostFlag(result.data, QStringLiteral("is_pinned"), &parsed, &error)) {
            dialogs::warn(this, QStringLiteral("通信数据异常"), error);
            return;
        }
        if (m_context.nav)
            m_context.nav->notify(parsed.changed ? QStringLiteral("置顶状态已更新")
                                                 : QStringLiteral("置顶状态未变化"));
        load();
    });
}

void ModerationPage::toggleFeatured(const PostDetail& detail)
{
    if (!m_context.isValid())
        return;

    const bool target = !detail.isFeatured;
    if (!dialogs::confirm(this, target ? QStringLiteral("设为精华") : QStringLiteral("取消精华"),
                          target ? QStringLiteral("确认将该帖标记为精华？")
                                 : QStringLiteral("确认取消该帖的精华标记？"),
                          target ? QStringLiteral("设为精华") : QStringLiteral("取消精华")))
        return;

    m_featureButton->setEnabled(false);
    m_context.api->moderateSetFeatured(detail.postId, target, this,
                                       [this](const ApiResult& result) {
        m_featureButton->setEnabled(true);
        if (!result.ok) {
            dialogs::warn(this, QStringLiteral("操作未生效"), result.displayMessage());
            return;
        }
        PostFlagResult parsed;
        QString error;
        if (!json::parsePostFlag(result.data, QStringLiteral("is_featured"), &parsed, &error)) {
            dialogs::warn(this, QStringLiteral("通信数据异常"), error);
            return;
        }
        if (m_context.nav)
            m_context.nav->notify(parsed.changed ? QStringLiteral("精华标记已更新")
                                                 : QStringLiteral("精华标记未变化"));
        load();
    });
}

} // namespace bbs
