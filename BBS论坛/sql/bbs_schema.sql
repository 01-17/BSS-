-- BBS 论坛服务端数据库结构（SQLite 3，幂等建表）
-- 依据：Day 4《类图与数据库设计说明书》表 4-1 至表 4-7、表 5-1；Day 5《详细设计说明书》表 4-2、表 4-3
PRAGMA foreign_keys = ON;

-- 基础角色：role_id 固定 1 member、2 admin
CREATE TABLE IF NOT EXISTS roles (
    role_id   INTEGER PRIMARY KEY CHECK (role_id > 0),
    role_code TEXT NOT NULL UNIQUE CHECK (role_code IN ('member', 'admin')),
    role_name TEXT NOT NULL UNIQUE
              CHECK (length(trim(role_name)) BETWEEN 1 AND 30)
);

CREATE TABLE IF NOT EXISTS users (
    user_id             INTEGER PRIMARY KEY CHECK (user_id > 0),
    username            TEXT NOT NULL COLLATE NOCASE UNIQUE
                        CHECK (length(username) BETWEEN 4 AND 20
                               AND username NOT GLOB '*[^A-Za-z0-9_]*'),
    password_hash       TEXT NOT NULL
                        CHECK (length(password_hash) = 64
                               AND password_hash NOT GLOB '*[^0-9a-f]*'),
    password_salt       TEXT NOT NULL
                        CHECK (length(password_salt) = 32
                               AND password_salt NOT GLOB '*[^0-9a-f]*'),
    password_iterations INTEGER NOT NULL DEFAULT 600000
                        CHECK (password_iterations >= 600000),
    nickname            TEXT NOT NULL
                        CHECK (length(trim(nickname)) BETWEEN 1 AND 30),
    role_id             INTEGER NOT NULL DEFAULT 1
                        REFERENCES roles(role_id)
                        ON UPDATE RESTRICT ON DELETE RESTRICT,
    status              INTEGER NOT NULL DEFAULT 1 CHECK (status IN (0, 1)),
    session_version     INTEGER NOT NULL DEFAULT 0 CHECK (session_version >= 0),
    registered_at       TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS boards (
    board_id    INTEGER PRIMARY KEY CHECK (board_id > 0),
    name        TEXT NOT NULL UNIQUE COLLATE BINARY
                CHECK (length(trim(name)) BETWEEN 1 AND 30),
    description TEXT NOT NULL DEFAULT '' CHECK (length(description) <= 200),
    status      INTEGER NOT NULL DEFAULT 1 CHECK (status IN (0, 1)),
    created_at  TEXT NOT NULL,
    updated_at  TEXT NOT NULL,
    CHECK (updated_at >= created_at)
);

CREATE TABLE IF NOT EXISTS posts (
    post_id     INTEGER PRIMARY KEY CHECK (post_id > 0),
    board_id    INTEGER NOT NULL REFERENCES boards(board_id)
                ON UPDATE RESTRICT ON DELETE RESTRICT,
    author_id   INTEGER NOT NULL REFERENCES users(user_id)
                ON UPDATE RESTRICT ON DELETE RESTRICT,
    title       TEXT NOT NULL
                CHECK (length(trim(title)) BETWEEN 1 AND 100),
    content     TEXT NOT NULL
                CHECK (length(trim(content)) BETWEEN 1 AND 5000),
    status      INTEGER NOT NULL DEFAULT 1 CHECK (status IN (0, 1)),
    is_pinned   INTEGER NOT NULL DEFAULT 0 CHECK (is_pinned IN (0, 1)),
    is_featured INTEGER NOT NULL DEFAULT 0 CHECK (is_featured IN (0, 1)),
    created_at  TEXT NOT NULL,
    updated_at  TEXT NOT NULL,
    CHECK (updated_at >= created_at)
);

CREATE TABLE IF NOT EXISTS replies (
    reply_id   INTEGER PRIMARY KEY CHECK (reply_id > 0),
    post_id    INTEGER NOT NULL REFERENCES posts(post_id)
               ON UPDATE RESTRICT ON DELETE RESTRICT,
    author_id  INTEGER NOT NULL REFERENCES users(user_id)
               ON UPDATE RESTRICT ON DELETE RESTRICT,
    content    TEXT NOT NULL
               CHECK (length(trim(content)) BETWEEN 1 AND 1000),
    status     INTEGER NOT NULL DEFAULT 1 CHECK (status IN (0, 1)),
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL,
    CHECK (updated_at >= created_at)
);

-- 版主授权：仅保存当前有效授权，撤销即删除关系行
CREATE TABLE IF NOT EXISTS board_moderators (
    user_id     INTEGER NOT NULL REFERENCES users(user_id)
                ON UPDATE RESTRICT ON DELETE RESTRICT,
    board_id    INTEGER NOT NULL REFERENCES boards(board_id)
                ON UPDATE RESTRICT ON DELETE RESTRICT,
    assigned_at TEXT NOT NULL,
    PRIMARY KEY (user_id, board_id)
);

-- 管理日志：仅追加，动作与目标外键组合须匹配，内容移除原因必填
CREATE TABLE IF NOT EXISTS moderation_logs (
    log_id          INTEGER PRIMARY KEY CHECK (log_id > 0),
    actor_id        INTEGER NOT NULL REFERENCES users(user_id)
                    ON UPDATE RESTRICT ON DELETE RESTRICT,
    action          TEXT NOT NULL CHECK (action IN (
                        'board_create', 'board_update', 'board_disable', 'board_enable',
                        'user_disable', 'user_enable', 'moderator_grant', 'moderator_revoke',
                        'post_remove', 'reply_remove', 'post_pin', 'post_unpin',
                        'post_feature', 'post_unfeature')),
    target_user_id  INTEGER REFERENCES users(user_id)
                    ON UPDATE RESTRICT ON DELETE RESTRICT,
    target_board_id INTEGER REFERENCES boards(board_id)
                    ON UPDATE RESTRICT ON DELETE RESTRICT,
    target_post_id  INTEGER REFERENCES posts(post_id)
                    ON UPDATE RESTRICT ON DELETE RESTRICT,
    target_reply_id INTEGER REFERENCES replies(reply_id)
                    ON UPDATE RESTRICT ON DELETE RESTRICT,
    reason          TEXT,
    created_at      TEXT NOT NULL,
    CHECK (
        (action IN ('user_disable', 'user_enable')
             AND target_user_id IS NOT NULL AND target_board_id IS NULL
             AND target_post_id IS NULL AND target_reply_id IS NULL AND reason IS NULL)
        OR (action IN ('board_create', 'board_update', 'board_disable', 'board_enable')
             AND target_board_id IS NOT NULL AND target_user_id IS NULL
             AND target_post_id IS NULL AND target_reply_id IS NULL AND reason IS NULL)
        OR (action IN ('moderator_grant', 'moderator_revoke')
             AND target_user_id IS NOT NULL AND target_board_id IS NOT NULL
             AND target_post_id IS NULL AND target_reply_id IS NULL AND reason IS NULL)
        OR (action = 'post_remove'
             AND target_post_id IS NOT NULL AND target_user_id IS NULL
             AND target_board_id IS NULL AND target_reply_id IS NULL
             AND reason IS NOT NULL AND length(trim(reason)) BETWEEN 1 AND 200)
        OR (action = 'reply_remove'
             AND target_reply_id IS NOT NULL AND target_user_id IS NULL
             AND target_board_id IS NULL AND target_post_id IS NULL
             AND reason IS NOT NULL AND length(trim(reason)) BETWEEN 1 AND 200)
        OR (action IN ('post_pin', 'post_unpin', 'post_feature', 'post_unfeature')
             AND target_post_id IS NOT NULL AND target_user_id IS NULL
             AND target_board_id IS NULL AND target_reply_id IS NULL AND reason IS NULL)
    )
);

-- 十二个显式索引（唯一约束与授权联合主键的索引由 SQLite 自动维护）
CREATE INDEX IF NOT EXISTS idx_users_role ON users(role_id);
CREATE INDEX IF NOT EXISTS idx_posts_board_list
    ON posts(board_id, status, is_pinned DESC, created_at DESC, post_id DESC);
CREATE INDEX IF NOT EXISTS idx_posts_search_time
    ON posts(status, created_at DESC, post_id DESC);
CREATE INDEX IF NOT EXISTS idx_posts_author ON posts(author_id);
CREATE INDEX IF NOT EXISTS idx_replies_post_list
    ON replies(post_id, status, created_at, reply_id);
CREATE INDEX IF NOT EXISTS idx_replies_author ON replies(author_id);
CREATE INDEX IF NOT EXISTS idx_board_moderators_board ON board_moderators(board_id, user_id);
CREATE INDEX IF NOT EXISTS idx_logs_actor_time
    ON moderation_logs(actor_id, created_at DESC, log_id DESC);
CREATE INDEX IF NOT EXISTS idx_logs_target_user ON moderation_logs(target_user_id);
CREATE INDEX IF NOT EXISTS idx_logs_target_board ON moderation_logs(target_board_id);
CREATE INDEX IF NOT EXISTS idx_logs_target_post ON moderation_logs(target_post_id);
CREATE INDEX IF NOT EXISTS idx_logs_target_reply ON moderation_logs(target_reply_id);
