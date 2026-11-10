#pragma once

// JSON -> DTO 映射。解析失败时返回 false 并给出中文缺失字段说明，
// 调用方按「通信数据异常」处理（不猜测字段、不静默兜底默认值）。

#include "model/Dtos.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

namespace bbs {
namespace json {

// Qt 5 的 QJsonValue 以 double 承载所有数字，超过 2^53 的 64 位 ID 会丢精度。
// 该函数把 JSON 文本中「值位置」的纯整数 token 改写为等值字符串（字符串内部与键名不动），
// 之后 readInt64 / readInt 可直接从字符串还原精确整数值。仅在解析响应时使用。
QByteArray protectIntegerTokens(const QByteArray& rawJson);

// 基础读取（含类型强校验）
bool readInt64(const QJsonValue& value, qint64* out, const QString& field, QString* error);
bool readInt(const QJsonValue& value, int* out, const QString& field, QString* error);
bool readString(const QJsonValue& value, QString* out, const QString& field, QString* error);
bool readBool(const QJsonValue& value, bool* out, const QString& field, QString* error);
bool readObject(const QJsonValue& value, QJsonObject* out, const QString& field, QString* error);
bool readArray(const QJsonValue& value, QJsonArray* out, const QString& field, QString* error);

// 对象级映射
bool parseAuthor(const QJsonValue& value, AuthorView* out, QString* error);
bool parseRegisteredUser(const QJsonObject& object, RegisteredUser* out, QString* error);
bool parseAssignment(const QJsonValue& value, ModeratorAssignment* out, QString* error);
bool parseUserProfile(const QJsonObject& object, UserProfile* out, QString* error);
bool parseLoginResult(const QJsonObject& object, LoginResult* out, QString* error);
bool parseBoard(const QJsonValue& value, BoardView* out, QString* error);
bool parsePostSummary(const QJsonValue& value, PostSummary* out, QString* error);
bool parseReply(const QJsonValue& value, ReplyView* out, QString* error);
bool parsePostDetail(const QJsonObject& object, PostDetail* out, QString* error);

// data 对象内的列表映射（data.items）
bool parseBoardList(const QJsonObject& data, QVector<BoardView>* out, QString* error);
bool parsePostList(const QJsonObject& data, QVector<PostSummary>* out, QString* error);
bool parseReplyList(const QJsonObject& data, QVector<ReplyView>* out, QString* error);
bool parseUserList(const QJsonObject& data, QVector<UserProfile>* out, QString* error);

// 写操作返回体
bool parseBoardMutation(const QJsonObject& data, BoardMutationResult* out, QString* error);
bool parseAccountStatus(const QJsonObject& data, AccountStatusResult* out, QString* error);
bool parseGrant(const QJsonObject& data, GrantResult* out, QString* error);
bool parseRevoke(const QJsonObject& data, RevokeResult* out, QString* error);
bool parsePostStatus(const QJsonObject& data, PostStatusResult* out, QString* error);
bool parseReplyStatus(const QJsonObject& data, ReplyStatusResult* out, QString* error);
bool parsePostFlag(const QJsonObject& data, const QString& flagField, PostFlagResult* out,
                   QString* error);

} // namespace json
} // namespace bbs
