#pragma once

// P13 内容处置（管理员或该版块版主）
// 数据源：GET /posts/{post_id}（含 replies）
// 操作：
//   POST   /moderation/posts/{post_id}/removal      body {reason}（1-200 字符）
//   POST   /moderation/replies/{reply_id}/removal   body {reason}
//   PUT    /moderation/posts/{post_id}/pinned       body {is_pinned}
//   PUT    /moderation/posts/{post_id}/featured     body {is_featured}
// 服务端按 role_code=admin 或 moderator_assignments 是否覆盖该帖所属版块判权，
// 客户端仅做入口显隐，最终以服务端 403 为准（此处不猜测权限）。

#include "ui/Page.h"

#include <QVector>

class QLabel;
class QPushButton;
class QVBoxLayout;
class QWidget;

namespace bbs {

class StateView;

class ModerationPage : public Page
{
    Q_OBJECT
public:
    explicit ModerationPage(const PageContext& context, QWidget* parent = nullptr);

    void setPostId(qint64 postId);
    void onEnter() override;

private:
    void load();
    void render(const PostDetail& detail);
    void removePost(const PostDetail& detail);
    void removeReply(const ReplyView& reply);
    void togglePinned(const PostDetail& detail);
    void toggleFeatured(const PostDetail& detail);

    qint64 m_postId = 0;

    QLabel* m_stepLabel = nullptr;
    QPushButton* m_pinButton = nullptr;
    QPushButton* m_featureButton = nullptr;
    QPushButton* m_removeButton = nullptr;
    StateView* m_state = nullptr;
    QWidget* m_content = nullptr;
    QVBoxLayout* m_contentLayout = nullptr;
};

} // namespace bbs
