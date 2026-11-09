// 管理与版主服务：版块管理、账号管理、授权、内容移除与标记
#pragma once

#include <vector>

#include "../common/Types.h"
#include "../dao/BoardRepository.h"
#include "../dao/DatabaseManager.h"
#include "../dao/Entities.h"
#include "../dao/ModerationLogRepository.h"
#include "../dao/ModeratorAssignmentRepository.h"
#include "../dao/PostRepository.h"
#include "../dao/ReplyRepository.h"
#include "../dao/UserRepository.h"
#include "AuthService.h"
#include "PermissionService.h"

namespace bbs {

class ModerationService {
public:
    ModerationService(DatabaseManager& manager, UserRepository& users, BoardRepository& boards,
                      PostRepository& posts, ReplyRepository& replies,
                      ModeratorAssignmentRepository& assignments,
                      ModerationLogRepository& logs, PermissionService& permissions,
                      AuthService& auth);

    // 管理员：版块维护
    BoardChange createBoard(DbContext& cx, const SessionContext& session,
                            const Text& name, const Text& description);
    BoardChange updateBoard(DbContext& cx, const SessionContext& session, Id boardId,
                            const Text& name, const Text& description);
    BoardChange setBoardStatus(DbContext& cx, const SessionContext& session, Id boardId,
                               int status);

    // 管理员：账号与授权
    std::vector<UserProfile> listUsers(DbContext& cx, const SessionContext& session);
    AccountStatusChange setUserStatus(DbContext& cx, const SessionContext& session, Id userId,
                                      int status);
    GrantResult grantModerator(DbContext& cx, const SessionContext& session, Id boardId,
                               Id userId);
    RevokeResult revokeModerator(DbContext& cx, const SessionContext& session, Id boardId,
                                 Id userId);

    // 版主/管理员：内容处置
    void removePost(DbContext& cx, const SessionContext& session, Id postId,
                    const Text& reason);
    void removeReply(DbContext& cx, const SessionContext& session, Id replyId,
                     const Text& reason);
    PostFlagChange setPinned(DbContext& cx, const SessionContext& session, Id postId,
                             bool value);
    PostFlagChange setFeatured(DbContext& cx, const SessionContext& session, Id postId,
                               bool value);

private:
    DatabaseManager& manager_;
    UserRepository& users_;
    BoardRepository& boards_;
    PostRepository& posts_;
    ReplyRepository& replies_;
    ModeratorAssignmentRepository& assignments_;
    ModerationLogRepository& logs_;
    PermissionService& permissions_;
    AuthService& auth_;
};

} // namespace bbs
