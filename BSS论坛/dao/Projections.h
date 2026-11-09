// 实体到对外投影的转换
#pragma once

#include "Entities.h"

namespace bbs {
namespace projection {

inline AuthorView author(const PostRecord& post) {
    return AuthorView{post.authorId, post.authorNickname};
}

inline AuthorView author(const ReplyRecord& reply) {
    return AuthorView{reply.authorId, reply.authorNickname};
}

inline PostSummary summary(const PostRecord& post) {
    PostSummary view;
    view.postId = post.postId;
    view.boardId = post.boardId;
    view.author = author(post);
    view.title = post.title;
    view.status = post.status;
    view.isPinned = post.isPinned;
    view.isFeatured = post.isFeatured;
    view.createdAt = post.createdAt;
    view.updatedAt = post.updatedAt;
    view.replyCount = post.replyCount;
    return view;
}

inline ReplyView reply(const ReplyRecord& record) {
    ReplyView view;
    view.replyId = record.replyId;
    view.postId = record.postId;
    view.author = author(record);
    view.content = record.content;
    view.status = record.status;
    view.createdAt = record.createdAt;
    view.updatedAt = record.updatedAt;
    return view;
}

inline BoardView board(const BoardRecord& record) {
    BoardView view;
    view.boardId = record.boardId;
    view.name = record.name;
    view.description = record.description;
    view.status = record.status;
    view.createdAt = record.createdAt;
    view.updatedAt = record.updatedAt;
    return view;
}

} // namespace projection
} // namespace bbs
