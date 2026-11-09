#include "BoardService.h"

#include "../common/Error.h"
#include "../dao/Projections.h"

namespace bbs {

std::vector<BoardView> BoardService::listBoards(DbContext& cx,
                                                const SessionContext& session) {
    permissions_.requireActiveUser(cx, session);
    std::vector<BoardView> views;
    for (const BoardRecord& record : boards_.list(cx)) {
        views.push_back(projection::board(record));
    }
    return views;
}

std::vector<PostSummary> BoardService::getBoardPosts(DbContext& cx,
                                                     const SessionContext& session,
                                                     Id boardId) {
    permissions_.requireActiveUser(cx, session);
    if (!boards_.findById(cx, boardId)) {
        throw errors::notFound("版块不存在");
    }

    std::vector<PostSummary> views;
    for (const PostRecord& record : posts_.listByBoard(cx, boardId)) {
        views.push_back(projection::summary(record));
    }
    return views;
}

} // namespace bbs

