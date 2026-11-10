#include "model/JsonMapper.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>

#include <limits>

namespace bbs {
namespace json {

QByteArray protectIntegerTokens(const QByteArray& rawJson)
{
    // 逐字符扫描：跳过字符串字面量（含转义），把「值位置」上的纯十进制整数 token 加引号。
    // 例：{"post_id":9007199254740993,"title":"a:1"} ->
    //     {"post_id":"9007199254740993","title":"a:1"}
    QByteArray out;
    out.reserve(rawJson.size() + 32);

    const int size = rawJson.size();
    bool inString = false;
    bool escaped = false;

    for (int i = 0; i < size; ++i) {
        const char ch = rawJson.at(i);

        if (inString) {
            out.append(ch);
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                inString = false;
            }
            continue;
        }

        if (ch == '"') {
            inString = true;
            out.append(ch);
            continue;
        }

        const bool startsNumber = (ch == '-' || (ch >= '0' && ch <= '9'));
        if (!startsNumber) {
            out.append(ch);
            continue;
        }

        // 判定是否为「无小数、无指数的纯整数」token
        int end = i;
        if (rawJson.at(end) == '-')
            ++end;
        const int digitBegin = end;
        while (end < size && rawJson.at(end) >= '0' && rawJson.at(end) <= '9')
            ++end;
        const bool hasDigits = (end > digitBegin);
        bool plainInteger = hasDigits;
        if (plainInteger && end < size) {
            const char next = rawJson.at(end);
            if (next == '.' || next == 'e' || next == 'E')
                plainInteger = false;
        }

        if (!plainInteger) {
            // 小数 / 指数形式不是 ID，整体原样保留，避免把小数部分（"1.5" 的 "5"）
            // 误判成独立整数 token 而被加引号
            int end2 = i;
            while (end2 < size) {
                const char inner = rawJson.at(end2);
                const bool numberChar = (inner >= '0' && inner <= '9') || inner == '.' || inner == 'e'
                                        || inner == 'E' || inner == '+' || inner == '-';
                if (!numberChar)
                    break;
                ++end2;
            }
            out.append(rawJson.mid(i, end2 - i));
            i = end2 - 1;
            continue;
        }

        out.append('"');
        out.append(rawJson.mid(i, end - i));
        out.append('"');
        i = end - 1;
    }

