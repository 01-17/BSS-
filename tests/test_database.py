"""第7组 BBS 论坛数据库回归测试（Python 3 标准库，无需第三方依赖）。

运行：在包含 BBS论坛/ 和 tests/ 的目录执行 python -m unittest discover -s tests -v。
测试使用独立的内存数据库，不连接或修改运行中的 bbs.db。
"""
from pathlib import Path
import sqlite3
import unittest

ROOT = Path(__file__).resolve().parents[1]
SQL_DIR = ROOT / 'BBS论坛' / 'sql'
SCHEMA = (SQL_DIR / 'bbs_schema.sql').read_text(encoding='utf-8-sig')
SEED = (SQL_DIR / 'bbs_seed.sql').read_text(encoding='utf-8-sig')
REPO_SOURCE = (ROOT / 'BBS论坛' / 'dao' / 'PostRepository.cpp').read_text(encoding='utf-8-sig')
SERVICE_SOURCE = (ROOT / 'BBS论坛' / 'service' / 'PostService.cpp').read_text(encoding='utf-8-sig')

class DatabaseRegressionTests(unittest.TestCase):
    def setUp(self):
        self.db = sqlite3.connect(':memory:')
        self.db.execute('PRAGMA foreign_keys = ON')
        self.db.executescript(SCHEMA)
        self.db.executescript(SEED)

    def tearDown(self):
        self.db.close()

    def count(self, table):
        return self.db.execute('SELECT COUNT(*) FROM ' + table).fetchone()[0]

    def test_initial_tables_indexes_and_integrity(self):
        expected = {
            'roles': 2, 'users': 5, 'boards': 3, 'posts': 5,
            'replies': 6, 'board_moderators': 1, 'moderation_logs': 7,
        }
        self.assertEqual({t: self.count(t) for t in expected}, expected)
        tables = self.db.execute("SELECT name FROM sqlite_master WHERE type='table'").fetchall()
        self.assertEqual({x[0] for x in tables}, set(expected))
        explicit_indexes = self.db.execute(
            "SELECT name FROM sqlite_master WHERE type='index' AND name NOT LIKE 'sqlite_autoindex_%'"
        ).fetchall()
        self.assertEqual(len(explicit_indexes), 12)
        self.assertEqual(self.db.execute('PRAGMA integrity_check').fetchone()[0], 'ok')
        self.assertEqual(self.db.execute('PRAGMA foreign_key_check').fetchall(), [])

    def test_restart_seed_keeps_revoked_moderator_revoked(self):
        self.assertEqual(self.count('board_moderators'), 1)
        # 模拟管理员撤销授权并留下同事务日志。
        self.db.execute('DELETE FROM board_moderators WHERE user_id=2 AND board_id=2')
        self.db.execute('''INSERT INTO moderation_logs
            (actor_id, action, target_user_id, target_board_id, created_at)
            VALUES (1, 'moderator_revoke', 2, 2, '2026-11-10 09:00:00')''')
        self.db.commit()
        self.db.executescript(SEED)  # 模拟服务端重启再次执行脚本。
        self.assertEqual(self.count('board_moderators'), 0,
                         '旧版本会错误地在重启后恢复已撤销授权')
        self.assertEqual(self.count('moderation_logs'), 8)

    def test_repeat_seed_does_not_duplicate_business_records(self):
        tables = ['users', 'boards', 'posts', 'replies', 'board_moderators', 'moderation_logs']
        before = [self.count(t) for t in tables]
        self.db.executescript(SEED)
        self.assertEqual([self.count(t) for t in tables], before)

    def test_invalid_time_rejected_by_schema(self):
        with self.assertRaises(sqlite3.IntegrityError):
            self.db.execute('''INSERT INTO boards
                (name, description, created_at, updated_at)
                VALUES ('非法时间测试', '', 'invalid-time', 'invalid-time')''')

    def test_foreign_key_integrity(self):
        with self.assertRaises(sqlite3.IntegrityError):
            self.db.execute('''INSERT INTO replies
                (post_id, author_id, content, created_at, updated_at)
                VALUES (99999, 3, '外键测试', '2026-11-10 09:00:00', '2026-11-10 09:00:00')''')

    def test_management_action_and_log_rollback_together(self):
        before = self.db.execute('SELECT status FROM posts WHERE post_id=1').fetchone()[0]
        count_before = self.count('moderation_logs')
        self.db.executescript('''CREATE TRIGGER reject_removal_log
            BEFORE INSERT ON moderation_logs WHEN NEW.action='post_remove'
            BEGIN SELECT RAISE(ABORT, 'simulated log failure'); END;''')
        with self.assertRaises(sqlite3.IntegrityError):
            try:
                self.db.execute('BEGIN IMMEDIATE')
                self.db.execute('UPDATE posts SET status=0 WHERE post_id=1 AND status=1')
                self.db.execute('''INSERT INTO moderation_logs
                    (actor_id, action, target_post_id, reason, created_at)
                    VALUES (1, 'post_remove', 1, '测试回滚', '2026-11-10 09:00:00')''')
                self.db.commit()
            except sqlite3.Error:
                self.db.rollback()
                raise
        self.assertEqual(self.db.execute('SELECT status FROM posts WHERE post_id=1').fetchone()[0], before)
        self.assertEqual(self.count('moderation_logs'), count_before)

    def test_own_post_edit_has_author_guard(self):
        self.assertIn('WHERE post_id = ? AND status = 1 AND author_id = ?', REPO_SOURCE)
        self.assertIn('permissions_.requireOwner(current.userId, record->authorId);', SERVICE_SOURCE)
        self.assertNotIn('if (!current.isAdmin())', SERVICE_SOURCE)
        # 与 Repository 使用相同的条件，验证管理员 ID 不等于作者时不会修改记录。
        sql = '''UPDATE posts SET title=?, content=?, updated_at=MAX(updated_at, ?)
                 WHERE post_id=? AND status=1 AND author_id=?'''
        self.db.execute(sql, ('越权标题', '越权内容', '2026-11-10 12:00:00', 1, 1))
        self.assertEqual(self.db.execute('SELECT title FROM posts WHERE post_id=1').fetchone()[0],
                         '新人报到：第一次使用 BBS 论坛')
        self.db.execute(sql, ('本人修改', '本人正文', '2026-11-10 12:00:00', 1, 3))
        self.assertEqual(self.db.execute('SELECT title FROM posts WHERE post_id=1').fetchone()[0],
                         '本人修改')

if __name__ == '__main__':
    unittest.main(verbosity=2)
