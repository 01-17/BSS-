#pragma once

// P02 注册页
// 注册成功后自动用同一凭据登录（服务端注册不返回令牌），进入主框架；
// 自动登录失败时回到登录页并预填用户名，提示手动登录。

#include "ui/Page.h"

class QLabel;
class QLineEdit;
class QPushButton;

namespace bbs {

class RegisterPage : public Page
{
    Q_OBJECT
public:
    explicit RegisterPage(const PageContext& context, QWidget* parent = nullptr);

    void onEnter() override;

private:
    void submit();
    void setBusy(bool busy);
    void showError(const QString& text);

    QLineEdit* m_usernameEdit = nullptr;
    QLineEdit* m_passwordEdit = nullptr;
    QLineEdit* m_confirmEdit = nullptr;
    QLineEdit* m_nicknameEdit = nullptr;
    QPushButton* m_submitButton = nullptr;
    QPushButton* m_backButton = nullptr;
    QLabel* m_errorLabel = nullptr;
};

} // namespace bbs
