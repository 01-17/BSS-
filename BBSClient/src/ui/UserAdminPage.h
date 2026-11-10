#pragma once

// P10 账号管理（仅管理员）
// 数据源：GET /admin/users；操作：PUT /admin/users/{user_id}/status（body 仅 {status}）。
// 服务端禁止管理管理员账号（403 "不能管理管理员账号"），因此管理员行禁用启停按钮；
// 停用/启用属破坏性变更，必须二次确认，且确认框默认焦点在"取消"。

#include "ui/Page.h"

#include <QVector>

class QLabel;
class QLineEdit;
class QVBoxLayout;
class QWidget;

namespace bbs {

class StateView;

class UserAdminPage : public Page
{
    Q_OBJECT
public:
    explicit UserAdminPage(const PageContext& context, QWidget* parent = nullptr);

    void onEnter() override;

private:
    void load();
    void rebuild();
    void setStatus(qint64 userId, int status, const QString& nickname);

    QVector<UserProfile> m_users;
    QLineEdit* m_filterEdit = nullptr;
    QLabel* m_summaryLabel = nullptr;
    StateView* m_state = nullptr;
    QWidget* m_listContainer = nullptr;
    QVBoxLayout* m_listLayout = nullptr;
};

} // namespace bbs
