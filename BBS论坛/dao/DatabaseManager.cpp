#include "DatabaseManager.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "../common/Error.h"
#include "../common/Util.h"

namespace bbs {

namespace {

[[noreturn]] void throwDb(sqlite3* db, int code, const char* action) {
    std::string message = action;
    if (db) {
        message += ": ";
        message += sqlite3_errmsg(db);
    }
    throw DbError(code, message);
}

} // namespace

Statement::~Statement() {
    if (stmt_) sqlite3_finalize(stmt_);
}

Statement::Statement(Statement&& other) noexcept : stmt_(other.stmt_) {
    other.stmt_ = nullptr;
}

Statement& Statement::operator=(Statement&& other) noexcept {
    if (this != &other) {
        if (stmt_) sqlite3_finalize(stmt_);
        stmt_ = other.stmt_;
        other.stmt_ = nullptr;
    }
    return *this;
}

Statement& Statement::bind(int index, Id value) {
    if (sqlite3_bind_int64(stmt_, index, value) != SQLITE_OK) {
        throwDb(sqlite3_db_handle(stmt_), sqlite3_errcode(sqlite3_db_handle(stmt_)), "绑定参数失败");
    }
    return *this;
}

Statement& Statement::bind(int index, int value) {
    if (sqlite3_bind_int(stmt_, index, value) != SQLITE_OK) {
        throwDb(sqlite3_db_handle(stmt_), sqlite3_errcode(sqlite3_db_handle(stmt_)), "绑定参数失败");
    }
    return *this;
}

Statement& Statement::bind(int index, const std::string& value) {
    if (sqlite3_bind_text(stmt_, index, value.data(), static_cast<int>(value.size()),
                          SQLITE_TRANSIENT) != SQLITE_OK) {
        throwDb(sqlite3_db_handle(stmt_), sqlite3_errcode(sqlite3_db_handle(stmt_)), "绑定参数失败");
    }
    return *this;
}

Statement& Statement::bindNull(int index) {
    if (sqlite3_bind_null(stmt_, index) != SQLITE_OK) {
        throwDb(sqlite3_db_handle(stmt_), sqlite3_errcode(sqlite3_db_handle(stmt_)), "绑定参数失败");
    }
    return *this;
}

bool Statement::step() {
    const int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;
    throwDb(sqlite3_db_handle(stmt_), rc, "执行语句失败");
}

void Statement::exec() {
    const int rc = sqlite3_step(stmt_);
    if (rc != SQLITE_DONE) {
        throwDb(sqlite3_db_handle(stmt_), rc, "执行语句失败");
    }
}

Id Statement::lastInsertId() const {
    return static_cast<Id>(sqlite3_last_insert_rowid(sqlite3_db_handle(stmt_)));
}

RowCount Statement::changes() const {
    return static_cast<RowCount>(sqlite3_changes(sqlite3_db_handle(stmt_)));
}

bool Statement::isNull(int column) const {
    return sqlite3_column_type(stmt_, column) == SQLITE_NULL;
}

Text Statement::text(int column) const {
    const unsigned char* data = sqlite3_column_text(stmt_, column);
    if (!data) return {};
    return Text(reinterpret_cast<const char*>(data),
                static_cast<std::size_t>(sqlite3_column_bytes(stmt_, column)));
}

Id Statement::id(int column) const {
    return static_cast<Id>(sqlite3_column_int64(stmt_, column));
}

int Statement::integer(int column) const {
    return sqlite3_column_int(stmt_, column);
}

bool Statement::boolean(int column) const {
    return sqlite3_column_int(stmt_, column) != 0;
}

DatabaseManager::~DatabaseManager() {
    close();
}

void DatabaseManager::open(const std::string& path) {
    close();
    sqlite3* db = nullptr;
    const int rc = sqlite3_open_v2(path.c_str(), &db,
                                   SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                                       SQLITE_OPEN_FULLMUTEX,
                                   nullptr);
    if (rc != SQLITE_OK) {
        const std::string message = db ? sqlite3_errmsg(db) : "无法创建数据库连接";
        if (db) sqlite3_close(db);
        throw DbError(rc, "打开数据库失败: " + message);
    }
    db_ = db;
    // 连接级配置：锁等待 2000 ms、开启外键并核验、沿用默认回滚日志模式
    sqlite3_busy_timeout(db_, 2000);
    if (sqlite3_exec(db_, "PRAGMA foreign_keys = ON", nullptr, nullptr, nullptr) != SQLITE_OK) {
        close();
        throw DbError(SQLITE_ERROR, "开启外键约束失败");
    }
    sqlite3_stmt* check = nullptr;
    int enabled = 0;
    if (sqlite3_prepare_v2(db_, "PRAGMA foreign_keys", -1, &check, nullptr) == SQLITE_OK) {
        if (sqlite3_step(check) == SQLITE_ROW) enabled = sqlite3_column_int(check, 0);
        sqlite3_finalize(check);
    }
    if (enabled != 1) {
        close();
        throw DbError(SQLITE_ERROR, "外键约束未生效");
    }
}

void DatabaseManager::close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

DbContext DatabaseManager::context() {
    return DbContext(db_, this);
}

Statement DbContext::prepare(const std::string& sql) {
    sqlite3_stmt* stmt = nullptr;
    const int rc = sqlite3_prepare_v2(db_, sql.c_str(), static_cast<int>(sql.size()),
                                      &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throwDb(db_, rc, "准备语句失败");
    }
    return Statement(stmt);
}

void DatabaseManager::execute(const std::string& sql) {
    if (!db_) throw DbError(SQLITE_MISUSE, "数据库尚未打开");
    char* error = nullptr;
    const int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error);
    if (rc != SQLITE_OK) {
        std::string message = error ? error : "执行脚本失败";
        if (error) sqlite3_free(error);
        throw DbError(rc, message);
    }
}

