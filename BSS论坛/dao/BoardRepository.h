// 版块数据访问
#pragma once

#include <optional>
#include <vector>

#include "DatabaseManager.h"
#include "Entities.h"

namespace bbs {

class BoardRepository {
public:
    std::optional<BoardRecord> findById(DbContext& cx, Id boardId);
    std::optional<BoardRecord> findByName(DbContext& cx, const Text& name);
    std::vector<BoardRecord> list(DbContext& cx);

    Id insert(DbContext& cx, const Text& name, const Text& description, const Time& at);
    RowCount updateInfo(DbContext& cx, Id boardId, const Text& name,
                        const Text& description, const Time& at);
    RowCount setStatus(DbContext& cx, Id boardId, int status, const Time& at);
};

} // namespace bbs