    return out;
}

namespace {

void setError(QString* error, const QString& message)
{
    if (error)
        *error = message;
}

// 严格解析十进制整数字符串（protectIntegerTokens 改写后的形态），拒绝空串、符号、空白、溢出
bool parseInt64Text(const QString& text, qint64* out)
{
    if (text.isEmpty())
        return false;

    int index = 0;
    const bool negative = (text.at(0) == QLatin1Char('-'));
    if (negative)
        index = 1;
    if (index >= text.size())
        return false;

    const qint64 limit = (std::numeric_limits<qint64>::max)();
    qint64 value = 0;
    for (; index < text.size(); ++index) {
        const QChar ch = text.at(index);
        if (ch < QLatin1Char('0') || ch > QLatin1Char('9'))
            return false;
        const int digit = ch.unicode() - 0x30; // 已验证为 '0'..'9'
        if (value > (limit - digit) / 10)
            return false;
        value = value * 10 + digit;
    }

    if (out)
        *out = negative ? -value : value;
    return true;
}

// 2^53：double 能精确表示的最大连续整数
const double kMaxExactInteger = 9007199254740992.0;

// 统一取值：字符串形态（已经过整数保护）或 double 形态（要求无小数且在 2^53 以内）
bool integerAsInt64(const QJsonValue& value, qint64* out)
{
    if (value.isString())
        return parseInt64Text(value.toString(), out);

    if (!value.isDouble())
        return false;

    const double d = value.toDouble();
    if (d != static_cast<double>(static_cast<qint64>(d)))
        return false;
    if (d > kMaxExactInteger || d < -kMaxExactInteger)
        return false; // 超大整数未经保护，无法保证精度，按类型不符处理

    if (out)
        *out = static_cast<qint64>(d);
    return true;
}

QString missing(const QString& field)
{
    return QStringLiteral("缺少字段或类型不符：%1").arg(field);
}

} // namespace

bool readInt64(const QJsonValue& value, qint64* out, const QString& field, QString* error)
{
    if (!integerAsInt64(value, out)) {
        setError(error, missing(field));
        return false;
    }
    return true;
}

bool readInt(const QJsonValue& value, int* out, const QString& field, QString* error)
{
    qint64 parsed = 0;
    if (!integerAsInt64(value, &parsed)) {
        setError(error, missing(field));
        return false;
    }
    if (parsed < (std::numeric_limits<int>::min)() || parsed > (std::numeric_limits<int>::max)()) {
        setError(error, QStringLiteral("字段超出取值范围：%1").arg(field));
        return false;
    }
    if (out)
        *out = static_cast<int>(parsed);
    return true;
}

bool readString(const QJsonValue& value, QString* out, const QString& field, QString* error)
{
    if (!value.isString()) {
        setError(error, missing(field));
        return false;
    }
    if (out)
        *out = value.toString();
    return true;
}

bool readBool(const QJsonValue& value, bool* out, const QString& field, QString* error)
{
    if (!value.isBool()) {
        setError(error, missing(field));
        return false;
    }
    if (out)
        *out = value.toBool();
    return true;
}

bool readObject(const QJsonValue& value, QJsonObject* out, const QString& field, QString* error)
{
    if (!value.isObject()) {
        setError(error, missing(field));
        return false;
    }
    if (out)
        *out = value.toObject();
    return true;
}

bool readArray(const QJsonValue& value, QJsonArray* out, const QString& field, QString* error)
{
    if (!value.isArray()) {
        setError(error, missing(field));
        return false;
    }
    if (out)
        *out = value.toArray();
    return true;
}

bool parseAuthor(const QJsonValue& value, AuthorView* out, QString* error)
{
    QJsonObject object;
    if (!readObject(value, &object, QStringLiteral("author"), error))
        return false;
    AuthorView author;
    if (!readInt64(object.value(QStringLiteral("user_id")), &author.userId,
                   QStringLiteral("author.user_id"), error))
        return false;
    if (!readString(object.value(QStringLiteral("nickname")), &author.nickname,
                    QStringLiteral("author.nickname"), error))
        return false;
    if (out)
        *out = author;
    return true;
}

bool parseRegisteredUser(const QJsonObject& object, RegisteredUser* out, QString* error)
{
    RegisteredUser user;
    if (!readInt64(object.value(QStringLiteral("user_id")), &user.userId,
                   QStringLiteral("user_id"), error))
        return false;
    if (!readString(object.value(QStringLiteral("username")), &user.username,
                    QStringLiteral("username"), error))
        return false;
    if (!readString(object.value(QStringLiteral("nickname")), &user.nickname,
                    QStringLiteral("nickname"), error))
        return false;
    if (!readString(object.value(QStringLiteral("role_code")), &user.roleCode,
                    QStringLiteral("role_code"), error))
        return false;
    if (out)
        *out = user;
    return true;
}

bool parseAssignment(const QJsonValue& value, ModeratorAssignment* out, QString* error)
{
    QJsonObject object;
    if (!readObject(value, &object, QStringLiteral("assignments[]"), error))
        return false;
    ModeratorAssignment item;
    if (!readInt64(object.value(QStringLiteral("user_id")), &item.userId,
                   QStringLiteral("assignments[].user_id"), error))
        return false;
    if (!readInt64(object.value(QStringLiteral("board_id")), &item.boardId,
                   QStringLiteral("assignments[].board_id"), error))
        return false;
    if (!readString(object.value(QStringLiteral("assigned_at")), &item.assignedAt,
                    QStringLiteral("assignments[].assigned_at"), error))
        return false;
    if (out)
        *out = item;
    return true;
}

bool parseUserProfile(const QJsonObject& object, UserProfile* out, QString* error)
{
    UserProfile user;
    if (!readInt64(object.value(QStringLiteral("user_id")), &user.userId,
                   QStringLiteral("user_id"), error))
        return false;
    if (!readString(object.value(QStringLiteral("username")), &user.username,
                    QStringLiteral("username"), error))
        return false;
    if (!readString(object.value(QStringLiteral("nickname")), &user.nickname,
                    QStringLiteral("nickname"), error))
        return false;
    if (!readString(object.value(QStringLiteral("role_code")), &user.roleCode,
                    QStringLiteral("role_code"), error))
        return false;
    if (!readInt(object.value(QStringLiteral("status")), &user.status,
                 QStringLiteral("status"), error))
        return false;
    if (!readString(object.value(QStringLiteral("registered_at")), &user.registeredAt,
                    QStringLiteral("registered_at"), error))
        return false;

    QJsonArray assignments;
    // 服务端字段名固定为 moderator_assignments（见 JsonWriter::profile）
    if (!readArray(object.value(QStringLiteral("moderator_assignments")), &assignments,
                   QStringLiteral("moderator_assignments"), error))
        return false;
    for (int i = 0; i < assignments.size(); ++i) {
        ModeratorAssignment item;
        if (!parseAssignment(assignments.at(i), &item, error))
            return false;
        user.assignments.append(item);
    }
    if (out)
        *out = user;
    return true;
}

bool parseLoginResult(const QJsonObject& object, LoginResult* out, QString* error)
{
    LoginResult result;
    if (!readString(object.value(QStringLiteral("token")), &result.token,
                    QStringLiteral("token"), error))
        return false;
    if (!readString(object.value(QStringLiteral("expires_at")), &result.expiresAt,
                    QStringLiteral("expires_at"), error))
        return false;
    QJsonObject userObject;
    if (!readObject(object.value(QStringLiteral("user")), &userObject,
                    QStringLiteral("user"), error))
        return false;
    if (!parseUserProfile(userObject, &result.user, error))
        return false;
    if (out)
        *out = result;
    return true;
}

bool parseBoard(const QJsonValue& value, BoardView* out, QString* error)
{
    QJsonObject object;
    if (!readObject(value, &object, QStringLiteral("board"), error))
        return false;
    BoardView board;
    if (!readInt64(object.value(QStringLiteral("board_id")), &board.boardId,
                   QStringLiteral("board_id"), error))
        return false;
    if (!readString(object.value(QStringLiteral("name")), &board.name,
                    QStringLiteral("name"), error))
        return false;
    if (!readString(object.value(QStringLiteral("description")), &board.description,
                    QStringLiteral("description"), error))
        return false;
    if (!readInt(object.value(QStringLiteral("status")), &board.status,
                 QStringLiteral("status"), error))
        return false;
    if (!readString(object.value(QStringLiteral("created_at")), &board.createdAt,
                    QStringLiteral("created_at"), error))
        return false;
    if (!readString(object.value(QStringLiteral("updated_at")), &board.updatedAt,
                    QStringLiteral("updated_at"), error))
        return false;
    if (out)
        *out = board;
    return true;
}

bool parsePostSummary(const QJsonValue& value, PostSummary* out, QString* error)
{
    QJsonObject object;
    if (!readObject(value, &object, QStringLiteral("items[]"), error))
        return false;
    PostSummary post;
    if (!readInt64(object.value(QStringLiteral("post_id")), &post.postId,
                   QStringLiteral("items[].post_id"), error))
        return false;
    if (!readInt64(object.value(QStringLiteral("board_id")), &post.boardId,
                   QStringLiteral("items[].board_id"), error))
        return false;
    if (!parseAuthor(object.value(QStringLiteral("author")), &post.author, error))
        return false;
    if (!readString(object.value(QStringLiteral("title")), &post.title,
                    QStringLiteral("items[].title"), error))
        return false;
    if (!readInt(object.value(QStringLiteral("status")), &post.status,
                 QStringLiteral("items[].status"), error))
        return false;
    if (!readBool(object.value(QStringLiteral("is_pinned")), &post.isPinned,
                  QStringLiteral("items[].is_pinned"), error))
        return false;
    if (!readBool(object.value(QStringLiteral("is_featured")), &post.isFeatured,
                  QStringLiteral("items[].is_featured"), error))
        return false;
    if (!readString(object.value(QStringLiteral("created_at")), &post.createdAt,
                    QStringLiteral("items[].created_at"), error))
        return false;
    if (!readString(object.value(QStringLiteral("updated_at")), &post.updatedAt,
                    QStringLiteral("items[].updated_at"), error))
        return false;
    if (!readInt(object.value(QStringLiteral("reply_count")), &post.replyCount,
                 QStringLiteral("items[].reply_count"), error))
        return false;
    if (out)
        *out = post;
    return true;
}

bool parseReply(const QJsonValue& value, ReplyView* out, QString* error)
{
    QJsonObject object;
    if (!readObject(value, &object, QStringLiteral("items[]"), error))
        return false;
    ReplyView reply;
    if (!readInt64(object.value(QStringLiteral("reply_id")), &reply.replyId,
                   QStringLiteral("items[].reply_id"), error))
        return false;
    if (!readInt64(object.value(QStringLiteral("post_id")), &reply.postId,
                   QStringLiteral("items[].post_id"), error))
        return false;
    if (!parseAuthor(object.value(QStringLiteral("author")), &reply.author, error))
        return false;
    if (!readString(object.value(QStringLiteral("content")), &reply.content,
                    QStringLiteral("items[].content"), error))
        return false;
    if (!readInt(object.value(QStringLiteral("status")), &reply.status,
                 QStringLiteral("items[].status"), error))
        return false;
    if (!readString(object.value(QStringLiteral("created_at")), &reply.createdAt,
                    QStringLiteral("items[].created_at"), error))
        return false;
    if (!readString(object.value(QStringLiteral("updated_at")), &reply.updatedAt,
                    QStringLiteral("items[].updated_at"), error))
        return false;
    if (out)
        *out = reply;
    return true;
}

bool parsePostDetail(const QJsonObject& object, PostDetail* out, QString* error)
{
    PostDetail detail;
    if (!readInt64(object.value(QStringLiteral("post_id")), &detail.postId,
                   QStringLiteral("post_id"), error))
        return false;
    if (!readInt64(object.value(QStringLiteral("board_id")), &detail.boardId,
                   QStringLiteral("board_id"), error))
        return false;
    if (!parseAuthor(object.value(QStringLiteral("author")), &detail.author, error))
        return false;
    if (!readString(object.value(QStringLiteral("title")), &detail.title,
                    QStringLiteral("title"), error))
        return false;
    if (!readInt(object.value(QStringLiteral("status")), &detail.status,
                 QStringLiteral("status"), error))
        return false;
    if (!readBool(object.value(QStringLiteral("is_pinned")), &detail.isPinned,
                  QStringLiteral("is_pinned"), error))
        return false;
    if (!readBool(object.value(QStringLiteral("is_featured")), &detail.isFeatured,
                  QStringLiteral("is_featured"), error))
        return false;
    if (!readString(object.value(QStringLiteral("created_at")), &detail.createdAt,
                    QStringLiteral("created_at"), error))
        return false;
    if (!readString(object.value(QStringLiteral("updated_at")), &detail.updatedAt,
                    QStringLiteral("updated_at"), error))
        return false;
    if (!readInt(object.value(QStringLiteral("reply_count")), &detail.replyCount,
                 QStringLiteral("reply_count"), error))
        return false;
    if (!readString(object.value(QStringLiteral("content")), &detail.content,
                    QStringLiteral("content"), error))
        return false;

    QJsonArray replies;
    if (!readArray(object.value(QStringLiteral("replies")), &replies,
                   QStringLiteral("replies"), error))
        return false;
    for (int i = 0; i < replies.size(); ++i) {
        ReplyView reply;
        if (!parseReply(replies.at(i), &reply, error))
            return false;
        detail.replies.append(reply);
    }
    if (out)
        *out = detail;
    return true;
}

bool parseBoardList(const QJsonObject& data, QVector<BoardView>* out, QString* error)
{
    QJsonArray items;
    if (!readArray(data.value(QStringLiteral("items")), &items, QStringLiteral("items"), error))
        return false;
    QVector<BoardView> boards;
    for (int i = 0; i < items.size(); ++i) {
        BoardView board;
        if (!parseBoard(items.at(i), &board, error))
            return false;
        boards.append(board);
    }
    if (out)
        *out = boards;
    return true;
}

bool parsePostList(const QJsonObject& data, QVector<PostSummary>* out, QString* error)
{
    QJsonArray items;
    if (!readArray(data.value(QStringLiteral("items")), &items, QStringLiteral("items"), error))
        return false;
    QVector<PostSummary> posts;
    for (int i = 0; i < items.size(); ++i) {
        PostSummary post;
        if (!parsePostSummary(items.at(i), &post, error))
            return false;
        posts.append(post);
    }
    if (out)
        *out = posts;
    return true;
}

bool parseReplyList(const QJsonObject& data, QVector<ReplyView>* out, QString* error)
{
    QJsonArray items;
    if (!readArray(data.value(QStringLiteral("items")), &items, QStringLiteral("items"), error))
        return false;
    QVector<ReplyView> replies;
    for (int i = 0; i < items.size(); ++i) {
        ReplyView reply;
        if (!parseReply(items.at(i), &reply, error))
            return false;
        replies.append(reply);
    }
    if (out)
        *out = replies;
    return true;
}

bool parseUserList(const QJsonObject& data, QVector<UserProfile>* out, QString* error)
{
    QJsonArray items;
    if (!readArray(data.value(QStringLiteral("items")), &items, QStringLiteral("items"), error))
        return false;
    QVector<UserProfile> users;
    for (int i = 0; i < items.size(); ++i) {
        QJsonObject object;
        if (!readObject(items.at(i), &object, QStringLiteral("items[]"), error))
            return false;
        UserProfile user;
        if (!parseUserProfile(object, &user, error))
            return false;
        users.append(user);
    }
    if (out)
        *out = users;
    return true;
}

bool parseBoardMutation(const QJsonObject& data, BoardMutationResult* out, QString* error)
{
    BoardMutationResult result;
    if (!readBool(data.value(QStringLiteral("changed")), &result.changed,
                  QStringLiteral("changed"), error))
        return false;
    // 服务端在 create/update/status 三条路径都返回 board 对象（changed=false 时也返回当前值）
    if (data.value(QStringLiteral("board")).isObject()) {
        if (!parseBoard(data.value(QStringLiteral("board")), &result.board, error))
            return false;
    }
    if (out)
        *out = result;
    return true;
}

bool parseAccountStatus(const QJsonObject& data, AccountStatusResult* out, QString* error)
{
    AccountStatusResult result;
    if (!readBool(data.value(QStringLiteral("changed")), &result.changed,
                  QStringLiteral("changed"), error))
        return false;
    if (!readInt64(data.value(QStringLiteral("user_id")), &result.userId,
                   QStringLiteral("user_id"), error))
        return false;
    if (!readInt(data.value(QStringLiteral("status")), &result.status,
                 QStringLiteral("status"), error))
        return false;
    if (out)
        *out = result;
    return true;
}

bool parseGrant(const QJsonObject& data, GrantResult* out, QString* error)
{
    GrantResult result;
    if (!readBool(data.value(QStringLiteral("assigned")), &result.assigned,
                  QStringLiteral("assigned"), error))
        return false;
    if (!parseAssignment(data.value(QStringLiteral("assignment")), &result.assignment, error))
        return false;
    if (out)
        *out = result;
    return true;
}

bool parseRevoke(const QJsonObject& data, RevokeResult* out, QString* error)
{
    RevokeResult result;
    if (!readBool(data.value(QStringLiteral("assigned")), &result.assigned,
                  QStringLiteral("assigned"), error))
        return false;
    if (!readInt64(data.value(QStringLiteral("user_id")), &result.userId,
                   QStringLiteral("user_id"), error))
        return false;
    if (!readInt64(data.value(QStringLiteral("board_id")), &result.boardId,
                   QStringLiteral("board_id"), error))
        return false;
    if (out)
        *out = result;
    return true;
}

bool parsePostStatus(const QJsonObject& data, PostStatusResult* out, QString* error)
{
    PostStatusResult result;
    if (!readInt64(data.value(QStringLiteral("post_id")), &result.postId,
                   QStringLiteral("post_id"), error))
        return false;
    if (!readInt(data.value(QStringLiteral("status")), &result.status,
                 QStringLiteral("status"), error))
        return false;
    if (out)
        *out = result;
    return true;
}

bool parseReplyStatus(const QJsonObject& data, ReplyStatusResult* out, QString* error)
{
    ReplyStatusResult result;
    if (!readInt64(data.value(QStringLiteral("reply_id")), &result.replyId,
                   QStringLiteral("reply_id"), error))
        return false;
    if (!readInt(data.value(QStringLiteral("status")), &result.status,
                 QStringLiteral("status"), error))
        return false;
    if (out)
        *out = result;
    return true;
}

bool parsePostFlag(const QJsonObject& data, const QString& flagField, PostFlagResult* out,
                   QString* error)
{
    PostFlagResult result;
    if (!readBool(data.value(QStringLiteral("changed")), &result.changed,
                  QStringLiteral("changed"), error))
        return false;
    if (!readInt64(data.value(QStringLiteral("post_id")), &result.postId,
                   QStringLiteral("post_id"), error))
        return false;
    if (!readBool(data.value(flagField), &result.flag, flagField, error))
        return false;
    if (!readString(data.value(QStringLiteral("updated_at")), &result.updatedAt,
                    QStringLiteral("updated_at"), error))
        return false;
    if (out)
        *out = result;
    return true;
}

} // namespace json
} // namespace bbs
