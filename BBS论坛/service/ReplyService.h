// 回复服务：列表、发布、本人编辑删除
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

class ReplyService {
public:
    ReplyService(DatabaseManager& manager, BoardRepository& boards, PostRepository& posts,
                 ReplyRepository& replies, PermissionService& permissions)
        : manager_(manager), boards_(boards), posts_(posts), replies_(replies),
          permissions_(permissions) {}

    std::vector<ReplyView> listReplies(DbContext& cx, const SessionContext& session,
                                       Id postId);
    ReplyView addReply(DbContext& cx, const SessionContext& session, Id postId,
                       const Text& content);
    ReplyView editOwnReply(DbContext& cx, const SessionContext& session, Id replyId,
                           const Text& content);
    void deleteOwnReply(DbContext& cx, const SessionContext& session, Id replyId);

private:
    ReplyView loadReplyView(DbContext& cx, Id replyId);

    DatabaseManager& manager_;
    BoardRepository& boards_;
    PostRepository& posts_;
    ReplyRepository& replies_;
    PermissionService& permissions_;
};

} // namespace bbs
