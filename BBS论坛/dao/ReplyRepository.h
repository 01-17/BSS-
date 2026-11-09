// 回复数据访问
#pragma once

#include <optional>
#include <vector>

#include "DatabaseManager.h"
#include "Entities.h"

namespace bbs {

class ReplyRepository {
public:
    // 仅返回可见回复，且要求父帖可见
    std::optional<ReplyRecord> findVisible(DbContext& cx, Id replyId);
    std::vector<ReplyRecord> listVisible(DbContext& cx, Id postId);

    Id insert(DbContext& cx, const ReplyInsert& data);
    RowCount updateOwn(DbContext& cx, Id replyId, Id expectedAuthorId,
                       const Text& content, const Time& at);
    RowCount markDeleted(DbContext& cx, Id replyId, std::optional<Id> expectedAuthorId,
                         const Time& at);
};

} // namespace bbs
