#include "ReplyService.h"

#include "../common/Error.h"
#include "../common/Util.h"
#include "../dao/Projections.h"
#include "Validation.h"

namespace bbs {

ReplyView ReplyService::loadReplyView(DbContext& cx, Id replyId) {
    const auto record = replies_.findVisible(cx, replyId);
    if (!record) {
        throw errors::notFound("回复不存在或已删除");
    }
    return projection::reply(*record);
}

std::vector<ReplyView> ReplyService::listReplies(DbContext& cx, const SessionContext& session,
                                                 Id postId) {
    permissions_.requireActiveUser(cx, session);
    if (!posts_.findVisible(cx, postId)) {
        throw errors::notFound("帖子不存在或已删除");
    }
    std::vector<ReplyView> views;
    for (const ReplyRecord& record : replies_.listVisible(cx, postId)) {
        views.push_back(projection::reply(record));
    }
    return views;
}

ReplyView ReplyService::addReply(DbContext& cx, const SessionContext& session, Id postId,
                                 const Text& rawContent) {
    const CurrentUser current = permissions_.requireActiveUser(cx, session);
    const Text content = validation::text(rawContent, "content", 1, 1000);

    Transaction transaction(manager_, cx);
    const auto post = posts_.findVisible(cx, postId);
    if (!post) {
        throw errors::notFound("帖子不存在或已删除");
    }
    const auto board = boards_.findById(cx, post->boardId);
    if (!board || board->status == 0) {
        throw errors::conflict("版块已停用");
    }

    ReplyInsert data;
    data.postId = postId;
    data.authorId = current.userId;
    data.content = content;
    data.at = util::nowUtc();
    const Id replyId = replies_.insert(cx, data);
    transaction.commit();

    return loadReplyView(cx, replyId);
}

ReplyView ReplyService::editOwnReply(DbContext& cx, const SessionContext& session, Id replyId,
                                     const Text& rawContent) {
    const CurrentUser current = permissions_.requireActiveUser(cx, session);
    const Text content = validation::text(rawContent, "content", 1, 1000);

    Transaction transaction(manager_, cx);
    const auto record = replies_.findVisible(cx, replyId);
    if (!record) {
        throw errors::notFound("回复不存在或已删除");
    }
    permissions_.requireOwner(current.userId, record->authorId);
    if (replies_.updateOwn(cx, replyId, current.userId, content, util::nowUtc()) == 0) {
        throw errors::notFound("回复不存在或已删除");
    }
    transaction.commit();

    return loadReplyView(cx, replyId);
}

void ReplyService::deleteOwnReply(DbContext& cx, const SessionContext& session, Id replyId) {
    const CurrentUser current = permissions_.requireActiveUser(cx, session);

    Transaction transaction(manager_, cx);
    const auto record = replies_.findVisible(cx, replyId);
    if (!record) {
        throw errors::notFound("回复不存在或已删除");
    }
    permissions_.requireOwner(current.userId, record->authorId);
    if (replies_.markDeleted(cx, replyId, current.userId, util::nowUtc()) == 0) {
        throw errors::notFound("回复不存在或已删除");
    }
    transaction.commit();
}

} // namespace bbs
