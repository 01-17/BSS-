-- 演示种子数据（幂等）：2 角色、5 账号、3 版块、5 帖子、6 回复、1 项授权、7 条管理记录
-- 依据：Day 4《类图与数据库设计说明书》表 5-2 课程演示账号
-- 预置口令（PBKDF2-HMAC-SHA256，盐 16 字节，迭代 600000 次，摘要 32 字节）：
--   admin07     / AdminDemo2026!     管理员，正常
--   moderator07 / ModDemo2026!       技术交流版块版主，正常
--   alice07     / AliceDemo2026!     普通用户，正常
--   bob07       / BobDemo2026!       普通用户，正常
--   disabled07  / DisabledDemo2026!  普通用户，停用

INSERT OR IGNORE INTO roles(role_id, role_code, role_name) VALUES
    (1, 'member', '普通用户'),
    (2, 'admin',  '管理员');

INSERT OR IGNORE INTO users(user_id, username, password_hash, password_salt,
                            password_iterations, nickname, role_id, status,
                            session_version, registered_at) VALUES
    (1, 'admin07',
     'eb403011bc9f9c11a85be22c29bce571744d062c853cade473272fcd2c735ac1',
     '4274c6976053a71048500858e72e672b', 600000, '系统管理员', 2, 1, 0,
     '2026-11-01 08:00:00'),
    (2, 'moderator07',
     'd14549e95bdc473cdc5287ebedae5af363f73ea1cf635598b9a4d355a535dea6',
     '83abf16780d37121f7707ef482788e4a', 600000, '技术版主', 1, 1, 0,
     '2026-11-01 08:10:00'),
    (3, 'alice07',
     'c090dbc5f2f12e15c7c7d263bb91476d3c41fe008996aa41e32ec3e9541bf15c',
     '8d9620142096a92b072a6d1913fc66e7', 600000, '爱丽丝', 1, 1, 0,
     '2026-11-01 08:20:00'),
    (4, 'bob07',
     '13173a45209b39e10da48f02592bc92c56b5701e14f96fd841c003e5eaf3be45',
     '7fed47b96bf66f3450f4530e3f57e2e6', 600000, '鲍勃', 1, 1, 0,
     '2026-11-01 08:30:00'),
    (5, 'disabled07',
     'cc4d4aaf4aa55dd5e48cbb8410e1344ee192029994965e624df0cb541c6ab742',
     '232f9720cf159e468ce198d27f20632f', 600000, '停用账号', 1, 0, 1,
     '2026-11-01 08:40:00');

INSERT OR IGNORE INTO boards(board_id, name, description, status, created_at, updated_at) VALUES
    (1, '综合讨论', '校园生活与日常交流', 1,
     '2026-11-01 09:00:00', '2026-11-01 09:00:00'),
    (2, '技术交流', '编程、课程设计与技术分享', 1,
     '2026-11-01 09:05:00', '2026-11-01 09:05:00'),
    (3, '站务公告', '论坛规则与历史公告', 0,
     '2026-11-01 09:10:00', '2026-11-04 18:00:00');

INSERT OR IGNORE INTO board_moderators(user_id, board_id, assigned_at) VALUES
    (2, 2, '2026-11-01 09:30:00');

INSERT OR IGNORE INTO posts(post_id, board_id, author_id, title, content, status,
                            is_pinned, is_featured, created_at, updated_at) VALUES
    (1, 1, 3, '新人报到：第一次使用 BBS 论坛',
     '大家好，我是爱丽丝，很高兴加入论坛，请多关照。',
     1, 1, 0, '2026-11-02 10:00:00', '2026-11-03 09:00:00'),
    (2, 1, 4, '周末一起去图书馆自习吗',
     '本周六上午图书馆四楼，有一起自习的同学吗？',
     1, 0, 0, '2026-11-02 11:00:00', '2026-11-02 11:00:00'),
    (3, 2, 2, 'C++ 服务端线程模型小结',
     '本次课程设计采用 thread-per-connection 收发报文，业务与数据库操作由单个工作线程串行执行。',
     1, 0, 1, '2026-11-03 14:00:00', '2026-11-04 10:00:00'),
    (4, 2, 4, 'SQLite 事务与锁等待的疑问',
     'BEGIN IMMEDIATE 与 BEGIN 的区别是什么？锁等待超时应该怎么处理？',
     1, 0, 0, '2026-11-04 09:00:00', '2026-11-04 09:00:00'),
    (5, 2, 3, '重复灌水内容示例',
     '本楼层为演示用的违规内容，已被版主移除。',
     0, 0, 0, '2026-11-04 15:00:00', '2026-11-04 16:00:00');

INSERT OR IGNORE INTO replies(reply_id, post_id, author_id, content, status,
                             created_at, updated_at) VALUES
    (1, 1, 2, '欢迎欢迎，技术问题可以到技术交流版块提问。', 1,
     '2026-11-02 10:30:00', '2026-11-02 10:30:00'),
    (2, 1, 4, '欢迎新同学。', 1, '2026-11-02 10:40:00', '2026-11-02 10:40:00'),
    (3, 2, 3, '我可以参加，早上八点半到。', 1,
     '2026-11-02 12:00:00', '2026-11-02 12:00:00'),
    (4, 3, 4, '单线程串行确实能避免事务被打断。', 1,
     '2026-11-03 15:00:00', '2026-11-03 15:00:00'),
    (5, 3, 1, '记得补充锁等待 2000 毫秒的配置说明。', 1,
     '2026-11-04 08:30:00', '2026-11-04 08:30:00'),
    (6, 4, 4, '自问自答占位内容，作者已删除。', 0,
     '2026-11-04 09:30:00', '2026-11-04 09:40:00');

INSERT OR IGNORE INTO moderation_logs(log_id, actor_id, action, target_user_id,
                                      target_board_id, target_post_id, target_reply_id,
                                      reason, created_at) VALUES
    (1, 1, 'board_create', NULL, 1, NULL, NULL, NULL, '2026-11-01 09:00:00'),
    (2, 1, 'board_create', NULL, 2, NULL, NULL, NULL, '2026-11-01 09:05:00'),
    (3, 1, 'board_create', NULL, 3, NULL, NULL, NULL, '2026-11-01 09:10:00'),
    (4, 1, 'moderator_grant', 2, 2, NULL, NULL, NULL, '2026-11-01 09:30:00'),
    (5, 1, 'board_disable', NULL, 3, NULL, NULL, NULL, '2026-11-04 18:00:00'),
    (6, 1, 'user_disable', 5, NULL, NULL, NULL, NULL, '2026-11-04 18:05:00'),
    (7, 2, 'post_remove', NULL, NULL, 5, NULL,
     '重复灌水，与版块主题无关。', '2026-11-04 16:00:00');
