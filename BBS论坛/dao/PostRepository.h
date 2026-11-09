// 帖子数据访问
#pragma once

#include <optional>
#include <vector>

#include "DatabaseManager.h"
#include "Entities.h"

namespace bbs {

class PostRepository {
public:
    // 仅返回 status=1 的帖子（含作者昵称与可见回复数）
    std::optional<PostRecord> findVisible(DbContext& cx, Id postId);
    std::vector<PostRecord> listByBoard(DbContext& cx, Id boardId);
    std::vector<PostRecord> search(DbContext& cx, const PostQuery& query);

    Id insert(DbContext& cx, const PostInsert& data);
    RowCount updateOwn(DbContext& cx, Id postId, std::optional<Id> expectedAuthorId,
                       const Text& title, const Text& content, const Time& at);
    // expectedAuthorId 为空表示管理者移除；仅作用于可见帖子
    RowCount markDeleted(DbContext& cx, Id postId, std::optional<Id> expectedAuthorId,
                         const Time& at);

    RowCount setPinned(DbContext& cx, Id postId, bool value, const Time& at);
    RowCount setFeatured(DbContext& cx, Id postId, bool value, const Time& at);
};

} // namespace bbs
