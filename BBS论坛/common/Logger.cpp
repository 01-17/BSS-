#include "Logger.h"

#include <iostream>

#include "Util.h"

namespace bbs {

void Logger::init(const std::string& filePath, LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    minLevel_ = level;
    if (!filePath.empty()) {
        file_.open(filePath, std::ios::app);
    }
}

void Logger::info(const std::string& message) { write(LogLevel::Info, "INFO", message); }
void Logger::warn(const std::string& message) { write(LogLevel::Warn, "WARN", message); }
void Logger::error(const std::string& message) { write(LogLevel::Error, "ERROR", message); }

void Logger::write(LogLevel level, const char* tag, const std::string& message) {
    if (static_cast<int>(level) < static_cast<int>(minLevel_)) return;
    const std::string line = "[" + util::nowUtc() + "] [" + tag + "] " + message;
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << line << std::endl;
    if (file_.is_open()) {
        file_ << line << std::endl;
        file_.flush();
    }
}

} // namespace bbs
