#include "ModerationService.h"

#include <unordered_map>

#include "../common/Error.h"
#include "../common/Util.h"
#include "../dao/Projections.h"
#include "Validation.h"

namespace bbs {

ModerationService::ModerationService(DatabaseManager& manager, UserRepository& users,
                                     BoardRepository& boards, PostRepository& posts,
                                     ReplyRepository& replies,
                                     ModeratorAssignmentRepository& assignments,
                                     ModerationLogRepository& logs,
                                     PermissionService& permissions, AuthService& auth)
    : manager_(manager), users_(users), boards_(boards), posts_(posts), replies_(replies),
      assignments_(assignments), logs_(logs), permissions_(permissions), auth_(auth) {}

BoardChange ModerationService::createBoard(DbContext& cx, const SessionContext& session,
                                           const Text& rawName, const Text& rawDescription) {
    const CurrentUser current = permissions_.requireAdmin(cx, session);
    const Text name = validation::boardName(rawName);
    const Text description = validation::boardDescription(rawDescription);

    Transaction transaction(manager_, cx);
    if (boards_.findByName(cx, name)) {
        throw errors::conflict("版块名称已存在");
    }
    const Time now = util::nowUtc();
    const Id boardId = boards_.insert(cx, name, description, now);

    ModerationLogDraft log;
    log.actorId = current.userId;
    log.action = "board_create";
    log.targetBoardId = boardId;
    log.at = now;
    logs_.append(cx, log);
    transaction.commit();

    BoardChange result;
    result.changed = true;
    const auto record = boards_.findById(cx, boardId);
    if (record) result.board = projection::board(*record);
    return result;
}

BoardChange ModerationService::updateBoard(DbContext& cx, const SessionContext& session,
                                           Id boardId, const Text& rawName,
                                           const Text& rawDescription) {
    const CurrentUser current = permissions_.requireAdmin(cx, session);
    const Text name = validation::boardName(rawName);
    const Text description = validation::boardDescription(rawDescription);

    Transaction transaction(manager_, cx);
    const auto existing = boards_.findById(cx, boardId);
    if (!existing) {
        throw errors::notFound("版块不存在");
    }
    const auto sameName = boards_.findByName(cx, name);
    if (sameName && sameName->boardId != boardId) {
        throw errors::conflict("版块名称已存在");
    }

    BoardChange result;
    if (existing->name == name && existing->description == description) {
        result.changed = false;
        result.board = projection::board(*existing);
        transaction.commit();
        return result;
    }

    const Time now = util::nowUtc();
    boards_.updateInfo(cx, boardId, name, description, now);
    ModerationLogDraft log;
    log.actorId = current.userId;
    log.action = "board_update";
    log.targetBoardId = boardId;
    log.at = now;
    logs_.append(cx, log);
    transaction.commit();

    result.changed = true;
    const auto record = boards_.findById(cx, boardId);
    if (record) result.board = projection::board(*record);
    return result;
}

BoardChange ModerationService::setBoardStatus(DbContext& cx, const SessionContext& session,
                                              Id boardId, int status) {
    const CurrentUser current = permissions_.requireAdmin(cx, session);

    Transaction transaction(manager_, cx);
    const auto existing = boards_.findById(cx, boardId);
    if (!existing) {
        throw errors::notFound("版块不存在");
    }

    BoardChange result;
    if (existing->status == status) {
        result.changed = false;
        result.board = projection::board(*existing);
        transaction.commit();
        return result;
    }

    const Time now = util::nowUtc();
    boards_.setStatus(cx, boardId, status, now);

    ModerationLogDraft log;
    log.actorId = current.userId;
    log.action = status == 1 ? "board_enable" : "board_disable";
    log.targetBoardId = boardId;
    log.at = now;
    logs_.append(cx, log);
    transaction.commit();

    result.changed = true;
    const auto record = boards_.findById(cx, boardId);
    if (record) result.board = projection::board(*record);
    return result;
}

std::vector<UserProfile> ModerationService::listUsers(DbContext& cx,
                                                      const SessionContext& session) {
    permissions_.requireAdmin(cx, session);

    std::unordered_map<Id, std::vector<ModeratorAssignment>> grouped;
    for (const ModeratorAssignment& item : assignments_.listAll(cx)) {
        grouped[item.userId].push_back(item);
    }

    std::vector<UserProfile> profiles;
    for (const UserRecord& user : users_.listWithRole(cx)) {
        UserProfile profile;
        profile.userId = user.userId;
        profile.username = user.username;
        profile.nickname = user.nickname;
        profile.roleCode = user.roleCode;
        profile.status = user.status;
        profile.registeredAt = user.registeredAt;
        auto it = grouped.find(user.userId);
        if (it != grouped.end()) profile.moderatorAssignments = it->second;
        profiles.push_back(std::move(profile));
    }
    return profiles;
}

AccountStatusChange ModerationService::setUserStatus(DbContext& cx,
                                                     const SessionContext& session,
                                                     Id userId, int status) {
    const CurrentUser current = permissions_.requireAdmin(cx, session);

    Transaction transaction(manager_, cx);
    const auto target = users_.findById(cx, userId);
    if (!target) {
        throw errors::notFound("用户不存在");
    }
    if (target->roleCode == "admin") {
        throw errors::forbidden("不能管理管理员账号");
    }

    AccountStatusChange result;
    result.userId = userId;
    result.status = status;
    if (target->status == status) {
        result.changed = false;
        transaction.commit();
        return result;
    }

    const Time now = util::nowUtc();
    users_.setStatus(cx, userId, status);
    ModerationLogDraft log;
    log.actorId = current.userId;
    log.action = status == 1 ? "user_enable" : "user_disable";
    log.targetUserId = userId;
    log.at = now;
    logs_.append(cx, log);
    transaction.commit();

    result.changed = true;
    if (status == 0) auth_.invalidateUserSessions(userId);
    return result;
}

GrantResult ModerationService::grantModerator(DbContext& cx, const SessionContext& session,
                                              Id boardId, Id userId) {
    const CurrentUser current = permissions_.requireAdmin(cx, session);

    Transaction transaction(manager_, cx);
    if (!boards_.findById(cx, boardId)) {
        throw errors::notFound("版块不存在");
    }
    const auto target = users_.findById(cx, userId);
    if (!target) {
        throw errors::notFound("用户不存在");
    }
    if (target->roleCode == "admin") {
        throw errors::forbidden("管理员不需要版主授权");
    }
    if (target->status == 0) {
        throw errors::conflict("账号已停用，无法授予版主");
    }
    if (assignments_.exists(cx, userId, boardId)) {
        throw errors::conflict("该用户已是该版块的版主");
    }

    const Time now = util::nowUtc();
    assignments_.insert(cx, userId, boardId, now);

    ModerationLogDraft log;
    log.actorId = current.userId;
    log.action = "moderator_grant";
    log.targetUserId = userId;
    log.targetBoardId = boardId;
    log.at = now;
    logs_.append(cx, log);
    transaction.commit();

    GrantResult result;
    result.assignment = ModeratorAssignment{userId, boardId, now};
    return result;
}

RevokeResult ModerationService::revokeModerator(DbContext& cx,
                                                const SessionContext& session, Id boardId,
                                                Id userId) {
    const CurrentUser current = permissions_.requireAdmin(cx, session);

    Transaction transaction(manager_, cx);
    if (!boards_.findById(cx, boardId)) {
        throw errors::notFound("版块不存在");
    }
    const auto target = users_.findById(cx, userId);
    if (!target) {
        throw errors::notFound("用户不存在");
    }
    if (target->roleCode == "admin") {
        throw errors::forbidden("管理员不需要版主授权");
    }
    if (!assignments_.exists(cx, userId, boardId)) {
        throw errors::conflict("该用户不是该版块的版主");
    }

    const Time now = util::nowUtc();
    assignments_.remove(cx, userId, boardId);

    ModerationLogDraft log;
    log.actorId = current.userId;
    log.action = "moderator_revoke";
    log.targetUserId = userId;
    log.targetBoardId = boardId;
    log.at = now;
    logs_.append(cx, log);
    transaction.commit();

    RevokeResult result;
    result.userId = userId;
    result.boardId = boardId;
    return result;
}

void ModerationService::removePost(DbContext& cx, const SessionContext& session, Id postId,
                                   const Text& rawReason) {
    const CurrentUser current = permissions_.requireActiveUser(cx, session);
    const Text reason = validation::text(rawReason, "reason", 1, 200);

    Transaction transaction(manager_, cx);
    const auto post = posts_.findVisible(cx, postId);
    if (!post) {
        throw errors::notFound("帖子不存在或已删除");
    }
    if (!permissions_.canModerate(cx, current, post->boardId)) {
        throw errors::forbidden("无权管理该版块内容");
    }

    const Time now = util::nowUtc();
    if (posts_.markDeleted(cx, postId, std::nullopt, now) == 0) {
        throw errors::notFound("帖子不存在或已删除");
    }
    ModerationLogDraft log;
    log.actorId = current.userId;
    log.action = "post_remove";
    log.targetPostId = postId;
    log.reason = reason;
    log.at = now;
    logs_.append(cx, log);
    transaction.commit();
}

void ModerationService::removeReply(DbContext& cx, const SessionContext& session, Id replyId,
                                    const Text& rawReason) {
    const CurrentUser current = permissions_.requireActiveUser(cx, session);
    const Text reason = validation::text(rawReason, "reason", 1, 200);

    Transaction transaction(manager_, cx);
    const auto reply = replies_.findVisible(cx, replyId);
    if (!reply) {
        throw errors::notFound("回复不存在或已删除");
    }
    const auto post = posts_.findVisible(cx, reply->postId);
    if (!post) {
        throw errors::notFound("回复不存在或已删除");
    }
    if (!permissions_.canModerate(cx, current, post->boardId)) {
        throw errors::forbidden("无权管理该版块内容");
    }

    const Time now = util::nowUtc();
    if (replies_.markDeleted(cx, replyId, std::nullopt, now) == 0) {
        throw errors::notFound("回复不存在或已删除");
    }
    ModerationLogDraft log;
    log.actorId = current.userId;
    log.action = "reply_remove";
    log.targetReplyId = replyId;
    log.reason = reason;
    log.at = now;
    logs_.append(cx, log);
    transaction.commit();
}

PostFlagChange ModerationService::setPinned(DbContext& cx, const SessionContext& session,
                                            Id postId, bool value) {
    const CurrentUser current = permissions_.requireActiveUser(cx, session);

    Transaction transaction(manager_, cx);
    const auto post = posts_.findVisible(cx, postId);
    if (!post) {
        throw errors::notFound("帖子不存在或已删除");
    }
    if (!permissions_.canModerate(cx, current, post->boardId)) {
        throw errors::forbidden("无权管理该版块内容");
    }

    PostFlagChange result;
    result.postId = postId;
    result.value = value;

    const Time now = util::nowUtc();
    if (posts_.setPinned(cx, postId, value, now) == 0) {
        result.changed = false;
        result.updatedAt = post->updatedAt;
        transaction.commit();
        return result;
    }

    ModerationLogDraft log;
    log.actorId = current.userId;
    log.action = value ? "post_pin" : "post_unpin";
    log.targetPostId = postId;
    log.at = now;
    logs_.append(cx, log);
    transaction.commit();

    result.changed = true;
    const auto updated = posts_.findVisible(cx, postId);
    result.updatedAt = updated ? updated->updatedAt : now;
    return result;
}

PostFlagChange ModerationService::setFeatured(DbContext& cx, const SessionContext& session,
                                              Id postId, bool value) {
    const CurrentUser current = permissions_.requireActiveUser(cx, session);

    Transaction transaction(manager_, cx);
    const auto post = posts_.findVisible(cx, postId);
    if (!post) {
        throw errors::notFound("帖子不存在或已删除");
    }
    if (!permissions_.canModerate(cx, current, post->boardId)) {
        throw errors::forbidden("无权管理该版块内容");
    }

    PostFlagChange result;
    result.postId = postId;
    result.value = value;

    const Time now = util::nowUtc();
    if (posts_.setFeatured(cx, postId, value, now) == 0) {
        result.changed = false;
        result.updatedAt = post->updatedAt;
        transaction.commit();
        return result;
    }

    ModerationLogDraft log;
    log.actorId = current.userId;
    log.action = value ? "post_feature" : "post_unfeature";
    log.targetPostId = postId;
    log.at = now;
    logs_.append(cx, log);
    transaction.commit();

    result.changed = true;
    const auto updated = posts_.findVisible(cx, postId);
    result.updatedAt = updated ? updated->updatedAt : now;
    return result;
}

} // namespace bbs

