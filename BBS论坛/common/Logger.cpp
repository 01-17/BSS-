#include "Logger.h"

#include <windows.h>

#include <iostream>

#include "Util.h"

namespace bbs {

namespace {

// 控制台按代码页解码字节流，中文系统默认 936(GBK)，直接输出 UTF-8 会乱码。
// 这里在检测到标准输出是真实控制台时改用 WriteConsoleW 输出宽字符，
// 与代码页无关；输出被重定向到文件或管道时仍写原始 UTF-8 字节。
bool consoleAvailable() {
    static const bool available = [] {
        DWORD mode = 0;
        return GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &mode) != 0;
    }();
    return available;
}

void writeConsole(const std::string& utf8Line) {
    if (!consoleAvailable()) {
        std::cout << utf8Line << std::flush;
        return;
    }
    const int length = MultiByteToWideChar(CP_UTF8, 0, utf8Line.data(),
                                           static_cast<int>(utf8Line.size()), nullptr, 0);
    if (length <= 0) return;
    std::wstring wide(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8Line.data(), static_cast<int>(utf8Line.size()),
                        wide.data(), length);
    DWORD written = 0;
    WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), wide.data(),
                  static_cast<DWORD>(wide.size()), &written, nullptr);
}

} // namespace

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
    writeConsole(line + "\n");
    if (file_.is_open()) {
        file_ << line << std::endl;
        file_.flush();
    }
}

} // namespace bbs
