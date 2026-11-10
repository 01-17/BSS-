#pragma once

// P06 帖子详情
// 数据源：GET /posts/{post_id}（返回 PostSummary 字段 + content + replies）。
// 能力：
//  - 正文纯文本渲染（Qt::PlainText，杜绝 HTML 注入与外部资源加载）；
//  - 本人可编辑 / 删除自己的帖子与回复（删除必须二次确认）；
//  - 管理员或本版块版主可"内容处置"：置顶、加精、移除帖子、移除回复（填理由）；
//  - 回复发布/编辑为写请求：提交中禁用按钮，超时保留输入且不自动重发。

#include "ui/Page.h"

class QLabel;
class QPushButton;
class QTextEdit;
class QVBoxLayout;
class QWidget;

namespace bbs {

class StateView;
struct ApiResult;

class PostDetailPage : public Page
{
    Q_OBJECT
public:
    explicit PostDetailPage(const PageContext& context, QWidget* parent = nullptr);

    void setPost(qint64 postId);
    void onEnter() override;

private:
    void load();
    void render(const PostDetail& detail);
    void renderReplies();

    void editPost();
    void deleteOwnPost();
    void togglePinned();
    void toggleFeatured();
    void openModeration();

    void submitReply();
    void beginEditReply(qint64 replyId);
    void cancelReplyEdit();
    void deleteOwnReply(qint64 replyId);
    void removeReplyByModerator(qint64 replyId);

    void applyFlagResult(const ApiResult& result, bool pinnedFlag);
    void setReplyBusy(bool busy);

    qint64 m_postId = 0;
    PostDetail m_detail;
    qint64 m_editingReplyId = 0;

    QLabel* m_titleLabel = nullptr;
    QLabel* m_tagRow = nullptr;
    QLabel* m_metaLabel = nullptr;
    QLabel* m_contentLabel = nullptr;
    QLabel* m_replyCountLabel = nullptr;

    QPushButton* m_editButton = nullptr;
    QPushButton* m_deleteButton = nullptr;
    QPushButton* m_pinButton = nullptr;
    QPushButton* m_featureButton = nullptr;
    QPushButton* m_moderateButton = nullptr;
    QPushButton* m_refreshButton = nullptr;

    QWidget* m_replyList = nullptr;
    QVBoxLayout* m_replyLayout = nullptr;

    QTextEdit* m_replyEdit = nullptr;
    QPushButton* m_replySubmitButton = nullptr;
    QPushButton* m_replyCancelButton = nullptr;
    QLabel* m_replyHint = nullptr;

    StateView* m_state = nullptr;
};

} // namespace bbs