Statement DatabaseManager::prepare(DbContext& context, const std::string& sql) {
    return context.prepare(sql);
}

void DatabaseManager::beginImmediate(DbContext& context) {
    execute("BEGIN IMMEDIATE");
}

void DatabaseManager::commit(DbContext& context) {
    execute("COMMIT");
}

void DatabaseManager::rollback(DbContext& context) noexcept {
    if (!db_) return;
    sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
}

void DatabaseManager::backup(const std::filesystem::path& destination) {
    if (!db_) throw DbError(SQLITE_MISUSE, "数据库尚未打开");

    std::error_code ec;
    if (destination.has_parent_path()) {
        std::filesystem::create_directories(destination.parent_path(), ec);
    }
    if (!destination.parent_path().empty()) {
        // 同名旧文件先移除，避免备份目标处于非空状态
        std::filesystem::remove(destination, ec);
    }

    sqlite3* target = nullptr;
    const int openRc = sqlite3_open_v2(destination.string().c_str(), &target,
                                       SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    if (openRc != SQLITE_OK) {
        const std::string message = target ? sqlite3_errmsg(target) : "无法创建备份文件";
        if (target) sqlite3_close(target);
        throw DbError(openRc, "创建备份文件失败: " + message);
    }

    sqlite3_backup* handle = sqlite3_backup_init(target, "main", db_, "main");
    if (!handle) {
        const int rc = sqlite3_errcode(target);
        const std::string message = sqlite3_errmsg(target);
        sqlite3_close(target);
        throw DbError(rc, "启动备份失败: " + message);
    }
    int rc = SQLITE_OK;
    do {
        rc = sqlite3_backup_step(handle, 128);
    } while (rc == SQLITE_OK || rc == SQLITE_BUSY || rc == SQLITE_LOCKED);
    const int finishRc = sqlite3_backup_finish(handle);
    if (rc != SQLITE_DONE || finishRc != SQLITE_OK) {
        const int code = finishRc != SQLITE_OK ? finishRc : rc;
        sqlite3_close(target);
        throw DbError(code, "复制数据库失败");
    }

    sqlite3_stmt* check = nullptr;
    int healthy = 0;
    if (sqlite3_prepare_v2(target, "PRAGMA integrity_check", -1, &check, nullptr) == SQLITE_OK) {
        if (sqlite3_step(check) == SQLITE_ROW) {
            const unsigned char* text = sqlite3_column_text(check, 0);
            healthy = text && std::string(reinterpret_cast<const char*>(text)) == "ok";
        }
        sqlite3_finalize(check);
    }
    sqlite3_close(target);
    if (!healthy) {
        std::filesystem::remove(destination, ec);
        throw DbError(SQLITE_CORRUPT, "备份完整性检查未通过");
    }
}

void DatabaseManager::backupDaily(const std::filesystem::path& directory, int keep) {
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);

    // 文件名取 UTC 日期，保证同一服务端日只生成一份备份
    const std::string today = util::nowUtc().substr(0, 10);
    const std::filesystem::path target = directory / ("bbs-" + today + ".db");
    if (!std::filesystem::exists(target)) {
        backup(target);
    }

    // 仅保留最近 keep 份，按文件名（日期）升序删除最旧的
    std::vector<std::filesystem::path> existing;
    for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
        if (!entry.is_regular_file()) continue;
        const std::string name = entry.path().filename().string();
        if (name.rfind("bbs-", 0) == 0 && entry.path().extension() == ".db") {
            existing.push_back(entry.path());
        }
    }
    std::sort(existing.begin(), existing.end());
    while (keep >= 0 && existing.size() > static_cast<std::size_t>(keep)) {
        std::filesystem::remove(existing.front(), ec);
        existing.erase(existing.begin());
    }
}

Transaction::Transaction(DatabaseManager& manager, DbContext& context)
    : manager_(manager), context_(context) {
    manager_.beginImmediate(context_);
}

Transaction::~Transaction() {
    if (!finished_) manager_.rollback(context_);
}

void Transaction::commit() {
    manager_.commit(context_);
    finished_ = true;
}

} // namespace bbs
