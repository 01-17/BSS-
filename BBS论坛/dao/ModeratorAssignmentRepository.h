// 版主授权关系数据访问
#pragma once

#include <vector>

#include "DatabaseManager.h"
#include "Entities.h"

namespace bbs {

class ModeratorAssignmentRepository {
public:
    bool exists(DbContext& cx, Id userId, Id boardId);
    RowCount insert(DbContext& cx, Id userId, Id boardId, const Time& at);
    RowCount remove(DbContext& cx, Id userId, Id boardId);

    std::vector<ModeratorAssignment> listByUser(DbContext& cx, Id userId);
    std::vector<ModeratorAssignment> listAll(DbContext& cx);
};

} // namespace bbs
