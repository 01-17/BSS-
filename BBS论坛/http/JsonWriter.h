// DTO 到 JSON 的序列化（时间统一输出 YYYY-MM-DDTHH:MM:SSZ）
#pragma once

#include "../dao/Entities.h"
#include "JsonUtil.h"

namespace bbs {

class JsonWriter {
public:
    static json assignment(const ModeratorAssignment& item);
    static json profile(const UserProfile& user);
    static json registeredUser(const RegisteredUser& user);
    static json board(const BoardView& board);
    static json postSummary(const PostSummary& post);
    static json reply(const ReplyView& reply);
    static json postDetail(const PostDetail& detail);
};

} // namespace bbs
