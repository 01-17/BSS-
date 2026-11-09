// HTTP 报文结构
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace bbs {

struct HttpRequest {
    std::string method;
    std::string target;
    std::string path;
    std::string rawQuery;
    std::string version;
    std::vector<std::pair<std::string, std::string>> headers; // 名称统一小写
    std::string body;
    bool keepAlive = true;

    bool hasHeader(const std::string& name) const;
    std::string header(const std::string& name) const;
};

struct HttpResponse {
    int status = 200;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
};

} // namespace bbs
