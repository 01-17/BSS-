#include "PermissionService.h"

#include "../common/Error.h"
#include "../common/Util.h"

namespace bbs {

CurrentUser PermissionService::requireActiveUser(DbContext& cx,
                                                 const SessionContext& session) {
    const std::string now = util::nowUtc();
    if (now >= session.expiresAt) {
        throw errors::unauthenticated("登录状态已失效，请重新登录");
    }

    const auto user = users_.findById(cx, session.userId);
    if (!user || user->status == 0) {
        throw errors::unauthenticated("账号已停用");
    }
    if (user->sessionVersion != session.sessionVersion) {
        throw errors::unauthenticated("登录状态已失效，请重新登录");
    }

    CurrentUser current;
    current.userId = user->userId;
    current.roleCode = user->roleCode;
    current.sessionVersion = user->sessionVersion;
    return current;
}

CurrentUser PermissionService::requireAdmin(DbContext& cx, const SessionContext& session) {
    CurrentUser current = requireActiveUser(cx, session);
    if (!current.isAdmin()) {
        throw errors::forbidden("无管理员权限");
    }
    return current;
}

void PermissionService::requireOwner(Id actorId, Id authorId) const {
    if (actorId != authorId) {
        throw errors::forbidden("无权操作他人内容");
    }
}

bool PermissionService::canModerate(DbContext& cx, const CurrentUser& actor, Id boardId) {
    if (actor.isAdmin()) return true;
    return assignments_.exists(cx, actor.userId, boardId);
}

std::vector<ModeratorAssignment> PermissionService::listAssignments(DbContext& cx, Id userId) {
    return assignments_.listByUser(cx, userId);
}

} // namespace bbs
