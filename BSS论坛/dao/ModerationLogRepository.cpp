#include "ModerationLogRepository.h"

namespace bbs {

Id ModerationLogRepository::append(DbContext& cx, const ModerationLogDraft& data) {
    Statement stmt = cx.prepare(
        "INSERT INTO moderation_logs(actor_id, action, target_user_id, target_board_id, "
        "target_post_id, target_reply_id, reason, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    stmt.bind(1, data.actorId).bind(2, data.action);

    if (data.targetUserId) {
        stmt.bind(3, *data.targetUserId);
    } else {
        stmt.bindNull(3);
    }
    if (data.targetBoardId) {
        stmt.bind(4, *data.targetBoardId);
    } else {
        stmt.bindNull(4);
    }
    if (data.targetPostId) {
        stmt.bind(5, *data.targetPostId);
    } else {
        stmt.bindNull(5);
    }
    if (data.targetReplyId) {
        stmt.bind(6, *data.targetReplyId);
    } else {
        stmt.bindNull(6);
    }
    if (data.reason) {
        stmt.bind(7, *data.reason);
    } else {
        stmt.bindNull(7);
    }
    stmt.bind(8, data.at);
    stmt.exec();
    return stmt.lastInsertId();
}

} // namespace bbs
