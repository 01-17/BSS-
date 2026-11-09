// 路由层：路径匹配、鉴权、请求校验与统一响应
#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "../dao/DatabaseManager.h"
#include "../service/AuthService.h"
#include "../service/BoardService.h"
#include "../service/ModerationService.h"
#include "../service/PostService.h"
#include "../service/ReplyService.h"
#include "HttpMessage.h"
#include "JsonUtil.h"

namespace bbs {

class Logger;

class ApiRouter {
public:
    ApiRouter(DatabaseManager& database, AuthService& auth, BoardService& boards,
              PostService& posts, ReplyService& replies, ModerationService& moderation,
              Logger& logger);

    HttpResponse handle(const HttpRequest& request);

private:
    using Params = std::map<std::string, std::string>;
    using HandlerResult = std::pair<int, json>;
    using Handler = std::function<HandlerResult(const HttpRequest&, const Params&, DbContext&,
                                                const SessionContext&)>;

    struct Route {
        std::string method;
        std::vector<std::string> segments;
        bool needAuth = true;
        Handler handler;
    };

    void registerRoutes();
    void add(const std::string& method, const std::string& pattern, bool needAuth,
             Handler handler);

    HttpResponse dispatch(const HttpRequest& request);
    void logRequest(const HttpRequest& request, const HttpResponse& response,
                    std::chrono::steady_clock::time_point started);

    static HttpResponse success(int status, const json& data);
    static HttpResponse failure(int status, const std::string& code, const std::string& message);

    static Params parseQuery(const HttpRequest& request,
                             std::initializer_list<const char*> allowed);
    static json requireJsonBody(const HttpRequest& request);
    static void ensureNoBody(const HttpRequest& request);
    static Text bearerToken(const HttpRequest& request);

    DatabaseManager& database_;
    AuthService& auth_;
    BoardService& boards_;
    PostService& posts_;
    ReplyService& replies_;
    ModerationService& moderation_;
    Logger& logger_;
    std::vector<Route> routes_;
};

} // namespace bbs
