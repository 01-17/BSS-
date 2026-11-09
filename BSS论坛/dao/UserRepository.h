// 用户与角色数据访问
#pragma once

#include <optional>
#include <vector>

#include "DatabaseManager.h"
#include "Entities.h"

namespace bbs {

class UserRepository {
public:
    std::optional<UserRecord> findById(DbContext& cx, Id userId);
    std::optional<UserRecord> findByUsername(DbContext& cx, const Text& username);
    bool existsUsername(DbContext& cx, const Text& username);

    // 注册：role_code 固定 member，返回新 user_id
    Id insert(DbContext& cx, const Text& username, const Text& nickname,
              const Credential& credential, const Time& at);

    RowCount updateNickname(DbContext& cx, Id userId, const Text& nickname);
    // 条件更新：会话版本未变化时才替换凭据，并把版本 +1
    RowCount updateCredential(DbContext& cx, Id userId, std::int64_t expectedVersion,
                              const Credential& replacement);
    // 条件更新：状态不同才写入；停用时递增会话版本
    RowCount setStatus(DbContext& cx, Id userId, int status);

    std::vector<UserRecord> listWithRole(DbContext& cx);
};

} // namespace bbs

