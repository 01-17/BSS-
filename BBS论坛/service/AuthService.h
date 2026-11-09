// 认证服务：注册、登录、退出、资料与凭据
#pragma once

#include <unordered_map>

#include "../common/Types.h"
#include "../dao/DatabaseManager.h"
#include "../dao/Entities.h"
#include "../dao/ModeratorAssignmentRepository.h"
#include "../dao/UserRepository.h"
#include "PermissionService.h"

namespace bbs {

class AuthService {
public:
    AuthService(DatabaseManager& manager, UserRepository& users,
                ModeratorAssignmentRepository& assignments, PermissionService& permissions)
        : manager_(manager), users_(users), assignments_(assignments),
          permissions_(permissions) {}

    RegisteredUser registerUser(DbContext& cx, const Text& username,
                                const Text& password);
    LoginResult login(DbContext& cx, const Text& username, const Text& password);

    // 令牌无效返回 401
    SessionContext requireSession(DbContext& cx, const Text& token);
    void logout(DbContext& cx, const SessionContext& session);

    UserProfile getProfile(DbContext& cx, const SessionContext& session);
    UserProfile updateNickname(DbContext& cx, const SessionContext& session,
                               const Text& nickname);
    void changePassword(DbContext& cx, const SessionContext& session,
                        const Text& currentPassword, const Text& newPassword);

    // 清理某账号的全部内存令牌（改密、停用）
    void invalidateUserSessions(Id userId) noexcept;

private:
    UserProfile buildProfile(DbContext& cx, Id userId);
    static bool verifyPassword(const UserRecord& user, const Text& password);
    static Credential makeCredential(const Text& password);

    DatabaseManager& manager_;
    UserRepository& users_;
    ModeratorAssignmentRepository& assignments_;
    PermissionService& permissions_;
    std::unordered_map<Text, SessionContext> sessions_;
};

} // namespace bbs
