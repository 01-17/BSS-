// 管理日志数据访问
#pragma once

#include "DatabaseManager.h"
#include "Entities.h"

namespace bbs {

class ModerationLogRepository {
public:
    Id append(DbContext& cx, const ModerationLogDraft& data);
};

} // namespace bbs
