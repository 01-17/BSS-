// 实体记录、写入参数与对外投影（DTO）
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "../common/Types.h"

namespace bbs {

// ---------------- 数据库实体 ----------------
struct UserRecord {
    Id userId = 0;
    Id roleId = 0;
    Text roleCode;          // 基础角色 member / admin
    Text username;
    Text nickname;
    Text passwordHash;      // PBKDF2 十六进制
    Text passwordSalt;      // 十六进制
    std::int64_t passwordIterations = 0;
    std::int64_t sessionVersion = 0;
    int status = 1;         // 0 停用、1 正常
    Time registeredAt;
};

struct Credential {
    Text hash;
    Text salt;
    std::int64_t iterations = 0;
};

struct BoardRecord {
    Id boardId = 0;
    Text name;
    Text description;
    int status = 1;
    Time createdAt;
    Time updatedAt;
};

struct PostRecord {
    Id postId = 0;
    Id boardId = 0;
    Id authorId = 0;
    Text authorNickname;
    Text title;
    Text content;
    int status = 1;
    bool isPinned = false;
    bool isFeatured = false;
    Time createdAt;
    Time updatedAt;
    RowCount replyCount = 0;
};

struct ReplyRecord {
    Id replyId = 0;
    Id postId = 0;
    Id authorId = 0;
    Text authorNickname;
    Text content;
    int status = 1;
    Time createdAt;
    Time updatedAt;
};

struct PostInsert {
    Id boardId = 0;
    Id authorId = 0;
    Text title;
    Text content;
    Time at;
};

struct ReplyInsert {
    Id postId = 0;
    Id authorId = 0;
    Text content;
    Time at;
};

// 以 UTC 边界表达的时间区间，代码层不再处理本地日期
struct PostQuery {
    Text keyword;
    std::optional<Id> boardId;
    std::optional<Time> utcStartInclusive;
    std::optional<Time> utcEndExclusive;
    SearchSort sort = SearchSort::Time;
};

struct ModerationLogDraft {
    Id actorId = 0;
    Text action;
    std::optional<Id> targetUserId;
    std::optional<Id> targetBoardId;
    std::optional<Id> targetPostId;
    std::optional<Id> targetReplyId;
    std::optional<Text> reason;
    Time at;
};

// ---------------- 对外投影 ----------------
struct AuthorView {
    Id userId = 0;
    Text nickname;
};

struct ModeratorAssignment {
    Id userId = 0;
    Id boardId = 0;
    Time assignedAt;
};

struct UserProfile {
    Id userId = 0;
    Text username;
    Text nickname;
    Text roleCode;
    int status = 1;
    Time registeredAt;
    std::vector<ModeratorAssignment> moderatorAssignments;
};

struct RegisteredUser {
    Id userId = 0;
    Text username;
    Text nickname;
    Text roleCode;
};

struct LoginResult {
    Text token;
    Time expiresAt;
    UserProfile user;
};

struct BoardView {
    Id boardId = 0;
    Text name;
    Text description;
    int status = 1;
    Time createdAt;
    Time updatedAt;
};

struct PostSummary {
    Id postId = 0;
    Id boardId = 0;
    AuthorView author;
    Text title;
    int status = 1;
    bool isPinned = false;
    bool isFeatured = false;
    Time createdAt;
    Time updatedAt;
    RowCount replyCount = 0;
};

struct ReplyView {
    Id replyId = 0;
    Id postId = 0;
    AuthorView author;
    Text content;
    int status = 1;
    Time createdAt;
    Time updatedAt;
};

struct PostDetail {
    PostSummary summary;
    Text content;
    std::vector<ReplyView> replies;
};

struct BoardChange {
    bool changed = false;
    BoardView board;
};

struct AccountStatusChange {
    bool changed = false;
    Id userId = 0;
    int status = 1;
};

struct GrantResult {
    ModeratorAssignment assignment;
};

struct RevokeResult {
    Id userId = 0;
    Id boardId = 0;
};

struct PostFlagChange {
    bool changed = false;
    Id postId = 0;
    bool value = false;
    Time updatedAt;
};

} // namespace bbs
