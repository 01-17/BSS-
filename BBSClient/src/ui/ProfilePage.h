#pragma once

// P09 个人中心
// 数据源：GET /users/me（含 moderator_assignments，用于展示"我管理的版块"）。
// 操作：修改昵称（PUT /users/me）、修改密码（PUT /users/me/password）、退出登录。

#include "ui/Page.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QVBoxLayout;
class QWidget;

namespace bbs {

class StateView;

class ProfilePage : public Page
{
    Q_OBJECT
public:
    explicit ProfilePage(const PageContext& context, QWidget* parent = nullptr);

    void onEnter() override;

private:
    void load();
    void render();
    void submitNickname();
    void submitPassword();
    void logout();

    QLabel* m_usernameLabel = nullptr;
    QLabel* m_roleLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_registeredLabel = nullptr;
    QLabel* m_boardsLabel = nullptr;

    QLineEdit* m_nicknameEdit = nullptr;
    QPushButton* m_nicknameButton = nullptr;
    QLabel* m_nicknameError = nullptr;

    QLineEdit* m_currentPasswordEdit = nullptr;
    QLineEdit* m_newPasswordEdit = nullptr;
    QLineEdit* m_confirmPasswordEdit = nullptr;
    QPushButton* m_passwordButton = nullptr;
    QLabel* m_passwordError = nullptr;

    StateView* m_state = nullptr;
    QWidget* m_content = nullptr;
};

} // namespace bbs
