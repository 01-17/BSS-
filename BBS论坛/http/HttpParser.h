// HTTP 请求读取与响应序列化（4 字节长度前缀的替代：标准 HTTP/1.1 报文）
#pragma once

#include <winsock2.h>

#include <string>

#include "HttpMessage.h"

namespace bbs {

enum class ParseStatus { Ok, Closed, BadRequest };

class HttpParser {
public:
    // 读取并解析一个完整请求；BadRequest 时 error 带中文原因
    static ParseStatus readRequest(SOCKET socket, HttpRequest& request, std::string& error);

    // 序列化响应；keepAlive 决定 Connection 头
    static std::string serialize(const HttpResponse& response, bool keepAlive);

    static const char* statusText(int status);
};

} // namespace bbs
