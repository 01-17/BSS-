#include "ApiRouter.h"

#include <algorithm>
#include <set>

#include "../common/Error.h"
#include "../common/Logger.h"
#include "../common/Util.h"
#include "JsonWriter.h"

namespace bbs {

namespace {

std::vector<std::string> splitPath(const std::string& path) {
    std::vector<std::string> segments;
    std::size_t start = 0;
    while (start < path.size()) {
        if (path[start] == '/') {
            ++start;
            continue;
        }
        std::size_t end = path.find('/', start);
        if (end == std::string::npos) end = path.size();
        std::string raw = path.substr(start, end - start);
        std::string decoded;
        if (!util::percentDecode(raw, decoded)) {
            throw errors::invalidArgument("请求路径包含非法的百分号编码");
        }
        segments.push_back(decoded);
        start = end;
    }
    return segments;
}

bool isPlaceholder(const std::string& segment) {
    return segment.size() >= 2 && segment.front() == '{' && segment.back() == '}';
}

struct MatchResult {
    bool matched = false;
    std::map<std::string, std::string> params;
};

MatchResult matchRoute(const std::vector<std::string>& pattern,
                       const std::vector<std::string>& actual) {
    MatchResult result;
    if (pattern.size() != actual.size()) return result;
    for (std::size_t i = 0; i < pattern.size(); ++i) {
        const std::string& expected = pattern[i];
        if (isPlaceholder(expected)) {
            result.params[expected.substr(1, expected.size() - 2)] = actual[i];
        } else if (expected != actual[i]) {
            return MatchResult{};
        }
    }
    result.matched = true;
    return result;
}

} // namespace

ApiRouter::ApiRouter(DatabaseManager& database, AuthService& auth, BoardService& boards,
                     PostService& posts, ReplyService& replies,
                     ModerationService& moderation, Logger& logger)
    : database_(database), auth_(auth), boards_(boards), posts_(posts), replies_(replies),
      moderation_(moderation), logger_(logger) {
    registerRoutes();
}

void ApiRouter::add(const std::string& method, const std::string& pattern, bool needAuth,
                    Handler handler) {
    Route route;
    route.method = method;
    route.needAuth = needAuth;
    route.handler = std::move(handler);

    std::size_t start = 0;
    while (start < pattern.size()) {
        if (pattern[start] == '/') {
            ++start;
            continue;
        }
        std::size_t end = pattern.find('/', start);
        if (end == std::string::npos) end = pattern.size();
        route.segments.push_back(pattern.substr(start, end - start));
        start = end;
    }
    routes_.push_back(std::move(route));
}

HttpResponse ApiRouter::success(int status, const json& data) {
    HttpResponse response;
    response.status = status;
    json body{{"success", true}, {"code", "OK"}, {"message", "操作成功"}, {"data", data}};
    response.body = body.dump();
    return response;
}

HttpResponse ApiRouter::failure(int status, const std::string& code,
                                const std::string& message) {
    HttpResponse response;
    response.status = status;
    json body{{"success", false}, {"code", code}, {"message", message}, {"data", nullptr}};
    response.body = body.dump();
    return response;
}

ApiRouter::Params ApiRouter::parseQuery(const HttpRequest& request,
                                        std::initializer_list<const char*> allowed) {
    Params params;
    if (request.rawQuery.empty()) return params;

    std::size_t start = 0;
    while (start <= request.rawQuery.size()) {
        std::size_t end = request.rawQuery.find('&', start);
        if (end == std::string::npos) end = request.rawQuery.size();
        const std::string pair = request.rawQuery.substr(start, end - start);
        start = end + 1;
        if (pair.empty()) continue;

        const std::size_t equal = pair.find('=');
        std::string rawKey = equal == std::string::npos ? pair : pair.substr(0, equal);
        std::string rawValue = equal == std::string::npos ? "" : pair.substr(equal + 1);

        std::string key;
        std::string value;
        if (!util::percentDecode(rawKey, key) || !util::percentDecode(rawValue, value)) {
            throw errors::invalidArgument("查询参数包含非法的百分号编码");
        }
        if (key.empty()) {
            throw errors::invalidArgument("查询参数名不能为空");
        }
        bool known = false;
        for (const char* name : allowed) {
            if (key == name) {
                known = true;
                break;
            }
        }
        if (!known) {
            throw errors::invalidArgument("查询参数 " + key + " 不受支持");
        }
        if (params.count(key)) {
            throw errors::invalidArgument("查询参数 " + key + " 重复");
        }
        params.emplace(std::move(key), std::move(value));
    }
    return params;
}

json ApiRouter::requireJsonBody(const HttpRequest& request) {
    const std::string contentType = util::toLowerAscii(request.header("content-type"));
    if (contentType.rfind("application/json", 0) != 0) {
        throw errors::invalidArgument("请求体必须为 application/json");
    }
    return JsonUtil::parseObject(request.body);
}

void ApiRouter::ensureNoBody(const HttpRequest& request) {
    if (!request.body.empty()) {
        throw errors::invalidArgument("该接口不接受请求体");
    }
}

Text ApiRouter::bearerToken(const HttpRequest& request) {
    const std::string authorization = request.header("authorization");
    if (authorization.empty()) {
        throw errors::unauthenticated("请先登录");
    }
    const std::size_t space = authorization.find(' ');
    if (space == std::string::npos) {
        throw errors::unauthenticated("认证信息格式非法");
    }
    if (!util::equalsNoCaseAscii(authorization.substr(0, space), "Bearer")) {
        throw errors::unauthenticated("认证信息格式非法");
    }
    const Text token = util::trimUnicode(authorization.substr(space + 1));
    if (token.size() != 64) {
        throw errors::unauthenticated("认证信息格式非法");
    }
    for (char c : token) {
        const bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        if (!ok) throw errors::unauthenticated("认证信息格式非法");
    }
    return token;
}

void ApiRouter::registerRoutes() {
    // ---- 认证与个人资料 ----
    add("POST", "/api/v1/auth/register", false,
        [this](const HttpRequest& request, const Params&, DbContext& cx,
               const SessionContext&) -> HandlerResult {
            const json body = requireJsonBody(request);
            JsonUtil::requireExactFields(body, {"username", "password"});
            const RegisteredUser user = auth_.registerUser(
                cx, JsonUtil::getString(body, "username"),
                JsonUtil::getString(body, "password"));
            return {201, JsonWriter::registeredUser(user)};
        });

    add("POST", "/api/v1/auth/login", false,
        [this](const HttpRequest& request, const Params&, DbContext& cx,
               const SessionContext&) -> HandlerResult {
            const json body = requireJsonBody(request);
            JsonUtil::requireExactFields(body, {"username", "password"});
            const LoginResult result = auth_.login(
                cx, JsonUtil::getString(body, "username"),
                JsonUtil::getString(body, "password"));
            return {200, json{{"token", result.token},
                              {"expires_at", util::toJsonTime(result.expiresAt)},
                              {"user", JsonWriter::profile(result.user)}}};
        });

    add("POST", "/api/v1/auth/logout", true,
        [this](const HttpRequest& request, const Params&, DbContext& cx,
               const SessionContext& session) -> HandlerResult {
            ensureNoBody(request);
            auth_.logout(cx, session);
            return {200, json::object()};
        });

    add("GET", "/api/v1/users/me", true,
        [this](const HttpRequest& request, const Params&, DbContext& cx,
               const SessionContext& session) -> HandlerResult {
            parseQuery(request, {});
            return {200, JsonWriter::profile(auth_.getProfile(cx, session))};
        });

    add("PUT", "/api/v1/users/me", true,
        [this](const HttpRequest& request, const Params&, DbContext& cx,
               const SessionContext& session) -> HandlerResult {
            const json body = requireJsonBody(request);
            JsonUtil::requireExactFields(body, {"nickname"});
            return {200, JsonWriter::profile(auth_.updateNickname(
                             cx, session, JsonUtil::getString(body, "nickname")))};
        });

    add("PUT", "/api/v1/users/me/password", true,
        [this](const HttpRequest& request, const Params&, DbContext& cx,
               const SessionContext& session) -> HandlerResult {
            const json body = requireJsonBody(request);
            JsonUtil::requireExactFields(body, {"current_password", "new_password"});
            auth_.changePassword(cx, session, JsonUtil::getString(body, "current_password"),
                                 JsonUtil::getString(body, "new_password"));
            return {200, json::object()};
        });

    // ---- 版块 ----
    add("GET", "/api/v1/boards", true,
        [this](const HttpRequest& request, const Params&, DbContext& cx,
               const SessionContext& session) -> HandlerResult {
            parseQuery(request, {});
            json items = json::array();
            for (const BoardView& board : boards_.listBoards(cx, session)) {
                items.push_back(JsonWriter::board(board));
            }
            return {200, json{{"items", items}}};
        });

    add("GET", "/api/v1/boards/{board_id}/posts", true,
        [this](const HttpRequest& request, const Params& params, DbContext& cx,
               const SessionContext& session) -> HandlerResult {
            parseQuery(request, {});
            const Id boardId = JsonUtil::parsePathId(params.at("board_id"), "board_id");
            json items = json::array();
            for (const PostSummary& post : boards_.getBoardPosts(cx, session, boardId)) {
                items.push_back(JsonWriter::postSummary(post));
            }
            return {200, json{{"items", items}}};
        });

    add("POST", "/api/v1/boards", true,
        [this](const HttpRequest& request, const Params&, DbContext& cx,
               const SessionContext& session) -> HandlerResult {
            const json body = requireJsonBody(request);
            JsonUtil::requireFields(body, {"name"}, {"description"});
            const Text description =
                body.contains("description") ? JsonUtil::getString(body, "description") : Text();
            const BoardChange change = moderation_.createBoard(
                cx, session, JsonUtil::getString(body, "name"), description);
            return {201, json{{"changed", change.changed},
                              {"board", JsonWriter::board(change.board)}}};
        });

    add("PUT", "/api/v1/boards/{board_id}", true,
        [this](const HttpRequest& request, const Params& params, DbContext& cx,
               const SessionContext& session) -> HandlerResult {
            const json body = requireJsonBody(request);
            JsonUtil::requireExactFields(body, {"name", "description"});
            const Id boardId = JsonUtil::parsePathId(params.at("board_id"), "board_id");
            const BoardChange change = moderation_.updateBoard(
                cx, session, boardId, JsonUtil::getString(body, "name"),
                JsonUtil::getString(body, "description"));
            return {200, json{{"changed", change.changed},
                              {"board", JsonWriter::board(change.board)}}};
        });

    add("PUT", "/api/v1/boards/{board_id}/status", true,
        [this](const HttpRequest& request, const Params& params, DbContext& cx,
               const SessionContext& session) -> HandlerResult {
            const json body = requireJsonBody(request);
            JsonUtil::requireExactFields(body, {"status"});
            const Id boardId = JsonUtil::parsePathId(params.at("board_id"), "board_id");
            const int status = JsonUtil::getInt(body, "status");
            if (status != 0 && status != 1) {
                throw errors::invalidArgument("字段 status 只能为 0 或 1");
            }
            const BoardChange change =
                moderation_.setBoardStatus(cx, session, boardId, status);
            return {200, json{{"changed", change.changed},
                              {"board", JsonWriter::board(change.board)}}};
        });

    // ---- 帖子 ----
    add("POST", "/api/v1/boards/{board_id}/posts", true,
        [this](const HttpRequest& request, const Params& params, DbContext& cx,
               const SessionContext& session) -> HandlerResult {
            const json body = requireJsonBody(request);
            JsonUtil::requireExactFields(body, {"title", "content"});
            const Id boardId = JsonUtil::parsePathId(params.at("board_id"), "board_id");
            const PostDetail detail = posts_.createPost(
                cx, session, boardId, JsonUtil::getString(body, "title"),
                JsonUtil::getString(body, "content"));
            return {201, JsonWriter::postDetail(detail)};
        });

    add("GET", "/api/v1/posts/search", true,
        [this](const HttpRequest& request, const Params&, DbContext& cx,
               const SessionContext& session) -> HandlerResult {
            const Params query =
                parseQuery(request, {"keyword", "board_id", "start_date", "end_date", "sort"});
            SearchFilter filter;
            auto keyword = query.find("keyword");
            if (keyword != query.end()) filter.keyword = keyword->second;
            auto boardId = query.find("board_id");
            if (boardId != query.end()) {
                filter.boardId = JsonUtil::parsePathId(boardId->second, "board_id");
            }
            auto startDate = query.find("start_date");
            if (startDate != query.end()) filter.startDate = startDate->second;
            auto endDate = query.find("end_date");
            if (endDate != query.end()) filter.endDate = endDate->second;
            auto sort = query.find("sort");
            if (sort != query.end()) {
                if (sort->second == "time") {
                    filter.sort = SearchSort::Time;
                } else if (sort->second == "heat") {
                    filter.sort = SearchSort::Heat;
                } else {
                    throw errors::invalidArgument("查询参数 sort 只能为 time 或 heat");
                }
            }
            json items = json::array();
            for (const PostSummary& post : posts_.searchPosts(cx, session, filter)) {
                items.push_back(JsonWriter::postSummary(post));
            }
            return {200, json{{"items", items}}};
        });

    add("GET", "/api/v1/posts/{post_id}", true,
        [this](const HttpRequest& request, const Params& params, DbContext& cx,
               const SessionContext& session) -> HandlerResult {
            parseQuery(request, {});
            const Id postId = JsonUtil::parsePathId(params.at("post_id"), "post_id");
            return {200, JsonWriter::postDetail(posts_.getDetail(cx, session, postId))};
        });

    add("PUT", "/api/v1/posts/{post_id}", true,
        [this](const HttpRequest& request, const Params& params, DbContext& cx,
               const SessionContext& session) -> HandlerResult {
            const json body = requireJsonBody(request);
            JsonUtil::requireExactFields(body, {"title", "content"});
            const Id postId = JsonUtil::parsePathId(params.at("post_id"), "post_id");
            return {200, JsonWriter::postDetail(posts_.editOwnPost(
                             cx, session, postId, JsonUtil::getString(body, "title"),
                             JsonUtil::getString(body, "content")))};
        });

    add("DELETE", "/api/v1/posts/{post_id}", true,
        [this](const HttpRequest& request, const Params& params, DbContext& cx,
               const SessionContext& session) -> HandlerResult {
            ensureNoBody(request);
            const Id postId = JsonUtil::parsePathId(params.at("post_id"), "post_id");
            posts_.deleteOwnPost(cx, session, postId);
            return {200, json{{"post_id", postId}, {"status", 0}}};
        });

    // ---- 回复 ----
    add("GET", "/api/v1/posts/{post_id}/replies", true,
        [this](const HttpRequest& request, const Params& params, DbContext& cx,
               const SessionContext& session) -> HandlerResult {
            parseQuery(request, {});
            const Id postId = JsonUtil::parsePathId(params.at("post_id"), "post_id");
            json items = json::array();
            for (const ReplyView& reply : replies_.listReplies(cx, session, postId)) {
                items.push_back(JsonWriter::reply(reply));
            }
            return {200, json{{"items", items}}};
        });

    add("POST", "/api/v1/posts/{post_id}/replies", true,
        [this](const HttpRequest& request, const Params& params, DbContext& cx,
               const SessionContext& session) -> HandlerResult {
            const json body = requireJsonBody(request);
            JsonUtil::requireExactFields(body, {"content"});
            const Id postId = JsonUtil::parsePathId(params.at("post_id"), "post_id");
            return {201, JsonWriter::reply(replies_.addReply(
                             cx, session, postId, JsonUtil::getString(body, "content")))};
        });

    add("PUT", "/api/v1/replies/{reply_id}", true,
        [this](const HttpRequest& request, const Params& params, DbContext& cx,
               const SessionContext& session) -> HandlerResult {
            const json body = requireJsonBody(request);
            JsonUtil::requireExactFields(body, {"content"});
            const Id replyId = JsonUtil::parsePathId(params.at("reply_id"), "reply_id");
            return {200, JsonWriter::reply(replies_.editOwnReply(
                             cx, session, replyId, JsonUtil::getString(body, "content")))};
        });

    add("DELETE", "/api/v1/replies/{reply_id}", true,
        [this](const HttpRequest& request, const Params& params, DbContext& cx,
               const SessionContext& session) -> HandlerResult {
            ensureNoBody(request);
            const Id replyId = JsonUtil::parsePathId(params.at("reply_id"), "reply_id");
            replies_.deleteOwnReply(cx, session, replyId);
            return {200, json{{"reply_id", replyId}, {"status", 0}}};
        });

    // ---- 管理员 ----
    add("GET", "/api/v1/admin/users", true,
        [this](const HttpRequest& request, const Params&, DbContext& cx,
               const SessionContext& session) -> HandlerResult {
            parseQuery(request, {});
            json items = json::array();
            for (const UserProfile& user : moderation_.listUsers(cx, session)) {
                items.push_back(JsonWriter::profile(user));
            }
            return {200, json{{"items", items}}};
        });

    add("PUT", "/api/v1/admin/users/{user_id}/status", true,
        [this](const HttpRequest& request, const Params& params, DbContext& cx,
               const SessionContext& session) -> HandlerResult {
            const json body = requireJsonBody(request);
            JsonUtil::requireExactFields(body, {"status"});
            const Id userId = JsonUtil::parsePathId(params.at("user_id"), "user_id");
            const int status = JsonUtil::getInt(body, "status");
            if (status != 0 && status != 1) {
                throw errors::invalidArgument("字段 status 只能为 0 或 1");
            }
            const AccountStatusChange change =
                moderation_.setUserStatus(cx, session, userId, status);
            return {200, json{{"user_id", change.userId},
                              {"status", change.status},
                              {"changed", change.changed}}};
        });

    add("PUT", "/api/v1/admin/boards/{board_id}/moderators/{user_id}", true,
        [this](const HttpRequest& request, const Params& params, DbContext& cx,
               const SessionContext& session) -> HandlerResult {
            ensureNoBody(request);
            const Id boardId = JsonUtil::parsePathId(params.at("board_id"), "board_id");
            const Id userId = JsonUtil::parsePathId(params.at("user_id"), "user_id");
            const GrantResult result =
                moderation_.grantModerator(cx, session, boardId, userId);
            return {201, json{{"assigned", true},
                              {"assignment", JsonWriter::assignment(result.assignment)}}};
        });

    add("DELETE", "/api/v1/admin/boards/{board_id}/moderators/{user_id}", true,
        [this](const HttpRequest& request, const Params& params, DbContext& cx,
               const SessionContext& session) -> HandlerResult {
            ensureNoBody(request);
            const Id boardId = JsonUtil::parsePathId(params.at("board_id"), "board_id");
            const Id userId = JsonUtil::parsePathId(params.at("user_id"), "user_id");
            const RevokeResult result =
                moderation_.revokeModerator(cx, session, boardId, userId);
            return {200, json{{"assigned", false},
                              {"user_id", result.userId},
                              {"board_id", result.boardId}}};
        });

    // ---- 内容处置 ----
    add("POST", "/api/v1/moderation/posts/{post_id}/removal", true,
        [this](const HttpRequest& request, const Params& params, DbContext& cx,
               const SessionContext& session) -> HandlerResult {
            const json body = requireJsonBody(request);
            JsonUtil::requireExactFields(body, {"reason"});
            const Id postId = JsonUtil::parsePathId(params.at("post_id"), "post_id");
            moderation_.removePost(cx, session, postId, JsonUtil::getString(body, "reason"));
            return {200, json{{"post_id", postId}, {"status", 0}}};
        });

    add("POST", "/api/v1/moderation/replies/{reply_id}/removal", true,
        [this](const HttpRequest& request, const Params& params, DbContext& cx,
               const SessionContext& session) -> HandlerResult {
            const json body = requireJsonBody(request);
            JsonUtil::requireExactFields(body, {"reason"});
            const Id replyId = JsonUtil::parsePathId(params.at("reply_id"), "reply_id");
            moderation_.removeReply(cx, session, replyId,
                                    JsonUtil::getString(body, "reason"));
            return {200, json{{"reply_id", replyId}, {"status", 0}}};
        });

    add("PUT", "/api/v1/moderation/posts/{post_id}/pinned", true,
        [this](const HttpRequest& request, const Params& params, DbContext& cx,
               const SessionContext& session) -> HandlerResult {
            const json body = requireJsonBody(request);
            JsonUtil::requireExactFields(body, {"is_pinned"});
            const Id postId = JsonUtil::parsePathId(params.at("post_id"), "post_id");
            const PostFlagChange change = moderation_.setPinned(
                cx, session, postId, JsonUtil::getBool(body, "is_pinned"));
            return {200, json{{"post_id", change.postId},
                              {"is_pinned", change.value},
                              {"changed", change.changed},
                              {"updated_at", util::toJsonTime(change.updatedAt)}}};
        });

    add("PUT", "/api/v1/moderation/posts/{post_id}/featured", true,
        [this](const HttpRequest& request, const Params& params, DbContext& cx,
               const SessionContext& session) -> HandlerResult {
            const json body = requireJsonBody(request);
            JsonUtil::requireExactFields(body, {"is_featured"});
            const Id postId = JsonUtil::parsePathId(params.at("post_id"), "post_id");
            const PostFlagChange change = moderation_.setFeatured(
                cx, session, postId, JsonUtil::getBool(body, "is_featured"));
            return {200, json{{"post_id", change.postId},
                              {"is_featured", change.value},
                              {"changed", change.changed},
                              {"updated_at", util::toJsonTime(change.updatedAt)}}};
        });
}

HttpResponse ApiRouter::handle(const HttpRequest& request) {
    const auto started = std::chrono::steady_clock::now();
    HttpResponse response = dispatch(request);
    logRequest(request, response, started);
    return response;
}

HttpResponse ApiRouter::dispatch(const HttpRequest& request) {
    try {
        const std::vector<std::string> segments = splitPath(request.path);

        const Route* found = nullptr;
        Params params;
        std::vector<std::string> allowedMethods;
        for (const Route& route : routes_) {
            const MatchResult match = matchRoute(route.segments, segments);
            if (!match.matched) continue;
            allowedMethods.push_back(route.method);
            if (!found && route.method == request.method) {
                found = &route;
                params = match.params;
            }
        }
        if (allowedMethods.empty()) {
            throw errors::notFound("接口不存在");
        }
        if (!found) {
            HttpResponse response =
                failure(405, "METHOD_NOT_ALLOWED", "请求方法不受支持");
            std::string allow;
            for (const std::string& method : allowedMethods) {
                if (!allow.empty()) allow += ", ";
                allow += method;
            }
            response.headers.emplace_back("Allow", allow);
            return response;
        }

        DbContext cx = database_.context();
        SessionContext session;
        if (found->needAuth) {
            session = auth_.requireSession(cx, bearerToken(request));
        } else if (request.hasHeader("authorization")) {
            throw errors::invalidArgument("该接口不支持认证信息");
        }

        const HandlerResult result = found->handler(request, params, cx, session);
        return success(result.first, result.second);
    } catch (const AppError& error) {
        return failure(error.status(), error.code(), error.message());
    } catch (const DbError& error) {
        logger_.error(std::string("数据库错误: ") + error.what());
        if (error.unavailable()) {
            return failure(503, "DATABASE_UNAVAILABLE", "数据库暂时不可用");
        }
        return failure(500, "INTERNAL_ERROR", "数据库操作失败");
    } catch (const std::exception& error) {
        logger_.error(std::string("未处理异常: ") + error.what());
        return failure(500, "INTERNAL_ERROR", "服务器内部错误");
    }
}

void ApiRouter::logRequest(const HttpRequest& request, const HttpResponse& response,
                           std::chrono::steady_clock::time_point started) {
    const long long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  std::chrono::steady_clock::now() - started)
                                  .count();

    // 业务码从统一响应信封中读取；解析失败时留空
    std::string code;
    try {
        const json body = json::parse(response.body);
        if (body.is_object() && body.contains("code") && body["code"].is_string()) {
            code = body["code"].get<std::string>();
        }
    } catch (const json::exception&) {
        code = "?";
    }

    const std::string line = request.method + " " + request.path + " -> " +
                             std::to_string(response.status) + " " + code + " " +
                             std::to_string(elapsed) + "ms";
    if (response.status >= 500) {
        logger_.error(line);
    } else if (response.status >= 400) {
        logger_.warn(line);
    } else {
        logger_.info(line);
    }
}

} // namespace bbs
