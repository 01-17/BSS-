// 统一业务错误与数据访问错误
#pragma once

#include <exception>
#include <string>
#include <utility>

namespace bbs {

// 业务错误：携带 HTTP 状态、稳定业务码与中文提示
class AppError : public std::exception {
public:
    AppError(int status, std::string code, std::string message)
        : status_(status), code_(std::move(code)), message_(std::move(message)) {}

    int status() const noexcept { return status_; }
    const std::string& code() const noexcept { return code_; }
    const std::string& message() const noexcept { return message_; }
    const char* what() const noexcept override { return message_.c_str(); }

private:
    int status_;
    std::string code_;
    std::string message_;
};

// 数据访问错误：底层 SQLite 返回码，供接口边界映射 503/500
class DbError : public std::exception {
public:
    explicit DbError(int sqliteCode, std::string message)
        : code_(sqliteCode), message_(std::move(message)) {}

    int code() const noexcept { return code_; }
    const char* what() const noexcept override { return message_.c_str(); }

    // 连接、锁等待、磁盘或 I/O 问题视为数据库不可用（SQLITE_BUSY/LOCKED/CANTOPEN/IOERR/FULL 等）
    bool unavailable() const noexcept {
        return code_ == 5 /*BUSY*/ || code_ == 6 /*LOCKED*/ || code_ == 14 /*CANTOPEN*/ ||
               code_ == 10 /*IOERR*/ || code_ == 13 /*FULL*/ || code_ == 11 /*CORRUPT*/ ||
               code_ == 26 /*NOTADB*/;
    }

private:
    int code_;
    std::string message_;
};

namespace errors {

inline AppError invalidArgument(std::string message) {
    return AppError(400, "INVALID_ARGUMENT", std::move(message));
}

inline AppError unauthenticated(std::string message) {
    return AppError(401, "UNAUTHENTICATED", std::move(message));
}

inline AppError forbidden(std::string message) {
    return AppError(403, "FORBIDDEN", std::move(message));
}

inline AppError notFound(std::string message) {
    return AppError(404, "RESOURCE_UNAVAILABLE", std::move(message));
}

inline AppError conflict(std::string message) {
    return AppError(409, "DATA_CONFLICT", std::move(message));
}

inline AppError internal(std::string message) {
    return AppError(500, "INTERNAL_ERROR", std::move(message));
}

inline AppError databaseUnavailable(std::string message) {
    return AppError(503, "DATABASE_UNAVAILABLE", std::move(message));
}

} // namespace errors
} // namespace bbs
