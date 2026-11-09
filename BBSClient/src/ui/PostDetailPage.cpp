#include "ui/PostDetailPage.h"

#include "model/JsonMapper.h"
#include "net/ApiClient.h"
#include "session/SessionManager.h"
#include "ui/Dialogs.h"
#include "ui/StateView.h"
#include "ui/Theme.h"
#include "util/TimeFormat.h"
#include "util/Validators.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTextEdit>
#include <QVBoxLayout>

namespace bbs {

PostDetailPage::PostDetailPage(const PageContext& context, QWidget* parent)
    : Page(context, parent)
{
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 18);
    root->setSpacing(12);

    QHBoxLayout* header = new QHBoxLayout;
    QPushButton* back = theme::normalButton(QStringLiteral("返回列表"), this);
    header->addWidget(back);
    header->addWidget(theme::pageTitle(QStringLiteral("帖子详情"), this));
    header->addStretch(1);
    m_editButton = theme::normalButton(QStringLiteral("编辑"), this);
    m_deleteButton = theme::dangerButton(QStringLiteral("删除"), this);
    m_pinButton = theme::normalButton(QStringLiteral("置顶"), this);
    m_featureButton = theme::normalButton(QStringLiteral("加精"), this);
    m_moderateButton = theme::normalButton(QStringLiteral("内容处置"), this);
    m_refreshButton = theme::normalButton(QStringLiteral("刷新"), this);
    header->addWidget(m_editButton);
    header->addWidget(m_deleteButton);
    header->addWidget(m_pinButton);
    header->addWidget(m_featureButton);
    header->addWidget(m_moderateButton);
    header->addWidget(m_refreshButton);
    root->addLayout(header);

    m_state = new StateView(this);
    root->addWidget(m_state, 1);

    QScrollArea* scroll = new QScrollArea(m_state);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    QWidget* content = new QWidget(scroll);
    QVBoxLayout* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(12);

    // ---- 帖子主体 ----
    QFrame* postCard = new QFrame(content);
    postCard->setObjectName(QStringLiteral("card"));
    QVBoxLayout* postLayout = new QVBoxLayout(postCard);
    postLayout->setContentsMargins(18, 16, 18, 16);
    postLayout->setSpacing(8);

    m_titleLabel = new QLabel(postCard);
    m_titleLabel->setTextFormat(Qt::PlainText);
    m_titleLabel->setWordWrap(true);
    m_titleLabel->setStyleSheet(QStringLiteral("font-size:18px; font-weight:600;"));
    postLayout->addWidget(m_titleLabel);

    m_tagRow = new QLabel(postCard);
    m_tagRow->setTextFormat(Qt::PlainText);
    postLayout->addWidget(m_tagRow);

    m_metaLabel = theme::metaLabel(QString(), postCard);
    postLayout->addWidget(m_metaLabel);

    m_contentLabel = new QLabel(postCard);
    m_contentLabel->setTextFormat(Qt::PlainText); // 纯文本渲染
    m_contentLabel->setWordWrap(true);
    m_contentLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_contentLabel->setStyleSheet(QStringLiteral("font-size:14px; line-height:1.6;"));
    postLayout->addWidget(m_contentLabel);

    contentLayout->addWidget(postCard);

    // ---- 回复区 ----
    QFrame* replyCard = new QFrame(content);
    replyCard->setObjectName(QStringLiteral("card"));
    QVBoxLayout* replyCardLayout = new QVBoxLayout(replyCard);
    replyCardLayout->setContentsMargins(18, 16, 18, 16);
    replyCardLayout->setSpacing(10);

    m_replyCountLabel = theme::sectionTitle(QStringLiteral("全部回复"), replyCard);
    replyCardLayout->addWidget(m_replyCountLabel);

    m_replyList = new QWidget(replyCard);
    m_replyLayout = new QVBoxLayout(m_replyList);
    m_replyLayout->setContentsMargins(0, 0, 0, 0);
    m_replyLayout->setSpacing(8);
    m_replyLayout->addStretch(1);
    replyCardLayout->addWidget(m_replyList);

