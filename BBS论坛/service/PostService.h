// 帖子服务：发布、详情、本人编辑删除、组合搜索
#pragma once

#include <vector>

#include "../common/Types.h"
#include "../dao/BoardRepository.h"
#include "../dao/DatabaseManager.h"
#include "../dao/Entities.h"
#include "../dao/PostRepository.h"
#include "../dao/ReplyRepository.h"
#include "PermissionService.h"

namespace bbs {

class PostService {
public:
    PostService(DatabaseManager& manager, BoardRepository& boards, PostRepository& posts,
                ReplyRepository& replies, PermissionService& permissions)
        : manager_(manager), boards_(boards), posts_(posts), replies_(replies),
          permissions_(permissions) {}

    PostDetail createPost(DbContext& cx, const SessionContext& session, Id boardId,
                          const Text& title, const Text& content);
    PostDetail getDetail(DbContext& cx, const SessionContext& session, Id postId);
    PostDetail editOwnPost(DbContext& cx, const SessionContext& session, Id postId,
                           const Text& title, const Text& content);
    void deleteOwnPost(DbContext& cx, const SessionContext& session, Id postId);

    std::vector<PostSummary> searchPosts(DbContext& cx, const SessionContext& session,
                                         const SearchFilter& filter);

private:
    PostDetail buildDetail(DbContext& cx, Id postId);

    DatabaseManager& manager_;
    BoardRepository& boards_;
    PostRepository& posts_;
    ReplyRepository& replies_;
    PermissionService& permissions_;
};

} // namespace bbs
