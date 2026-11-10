-- 第7组 BBS论坛 SQLite 3 建表脚本
-- 版本 1.0，2026-11-05；仅在服务端执行。
-- 初始化：先执行本文件，再在空白业务库执行 bbs_seed.sql。
-- 每个服务端连接均须在事务外执行 PRAGMA foreign_keys = ON。
-- 时间统一为 UTC TEXT，格式 YYYY-MM-DD HH:MM:SS；服务端生成并绑定时间。
-- 中文字段按 Unicode 码点校验；服务端先去除Unicode首尾空白并拒绝嵌入NUL。
-- SQLite trim()约束为空白校验的数据库补充，完整Unicode校验由服务端负责。
-- 业务值均使用参数化SQL；不要把本脚本拼接到用户输入中。
PRAGMA foreign_keys = ON;
BEGIN IMMEDIATE;

CREATE TABLE IF NOT EXISTS roles (
    role_id INTEGER PRIMARY KEY CHECK (role_id > 0),
    role_code TEXT NOT NULL UNIQUE CHECK (role_code IN ('member', 'admin')),
    role_name TEXT NOT NULL UNIQUE CHECK (length(trim(role_name)) BETWEEN 1 AND 30 AND instr(role_name, char(0)) = 0)
);

CREATE TABLE IF NOT EXISTS users (
    user_id INTEGER PRIMARY KEY CHECK (user_id > 0),
    username TEXT COLLATE NOCASE NOT NULL UNIQUE
        CHECK (length(username) BETWEEN 4 AND 20 AND username NOT GLOB '*[^A-Za-z0-9_]*' AND instr(username, char(0)) = 0),
    password_hash TEXT NOT NULL CHECK (length(password_hash) = 64 AND password_hash NOT GLOB '*[^0-9a-f]*'),
    password_salt TEXT NOT NULL CHECK (length(password_salt) = 32 AND password_salt NOT GLOB '*[^0-9a-f]*'),
    password_iterations INTEGER NOT NULL DEFAULT 600000 CHECK (typeof(password_iterations) = 'integer' AND password_iterations >= 600000),
    nickname TEXT NOT NULL CHECK (length(trim(nickname)) BETWEEN 1 AND 30 AND instr(nickname, char(0)) = 0),
    role_id INTEGER NOT NULL DEFAULT 1,
    status INTEGER NOT NULL DEFAULT 1 CHECK (status IN (0, 1)),
    session_version INTEGER NOT NULL DEFAULT 0 CHECK (typeof(session_version) = 'integer' AND session_version >= 0),
    registered_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        CHECK (length(registered_at) = 19 AND datetime(registered_at) IS NOT NULL AND strftime('%Y-%m-%d %H:%M:%S', registered_at) = registered_at),
    FOREIGN KEY (role_id) REFERENCES roles(role_id) ON UPDATE RESTRICT ON DELETE RESTRICT
);

CREATE TABLE IF NOT EXISTS boards (
    board_id INTEGER PRIMARY KEY CHECK (board_id > 0),
    name TEXT NOT NULL UNIQUE CHECK (length(trim(name)) BETWEEN 1 AND 30 AND instr(name, char(0)) = 0),
    description TEXT NOT NULL DEFAULT '' CHECK (length(description) <= 200 AND instr(description, char(0)) = 0),
    status INTEGER NOT NULL DEFAULT 1 CHECK (status IN (0, 1)),
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        CHECK (length(created_at) = 19 AND datetime(created_at) IS NOT NULL AND strftime('%Y-%m-%d %H:%M:%S', created_at) = created_at),
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        CHECK (length(updated_at) = 19 AND datetime(updated_at) IS NOT NULL AND strftime('%Y-%m-%d %H:%M:%S', updated_at) = updated_at AND updated_at >= created_at)
);

