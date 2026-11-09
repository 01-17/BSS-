#include "UserRepository.h"

namespace bbs {

namespace {

UserRecord readUser(Statement& stmt) {
    UserRecord user;
    user.userId = stmt.id(0);
    user.roleId = stmt.id(1);
    user.roleCode = stmt.text(2);
    user.username = stmt.text(3);
    user.nickname = stmt.text(4);
    user.passwordHash = stmt.text(5);
    user.passwordSalt = stmt.text(6);
    user.passwordIterations = stmt.id(7);
    user.sessionVersion = stmt.id(8);
    user.status = stmt.integer(9);
    user.registeredAt = stmt.text(10);
    return user;
}

const char* kUserColumns =
    "u.user_id, u.role_id, r.role_code, u.username, u.nickname, "
    "u.password_hash, u.password_salt, u.password_iterations, "
    "u.session_version, u.status, u.registered_at ";

} // namespace

std::optional<UserRecord> UserRepository::findById(DbContext& cx, Id userId) {
    Statement stmt = cx.prepare(std::string("SELECT ") + kUserColumns +
                                "FROM users u JOIN roles r ON r.role_id = u.role_id "
                                "WHERE u.user_id = ?");
    stmt.bind(1, userId);
    if (!stmt.step()) return std::nullopt;
    return readUser(stmt);
}

std::optional<UserRecord> UserRepository::findByUsername(DbContext& cx, const Text& username) {
    Statement stmt = cx.prepare(std::string("SELECT ") + kUserColumns +
                                "FROM users u JOIN roles r ON r.role_id = u.role_id "
                                "WHERE u.username = ?");
    stmt.bind(1, username);
    if (!stmt.step()) return std::nullopt;
    return readUser(stmt);
}

bool UserRepository::existsUsername(DbContext& cx, const Text& username) {
    Statement stmt = cx.prepare("SELECT 1 FROM users WHERE username = ? LIMIT 1");
    stmt.bind(1, username);
    return stmt.step();
}

Id UserRepository::insert(DbContext& cx, const Text& username, const Text& nickname,
                          const Credential& credential, const Time& at) {
    Statement stmt = cx.prepare(
        "INSERT INTO users(role_id, username, nickname, password_hash, password_salt, "
        "password_iterations, session_version, status, registered_at) "
        "VALUES ((SELECT role_id FROM roles WHERE role_code = 'member'), ?, ?, ?, ?, ?, 0, 1, ?)");
    stmt.bind(1, username).bind(2, nickname).bind(3, credential.hash)
        .bind(4, credential.salt).bind(5, credential.iterations).bind(6, at);
    stmt.exec();
    return stmt.lastInsertId();
}

RowCount UserRepository::updateNickname(DbContext& cx, Id userId, const Text& nickname) {
    Statement stmt = cx.prepare("UPDATE users SET nickname = ? WHERE user_id = ?");
    stmt.bind(1, nickname).bind(2, userId);
    stmt.exec();
    return stmt.changes();
}

RowCount UserRepository::updateCredential(DbContext& cx, Id userId,
                                          std::int64_t expectedVersion,
                                          const Credential& replacement) {
    Statement stmt = cx.prepare(
        "UPDATE users SET password_hash = ?, password_salt = ?, password_iterations = ?, "
        "session_version = session_version + 1 "
        "WHERE user_id = ? AND session_version = ?");
    stmt.bind(1, replacement.hash).bind(2, replacement.salt)
        .bind(3, replacement.iterations).bind(4, userId).bind(5, expectedVersion);
    stmt.exec();
    return stmt.changes();
}

RowCount UserRepository::setStatus(DbContext& cx, Id userId, int status) {
    Statement stmt = cx.prepare(
        "UPDATE users SET status = ?, "
        "session_version = CASE WHEN ? = 0 THEN session_version + 1 ELSE session_version END "
        "WHERE user_id = ? AND status <> ?");
    stmt.bind(1, status).bind(2, status).bind(3, userId).bind(4, status);
    stmt.exec();
    return stmt.changes();
}

std::vector<UserRecord> UserRepository::listWithRole(DbContext& cx) {
    Statement stmt = cx.prepare(std::string("SELECT ") + kUserColumns +
                                "FROM users u JOIN roles r ON r.role_id = u.role_id "
                                "ORDER BY u.user_id ASC");
    std::vector<UserRecord> users;
    while (stmt.step()) users.push_back(readUser(stmt));
    return users;
}

} // namespace bbs

