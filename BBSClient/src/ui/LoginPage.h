#pragma once

// P01 登录页
// 交互要点：
//  - 用户名/密码本地前置校验（口径同服务端），密码输入框遮蔽显示；
//  - 提交期间按钮禁用并改文案，防止重复提交；
//  - 5 秒超时：保留已输入的用户名与密码，仅提示失败原因，绝不自动重发；
//  - 登录成功后把令牌与用户资料写入 SessionManager（令牌仅内存）。

#include "ui/Page.h"

class QLabel;
class QLineEdit;
class QPushButton;

namespace bbs {

class LoginPage : public Page
{
    Q_OBJECT
public:
    explicit LoginPage(const PageContext& context, QWidget* parent = nullptr);

    void onEnter() override;
    // 由主框架在"注册成功/会话失效"时回填用户名与提示
    void presetUsername(const QString& username);
    void showHint(const QString& hint);

private:
    void submit();
    void setBusy(bool busy);

    QLineEdit* m_usernameEdit = nullptr;
    QLineEdit* m_passwordEdit = nullptr;
    QPushButton* m_loginButton = nullptr;
    QPushButton* m_registerButton = nullptr;
    QLabel* m_errorLabel = nullptr;
    QLabel* m_hintLabel = nullptr;
};

} // namespace bbs
