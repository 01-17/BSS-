#include "JsonWriter.h"

#include "../common/Util.h"

namespace bbs {

json JsonWriter::assignment(const ModeratorAssignment& item) {
    return json{{"user_id", item.userId},
                {"board_id", item.boardId},
                {"assigned_at", util::toJsonTime(item.assignedAt)}};
}

json JsonWriter::profile(const UserProfile& user) {
    json assignments = json::array();
    for (const ModeratorAssignment& item : user.moderatorAssignments) {
        assignments.push_back(assignment(item));
    }
    return json{{"user_id", user.userId},
                {"username", user.username},
                {"nickname", user.nickname},
                {"role_code", user.roleCode},
                {"status", user.status},
                {"registered_at", util::toJsonTime(user.registeredAt)},
                {"moderator_assignments", assignments}};
}

json JsonWriter::registeredUser(const RegisteredUser& user) {
    return json{{"user_id", user.userId},
                {"username", user.username},
                {"nickname", user.nickname},
                {"role_code", user.roleCode}};
}

json JsonWriter::board(const BoardView& board) {
    return json{{"board_id", board.boardId},
                {"name", board.name},
                {"description", board.description},
                {"status", board.status},
                {"created_at", util::toJsonTime(board.createdAt)},
                {"updated_at", util::toJsonTime(board.updatedAt)}};
}

json JsonWriter::postSummary(const PostSummary& post) {
    return json{{"post_id", post.postId},
                {"board_id", post.boardId},
                {"author", json{{"user_id", post.author.userId},
                                {"nickname", post.author.nickname}}},
                {"title", post.title},
                {"status", post.status},
                {"is_pinned", post.isPinned},
                {"is_featured", post.isFeatured},
                {"created_at", util::toJsonTime(post.createdAt)},
                {"updated_at", util::toJsonTime(post.updatedAt)},
                {"reply_count", post.replyCount}};
}

json JsonWriter::reply(const ReplyView& reply) {
    return json{{"reply_id", reply.replyId},
                {"post_id", reply.postId},
                {"author", json{{"user_id", reply.author.userId},
                                {"nickname", reply.author.nickname}}},
                {"content", reply.content},
                {"status", reply.status},
                {"created_at", util::toJsonTime(reply.createdAt)},
                {"updated_at", util::toJsonTime(reply.updatedAt)}};
}

json JsonWriter::postDetail(const PostDetail& detail) {
    json value = postSummary(detail.summary);
    value["content"] = detail.content;
    json replies = json::array();
    for (const ReplyView& item : detail.replies) {
        replies.push_back(reply(item));
    }
    value["replies"] = replies;
    return value;
}

} // namespace bbs
