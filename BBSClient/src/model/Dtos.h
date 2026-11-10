#pragma once

// 数据模型层：与《第7组_BBS论坛_HTTP_API接口交接规范》§3.2.2 公开响应对象一一对应。
// 约定：
//  - 标识一律使用 qint64（64 位十进制整数，不走 double 中转）；
//  - 时间字段保留服务端原始 UTC 字符串（yyyy-MM-ddTHH:mm:ssZ），展示时由 util/TimeFormat 转本地；
//  - status 使用整数 0/1 原样承载，布尔标记使用 bool（服务端为 JSON 布尔）。

#include <QString>
#include <QVector>

namespace bbs {

// 帖子 / 回复的作者显示信息（AuthorView）
struct AuthorView
{
    qint64 userId = 0;
    QString nickname;
};

// 注册成功结果（RegisteredUser）
struct RegisteredUser
{
    qint64 userId = 0;
    QString username;
    QString nickname;
    QString roleCode; // 固定 member
};

// 当前版主授权（ModeratorAssignment）
struct ModeratorAssignment
{
    qint64 userId = 0;
    qint64 boardId = 0;
    QString assignedAt;
};

// 本人资料 / 管理员账号条目（UserProfile）
struct UserProfile
{
    qint64 userId = 0;
    QString username;
    QString nickname;
    QString roleCode; // member | admin
    int status = 1;   // 1 正常，0 停用
    QString registeredAt;
    QVector<ModeratorAssignment> assignments;

    bool isAdmin() const { return roleCode == QStringLiteral("admin"); }
    bool isActive() const { return status == 1; }
    bool moderates(qint64 boardId) const
    {
        for (const ModeratorAssignment& item : assignments) {
            if (item.boardId == boardId)
                return true;
        }
        return false;
    }
};

// 登录结果（LoginResult）
struct LoginResult
{
    QString token;
    QString expiresAt;
    UserProfile user;
};

// 版块（BoardView）
struct BoardView
{
    qint64 boardId = 0;
    QString name;
    QString description;
    int status = 1; // 1 开放，0 停用
    QString createdAt;
    QString updatedAt;

    bool isOpen() const { return status == 1; }
};

// 帖子摘要（PostSummary）
struct PostSummary
{
    qint64 postId = 0;
    qint64 boardId = 0;
    AuthorView author;
    QString title;
    int status = 1;
    bool isPinned = false;
    bool isFeatured = false;
    QString createdAt;
    QString updatedAt;
    int replyCount = 0;
};

// 回复（ReplyView）
struct ReplyView
{
    qint64 replyId = 0;
    qint64 postId = 0;
    AuthorView author;
    QString content;
    int status = 1;
    QString createdAt;
    QString updatedAt;
};

// 帖子详情（PostDetail）：PostSummary 字段平铺 + content + replies
struct PostDetail
{
    qint64 postId = 0;
    qint64 boardId = 0;
    AuthorView author;
    QString title;
    int status = 1;
    bool isPinned = false;
    bool isFeatured = false;
    QString createdAt;
    QString updatedAt;
    int replyCount = 0;
    QString content;
    QVector<ReplyView> replies;
};

// 版块管理结果 {changed, board}
struct BoardMutationResult
{
    bool changed = false;
    BoardView board;
};

// 账号启停结果 {changed, user_id, status}
struct AccountStatusResult
{
    bool changed = false;
    qint64 userId = 0;
    int status = 1;
};

// 授予版主结果 {assigned=true, assignment}
struct GrantResult
{
    bool assigned = false;
    ModeratorAssignment assignment;
};

// 撤销版主结果 {assigned=false, user_id, board_id}
struct RevokeResult
{
    bool assigned = true; // 服务端固定返回 false
    qint64 userId = 0;
    qint64 boardId = 0;
};

// 删帖 / 移除帖子 {post_id, status=0}
struct PostStatusResult
{
    qint64 postId = 0;
    int status = 0;
};

// 删回复 / 移除回复 {reply_id, status=0}
struct ReplyStatusResult
{
    qint64 replyId = 0;
    int status = 0;
};

// 置顶 / 加精结果 {changed, post_id, is_pinned|is_featured, updated_at}
struct PostFlagResult
{
    bool changed = false;
    qint64 postId = 0;
    bool flag = false;
    QString updatedAt;
};

} // namespace bbs