    replyCardLayout->addWidget(theme::sectionTitle(QStringLiteral("发表回复"), replyCard));

    m_replyEdit = new QTextEdit(replyCard);
    m_replyEdit->setAcceptRichText(false); // 只接受纯文本
    m_replyEdit->setPlaceholderText(QStringLiteral("回复内容 1-1000 个字符"));
    m_replyEdit->setFixedHeight(96);
    replyCardLayout->addWidget(m_replyEdit);

    m_replyHint = theme::metaLabel(QStringLiteral("支持纯文本，最多 1000 个字符"), replyCard);
    replyCardLayout->addWidget(m_replyHint);

    QHBoxLayout* replyButtons = new QHBoxLayout;
    replyButtons->addStretch(1);
    m_replyCancelButton = theme::normalButton(QStringLiteral("取消编辑"), replyCard);
    m_replyCancelButton->setVisible(false);
    m_replySubmitButton = theme::primaryButton(QStringLiteral("发表回复"), replyCard);
    replyButtons->addWidget(m_replyCancelButton);
    replyButtons->addWidget(m_replySubmitButton);
    replyCardLayout->addLayout(replyButtons);

    contentLayout->addWidget(replyCard);
    contentLayout->addStretch(1);

    scroll->setWidget(content);
    m_state->setContentWidget(scroll);

    connect(back, &QPushButton::clicked, this, [this]() {
        if (m_context.nav)
            m_context.nav->goBack();
    });
    connect(m_refreshButton, &QPushButton::clicked, this, &PostDetailPage::load);
    connect(m_state, &StateView::retryRequested, this, &PostDetailPage::load);
    connect(m_editButton, &QPushButton::clicked, this, &PostDetailPage::editPost);
    connect(m_deleteButton, &QPushButton::clicked, this, &PostDetailPage::deleteOwnPost);
    connect(m_pinButton, &QPushButton::clicked, this, &PostDetailPage::togglePinned);
    connect(m_featureButton, &QPushButton::clicked, this, &PostDetailPage::toggleFeatured);
    connect(m_moderateButton, &QPushButton::clicked, this, &PostDetailPage::openModeration);
    connect(m_replySubmitButton, &QPushButton::clicked, this, &PostDetailPage::submitReply);
    connect(m_replyCancelButton, &QPushButton::clicked, this, &PostDetailPage::cancelReplyEdit);
}

void PostDetailPage::setPost(qint64 postId)
{
    m_postId = postId;
    m_editingReplyId = 0;
    m_detail = PostDetail();
}

void PostDetailPage::onEnter()
{
    if (m_postId <= 0) {
        m_state->showError(QStringLiteral("未指定帖子，请返回列表重新进入"));
        return;
    }
    cancelReplyEdit();
    load();
}

