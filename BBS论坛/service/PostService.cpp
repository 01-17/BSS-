#include "PostService.h"

#include "../common/Error.h"
#include "../common/Util.h"
#include "../dao/Projections.h"
#include "Validation.h"

namespace bbs {

PostDetail PostService::buildDetail(DbContext& cx, Id postId) {
    const auto record = posts_.findVisible(cx, postId);
    if (!record) {
        throw errors::notFound("帖子不存在或已删除");
    }
    PostDetail detail;
    detail.summary = projection::summary(*record);
    detail.content = record->content;
    for (const ReplyRecord& reply : replies_.listVisible(cx, postId)) {
        detail.replies.push_back(projection::reply(reply));
    }
    detail.summary.replyCount = static_cast<RowCount>(detail.replies.size());
    return detail;
}

PostDetail PostService::createPost(DbContext& cx, const SessionContext& session, Id boardId,
                                   const Text& rawTitle, const Text& rawContent) {
    const CurrentUser current = permissions_.requireActiveUser(cx, session);
    const Text title = validation::text(rawTitle, "title", 1, 100);
    const Text content = validation::text(rawContent, "content", 1, 5000);

    Transaction transaction(manager_, cx);
    const auto board = boards_.findById(cx, boardId);
    if (!board) {
        throw errors::notFound("版块不存在");
    }
    if (board->status == 0) {
        throw errors::conflict("版块已停用");
    }
    PostInsert data;
    data.boardId = boardId;
    data.authorId = current.userId;
    data.title = title;
    data.content = content;
    data.at = util::nowUtc();
    const Id postId = posts_.insert(cx, data);
    transaction.commit();

    return buildDetail(cx, postId);
}

PostDetail PostService::getDetail(DbContext& cx, const SessionContext& session, Id postId) {
    permissions_.requireActiveUser(cx, session);
    return buildDetail(cx, postId);
}

PostDetail PostService::editOwnPost(DbContext& cx, const SessionContext& session, Id postId,
                                    const Text& rawTitle, const Text& rawContent) {
    const CurrentUser current = permissions_.requireActiveUser(cx, session);
    const Text title = validation::text(rawTitle, "title", 1, 100);
    const Text content = validation::text(rawContent, "content", 1, 5000);

    Transaction transaction(manager_, cx);
    const auto record = posts_.findVisible(cx, postId);
    if (!record) {
        throw errors::notFound("帖子不存在或已删除");
    }
    // 普通编辑接口始终只允许作者本人；管理员与版主也不能代替作者修改正文。
    permissions_.requireOwner(current.userId, record->authorId);
    if (posts_.updateOwn(cx, postId, current.userId, title, content, util::nowUtc()) == 0) {
        throw errors::notFound("帖子不存在或已删除");
    }
    transaction.commit();

    return buildDetail(cx, postId);
}

void PostService::deleteOwnPost(DbContext& cx, const SessionContext& session, Id postId) {
    const CurrentUser current = permissions_.requireActiveUser(cx, session);

    Transaction transaction(manager_, cx);
    const auto record = posts_.findVisible(cx, postId);
    if (!record) {
        throw errors::notFound("帖子不存在或已删除");
    }
    permissions_.requireOwner(current.userId, record->authorId);
    if (posts_.markDeleted(cx, postId, current.userId, util::nowUtc()) == 0) {
        throw errors::notFound("帖子不存在或已删除");
    }
    transaction.commit();
}

std::vector<PostSummary> PostService::searchPosts(DbContext& cx,
                                                  const SessionContext& session,
                                                  const SearchFilter& filter) {
    permissions_.requireActiveUser(cx, session);

    PostQuery query;
    query.keyword = validation::keyword(filter.keyword);
    query.sort = filter.sort;

    if (filter.boardId) {
        if (!boards_.findById(cx, *filter.boardId)) {
            throw errors::notFound("版块不存在");
        }
        query.boardId = filter.boardId;
    }

    auto convertDate = [](const std::optional<Text>& date, bool nextDay) -> std::optional<Time> {
        if (!date) return std::nullopt;
        if (!util::validDate(*date)) {
            throw errors::invalidArgument("日期必须为 YYYY-MM-DD 格式的真实公历日期");
        }
        const auto converted = nextDay ? util::localDateNextDayUtc(*date)
                                       : util::localDateStartUtc(*date);
        if (!converted) {
            throw errors::invalidArgument("日期超出可表示范围");
        }
        return converted;
    };

    if (filter.startDate && filter.endDate && *filter.startDate > *filter.endDate) {
        throw errors::invalidArgument("起始日期不得晚于结束日期");
    }
    query.utcStartInclusive = convertDate(filter.startDate, false);
    query.utcEndExclusive = convertDate(filter.endDate, true);

    std::vector<PostSummary> views;
    for (const PostRecord& record : posts_.search(cx, query)) {
        views.push_back(projection::summary(record));
    }
    return views;
}

} // namespace bbs
