#include "BoardRepository.h"

namespace bbs {

namespace {

BoardRecord readBoard(Statement& stmt) {
    BoardRecord board;
    board.boardId = stmt.id(0);
    board.name = stmt.text(1);
    board.description = stmt.text(2);
    board.status = stmt.integer(3);
    board.createdAt = stmt.text(4);
    board.updatedAt = stmt.text(5);
    return board;
}

const char* kBoardColumns =
    "board_id, name, description, status, created_at, updated_at ";

} // namespace

std::optional<BoardRecord> BoardRepository::findById(DbContext& cx, Id boardId) {
    Statement stmt = cx.prepare(std::string("SELECT ") + kBoardColumns +
                                "FROM boards WHERE board_id = ?");
    stmt.bind(1, boardId);
    if (!stmt.step()) return std::nullopt;
    return readBoard(stmt);
}

std::optional<BoardRecord> BoardRepository::findByName(DbContext& cx, const Text& name) {
    Statement stmt = cx.prepare(std::string("SELECT ") + kBoardColumns +
                                "FROM boards WHERE name = ?");
    stmt.bind(1, name);
    if (!stmt.step()) return std::nullopt;
    return readBoard(stmt);
}

std::vector<BoardRecord> BoardRepository::list(DbContext& cx) {
    Statement stmt = cx.prepare(std::string("SELECT ") + kBoardColumns +
                                "FROM boards ORDER BY board_id ASC");
    std::vector<BoardRecord> boards;
    while (stmt.step()) boards.push_back(readBoard(stmt));
    return boards;
}

Id BoardRepository::insert(DbContext& cx, const Text& name, const Text& description,
                           const Time& at) {
    Statement stmt = cx.prepare(
        "INSERT INTO boards(name, description, status, created_at, updated_at) "
        "VALUES (?, ?, 1, ?, ?)");
    stmt.bind(1, name).bind(2, description).bind(3, at).bind(4, at);
    stmt.exec();
    return stmt.lastInsertId();
}

RowCount BoardRepository::updateInfo(DbContext& cx, Id boardId, const Text& name,
                                     const Text& description, const Time& at) {
    Statement stmt = cx.prepare(
        "UPDATE boards SET name = ?, description = ?, updated_at = MAX(updated_at, ?) "
        "WHERE board_id = ?");
    stmt.bind(1, name).bind(2, description).bind(3, at).bind(4, boardId);
    stmt.exec();
    return stmt.changes();
}

RowCount BoardRepository::setStatus(DbContext& cx, Id boardId, int status, const Time& at) {
    Statement stmt = cx.prepare(
        "UPDATE boards SET status = ?, updated_at = MAX(updated_at, ?) "
        "WHERE board_id = ? AND status <> ?");
    stmt.bind(1, status).bind(2, at).bind(3, boardId).bind(4, status);
    stmt.exec();
    return stmt.changes();
}

} // namespace bbs

