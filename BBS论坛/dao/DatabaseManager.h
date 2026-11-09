// 数据库管理：单连接（仅由数据库工作线程持有）、语句封装、显式事务
#pragma once

#include <filesystem>
#include <string>

#include "../third_party/sqlite3.h"
#include "Types.h"

namespace bbs {

class Statement;
class DatabaseManager;

// 请求级数据库上下文，保证同一请求内的语句固定在同一连接上
class DbContext {
public:
    DbContext(sqlite3* db, DatabaseManager* owner) : db_(db), owner_(owner) {}
    sqlite3* raw() const { return db_; }
    DatabaseManager* owner() const { return owner_; }

    // 在当前连接上预编译固定 SQL
    Statement prepare(const std::string& sql);

private:
    sqlite3* db_;
    DatabaseManager* owner_;
};

// RAII 语句封装：析构自动 finalize；错误统一抛 DbError
class Statement {
public:
    explicit Statement(sqlite3_stmt* stmt) : stmt_(stmt) {}
    ~Statement();
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    Statement(Statement&& other) noexcept;
    Statement& operator=(Statement&& other) noexcept;

    Statement& bind(int index, Id value);
    Statement& bind(int index, int value);
    Statement& bind(int index, const std::string& value);
    Statement& bindNull(int index);

    // true 表示读到一行；false 表示执行完成
    bool step();
    // 执行不返回行的语句
    void exec();

    Id lastInsertId() const;
    RowCount changes() const;

    bool isNull(int column) const;
    Text text(int column) const;
    Id id(int column) const;
    int integer(int column) const;
    bool boolean(int column) const;

private:
    sqlite3_stmt* stmt_ = nullptr;
};

class DatabaseManager {
public:
    DatabaseManager() = default;
    ~DatabaseManager();
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    void open(const std::string& path);
    void close();
    bool isOpen() const { return db_ != nullptr; }

    DbContext context();
    // 进入一次请求：取得该连接上的请求上下文
    DbContext enterRequest() { return context(); }
    sqlite3* raw() const { return db_; }

    // 执行可含多条语句的脚本（建表/种子数据）
    void execute(const std::string& sql);

    Statement prepare(DbContext& context, const std::string& sql);

    void beginImmediate(DbContext& context);
    void commit(DbContext& context);
    void rollback(DbContext& context) noexcept;

    // 使用 sqlite3_backup 生成一致副本，并校验目标库完整性
    void backup(const std::filesystem::path& destination);

    // 按日期生成 bbs-YYYY-MM-DD.db 备份，保留最近 keep 份；当日已备份则跳过
    void backupDaily(const std::filesystem::path& directory, int keep);

private:
    sqlite3* db_ = nullptr;
};

// 事务守卫：未 commit 即析构时自动回滚
class Transaction {
public:
    Transaction(DatabaseManager& manager, DbContext& context);
    ~Transaction();
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    void commit();

private:
    DatabaseManager& manager_;
    DbContext& context_;
    bool finished_ = false;
};

} // namespace bbs
