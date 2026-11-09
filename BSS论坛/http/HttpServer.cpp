#include "HttpServer.h"

#include <ws2tcpip.h>

#include <exception>

#include "../common/Logger.h"
#include "HttpParser.h"
#include "JsonUtil.h"

namespace bbs {

namespace {

HttpResponse serviceUnavailable() {
    HttpResponse response;
    response.status = 503;
    json body{{"success", false},
              {"code", "DATABASE_UNAVAILABLE"},
              {"message", "服务正在停止"},
              {"data", nullptr}};
    response.body = body.dump();
    return response;
}

HttpResponse badRequest(const std::string& message) {
    HttpResponse response;
    response.status = 400;
    json body{{"success", false},
              {"code", "INVALID_ARGUMENT"},
              {"message", message},
              {"data", nullptr}};
    response.body = body.dump();
    return response;
}

bool sendAll(SOCKET socket, const std::string& data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
        const int result = ::send(socket, data.data() + sent,
                                  static_cast<int>(data.size() - sent), 0);
        if (result <= 0) return false;
        sent += static_cast<std::size_t>(result);
    }
    return true;
}

} // namespace

DbWorker::DbWorker(std::function<HttpResponse(const HttpRequest&)> handler)
    : handler_(std::move(handler)) {}

DbWorker::~DbWorker() {
    stop();
}

void DbWorker::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (started_) return;
    started_ = true;
    stopping_ = false;
    thread_ = std::thread(&DbWorker::run, this);
}

void DbWorker::stop() {
    std::thread toJoin;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!started_) return;
        stopping_ = true;
        toJoin = std::move(thread_);
        started_ = false;
    }
    condition_.notify_all();
    if (toJoin.joinable()) toJoin.join();
}

HttpResponse DbWorker::submit(HttpRequest request) {
    auto task = std::make_shared<Task>();
    task->request = std::move(request);
    std::future<HttpResponse> future = task->promise.get_future();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!started_ || stopping_) return serviceUnavailable();
        tasks_.push_back(task);
    }
    condition_.notify_one();
    return future.get();
}

void DbWorker::run() {
    for (;;) {
        std::shared_ptr<Task> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });
            if (stopping_ && tasks_.empty()) return;
            task = tasks_.front();
            tasks_.pop_front();
        }
        try {
            task->promise.set_value(handler_(task->request));
        } catch (...) {
            task->promise.set_value(serviceUnavailable());
        }
    }
}

HttpServer::HttpServer(std::string bindAddress, unsigned short port,
                       std::function<HttpResponse(const HttpRequest&)> handler, Logger& logger)
    : bindAddress_(std::move(bindAddress)), port_(port), handler_(std::move(handler)),
      logger_(logger), worker_(handler_) {}

HttpServer::~HttpServer() {
    stop();
}

bool HttpServer::start() {
    WSADATA wsaData{};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        logger_.error("Winsock 初始化失败");
        return false;
    }

    listenSocket_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket_ == INVALID_SOCKET) {
        logger_.error("创建监听套接字失败");
        WSACleanup();
        return false;
    }

    BOOL reuse = TRUE;
    ::setsockopt(listenSocket_, SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port_);
    if (::inet_pton(AF_INET, bindAddress_.c_str(), &address.sin_addr) != 1) {
        address.sin_addr.s_addr = htonl(INADDR_ANY);
    }

    if (::bind(listenSocket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) ==
        SOCKET_ERROR) {
        logger_.error("绑定端口 " + std::to_string(port_) + " 失败");
        ::closesocket(listenSocket_);
        listenSocket_ = INVALID_SOCKET;
        WSACleanup();
        return false;
    }
    if (::listen(listenSocket_, SOMAXCONN) == SOCKET_ERROR) {
        logger_.error("监听失败");
        ::closesocket(listenSocket_);
        listenSocket_ = INVALID_SOCKET;
        WSACleanup();
        return false;
    }

    worker_.start();
    running_ = true;
    acceptThread_ = std::thread(&HttpServer::acceptLoop, this);
    logger_.info("HTTP 服务已启动，监听 " + bindAddress_ + ":" + std::to_string(port_));
    return true;
}

void HttpServer::stop() {
    if (!running_.exchange(false)) return;
    if (listenSocket_ != INVALID_SOCKET) {
        ::closesocket(listenSocket_);
        listenSocket_ = INVALID_SOCKET;
    }
    if (acceptThread_.joinable()) acceptThread_.join();
    worker_.stop();
    WSACleanup();
}

void HttpServer::acceptLoop() {
    while (running_) {
        sockaddr_in clientAddress{};
        int length = sizeof(clientAddress);
        const SOCKET client =
            ::accept(listenSocket_, reinterpret_cast<sockaddr*>(&clientAddress), &length);
        if (client == INVALID_SOCKET) {
            if (!running_) break;
            continue;
        }
        std::thread(&HttpServer::handleConnection, this, client).detach();
    }
}

void HttpServer::handleConnection(SOCKET client) {
    // 空闲连接 60 秒超时，避免线程长期占用
    DWORD timeoutMs = 60000;
    ::setsockopt(client, SOL_SOCKET, SO_RCVTIMEO,
                 reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));
    ::setsockopt(client, SOL_SOCKET, SO_SNDTIMEO,
                 reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));

    for (;;) {
        HttpRequest request;
        std::string error;
        const ParseStatus status = HttpParser::readRequest(client, request, error);
        if (status == ParseStatus::Closed) break;
        if (status == ParseStatus::BadRequest) {
            const HttpResponse response = badRequest(error);
            sendAll(client, HttpParser::serialize(response, false));
            break;
        }

        const bool keepAlive = request.keepAlive;
        HttpResponse response = worker_.submit(std::move(request));

        if (!sendAll(client, HttpParser::serialize(response, keepAlive))) {
            break;
        }
        if (!keepAlive) break;
    }
    ::shutdown(client, SD_BOTH);
    ::closesocket(client);
}

} // namespace bbs
