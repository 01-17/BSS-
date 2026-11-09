#include "PostRepository.h"

namespace bbs {

namespace {

PostRecord readPost(Statement& stmt) {
    PostRecord post;
    post.postId = stmt.id(0);
    post.boardId = stmt.id(1);
    post.authorId = stmt.id(2);
    post.authorNickname = stmt.text(3);
    post.title = stmt.text(4);
    post.content = stmt.text(5);
    post.status = stmt.integer(6);
    post.isPinned = stmt.boolean(7);
    post.isFeatured = stmt.boolean(8);
    post.createdAt = stmt.text(9);
    post.updatedAt = stmt.text(10);
    post.replyCount = stmt.id(11);
    return post;
}

const char* kPostColumns =
    "p.post_id, p.board_id, p.author_id, u.nickname, p.title, p.content, p.status, "
    "p.is_pinned, p.is_featured, p.created_at, p.updated_at, "
    "(SELECT COUNT(*) FROM replies r WHERE r.post_id = p.post_id AND r.status = 1) ";

const char* kPostJoin = "FROM posts p JOIN users u ON u.user_id = p.author_id ";

} // namespace

std::optional<PostRecord> PostRepository::findVisible(DbContext& cx, Id postId) {
    Statement stmt = cx.prepare(std::string("SELECT ") + kPostColumns + kPostJoin +
                                "WHERE p.post_id = ? AND p.status = 1");
    stmt.bind(1, postId);
    if (!stmt.step()) return std::nullopt;
    return readPost(stmt);
}

std::vector<PostRecord> PostRepository::listByBoard(DbContext& cx, Id boardId) {
    Statement stmt = cx.prepare(std::string("SELECT ") + kPostColumns + kPostJoin +
                                "WHERE p.board_id = ? AND p.status = 1 "
                                "ORDER BY p.is_pinned DESC, p.created_at DESC, p.post_id DESC");
    stmt.bind(1, boardId);
    std::vector<PostRecord> posts;
    while (stmt.step()) posts.push_back(readPost(stmt));
    return posts;
}

std::vector<PostRecord> PostRepository::search(DbContext& cx, const PostQuery& query) {
    std::string sql = std::string("SELECT ") + kPostColumns + kPostJoin + "WHERE p.status = 1";
    if (query.boardId) sql += " AND p.board_id = ?";
    if (query.utcStartInclusive) sql += " AND p.created_at >= ?";
    if (query.utcEndExclusive) sql += " AND p.created_at < ?";
    if (!query.keyword.empty()) {
        // instr 为字面量区分大小写子串匹配，% 与 _ 不作通配符
        sql += " AND (instr(p.title, ?) > 0 OR instr(p.content, ?) > 0)";
    }
    if (query.sort == SearchSort::Heat) {
        sql += " ORDER BY 12 DESC, p.created_at DESC, p.post_id DESC";
    } else {
        sql += " ORDER BY p.created_at DESC, p.post_id DESC";
    }

    Statement stmt = cx.prepare(sql);
    int index = 1;
    if (query.boardId) stmt.bind(index++, *query.boardId);
    if (query.utcStartInclusive) stmt.bind(index++, *query.utcStartInclusive);
    if (query.utcEndExclusive) stmt.bind(index++, *query.utcEndExclusive);
    if (!query.keyword.empty()) {
        stmt.bind(index++, query.keyword);
        stmt.bind(index++, query.keyword);
    }

    std::vector<PostRecord> posts;
    while (stmt.step()) posts.push_back(readPost(stmt));
    return posts;
}

Id PostRepository::insert(DbContext& cx, const PostInsert& data) {
    Statement stmt = cx.prepare(
        "INSERT INTO posts(board_id, author_id, title, content, status, is_pinned, "
        "is_featured, created_at, updated_at) VALUES (?, ?, ?, ?, 1, 0, 0, ?, ?)");
    stmt.bind(1, data.boardId).bind(2, data.authorId).bind(3, data.title)
        .bind(4, data.content).bind(5, data.at).bind(6, data.at);
    stmt.exec();
    return stmt.lastInsertId();
}

RowCount PostRepository::updateOwn(DbContext& cx, Id postId,
                                   std::optional<Id> expectedAuthorId,
                                   const Text& title, const Text& content, const Time& at) {
    std::string sql =
        "UPDATE posts SET title = ?, content = ?, updated_at = MAX(updated_at, ?) "
        "WHERE post_id = ? AND status = 1";
    if (expectedAuthorId) sql += " AND author_id = ?";
    Statement stmt = cx.prepare(sql);
    stmt.bind(1, title).bind(2, content).bind(3, at).bind(4, postId);
    if (expectedAuthorId) stmt.bind(5, *expectedAuthorId);
    stmt.exec();
    return stmt.changes();
}

RowCount PostRepository::markDeleted(DbContext& cx, Id postId,
                                     std::optional<Id> expectedAuthorId, const Time& at) {
    std::string sql =
        "UPDATE posts SET status = 0, updated_at = MAX(updated_at, ?) "
        "WHERE post_id = ? AND status = 1";
    if (expectedAuthorId) sql += " AND author_id = ?";
    Statement stmt = cx.prepare(sql);
    stmt.bind(1, at).bind(2, postId);
    if (expectedAuthorId) stmt.bind(3, *expectedAuthorId);
    stmt.exec();
    return stmt.changes();
}

RowCount PostRepository::setPinned(DbContext& cx, Id postId, bool value, const Time& at) {
    Statement stmt = cx.prepare(
        "UPDATE posts SET is_pinned = ?, updated_at = MAX(updated_at, ?) "
        "WHERE post_id = ? AND status = 1 AND is_pinned <> ?");
    stmt.bind(1, value ? 1 : 0).bind(2, at).bind(3, postId).bind(4, value ? 1 : 0);
    stmt.exec();
    return stmt.changes();
}

RowCount PostRepository::setFeatured(DbContext& cx, Id postId, bool value, const Time& at) {
    Statement stmt = cx.prepare(
        "UPDATE posts SET is_featured = ?, updated_at = MAX(updated_at, ?) "
        "WHERE post_id = ? AND status = 1 AND is_featured <> ?");
    stmt.bind(1, value ? 1 : 0).bind(2, at).bind(3, postId).bind(4, value ? 1 : 0);
    stmt.exec();
    return stmt.changes();
}

} // namespace bbs
