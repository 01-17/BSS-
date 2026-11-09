// HTTP 服务端：监听、每连接一线程收发、单线程串行执行数据库任务
#pragma once

#include <winsock2.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "HttpMessage.h"

namespace bbs {

class Logger;

// 业务执行工作线程：所有请求串行通过同一数据库连接执行
class DbWorker {
public:
    explicit DbWorker(std::function<HttpResponse(const HttpRequest&)> handler);
    ~DbWorker();
    DbWorker(const DbWorker&) = delete;
    DbWorker& operator=(const DbWorker&) = delete;

    void start();
    void stop();
    HttpResponse submit(HttpRequest request);

private:
    void run();

    struct Task {
        HttpRequest request;
        std::promise<HttpResponse> promise;
    };

    std::function<HttpResponse(const HttpRequest&)> handler_;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<std::shared_ptr<Task>> tasks_;
    bool stopping_ = false;
    bool started_ = false;
};

class HttpServer {
public:
    HttpServer(std::string bindAddress, unsigned short port,
               std::function<HttpResponse(const HttpRequest&)> handler, Logger& logger);
    ~HttpServer();
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    bool start();
    void stop();

private:
    void acceptLoop();
    void handleConnection(SOCKET client);

    std::string bindAddress_;
    unsigned short port_;
    std::function<HttpResponse(const HttpRequest&)> handler_;
    Logger& logger_;
    SOCKET listenSocket_ = INVALID_SOCKET;
    std::thread acceptThread_;
    DbWorker worker_;
    std::atomic<bool> running_{false};
};

} // namespace bbs
