// 分级日志：控制台与文件双输出，带 UTC 时间戳
#pragma once

#include <fstream>
#include <mutex>
#include <string>

namespace bbs {

enum class LogLevel { Info = 0, Warn = 1, Error = 2 };

class Logger {
public:
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // 初始化日志文件；level 为最低输出级别
    void init(const std::string& filePath, LogLevel level);

    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);

private:
    void write(LogLevel level, const char* tag, const std::string& message);

    std::mutex mutex_;
    std::ofstream file_;
    LogLevel minLevel_ = LogLevel::Info;
};

} // namespace bbs
