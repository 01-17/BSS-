#include "ReplyRepository.h"

namespace bbs {

namespace {

ReplyRecord readReply(Statement& stmt) {
    ReplyRecord reply;
    reply.replyId = stmt.id(0);
    reply.postId = stmt.id(1);
    reply.authorId = stmt.id(2);
    reply.authorNickname = stmt.text(3);
    reply.content = stmt.text(4);
    reply.status = stmt.integer(5);
    reply.createdAt = stmt.text(6);
    reply.updatedAt = stmt.text(7);
    return reply;
}

const char* kReplyColumns =
    "r.reply_id, r.post_id, r.author_id, u.nickname, r.content, r.status, "
    "r.created_at, r.updated_at ";

} // namespace

std::optional<ReplyRecord> ReplyRepository::findVisible(DbContext& cx, Id replyId) {
    Statement stmt = cx.prepare(
        std::string("SELECT ") + kReplyColumns +
        "FROM replies r JOIN users u ON u.user_id = r.author_id "
        "WHERE r.reply_id = ? AND r.status = 1 "
        "AND EXISTS (SELECT 1 FROM posts p WHERE p.post_id = r.post_id AND p.status = 1)");
    stmt.bind(1, replyId);
    if (!stmt.step()) return std::nullopt;
    return readReply(stmt);
}

std::vector<ReplyRecord> ReplyRepository::listVisible(DbContext& cx, Id postId) {
    Statement stmt = cx.prepare(
        std::string("SELECT ") + kReplyColumns +
        "FROM replies r JOIN users u ON u.user_id = r.author_id "
        "WHERE r.post_id = ? AND r.status = 1 ORDER BY r.created_at ASC, r.reply_id ASC");
    stmt.bind(1, postId);
    std::vector<ReplyRecord> replies;
    while (stmt.step()) replies.push_back(readReply(stmt));
    return replies;
}

Id ReplyRepository::insert(DbContext& cx, const ReplyInsert& data) {
    Statement stmt = cx.prepare(
        "INSERT INTO replies(post_id, author_id, content, status, created_at, updated_at) "
        "VALUES (?, ?, ?, 1, ?, ?)");
    stmt.bind(1, data.postId).bind(2, data.authorId).bind(3, data.content)
        .bind(4, data.at).bind(5, data.at);
    stmt.exec();
    return stmt.lastInsertId();
}

RowCount ReplyRepository::updateOwn(DbContext& cx, Id replyId, Id expectedAuthorId,
                                    const Text& content, const Time& at) {
    Statement stmt = cx.prepare(
        "UPDATE replies SET content = ?, updated_at = MAX(updated_at, ?) "
        "WHERE reply_id = ? AND status = 1 AND author_id = ?");
    stmt.bind(1, content).bind(2, at).bind(3, replyId).bind(4, expectedAuthorId);
    stmt.exec();
    return stmt.changes();
}

RowCount ReplyRepository::markDeleted(DbContext& cx, Id replyId,
                                      std::optional<Id> expectedAuthorId, const Time& at) {
    std::string sql =
        "UPDATE replies SET status = 0, updated_at = MAX(updated_at, ?) "
        "WHERE reply_id = ? AND status = 1";
    if (expectedAuthorId) sql += " AND author_id = ?";
    Statement stmt = cx.prepare(sql);
    stmt.bind(1, at).bind(2, replyId);
    if (expectedAuthorId) stmt.bind(3, *expectedAuthorId);
    stmt.exec();
    return stmt.changes();
}

} // namespace bbs
