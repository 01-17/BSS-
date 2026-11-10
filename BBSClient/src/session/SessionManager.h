#pragma once

// 会话层：令牌只存在于内存（不落盘、不写日志、不进 QSettings），
// 同时保存当前登录用户资料，并集中提供"是否管理员 / 是否能管理某版块"的判定。
// 判定依据与服务端一致：
//   - 管理员：role_code == "admin"
//   - 版主：moderator_assignments 中存在 board_id == 目标版块
// 服务端不区分"版主角色码"，因此前端绝不能用角色名硬编码版主身份。

#include <QObject>
#include <QString>

#include "model/Dtos.h"

namespace bbs {

class ApiClient;

class SessionManager : public QObject
{
    Q_OBJECT
public:
    explicit SessionManager(ApiClient* api, QObject* parent = nullptr);

    // 登录成功：写入令牌与用户资料（令牌一次性进入 ApiClient，不在本类重复暴露 getter 以外的用途）
    void applyLogin(const LoginResult& result);

    // 刷新资料（GET /users/me 或资料修改后回填）
    void updateProfile(const UserProfile& profile);

    // 退出登录 / 令牌失效：清空令牌与全部用户信息
    void clear();

    bool isLoggedIn() const { return !m_token.isEmpty(); }
    QString token() const { return m_token; }

    const UserProfile& profile() const { return m_profile; }

    qint64 userId() const { return m_profile.userId; }
    QString nickname() const { return m_profile.nickname; }
    QString roleCode() const { return m_profile.roleCode; }

    bool isAdmin() const;
    bool isMember() const;

    // 是否可以对该版块执行内容处置（管理员或该版块版主）
    bool canModerate(qint64 boardId) const;

    // 版主授权列表（用于 P09 展示"我管理的版块"）
    QVector<ModeratorAssignment> assignments() const { return m_profile.assignments; }

    // 已登录但资料为空（例如登录响应未带完整 profile 时的兜底提示）
    bool profileLoaded() const { return m_profile.userId > 0; }

signals:
    // 资料发生变化（登录、刷新、修改昵称后）
    void profileChanged();
    // 会话被清空（主动退出或 401）
    void signedOut();

private:
    ApiClient* m_api = nullptr;
    QString m_token;
    UserProfile m_profile;
};

} // namespace bbs