CREATE TABLE IF NOT EXISTS posts (
    post_id INTEGER PRIMARY KEY CHECK (post_id > 0),
    board_id INTEGER NOT NULL,
    author_id INTEGER NOT NULL,
    title TEXT NOT NULL CHECK (length(trim(title)) BETWEEN 1 AND 100 AND instr(title, char(0)) = 0),
    content TEXT NOT NULL CHECK (length(trim(content)) BETWEEN 1 AND 5000 AND instr(content, char(0)) = 0),
    status INTEGER NOT NULL DEFAULT 1 CHECK (status IN (0, 1)),
    is_pinned INTEGER NOT NULL DEFAULT 0 CHECK (is_pinned IN (0, 1)),
    is_featured INTEGER NOT NULL DEFAULT 0 CHECK (is_featured IN (0, 1)),
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        CHECK (length(created_at) = 19 AND datetime(created_at) IS NOT NULL AND strftime('%Y-%m-%d %H:%M:%S', created_at) = created_at),
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        CHECK (length(updated_at) = 19 AND datetime(updated_at) IS NOT NULL AND strftime('%Y-%m-%d %H:%M:%S', updated_at) = updated_at AND updated_at >= created_at),
    FOREIGN KEY (board_id) REFERENCES boards(board_id) ON UPDATE RESTRICT ON DELETE RESTRICT,
    FOREIGN KEY (author_id) REFERENCES users(user_id) ON UPDATE RESTRICT ON DELETE RESTRICT
);

CREATE TABLE IF NOT EXISTS replies (
    reply_id INTEGER PRIMARY KEY CHECK (reply_id > 0),
    post_id INTEGER NOT NULL,
    author_id INTEGER NOT NULL,
    content TEXT NOT NULL CHECK (length(trim(content)) BETWEEN 1 AND 1000 AND instr(content, char(0)) = 0),
    status INTEGER NOT NULL DEFAULT 1 CHECK (status IN (0, 1)),
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        CHECK (length(created_at) = 19 AND datetime(created_at) IS NOT NULL AND strftime('%Y-%m-%d %H:%M:%S', created_at) = created_at),
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        CHECK (length(updated_at) = 19 AND datetime(updated_at) IS NOT NULL AND strftime('%Y-%m-%d %H:%M:%S', updated_at) = updated_at AND updated_at >= created_at),
    FOREIGN KEY (post_id) REFERENCES posts(post_id) ON UPDATE RESTRICT ON DELETE RESTRICT,
    FOREIGN KEY (author_id) REFERENCES users(user_id) ON UPDATE RESTRICT ON DELETE RESTRICT
);

CREATE TABLE IF NOT EXISTS board_moderators (
    user_id INTEGER NOT NULL,
    board_id INTEGER NOT NULL,
    assigned_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        CHECK (length(assigned_at) = 19 AND datetime(assigned_at) IS NOT NULL AND strftime('%Y-%m-%d %H:%M:%S', assigned_at) = assigned_at),
    PRIMARY KEY (user_id, board_id),
    FOREIGN KEY (user_id) REFERENCES users(user_id) ON UPDATE RESTRICT ON DELETE RESTRICT,
    FOREIGN KEY (board_id) REFERENCES boards(board_id) ON UPDATE RESTRICT ON DELETE RESTRICT
);

