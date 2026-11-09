#include "net/ApiClient.h"

#include "model/JsonMapper.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QWidget> // QPointer<QWidget> 需要完整类型

namespace bbs {

namespace {
constexpr int kTimeoutMs = 5000; // NFR-003：每次请求 5 秒超时
constexpr const char* kDefaultBase = "http://127.0.0.1:8080/api/v1";
} // namespace

QString ApiResult::displayMessage() const
{
    if (ok)
        return message.isEmpty() ? QStringLiteral("操作成功") : message;

    switch (kind) {
    case FailureKind::Network:
        return write ? QStringLiteral("无法连接服务器，提交结果未知，请确认服务端状态后刷新核实")
                     : QStringLiteral("无法连接服务器，请确认服务端已启动或网络可用");
    case FailureKind::Timeout:
        // NFR-003：写请求超时的结果未知，必须提示刷新核实，且由用户决定是否重提
        return write ? QStringLiteral("提交超时，请刷新查看结果")
                     : QStringLiteral("请求超时（超过 5 秒），请稍后重试");
    case FailureKind::InvalidResponse:
        return QStringLiteral("通信数据异常，请稍后重试");
    case FailureKind::Business:
    case FailureKind::None:
        break;
    }

    if (!message.isEmpty())
        return message;

    if (code == QLatin1String("INVALID_ARGUMENT"))
        return QStringLiteral("请求参数不符合要求");
    if (code == QLatin1String("UNAUTHENTICATED"))
        return QStringLiteral("会话已失效，请重新登录");
    if (code == QLatin1String("FORBIDDEN"))
        return QStringLiteral("没有权限执行该操作");
    if (code == QLatin1String("RESOURCE_UNAVAILABLE"))
        return QStringLiteral("目标不存在、已删除或不可访问");
    if (code == QLatin1String("DATA_CONFLICT"))
        return QStringLiteral("数据状态冲突，请刷新后重试");
    if (code == QLatin1String("METHOD_NOT_ALLOWED"))
        return QStringLiteral("请求方式不被支持");
    if (code == QLatin1String("INTERNAL_ERROR"))
        return QStringLiteral("服务端处理失败，请稍后重试");
    if (code == QLatin1String("DATABASE_UNAVAILABLE"))
        return QStringLiteral("数据库暂不可用，请稍后重试");

    return QStringLiteral("操作失败（HTTP %1）").arg(httpStatus);
}

ApiClient::ApiClient(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_baseUrl(QString::fromLatin1(kDefaultBase))
{
    // 联调便利：环境变量 BBS_API_BASE 可覆盖默认基址（不带结尾斜杠）
    const QByteArray envBase = qgetenv("BBS_API_BASE");
    if (!envBase.isEmpty())
        setBaseUrl(QString::fromUtf8(envBase));
}

void ApiClient::setBaseUrl(const QString& baseUrl)
{
    QString trimmed = baseUrl.trimmed();
    while (trimmed.endsWith(QLatin1Char('/')))
        trimmed.chop(1);
    if (!trimmed.isEmpty())
        m_baseUrl = trimmed;
}

void ApiClient::setToken(const QString& token)
{
    m_token = token;
}

void ApiClient::clearToken()
{
    m_token.clear();
}

void ApiClient::setActivePage(QWidget* page)
{
    m_activePage = page;
}

quint64 ApiClient::send(const QString& method, const QString& path, const RequestOptions& options,
                        QObject* owner, const Callback& callback)
{
    const quint64 id = m_nextId++;
    const bool isWrite = (method != QLatin1String("GET"));

    // H1 但无令牌：不发请求，直接按未认证处理，避免无意义往返
    if (options.auth && m_token.isEmpty()) {
        ApiResult result;
        result.ok = false;
        result.httpStatus = 401;
        result.code = QStringLiteral("UNAUTHENTICATED");
        result.message = QStringLiteral("会话已失效，请重新登录");
        result.kind = FailureKind::Business;
        result.write = isWrite;
        QTimer::singleShot(0, this, [callback, result]() {
            if (callback)
                callback(result);
        });
        emit sessionExpired();
        return id;
    }

    QUrl url(m_baseUrl + path);
    if (!options.query.isEmpty()) {
        // QUrlQuery 会把空格编码为 '+'，而契约要求空格为 %20、加号为 %2B，
        // 且服务端不把 '+' 还原为空格，因此这里手动按 RFC 3986 百分号编码。
        QStringList pairs;
        pairs.reserve(options.query.size());
        for (int i = 0; i < options.query.size(); ++i) {
            pairs << QString::fromUtf8(QUrl::toPercentEncoding(options.query.at(i).first))
                         + QLatin1Char('=')
                         + QString::fromUtf8(QUrl::toPercentEncoding(options.query.at(i).second));
        }
        url.setQuery(pairs.join(QLatin1Char('&')));
    }

    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/json");
    if (options.auth)
        request.setRawHeader("Authorization", QByteArray("Bearer ") + m_token.toUtf8());
    if (options.hasBody)
        request.setHeader(QNetworkRequest::ContentTypeHeader,
                          QStringLiteral("application/json; charset=utf-8"));

    const QByteArray payload
        = options.hasBody ? QJsonDocument(options.body).toJson(QJsonDocument::Compact) : QByteArray();

    // GET / DELETE / PUT / POST 均走 sendCustomRequest，保证 H0/J/N 组合一致可控
    QNetworkReply* reply = m_nam->sendCustomRequest(request, method.toUtf8(), payload);

    Pending pending;
    pending.owner = owner;
    pending.page = m_activePage;
    pending.callback = callback;
    pending.write = isWrite;

    QTimer* timer = new QTimer(this);
    timer->setSingleShot(true);
    pending.timer = timer;
    m_pending.insert(id, pending);

    connect(timer, &QTimer::timeout, this, [this, id, reply]() {
        // 超时：标记后中断传输，由 finished 统一收口
        m_timedOut.insert(id, true);
        reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, id, reply]() { finish(id, reply); });
    timer->start(kTimeoutMs);

    return id;
}

void ApiClient::finish(quint64 id, QNetworkReply* reply)
{
    auto it = m_pending.find(id);
    if (it == m_pending.end()) {
        reply->deleteLater();
        return;
    }

    Pending pending = it.value();
    m_pending.erase(it);

    if (pending.timer) {
        pending.timer->stop();
        pending.timer->deleteLater();
        pending.timer = nullptr;
    }

    const bool timedOut = m_timedOut.remove(id);
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString transportError = reply->errorString();
    const QByteArray payload = reply->readAll();
    reply->deleteLater();

    ApiResult result;
    result.write = pending.write;
    if (timedOut) {
        result.ok = false;
        result.kind = FailureKind::Timeout;
        result.httpStatus = status;
        result.message = QStringLiteral("请求超时");
    } else if (status <= 0) {
        result.ok = false;
        result.kind = FailureKind::Network;
        result.message = transportError;
    } else {
        parseEnvelope(status, payload, &result);
    }

    // 401：任何接口收到即清空令牌（令牌持有在内存，无持久化残留）
    if (!result.ok && result.httpStatus == 401) {
        m_token.clear();
        emit sessionExpired();
    }

    // 请求上下文绑定：owner 已析构、或所属页面已被切走 → 丢弃迟到响应，
    // 防止过期数据覆盖当前页面；page 为空表示非页面级请求（如登录），不做切页校验。
    const bool ownerGone = pending.owner.isNull();
    const bool late = (!ownerGone && !pending.page.isNull() && m_activePage != pending.page.data());
    if (ownerGone || late)
        return;

    if (pending.callback)
        pending.callback(result);
}

void ApiClient::parseEnvelope(int status, const QByteArray& payload, ApiResult* result) const
{
    result->httpStatus = status;

    QJsonParseError parseError{};
    // 先做整数保护：Qt5 的 QJsonValue 以 double 承载数字，超过 2^53 的 64 位 ID 会丢精度，
    // 因此把整数 token 改写为等值字符串，再交给 DTO 映射按 qint64 精确还原。
    const QByteArray protectedPayload = json::protectIntegerTokens(payload);
    const QJsonDocument document = QJsonDocument::fromJson(protectedPayload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        result->ok = false;
        result->kind = FailureKind::InvalidResponse;
        result->message = QStringLiteral("响应不是合法 JSON 对象");
        return;
    }

    const QJsonObject envelope = document.object();
    const QJsonValue successValue = envelope.value(QStringLiteral("success"));
    const QJsonValue codeValue = envelope.value(QStringLiteral("code"));
    const QJsonValue messageValue = envelope.value(QStringLiteral("message"));
    const QJsonValue dataValue = envelope.value(QStringLiteral("data"));

    if (!successValue.isBool() || !codeValue.isString() || !messageValue.isString()
        || !envelope.contains(QStringLiteral("data"))) {
        result->ok = false;
        result->kind = FailureKind::InvalidResponse;
        result->message = QStringLiteral("响应信封缺少 success/code/message/data 字段");
        return;
    }

    const bool success = successValue.toBool();
    const QString code = codeValue.toString();
    const QString message = messageValue.toString();
    const bool okStatus = (status >= 200 && status < 300);

    if (success != okStatus) {
        result->ok = false;
        result->kind = FailureKind::InvalidResponse;
        result->message = QStringLiteral("响应信封 success 与 HTTP 状态不一致");
        return;
    }
    if (success && code != QLatin1String("OK")) {
        result->ok = false;
        result->kind = FailureKind::InvalidResponse;
        result->message = QStringLiteral("成功响应但业务码非 OK");
        return;
    }
    if (!success && code == QLatin1String("OK")) {
        result->ok = false;
        result->kind = FailureKind::InvalidResponse;
        result->message = QStringLiteral("失败响应带有 OK 业务码");
        return;
    }
    if (success && !dataValue.isObject()) {
        result->ok = false;
        result->kind = FailureKind::InvalidResponse;
        result->message = QStringLiteral("成功响应缺少 data 对象");
        return;
    }
    if (!success && !dataValue.isNull() && !dataValue.isObject()) {
        result->ok = false;
        result->kind = FailureKind::InvalidResponse;
        result->message = QStringLiteral("失败响应 data 必须为 null 或对象");
        return;
    }

    result->ok = success;
    result->code = code;
    result->message = message;
    result->data = dataValue.toObject();
    result->kind = success ? FailureKind::None : FailureKind::Business;
}

// ---------------------------------------------------------------------------
// 认证与账号
// ---------------------------------------------------------------------------

void ApiClient::registerAccount(const QString& username, const QString& password, QObject* owner,
                                const Callback& callback)
{
    RequestOptions options;
    options.auth = false;
    options.hasBody = true;
    options.body.insert(QStringLiteral("username"), username);
    options.body.insert(QStringLiteral("password"), password);
    send(QStringLiteral("POST"), QStringLiteral("/auth/register"), options, owner, callback);
}

void ApiClient::login(const QString& username, const QString& password, QObject* owner,
                      const Callback& callback)
{
    RequestOptions options;
    options.auth = false;
    options.hasBody = true;
    options.body.insert(QStringLiteral("username"), username);
    options.body.insert(QStringLiteral("password"), password);
    send(QStringLiteral("POST"), QStringLiteral("/auth/login"), options, owner, callback);
}

void ApiClient::logout(QObject* owner, const Callback& callback)
{
    RequestOptions options;
    options.auth = true;
    options.hasBody = false;
    send(QStringLiteral("POST"), QStringLiteral("/auth/logout"), options, owner, callback);
}

// ---------------------------------------------------------------------------
// 个人资料
// ---------------------------------------------------------------------------

void ApiClient::fetchMe(QObject* owner, const Callback& callback)
{
    RequestOptions options;
    send(QStringLiteral("GET"), QStringLiteral("/users/me"), options, owner, callback);
}

void ApiClient::updateNickname(const QString& nickname, QObject* owner, const Callback& callback)
{
    RequestOptions options;
    options.hasBody = true;
    options.body.insert(QStringLiteral("nickname"), nickname);
    // 服务端路由：PUT /api/v1/users/me，请求体精确字段 {nickname}
    send(QStringLiteral("PUT"), QStringLiteral("/users/me"), options, owner, callback);
}

void ApiClient::changePassword(const QString& currentPassword, const QString& newPassword,
                               QObject* owner, const Callback& callback)
{
    RequestOptions options;
    options.hasBody = true;
    options.body.insert(QStringLiteral("current_password"), currentPassword);
    options.body.insert(QStringLiteral("new_password"), newPassword);
    send(QStringLiteral("PUT"), QStringLiteral("/users/me/password"), options, owner, callback);
}

// ---------------------------------------------------------------------------
// 版块浏览
// ---------------------------------------------------------------------------

void ApiClient::listBoards(QObject* owner, const Callback& callback)
{
    RequestOptions options;
    send(QStringLiteral("GET"), QStringLiteral("/boards"), options, owner, callback);
}

void ApiClient::listBoardPosts(qint64 boardId, QObject* owner, const Callback& callback)
{
    RequestOptions options;
    // GET /boards/{board_id}/posts 需要登录（H1 + N），与 listBoards 一致
    send(QStringLiteral("GET"), QStringLiteral("/boards/%1/posts").arg(boardId), options, owner,
         callback);
}

// ---------------------------------------------------------------------------
// 版块管理
// ---------------------------------------------------------------------------

void ApiClient::createBoard(const QString& name, const QString& description, QObject* owner,
                            const Callback& callback)
{
    RequestOptions options;
    options.hasBody = true;
    options.body.insert(QStringLiteral("name"), name);
    options.body.insert(QStringLiteral("description"), description);
    // 服务端路由：POST /api/v1/boards（仅管理员），成功返回 201 {changed, board}
    send(QStringLiteral("POST"), QStringLiteral("/boards"), options, owner, callback);
}

void ApiClient::updateBoard(qint64 boardId, const QString& name, const QString& description,
                            QObject* owner, const Callback& callback)
{
    RequestOptions options;
    options.hasBody = true;
    options.body.insert(QStringLiteral("name"), name);
    options.body.insert(QStringLiteral("description"), description);
    // PUT /api/v1/boards/{board_id}，请求体精确字段 {name, description}
    send(QStringLiteral("PUT"), QStringLiteral("/boards/%1").arg(boardId), options, owner,
         callback);
}

void ApiClient::setBoardStatus(qint64 boardId, int status, QObject* owner, const Callback& callback)
{
    RequestOptions options;
    options.hasBody = true;
    options.body.insert(QStringLiteral("status"), status);
    // PUT /api/v1/boards/{board_id}/status，请求体精确字段 {status}
    send(QStringLiteral("PUT"), QStringLiteral("/boards/%1/status").arg(boardId), options,
         owner, callback);
}

// ---------------------------------------------------------------------------
// 帖子
// ---------------------------------------------------------------------------

void ApiClient::createPost(qint64 boardId, const QString& title, const QString& content,
                           QObject* owner, const Callback& callback)
{
    RequestOptions options;
    options.hasBody = true;
    options.body.insert(QStringLiteral("title"), title);
    options.body.insert(QStringLiteral("content"), content);
    send(QStringLiteral("POST"), QStringLiteral("/boards/%1/posts").arg(boardId), options, owner,
         callback);
}

void ApiClient::fetchPost(qint64 postId, QObject* owner, const Callback& callback)
{
    RequestOptions options;
    send(QStringLiteral("GET"), QStringLiteral("/posts/%1").arg(postId), options, owner, callback);
}

void ApiClient::updatePost(qint64 postId, const QString& title, const QString& content,
                           QObject* owner, const Callback& callback)
{
    RequestOptions options;
    options.hasBody = true;
    options.body.insert(QStringLiteral("title"), title);
    options.body.insert(QStringLiteral("content"), content);
    send(QStringLiteral("PUT"), QStringLiteral("/posts/%1").arg(postId), options, owner, callback);
}

void ApiClient::deletePost(qint64 postId, QObject* owner, const Callback& callback)
{
    RequestOptions options;
    options.hasBody = false;
    send(QStringLiteral("DELETE"), QStringLiteral("/posts/%1").arg(postId), options, owner,
         callback);
}

// ---------------------------------------------------------------------------
// 回复
// ---------------------------------------------------------------------------

void ApiClient::listReplies(qint64 postId, QObject* owner, const Callback& callback)
{
    RequestOptions options;
    options.auth = true;
    send(QStringLiteral("GET"), QStringLiteral("/posts/%1/replies").arg(postId), options, owner,
         callback);
}

void ApiClient::createReply(qint64 postId, const QString& content, QObject* owner,
                            const Callback& callback)
{
    RequestOptions options;
    options.hasBody = true;
    options.body.insert(QStringLiteral("content"), content);
    send(QStringLiteral("POST"), QStringLiteral("/posts/%1/replies").arg(postId), options, owner,
         callback);
}

void ApiClient::updateReply(qint64 replyId, const QString& content, QObject* owner,
                            const Callback& callback)
{
    RequestOptions options;
    options.hasBody = true;
    options.body.insert(QStringLiteral("content"), content);
    send(QStringLiteral("PUT"), QStringLiteral("/replies/%1").arg(replyId), options, owner,
         callback);
}

void ApiClient::deleteReply(qint64 replyId, QObject* owner, const Callback& callback)
{
    RequestOptions options;
    options.hasBody = false;
    send(QStringLiteral("DELETE"), QStringLiteral("/replies/%1").arg(replyId), options, owner,
         callback);
}

// ---------------------------------------------------------------------------
// 搜索
// ---------------------------------------------------------------------------

void ApiClient::searchPosts(const QString& keyword, qint64 boardId, const QString& startDate,
                            const QString& endDate, const QString& sort, QObject* owner,
                            const Callback& callback)
{
    RequestOptions options;
    options.auth = true;
    options.query.append(qMakePair(QStringLiteral("keyword"), keyword));
    if (boardId > 0)
        options.query.append(qMakePair(QStringLiteral("board_id"), QString::number(boardId)));
    if (!startDate.isEmpty())
        options.query.append(qMakePair(QStringLiteral("start_date"), startDate));
    if (!endDate.isEmpty())
        options.query.append(qMakePair(QStringLiteral("end_date"), endDate));
    if (!sort.isEmpty())
        options.query.append(qMakePair(QStringLiteral("sort"), sort));
    // 服务端路由：GET /api/v1/posts/search（keyword 可为空串，sort 仅 time|heat）
    send(QStringLiteral("GET"), QStringLiteral("/posts/search"), options, owner, callback);
}

// ---------------------------------------------------------------------------
// 账号管理
// ---------------------------------------------------------------------------

void ApiClient::adminListUsers(QObject* owner, const Callback& callback)
{
    RequestOptions options;
    send(QStringLiteral("GET"), QStringLiteral("/admin/users"), options, owner, callback);
}

void ApiClient::adminSetUserStatus(qint64 userId, int status, QObject* owner,
                                   const Callback& callback)
{
    RequestOptions options;
    options.hasBody = true;
    options.body.insert(QStringLiteral("status"), status);
    // PUT /api/v1/admin/users/{user_id}/status，请求体精确字段只有 {status}
    send(QStringLiteral("PUT"), QStringLiteral("/admin/users/%1/status").arg(userId), options,
         owner, callback);
}

// ---------------------------------------------------------------------------
// 版主授权
// ---------------------------------------------------------------------------

void ApiClient::adminGrantModerator(qint64 boardId, qint64 userId, QObject* owner,
                                    const Callback& callback)
{
    RequestOptions options;
    options.hasBody = false; // N：该接口不接受请求体
    // PUT /api/v1/admin/boards/{board_id}/moderators/{user_id}，成功返回 201
    send(QStringLiteral("PUT"),
         QStringLiteral("/admin/boards/%1/moderators/%2").arg(boardId).arg(userId), options,
         owner, callback);
}

void ApiClient::adminRevokeModerator(qint64 boardId, qint64 userId, QObject* owner,
                                     const Callback& callback)
{
    RequestOptions options;
    options.hasBody = false;
    // DELETE /api/v1/admin/boards/{board_id}/moderators/{user_id}
    send(QStringLiteral("DELETE"),
         QStringLiteral("/admin/boards/%1/moderators/%2").arg(boardId).arg(userId), options,
         owner, callback);
}

// ---------------------------------------------------------------------------
// 内容处置
// ---------------------------------------------------------------------------

void ApiClient::moderateRemovePost(qint64 postId, const QString& reason, QObject* owner,
                                   const Callback& callback)
{
    RequestOptions options;
    options.hasBody = true;
    options.body.insert(QStringLiteral("reason"), reason);
    // POST /api/v1/moderation/posts/{post_id}/removal，请求体 {reason}
    send(QStringLiteral("POST"), QStringLiteral("/moderation/posts/%1/removal").arg(postId),
         options, owner, callback);
}

void ApiClient::moderateRemoveReply(qint64 replyId, const QString& reason, QObject* owner,
                                    const Callback& callback)
{
    RequestOptions options;
    options.hasBody = true;
    options.body.insert(QStringLiteral("reason"), reason);
    // POST /api/v1/moderation/replies/{reply_id}/removal，请求体 {reason}
    send(QStringLiteral("POST"), QStringLiteral("/moderation/replies/%1/removal").arg(replyId),
         options, owner, callback);
}

void ApiClient::moderateSetPinned(qint64 postId, bool pinned, QObject* owner,
                                  const Callback& callback)
{
    RequestOptions options;
    options.hasBody = true;
    options.body.insert(QStringLiteral("is_pinned"), pinned);
    // PUT /api/v1/moderation/posts/{post_id}/pinned，请求体 {is_pinned}
    send(QStringLiteral("PUT"), QStringLiteral("/moderation/posts/%1/pinned").arg(postId),
         options, owner, callback);
}

void ApiClient::moderateSetFeatured(qint64 postId, bool featured, QObject* owner,
                                    const Callback& callback)
{
    RequestOptions options;
    options.hasBody = true;
    options.body.insert(QStringLiteral("is_featured"), featured);
    // PUT /api/v1/moderation/posts/{post_id}/featured，请求体 {is_featured}
    send(QStringLiteral("PUT"), QStringLiteral("/moderation/posts/%1/featured").arg(postId),
         options, owner, callback);
}

} // namespace bbs
