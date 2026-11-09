#include "ModeratorAssignmentRepository.h"

namespace bbs {

bool ModeratorAssignmentRepository::exists(DbContext& cx, Id userId, Id boardId) {
    Statement stmt = cx.prepare(
        "SELECT 1 FROM board_moderators WHERE user_id = ? AND board_id = ? LIMIT 1");
    stmt.bind(1, userId).bind(2, boardId);
    return stmt.step();
}

RowCount ModeratorAssignmentRepository::insert(DbContext& cx, Id userId, Id boardId,
                                               const Time& at) {
    Statement stmt = cx.prepare(
        "INSERT INTO board_moderators(user_id, board_id, assigned_at) VALUES (?, ?, ?)");
    stmt.bind(1, userId).bind(2, boardId).bind(3, at);
    stmt.exec();
    return stmt.changes();
}

RowCount ModeratorAssignmentRepository::remove(DbContext& cx, Id userId, Id boardId) {
    Statement stmt = cx.prepare(
        "DELETE FROM board_moderators WHERE user_id = ? AND board_id = ?");
    stmt.bind(1, userId).bind(2, boardId);
    stmt.exec();
    return stmt.changes();
}

std::vector<ModeratorAssignment> ModeratorAssignmentRepository::listByUser(DbContext& cx,
                                                                          Id userId) {
    Statement stmt = cx.prepare(
        "SELECT user_id, board_id, assigned_at FROM board_moderators "
        "WHERE user_id = ? ORDER BY board_id ASC");
    stmt.bind(1, userId);
    std::vector<ModeratorAssignment> items;
    while (stmt.step()) {
        ModeratorAssignment item;
        item.userId = stmt.id(0);
        item.boardId = stmt.id(1);
        item.assignedAt = stmt.text(2);
        items.push_back(std::move(item));
    }
    return items;
}

std::vector<ModeratorAssignment> ModeratorAssignmentRepository::listAll(DbContext& cx) {
    Statement stmt = cx.prepare(
        "SELECT user_id, board_id, assigned_at FROM board_moderators "
        "ORDER BY user_id ASC, board_id ASC");
    std::vector<ModeratorAssignment> items;
    while (stmt.step()) {
        ModeratorAssignment item;
        item.userId = stmt.id(0);
        item.boardId = stmt.id(1);
        item.assignedAt = stmt.text(2);
        items.push_back(std::move(item));
    }
    return items;
}

} // namespace bbs
