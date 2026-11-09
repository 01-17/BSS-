// 版块服务：列表与默认帖子列表
#pragma once

#include <vector>

#include "../common/Types.h"
#include "../dao/BoardRepository.h"
#include "../dao/DatabaseManager.h"
#include "../dao/Entities.h"
#include "../dao/PostRepository.h"
#include "PermissionService.h"

namespace bbs {

class BoardService {
public:
    BoardService(BoardRepository& boards, PostRepository& posts,
                 PermissionService& permissions)
        : boards_(boards), posts_(posts), permissions_(permissions) {}

    std::vector<BoardView> listBoards(DbContext& cx, const SessionContext& session);
    std::vector<PostSummary> getBoardPosts(DbContext& cx, const SessionContext& session,
                                           Id boardId);

private:
    BoardRepository& boards_;
    PostRepository& posts_;
    PermissionService& permissions_;
};

} // namespace bbs
