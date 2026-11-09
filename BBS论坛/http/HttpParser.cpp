#include "HttpParser.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

#include "../common/Util.h"

namespace bbs {

namespace {

constexpr std::size_t kMaxHeaderBytes = 16 * 1024;
constexpr std::size_t kMaxBodyBytes = 1024 * 1024;

bool isTokenChar(char c) {
    const unsigned char uc = static_cast<unsigned char>(c);
    if (std::isalnum(uc)) return true;
    switch (c) {
    case '!': case '#': case '$': case '%': case '&': case '\'': case '*':
    case '+': case '-': case '.': case '^': case '_': case '`': case '|': case '~':
        return true;
    default:
        return false;
    }
}

std::string trimAscii(const std::string& text) {
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && (text[begin] == ' ' || text[begin] == '\t')) ++begin;
    while (end > begin && (text[end - 1] == ' ' || text[end - 1] == '\t')) --end;
    return text.substr(begin, end - begin);
}

bool parseContentLength(const std::string& text, std::size_t& value) {
    if (text.empty()) return false;
    std::size_t result = 0;
    for (char c : text) {
        if (c < '0' || c > '9') return false;
        result = result * 10 + static_cast<std::size_t>(c - '0');
        if (result > kMaxBodyBytes) return false;
    }
    value = result;
    return true;
}

} // namespace

bool HttpRequest::hasHeader(const std::string& name) const {
    const std::string lower = util::toLowerAscii(name);
    for (const auto& item : headers) {
        if (item.first == lower) return true;
    }
    return false;
}

std::string HttpRequest::header(const std::string& name) const {
    const std::string lower = util::toLowerAscii(name);
    for (const auto& item : headers) {
        if (item.first == lower) return item.second;
    }
    return {};
}

ParseStatus HttpParser::readRequest(SOCKET socket, HttpRequest& request,
                                    std::string& error) {
    request = HttpRequest();
    std::string buffer;

    // 1. 读取到头部结束标记
    std::size_t headerEnd = std::string::npos;
    while (headerEnd == std::string::npos) {
        char chunk[4096];
        const int received = ::recv(socket, chunk, sizeof(chunk), 0);
        if (received == 0) return ParseStatus::Closed;
        if (received < 0) return ParseStatus::Closed;
        buffer.append(chunk, static_cast<std::size_t>(received));
        if (buffer.size() > kMaxHeaderBytes) {
            error = "请求头过大";
            return ParseStatus::BadRequest;
        }
        headerEnd = buffer.find("\r\n\r\n");
    }

    const std::string head = buffer.substr(0, headerEnd);
    std::string body = buffer.substr(headerEnd + 4);

    // 2. 请求行
    const std::size_t lineEnd = head.find("\r\n");
    const std::string requestLine = head.substr(0, lineEnd == std::string::npos ? head.size() : lineEnd);
    {
        const std::size_t firstSpace = requestLine.find(' ');
        const std::size_t secondSpace = firstSpace == std::string::npos
                                            ? std::string::npos
                                            : requestLine.find(' ', firstSpace + 1);
        if (firstSpace == std::string::npos || secondSpace == std::string::npos) {
            error = "请求行格式非法";
            return ParseStatus::BadRequest;
        }
        request.method = requestLine.substr(0, firstSpace);
        request.target = requestLine.substr(firstSpace + 1, secondSpace - firstSpace - 1);
        request.version = requestLine.substr(secondSpace + 1);
    }
    // 方法名只校验字符集，具体是否受支持由路由层返回 405
    if (request.method.empty()) {
        error = "请求行格式非法";
        return ParseStatus::BadRequest;
    }
    for (char c : request.method) {
        if (!isTokenChar(c)) {
            error = "请求方法非法";
            return ParseStatus::BadRequest;
        }
    }
    if (request.version != "HTTP/1.1" && request.version != "HTTP/1.0") {
        error = "不支持的 HTTP 版本";
        return ParseStatus::BadRequest;
    }
    if (request.target.empty() || request.target[0] != '/') {
        error = "请求目标非法";
        return ParseStatus::BadRequest;
    }
    const std::size_t question = request.target.find('?');
    if (question == std::string::npos) {
        request.path = request.target;
    } else {
        request.path = request.target.substr(0, question);
        request.rawQuery = request.target.substr(question + 1);
    }

    // 3. 请求头
    std::size_t position = (lineEnd == std::string::npos) ? head.size() : lineEnd + 2;
    while (position < head.size()) {
        const std::size_t end = head.find("\r\n", position);
        const std::string line = head.substr(position, end == std::string::npos
                                                           ? std::string::npos
                                                           : end - position);
        position = (end == std::string::npos) ? head.size() : end + 2;
        if (line.empty()) continue;
        const std::size_t colon = line.find(':');
        if (colon == std::string::npos || colon == 0) {
            error = "请求头格式非法";
            return ParseStatus::BadRequest;
        }
        for (std::size_t i = 0; i < colon; ++i) {
            if (!isTokenChar(line[i])) {
                error = "请求头名称非法";
                return ParseStatus::BadRequest;
            }
        }
        const std::string name = util::toLowerAscii(line.substr(0, colon));
        const std::string value = trimAscii(line.substr(colon + 1));
        if (request.hasHeader(name)) {
            error = "请求头重复";
            return ParseStatus::BadRequest;
        }
        request.headers.emplace_back(name, value);
    }

    if (request.hasHeader("transfer-encoding")) {
        error = "不支持分块传输";
        return ParseStatus::BadRequest;
    }

    // 4. 请求体
    std::size_t contentLength = 0;
    if (request.hasHeader("content-length")) {
        if (!parseContentLength(request.header("content-length"), contentLength)) {
            error = "Content-Length 非法";
            return ParseStatus::BadRequest;
        }
    }
    while (body.size() < contentLength) {
        char chunk[8192];
        const int received = ::recv(socket, chunk, sizeof(chunk), 0);
        if (received <= 0) return ParseStatus::Closed;
        body.append(chunk, static_cast<std::size_t>(received));
        if (body.size() > kMaxBodyBytes) {
            error = "请求体过大";
            return ParseStatus::BadRequest;
        }
    }
    if (body.size() > contentLength) body.resize(contentLength);
    request.body = std::move(body);

    const std::string connection = util::toLowerAscii(request.header("connection"));
    if (request.version == "HTTP/1.0") {
        request.keepAlive = (connection == "keep-alive");
    } else {
        request.keepAlive = (connection != "close");
    }
    return ParseStatus::Ok;
}

const char* HttpParser::statusText(int status) {
    switch (status) {
    case 200: return "OK";
    case 201: return "Created";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 409: return "Conflict";
    case 500: return "Internal Server Error";
    case 503: return "Service Unavailable";
    default: return "Unknown";
    }
}

std::string HttpParser::serialize(const HttpResponse& response, bool keepAlive) {
    std::string out;
    out.reserve(response.body.size() + 256);
    out += "HTTP/1.1 ";
    out += std::to_string(response.status);
    out += ' ';
    out += statusText(response.status);
    out += "\r\nContent-Type: application/json; charset=utf-8\r\n";
    out += "Content-Length: " + std::to_string(response.body.size()) + "\r\n";
    out += keepAlive ? "Connection: keep-alive\r\n" : "Connection: close\r\n";
    for (const auto& header : response.headers) {
        out += header.first + ": " + header.second + "\r\n";
    }
    out += "\r\n";
    out += response.body;
    return out;
}

} // namespace bbs