CREATE TABLE IF NOT EXISTS moderation_logs (
    log_id INTEGER PRIMARY KEY CHECK (log_id > 0),
    actor_id INTEGER NOT NULL,
    action TEXT NOT NULL CHECK (action IN (
        'user_disable', 'user_enable',
        'board_create', 'board_update', 'board_disable', 'board_enable',
        'moderator_grant', 'moderator_revoke',
        'post_remove', 'reply_remove',
        'post_pin', 'post_unpin', 'post_feature', 'post_unfeature'
    )),
    target_user_id INTEGER,
    target_board_id INTEGER,
    target_post_id INTEGER,
    target_reply_id INTEGER,
    reason TEXT CHECK (reason IS NULL OR (length(trim(reason)) BETWEEN 1 AND 200 AND instr(reason, char(0)) = 0)),
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        CHECK (length(created_at) = 19 AND datetime(created_at) IS NOT NULL AND strftime('%Y-%m-%d %H:%M:%S', created_at) = created_at),
    CHECK (
        (action IN ('user_disable','user_enable') AND target_user_id IS NOT NULL AND target_board_id IS NULL AND target_post_id IS NULL AND target_reply_id IS NULL)
        OR (action IN ('board_create','board_update','board_disable','board_enable') AND target_user_id IS NULL AND target_board_id IS NOT NULL AND target_post_id IS NULL AND target_reply_id IS NULL)
        OR (action IN ('moderator_grant','moderator_revoke') AND target_user_id IS NOT NULL AND target_board_id IS NOT NULL AND target_post_id IS NULL AND target_reply_id IS NULL)
        OR (action IN ('post_remove','post_pin','post_unpin','post_feature','post_unfeature') AND target_user_id IS NULL AND target_board_id IS NULL AND target_post_id IS NOT NULL AND target_reply_id IS NULL)
        OR (action = 'reply_remove' AND target_user_id IS NULL AND target_board_id IS NULL AND target_post_id IS NULL AND target_reply_id IS NOT NULL)
    ),
    CHECK (action NOT IN ('post_remove', 'reply_remove') OR reason IS NOT NULL),
    FOREIGN KEY (actor_id) REFERENCES users(user_id) ON UPDATE RESTRICT ON DELETE RESTRICT,
    FOREIGN KEY (target_user_id) REFERENCES users(user_id) ON UPDATE RESTRICT ON DELETE RESTRICT,
    FOREIGN KEY (target_board_id) REFERENCES boards(board_id) ON UPDATE RESTRICT ON DELETE RESTRICT,
    FOREIGN KEY (target_post_id) REFERENCES posts(post_id) ON UPDATE RESTRICT ON DELETE RESTRICT,
    FOREIGN KEY (target_reply_id) REFERENCES replies(reply_id) ON UPDATE RESTRICT ON DELETE RESTRICT
);

-- 角色查询和外键引用检查
CREATE INDEX IF NOT EXISTS idx_users_role ON users (role_id);
-- 版块默认列表，置顶优先
CREATE INDEX IF NOT EXISTS idx_posts_board_list ON posts (board_id, status, is_pinned DESC, created_at DESC, post_id DESC);
-- 全站可见帖及时间范围筛选/排序
CREATE INDEX IF NOT EXISTS idx_posts_search_time ON posts (status, created_at DESC, post_id DESC);
-- 作者内容查询及外键引用检查
CREATE INDEX IF NOT EXISTS idx_posts_author ON posts (author_id);
-- 可见回复顺序读取和热度计数
CREATE INDEX IF NOT EXISTS idx_replies_post_list ON replies (post_id, status, created_at, reply_id);
-- 作者回复查询及外键引用检查
CREATE INDEX IF NOT EXISTS idx_replies_author ON replies (author_id);
-- 按版块列授权及外键引用检查
CREATE INDEX IF NOT EXISTS idx_board_moderators_board ON board_moderators (board_id, user_id);
-- 按操作者追溯管理记录
CREATE INDEX IF NOT EXISTS idx_logs_actor_time ON moderation_logs (actor_id, created_at DESC, log_id DESC);
-- 按账号或授权目标追溯
CREATE INDEX IF NOT EXISTS idx_logs_target_user ON moderation_logs (target_user_id);
-- 按版块或授权目标追溯
CREATE INDEX IF NOT EXISTS idx_logs_target_board ON moderation_logs (target_board_id);
-- 按帖子目标追溯
CREATE INDEX IF NOT EXISTS idx_logs_target_post ON moderation_logs (target_post_id);
-- 按回复目标追溯
CREATE INDEX IF NOT EXISTS idx_logs_target_reply ON moderation_logs (target_reply_id);

COMMIT;

-- 管理写入约定（由C++服务端执行，不在此初始化脚本中自动运行）：
-- BEGIN IMMEDIATE;
-- 重新检查会话版本、账号状态、当前角色/版块授权及目标状态。
-- 条件UPDATE/INSERT/DELETE业务行，检查影响行数为1。
-- INSERT INTO moderation_logs(...) VALUES (...);  -- 同一连接、同一事务
-- COMMIT;  -- 任一步失败则ROLLBACK，不得只提交业务或只写日志。
-- 普通删帖/删回复：UPDATE状态；不执行物理DELETE。
-- 撤销授权：DELETE board_moderators，随后追加 moderator_revoke 日志。
-- 时间、作者、版块和管理目标均由服务端核验；更新时显式设置updated_at。
-- IF NOT EXISTS仅便于同版本重复初始化，不代替结构迁移。
