#pragma once

// P03 主框架壳：唯一导航实现（Navigator）。
// 职责：
//  - 控件树：顶部导航条（品牌、返回、版块/搜索/个人中心、管理员三项入口、当前用户与退出）+ 页面栈；
//  - 页面实例一次创建、复用，切换时调用 Page::onEnter() 触发本页加载；
//  - 简易页面栈支撑 goBack()，栈空时回落到版块列表；
//  - 令牌失效（ApiClient::sessionExpired）或主动退出时清空会话并回登录页；
//  - 管理员/版主入口按会话判定动态显隐（role_code / moderator_assignments），不硬编码角色名。

#include <QMainWindow>
#include <QVector>

#include "ui/Page.h"

class QLabel;
class QPushButton;
class QStackedWidget;

namespace bbs {

class ApiClient;
class SessionManager;

class BoardAdminPage;
class BoardListPage;
class LoginPage;
class ModerationPage;
class ModeratorPage;
class PostDetailPage;
class PostEditorPage;
class PostListPage;
class ProfilePage;
class RegisterPage;
class SearchPage;
class UserAdminPage;

class MainWindow : public QMainWindow, public Navigator
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // 启动：未登录时进入登录页
    void start();

    // ---- Navigator ----
    void showLogin(const QString& hint = QString()) override;
    void showRegister() override;
    void showBoardList() override;
    void showPostList(qint64 boardId, const QString& boardName) override;
    void showPostDetail(qint64 postId) override;
    void showPostEditor(qint64 boardId, qint64 postId) override;
    void showSearch() override;
    void showProfile() override;
    void showUserAdmin() override;
    void showBoardAdmin() override;
    void showModerator() override;
    void showModeration(qint64 postId) override;
    void goBack() override;
    void notify(const QString& text) override;

private:
    void buildUi();
    void buildNavBar();

    // 切换到指定页面；recordHistory=false 用于返回操作与登录跳转
    void switchTo(QWidget* page, bool recordHistory);
    void refreshNavBar();
    void clearHistory();

    // 会话层联动
    void handleSessionCleared(const QString& hint);
    void ensureLoggedIn();

    ApiClient* m_api = nullptr;
    SessionManager* m_session = nullptr;

    QStackedWidget* m_stack = nullptr;
    QWidget* m_navBar = nullptr;
    QPushButton* m_backButton = nullptr;
    QPushButton* m_boardsButton = nullptr;
    QPushButton* m_searchButton = nullptr;
    QPushButton* m_profileButton = nullptr;
    QPushButton* m_userAdminButton = nullptr;
    QPushButton* m_boardAdminButton = nullptr;
    QPushButton* m_moderatorButton = nullptr;
    QPushButton* m_logoutButton = nullptr;
    QLabel* m_userLabel = nullptr;
    QLabel* m_statusLabel = nullptr;

    LoginPage* m_loginPage = nullptr;
    RegisterPage* m_registerPage = nullptr;
    BoardListPage* m_boardListPage = nullptr;
    PostListPage* m_postListPage = nullptr;
    PostDetailPage* m_postDetailPage = nullptr;
    PostEditorPage* m_postEditorPage = nullptr;
    SearchPage* m_searchPage = nullptr;
    ProfilePage* m_profilePage = nullptr;
    UserAdminPage* m_userAdminPage = nullptr;
    BoardAdminPage* m_boardAdminPage = nullptr;
    ModeratorPage* m_moderatorPage = nullptr;
    ModerationPage* m_moderationPage = nullptr;

    QVector<QWidget*> m_history;
    bool m_switching = false;
};

} // namespace bbs
