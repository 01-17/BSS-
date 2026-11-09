#include "AuthService.h"

#include "../common/Crypto.h"
#include "../common/Error.h"
#include "../common/Util.h"
#include "Validation.h"

namespace bbs {

namespace {
constexpr std::int64_t kSessionSeconds = 8 * 60 * 60; // 令牌有效期 8 小时
} // namespace

Credential AuthService::makeCredential(const Text& password) {
    const std::vector<std::uint8_t> salt = crypto::randomBytes(crypto::kSaltBytes);
    const std::vector<std::uint8_t> key =
        crypto::deriveKey(password, salt, crypto::kPasswordIterations);
    Credential credential;
    credential.hash = crypto::hexEncode(key);
    credential.salt = crypto::hexEncode(salt);
    credential.iterations = crypto::kPasswordIterations;
    return credential;
}

bool AuthService::verifyPassword(const UserRecord& user, const Text& password) {
    std::vector<std::uint8_t> salt;
    if (!crypto::hexDecode(user.passwordSalt, salt)) return false;
    const std::vector<std::uint8_t> key =
        crypto::deriveKey(password, salt, user.passwordIterations);
    return crypto::constantTimeEquals(crypto::hexEncode(key), user.passwordHash);
}

RegisteredUser AuthService::registerUser(DbContext& cx, const Text& rawUsername,
                                         const Text& rawPassword) {
    const Text username = validation::username(rawUsername);
    const Text password = validation::password(rawPassword, "password");
    const Credential credential = makeCredential(password);
    const Time now = util::nowUtc();

    Transaction transaction(manager_, cx);
    if (users_.existsUsername(cx, username)) {
        throw errors::conflict("用户名已存在");
    }
    const Id userId = users_.insert(cx, username, username, credential, now);
    transaction.commit();

    RegisteredUser result;
    result.userId = userId;
    result.username = username;
    result.nickname = username;
    result.roleCode = "member";
    return result;
}

LoginResult AuthService::login(DbContext& cx, const Text& rawUsername,
                               const Text& rawPassword) {
    const Text username = validation::username(rawUsername);
    const Text password = validation::password(rawPassword, "password");

    const auto user = users_.findByUsername(cx, username);
    if (!user || !verifyPassword(*user, password)) {
        throw errors::unauthenticated("账号或密码错误");
    }
    if (user->status == 0) {
        throw errors::unauthenticated("账号已停用");
    }

    SessionContext session;
    session.token = crypto::randomToken();
    session.userId = user->userId;
    session.sessionVersion = user->sessionVersion;
    session.expiresAt = util::utcAfterSeconds(kSessionSeconds);
    sessions_[session.token] = session;

    LoginResult result;
    result.token = session.token;
    result.expiresAt = session.expiresAt;
    result.user = buildProfile(cx, user->userId);
    return result;
}

SessionContext AuthService::requireSession(DbContext& cx, const Text& token) {
    (void)cx;
    if (token.empty()) {
        throw errors::unauthenticated("请先登录");
    }
    auto it = sessions_.find(token);
    if (it == sessions_.end()) {
        throw errors::unauthenticated("登录状态已失效，请重新登录");
    }
    return it->second;
}

void AuthService::logout(DbContext& cx, const SessionContext& session) {
    (void)cx;
    sessions_.erase(session.token);
}

UserProfile AuthService::getProfile(DbContext& cx, const SessionContext& session) {
    const CurrentUser current = permissions_.requireActiveUser(cx, session);
    return buildProfile(cx, current.userId);
}

UserProfile AuthService::updateNickname(DbContext& cx, const SessionContext& session,
                                        const Text& rawNickname) {
    const CurrentUser current = permissions_.requireActiveUser(cx, session);
    const Text nickname = validation::text(rawNickname, "nickname", 1, 30);

    Transaction transaction(manager_, cx);
    users_.updateNickname(cx, current.userId, nickname);
    transaction.commit();

    return buildProfile(cx, current.userId);
}

void AuthService::changePassword(DbContext& cx, const SessionContext& session,
                                 const Text& rawCurrentPassword,
                                 const Text& rawNewPassword) {
    const CurrentUser current = permissions_.requireActiveUser(cx, session);
    const Text currentPassword = validation::password(rawCurrentPassword, "current_password");
    const Text newPassword = validation::password(rawNewPassword, "new_password");

    const auto user = users_.findById(cx, current.userId);
    if (!user) {
        throw errors::unauthenticated("登录状态已失效，请重新登录");
    }
    if (!verifyPassword(*user, currentPassword)) {
        throw errors::invalidArgument("当前密码错误");
    }

    const Credential replacement = makeCredential(newPassword);
    Transaction transaction(manager_, cx);
    const RowCount changed =
        users_.updateCredential(cx, current.userId, current.sessionVersion, replacement);
    if (changed == 0) {
        throw errors::unauthenticated("登录状态已失效，请重新登录");
    }
    transaction.commit();

    invalidateUserSessions(current.userId);
}

void AuthService::invalidateUserSessions(Id userId) noexcept {
    for (auto it = sessions_.begin(); it != sessions_.end();) {
        if (it->second.userId == userId) {
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
}

UserProfile AuthService::buildProfile(DbContext& cx, Id userId) {
    const auto user = users_.findById(cx, userId);
    if (!user) {
        throw errors::unauthenticated("账号不存在");
    }
    UserProfile profile;
    profile.userId = user->userId;
    profile.username = user->username;
    profile.nickname = user->nickname;
    profile.roleCode = user->roleCode;
    profile.status = user->status;
    profile.registeredAt = user->registeredAt;
    profile.moderatorAssignments = assignments_.listByUser(cx, userId);
    return profile;
}

} // namespace bbs
