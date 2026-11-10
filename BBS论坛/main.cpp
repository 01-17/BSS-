// BBS 论坛服务端入口：读配置 → 初始化日志与数据库 → 装配各层 → 启动 HTTP 服务
#include "http/HttpServer.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <atomic>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#include "common/Config.h"
#include "common/Logger.h"
#include "dao/BoardRepository.h"
#include "dao/DatabaseManager.h"
#include "dao/ModerationLogRepository.h"
#include "dao/ModeratorAssignmentRepository.h"
#include "dao/PostRepository.h"
#include "dao/ReplyRepository.h"
#include "dao/UserRepository.h"
#include "http/ApiRouter.h"
#include "service/AuthService.h"
#include "service/BoardService.h"
#include "service/ModerationService.h"
#include "service/PermissionService.h"
#include "service/PostService.h"
#include "service/ReplyService.h"

using namespace bbs;

namespace {

std::atomic<bool> g_running{true};

void handleSignal(int) {
    g_running = false;
}

std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return {};
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

LogLevel parseLevel(const std::string& text) {
    if (text == "WARN") return LogLevel::Warn;
    if (text == "ERROR") return LogLevel::Error;
    return LogLevel::Info;
}

void ensureParentDirectory(const std::string& filePath) {
    const std::filesystem::path path(filePath);
    if (path.has_parent_path()) {
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
    }
}

std::filesystem::path executableDirectory() {
    std::string buffer(MAX_PATH, '\0');
    const DWORD length = GetModuleFileNameA(nullptr, buffer.data(),
                                            static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) return {};
    return std::filesystem::path(buffer.substr(0, length)).parent_path();
}

// 控制台默认使用系统代码页（中文为 936/GBK），而本程序输出的中文字面量是 UTF-8，
// 不切换会显示为乱码。重定向到文件时该调用失败，无副作用。
void enableUtf8Console() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
}

std::string toUtf8(const std::filesystem::path& path) {
    const std::string text = path.u8string();
    return text;
}

// 依次在“当前工作目录、exe 所在目录及其上两级”查找 config.ini。
// 找到后把工作目录切到该目录，使 config.ini、sql/*.sql、data 等相对路径
// 在双击 out\Debug\BBS论坛.exe 这类场景下同样可用。
bool locateRootDirectory() {
    std::error_code error;
    if (std::filesystem::exists("config.ini", error)) {
        std::cout << "工作目录: " << toUtf8(std::filesystem::current_path()) << std::endl;
        return true;
    }

    std::filesystem::path directory = executableDirectory();
    for (int depth = 0; depth < 4 && !directory.empty(); ++depth) {
        if (std::filesystem::exists(directory / "config.ini", error)) {
            std::filesystem::current_path(directory, error);
            if (error) return false;
            std::cout << "工作目录: " << toUtf8(directory) << std::endl;
            return true;
        }
        directory = directory.parent_path();
    }
    return false;
}

} // namespace

int main() {
    enableUtf8Console();

    if (!locateRootDirectory()) {
        std::cerr << "无法定位 config.ini，请把本程序放在服务端根目录下，"
                     "或在服务端根目录执行 BBS论坛.exe" << std::endl;
        return 1;
    }

    Config config;
    if (!config.load("config.ini")) {
        std::cerr << "无法读取 config.ini，请检查文件编码与格式" << std::endl;
        return 1;
    }

    const std::string host = config.get("server", "host", "0.0.0.0");
    const int port = config.getInt("server", "port", 8080);
    const std::string dbPath = config.get("database", "path", "data/bbs.db");
    const std::string logFile = config.get("log", "file", "logs/server.log");
    const LogLevel level = parseLevel(config.get("log", "level", "INFO"));

    ensureParentDirectory(logFile);
    ensureParentDirectory(dbPath);

    Logger logger;
    logger.init(logFile, level);
    logger.info("=== BBS 论坛服务端启动 ===");

    DatabaseManager database;
    try {
        database.open(dbPath);
        logger.info("数据库已打开: " + dbPath);

        const std::string schema = readFile("sql/bbs_schema.sql");
        if (schema.empty()) {
            logger.error("缺少 sql/bbs_schema.sql，无法建表");
            return 1;
        }
        database.execute(schema);

        const std::string seed = readFile("sql/bbs_seed.sql");
        if (!seed.empty()) {
            database.execute(seed);
            logger.info("种子数据已校验（幂等）");
        }

        // 每日一致备份，保留最近 7 份（Day 4 §5.6 / Day 5 §6.4）
        const std::string backupDir = config.get("database", "backup_dir", "data/backups");
        database.backupDaily(backupDir, 7);
        logger.info("数据库备份已完成: " + backupDir);
    } catch (const std::exception& error) {
        logger.error(std::string("数据库初始化失败: ") + error.what());
        return 1;
    }

    // 数据访问层
    UserRepository users;
    BoardRepository boards;
    PostRepository posts;
    ReplyRepository replies;
    ModeratorAssignmentRepository assignments;
    ModerationLogRepository logs;

    // 业务层
    PermissionService permissions(users, assignments);
    AuthService auth(database, users, assignments, permissions);
    BoardService boardService(boards, posts, permissions);
    PostService postService(database, boards, posts, replies, permissions);
    ReplyService replyService(database, boards, posts, replies, permissions);
    ModerationService moderation(database, users, boards, posts, replies, assignments, logs,
                                 permissions, auth);

    ApiRouter router(database, auth, boardService, postService, replyService, moderation,
                     logger);
    HttpServer server(host, static_cast<unsigned short>(port),
                      [&router](const HttpRequest& request) { return router.handle(request); },
                      logger);

    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    if (!server.start()) {
        logger.error("HTTP 服务启动失败");
        return 1;
    }
    std::cout << "服务运行中，按 Ctrl+C 停止" << std::endl;

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    logger.info("正在停止服务...");
    server.stop();
    database.close();
    logger.info("=== BBS 论坛服务端已停止 ===");
    return 0;
}