void PostDetailPage::load()
{
    if (!m_context.isValid() || m_postId <= 0)
        return;

    m_state->showLoading(QStringLiteral("正在加载帖子详情…"));
    const qint64 postId = m_postId;
    m_context.api->fetchPost(postId, this, [this, postId](const ApiResult& result) {
        if (postId != m_postId)
            return; // 已切换到其它帖子，丢弃迟到响应
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

void PostDetailPage::render(const PostDetail& detail)
{
    m_detail = detail;

    m_titleLabel->setText(detail.title);

    QStringList tags;
    if (detail.isPinned)
        tags << QStringLiteral("〔置顶〕");
    if (detail.isFeatured)
        tags << QStringLiteral("〔精华〕");
    if (detail.status != 1)
        tags << QStringLiteral("〔已移除〕");
    m_tagRow->setText(tags.join(QStringLiteral(" ")));
    m_tagRow->setVisible(!tags.isEmpty());

    m_metaLabel->setText(QStringLiteral("作者：%1 · 发布 %2 · 更新 %3 · 共 %4 条回复")
                             .arg(detail.author.nickname)
                             .arg(timefmt::toLocalDisplay(detail.createdAt))
                             .arg(timefmt::toLocalDisplay(detail.updatedAt))
                             .arg(detail.replyCount));
    m_contentLabel->setText(detail.content);

    const bool loggedIn = m_context.session && m_context.session->isLoggedIn();
    const bool isOwner = loggedIn && m_context.session->userId() == detail.author.userId;
    // 管理员可编辑任意用户的帖子（与服务端 PUT /posts/{id} 放行管理员一致）
    const bool canEdit = isOwner || (loggedIn && m_context.session->isAdmin());
    const bool canModerate = loggedIn && m_context.session->canModerate(detail.boardId);
    const bool visible = detail.status == 1;

    m_editButton->setVisible(canEdit && visible);
    m_deleteButton->setVisible(isOwner && visible);
    m_moderateButton->setVisible(canModerate);
    m_pinButton->setVisible(canModerate);
    m_featureButton->setVisible(canModerate);
    m_pinButton->setText(detail.isPinned ? QStringLiteral("取消置顶") : QStringLiteral("置顶"));
    m_featureButton->setText(detail.isFeatured ? QStringLiteral("取消加精")
                                               : QStringLiteral("设为精华"));

    // 已移除的帖子：关闭回复入口，避免"对已删除内容继续互动"
    const bool canReply = loggedIn && visible;
    m_replyEdit->setEnabled(canReply);
    m_replySubmitButton->setEnabled(canReply);
    m_replyHint->setText(canReply ? QStringLiteral("支持纯文本，最多 1000 个字符")
                                  : QStringLiteral("该帖子已不可回复"));

    renderReplies();
}

void PostDetailPage::renderReplies()
{
    while (m_replyLayout->count() > 1) {
        QLayoutItem* item = m_replyLayout->takeAt(0);
        if (QWidget* widget = item->widget())
            widget->deleteLater();
        delete item;
    }

    m_replyCountLabel->setText(QStringLiteral("全部回复（%1）").arg(m_detail.replies.size()));

    if (m_detail.replies.isEmpty()) {
        QLabel* empty = theme::metaLabel(QStringLiteral("还没有回复，来发表第一条回复吧。"),
                                        m_replyList);
        m_replyLayout->insertWidget(0, empty);
        return;
    }

    const bool loggedIn = m_context.session && m_context.session->isLoggedIn();
    const bool canModerate = loggedIn && m_context.session->canModerate(m_detail.boardId);

    for (const ReplyView& reply : m_detail.replies) {
        QFrame* item = new QFrame(m_replyList);
        item->setObjectName(QStringLiteral("replyItem"));
        QVBoxLayout* itemLayout = new QVBoxLayout(item);
        itemLayout->setContentsMargins(12, 10, 12, 10);
        itemLayout->setSpacing(6);

        QHBoxLayout* metaRow = new QHBoxLayout;
        QLabel* author = new QLabel(reply.author.nickname, item);
        author->setStyleSheet(QStringLiteral("font-weight:600;"));
        metaRow->addWidget(author);
        metaRow->addWidget(theme::metaLabel(
            QStringLiteral("%1 · 更新 %2")
                .arg(timefmt::toLocalDisplay(reply.createdAt),
                     timefmt::toLocalDisplay(reply.updatedAt)),
            item));
        if (reply.status != 1)
            metaRow->addWidget(theme::tagLabel(QStringLiteral("已移除"),
                                               QStringLiteral("#9ca3af"), item));
        metaRow->addStretch(1);

        const bool isOwner = loggedIn && m_context.session->userId() == reply.author.userId;
        if (isOwner && reply.status == 1) {
            QPushButton* edit = theme::normalButton(QStringLiteral("编辑"), item);
            QPushButton* remove = theme::dangerButton(QStringLiteral("删除"), item);
            const qint64 replyId = reply.replyId;
            connect(edit, &QPushButton::clicked, this,
                    [this, replyId]() { beginEditReply(replyId); });
            connect(remove, &QPushButton::clicked, this,
                    [this, replyId]() { deleteOwnReply(replyId); });
            metaRow->addWidget(edit);
            metaRow->addWidget(remove);
        }
        if (canModerate && reply.status == 1) {
            QPushButton* moderate = theme::normalButton(QStringLiteral("移除"), item);
            const qint64 replyId = reply.replyId;
            connect(moderate, &QPushButton::clicked, this,
                    [this, replyId]() { removeReplyByModerator(replyId); });
            metaRow->addWidget(moderate);
        }

        itemLayout->addLayout(metaRow);

        QLabel* text = new QLabel(reply.content, item);
        text->setTextFormat(Qt::PlainText);
        text->setWordWrap(true);
        text->setTextInteractionFlags(Qt::TextSelectableByMouse);
        itemLayout->addWidget(text);

        m_replyLayout->insertWidget(m_replyLayout->count() - 1, item);
    }
}

void PostDetailPage::editPost()
{
    if (!m_context.nav)
        return;
    m_context.nav->showPostEditor(m_detail.boardId, m_detail.postId);
}

void PostDetailPage::deleteOwnPost()
{
    if (!m_context.isValid() || m_detail.postId <= 0)
        return;
    if (!dialogs::confirmDestructive(
            this, QStringLiteral("删除帖子"),
            QStringLiteral("确认删除《%1》？删除后该帖子将从版块列表消失，且无法自行恢复。")
                .arg(m_detail.title),
            QStringLiteral("删除")))
        return;

    m_deleteButton->setEnabled(false);
    const qint64 postId = m_detail.postId;
    m_context.api->deletePost(postId, this, [this](const ApiResult& result) {
        m_deleteButton->setEnabled(true);
        if (!result.ok) {
            dialogs::warn(this, QStringLiteral("删除失败"), result.displayMessage());
            return;
        }
        PostStatusResult parsed;
        QString error;
        if (!json::parsePostStatus(result.data, &parsed, &error)) {
            dialogs::warn(this, QStringLiteral("通信数据异常"), error);
            return;
        }
        if (m_context.nav) {
            m_context.nav->notify(QStringLiteral("帖子已删除"));
            m_context.nav->showPostList(m_detail.boardId, QString());
        }
    });
}

void PostDetailPage::togglePinned()
{
    if (!m_context.isValid())
        return;
    const bool target = !m_detail.isPinned;
    m_pinButton->setEnabled(false);
    const qint64 postId = m_detail.postId;
    m_context.api->moderateSetPinned(postId, target, this,
                                     [this, target](const ApiResult& result) {
        m_pinButton->setEnabled(true);
        applyFlagResult(result, target);
    });
}

void PostDetailPage::toggleFeatured()
{
    if (!m_context.isValid())
        return;
    const bool target = !m_detail.isFeatured;
    m_featureButton->setEnabled(false);
    const qint64 postId = m_detail.postId;
    m_context.api->moderateSetFeatured(postId, target, this,
                                       [this, target](const ApiResult& result) {
        m_featureButton->setEnabled(true);
        applyFlagResult(result, target);
    });
}

void PostDetailPage::applyFlagResult(const ApiResult& result, bool pinnedFlag)
{
    if (!result.ok) {
        dialogs::warn(this, QStringLiteral("操作未生效"), result.displayMessage());
        return;
    }
    if (m_context.nav)
        m_context.nav->notify(QStringLiteral("操作已生效"));
    Q_UNUSED(pinnedFlag)
    load(); // 以服务端返回为准重新拉取（changed=false 时界面也不会与真实状态不一致）
}

void PostDetailPage::openModeration()
{
    if (m_context.nav)
        m_context.nav->showModeration(m_detail.postId);
}

void PostDetailPage::submitReply()
{
    if (!m_context.isValid() || m_detail.postId <= 0)
        return;

    const QString raw = m_replyEdit->toPlainText();
    const validators::Check check = validators::replyContent(raw);
    if (!check.ok) {
        m_replyHint->setText(check.error);
        return;
    }
    m_replyHint->setText(QStringLiteral("支持纯文本，最多 1000 个字符"));

    setReplyBusy(true);
    const QString content = check.normalized;

    if (m_editingReplyId == 0) {
        const qint64 postId = m_detail.postId;
        m_context.api->createReply(postId, content, this, [this](const ApiResult& result) {
            setReplyBusy(false);
            if (!result.ok) {
                // 超时/失败：保留输入，由用户自行决定是否重发（禁止自动重发）
                m_replyHint->setText(result.isTimeout()
                                         ? QStringLiteral("提交超时，内容已保留，请检查网络后手动重新提交")
                                         : result.displayMessage());
                return;
            }
            m_replyEdit->clear();
            m_replyHint->setText(QStringLiteral("回复已发布"));
            load();
        });
        return;
    }

    const qint64 replyId = m_editingReplyId;
    m_context.api->updateReply(replyId, content, this, [this](const ApiResult& result) {
        setReplyBusy(false);
        if (!result.ok) {
            m_replyHint->setText(result.isTimeout()
                                     ? QStringLiteral("提交超时，内容已保留，请检查网络后手动重新提交")
                                     : result.displayMessage());
            return;
        }
        cancelReplyEdit();
        load();
    });
}

void PostDetailPage::beginEditReply(qint64 replyId)
{
    for (const ReplyView& reply : m_detail.replies) {
        if (reply.replyId != replyId)
            continue;
        m_editingReplyId = replyId;
        m_replyEdit->setPlainText(reply.content);
        m_replyEdit->setFocus();
        m_replySubmitButton->setText(QStringLiteral("保存修改"));
        m_replyCancelButton->setVisible(true);
        m_replyHint->setText(QStringLiteral("正在编辑该回复（不会自动提交）"));
        return;
    }
}

void PostDetailPage::cancelReplyEdit()
{
    m_editingReplyId = 0;
    m_replyEdit->clear();
    m_replySubmitButton->setText(QStringLiteral("发表回复"));
    m_replyCancelButton->setVisible(false);
    m_replyHint->setText(QStringLiteral("支持纯文本，最多 1000 个字符"));
}

void PostDetailPage::setReplyBusy(bool busy)
{
    m_replySubmitButton->setEnabled(!busy);
    m_replySubmitButton->setText(busy ? QStringLiteral("提交中…")
                                      : (m_editingReplyId == 0 ? QStringLiteral("发表回复")
                                                               : QStringLiteral("保存修改")));
    m_replyCancelButton->setEnabled(!busy);
    m_replyEdit->setReadOnly(busy);
}

void PostDetailPage::deleteOwnReply(qint64 replyId)
{
    if (!m_context.isValid())
        return;
    if (!dialogs::confirmDestructive(this, QStringLiteral("删除回复"),
                                     QStringLiteral("确认删除这条回复？删除后无法恢复。"),
                                     QStringLiteral("删除")))
        return;

    m_context.api->deleteReply(replyId, this, [this](const ApiResult& result) {
        if (!result.ok) {
            dialogs::warn(this, QStringLiteral("删除失败"), result.displayMessage());
            return;
        }
        if (m_editingReplyId > 0)
            cancelReplyEdit();
        if (m_context.nav)
            m_context.nav->notify(QStringLiteral("回复已删除"));
        load();
    });
}

void PostDetailPage::removeReplyByModerator(qint64 replyId)
{
    if (!m_context.isValid())
        return;
    QString reason;
    if (!dialogs::promptReason(this, QStringLiteral("移除回复"),
                               QStringLiteral("该回复将被移出可见范围（逻辑删除，可由管理员追溯）。"),
                               QStringLiteral("处置理由"), &reason))
        return;

    m_context.api->moderateRemoveReply(replyId, reason, this, [this](const ApiResult& result) {
        if (!result.ok) {
            dialogs::warn(this, QStringLiteral("移除失败"), result.displayMessage());
            return;
        }
        if (m_context.nav)
            m_context.nav->notify(QStringLiteral("回复已移除"));
        load();
    });
}

} // namespace bbs
