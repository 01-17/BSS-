// 会话主体核验与权限判定
#pragma once

#include <vector>

#include "../common/Types.h"
#include "../dao/DatabaseManager.h"
#include "../dao/Entities.h"
#include "../dao/ModeratorAssignmentRepository.h"
#include "../dao/UserRepository.h"

namespace bbs {

class PermissionService {
public:
    PermissionService(UserRepository& users, ModeratorAssignmentRepository& assignments)
        : users_(users), assignments_(assignments) {}

    // 复核会话有效期、账号状态与版本；失败 401
    CurrentUser requireActiveUser(DbContext& cx, const SessionContext& session);
    // 在 requireActiveUser 基础上要求基础角色为 admin；失败 403
    CurrentUser requireAdmin(DbContext& cx, const SessionContext& session);
    // 本人校验；失败 403
    void requireOwner(Id actorId, Id authorId) const;
    // 管理员放行；非管理员须存在该版块的当前授权
    bool canModerate(DbContext& cx, const CurrentUser& actor, Id boardId);
    std::vector<ModeratorAssignment> listAssignments(DbContext& cx, Id userId);

private:
    UserRepository& users_;
    ModeratorAssignmentRepository& assignments_;
};

} // namespace bbs
