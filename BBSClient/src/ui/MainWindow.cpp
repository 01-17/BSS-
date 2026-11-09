#include "ui/MainWindow.h"

#include "net/ApiClient.h"
#include "session/SessionManager.h"
#include "ui/BoardAdminPage.h"
#include "ui/BoardListPage.h"
#include "ui/Dialogs.h"
#include "ui/LoginPage.h"
#include "ui/ModerationPage.h"
#include "ui/ModeratorPage.h"
#include "ui/PostDetailPage.h"
#include "ui/PostEditorPage.h"
#include "ui/PostListPage.h"
#include "ui/ProfilePage.h"
#include "ui/RegisterPage.h"
#include "ui/SearchPage.h"
#include "ui/Theme.h"
#include "ui/UserAdminPage.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace bbs {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    m_api = new ApiClient(this);
    m_session = new SessionManager(m_api, this);

    PageContext context;
    context.api = m_api;
    context.session = m_session;
    context.nav = this;

    buildUi();

    m_loginPage = new LoginPage(context, this);
    m_registerPage = new RegisterPage(context, this);
    m_boardListPage = new BoardListPage(context, this);
    m_postListPage = new PostListPage(context, this);
    m_postDetailPage = new PostDetailPage(context, this);
    m_postEditorPage = new PostEditorPage(context, this);
    m_searchPage = new SearchPage(context, this);
    m_profilePage = new ProfilePage(context, this);
    m_userAdminPage = new UserAdminPage(context, this);
    m_boardAdminPage = new BoardAdminPage(context, this);
    m_moderatorPage = new ModeratorPage(context, this);
    m_moderationPage = new ModerationPage(context, this);

    QWidget* pages[] = { m_loginPage,     m_registerPage,   m_boardListPage, m_postListPage,
                         m_postDetailPage, m_postEditorPage, m_searchPage,   m_profilePage,
                         m_userAdminPage, m_boardAdminPage,  m_moderatorPage, m_moderationPage };
    for (QWidget* page : pages)
        m_stack->addWidget(page);

    connect(m_api, &ApiClient::sessionExpired, this, [this]() {
        // 401：令牌由 ApiClient 清空，这里清用户资料并强制回登录页
        handleSessionCleared(QStringLiteral("登录状态已失效，请重新登录。"));
    });
    connect(m_session, &SessionManager::signedOut, this, &MainWindow::refreshNavBar);
    connect(m_session, &SessionManager::profileChanged, this, &MainWindow::refreshNavBar);
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi()
{
    setWindowTitle(QStringLiteral("BBS 论坛客户端"));
    resize(1120, 760);
    setMinimumSize(960, 640);

    QWidget* central = new QWidget(this);
    QVBoxLayout* root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    buildNavBar();
    root->addWidget(m_navBar);

    m_stack = new QStackedWidget(central);
    root->addWidget(m_stack, 1);

    QFrame* statusBar = new QFrame(central);
    statusBar->setObjectName(QStringLiteral("statusBar"));
    QHBoxLayout* statusLayout = new QHBoxLayout(statusBar);
    statusLayout->setContentsMargins(16, 6, 16, 6);
    m_statusLabel = new QLabel(QStringLiteral("就绪"), statusBar);
    m_statusLabel->setObjectName(QStringLiteral("statusText"));
    statusLayout->addWidget(m_statusLabel);
    statusLayout->addStretch(1);
    QLabel* apiLabel =
        new QLabel(QStringLiteral("服务端 %1").arg(m_api->baseUrl()), statusBar);
    apiLabel->setObjectName(QStringLiteral("statusText"));
    statusLayout->addWidget(apiLabel);
    root->addWidget(statusBar);

    setCentralWidget(central);
}

void MainWindow::buildNavBar()
{
    m_navBar = new QFrame(this);
    m_navBar->setObjectName(QStringLiteral("navBar"));
    QHBoxLayout* layout = new QHBoxLayout(m_navBar);
    layout->setContentsMargins(16, 10, 16, 10);
    layout->setSpacing(8);

    QLabel* brand = new QLabel(QStringLiteral("BBS 论坛"), m_navBar);
    brand->setObjectName(QStringLiteral("brandLabel"));
    layout->addWidget(brand);
    layout->addSpacing(8);

    m_backButton = theme::normalButton(QStringLiteral("← 返回"), m_navBar);
    m_backButton->setToolTip(QStringLiteral("返回上一页"));
    layout->addWidget(m_backButton);

    m_boardsButton = theme::navButton(QStringLiteral("版块"), m_navBar);
    m_searchButton = theme::navButton(QStringLiteral("搜索"), m_navBar);
    m_profileButton = theme::navButton(QStringLiteral("个人中心"), m_navBar);
    m_userAdminButton = theme::navButton(QStringLiteral("账号管理"), m_navBar);
    m_boardAdminButton = theme::navButton(QStringLiteral("版块管理"), m_navBar);
    m_moderatorButton = theme::navButton(QStringLiteral("版主授权"), m_navBar);

    layout->addWidget(m_boardsButton);
    layout->addWidget(m_searchButton);
    layout->addWidget(m_profileButton);
    layout->addWidget(m_userAdminButton);
    layout->addWidget(m_boardAdminButton);
    layout->addWidget(m_moderatorButton);

    layout->addStretch(1);

    m_userLabel = new QLabel(QString(), m_navBar);
    m_userLabel->setObjectName(QStringLiteral("metaText"));
    layout->addWidget(m_userLabel);

    m_logoutButton = theme::normalButton(QStringLiteral("退出登录"), m_navBar);
    layout->addWidget(m_logoutButton);

    connect(m_backButton, &QPushButton::clicked, this, &MainWindow::goBack);
    connect(m_boardsButton, &QPushButton::clicked, this, &MainWindow::showBoardList);
    connect(m_searchButton, &QPushButton::clicked, this, &MainWindow::showSearch);
    connect(m_profileButton, &QPushButton::clicked, this, &MainWindow::showProfile);
    connect(m_userAdminButton, &QPushButton::clicked, this, &MainWindow::showUserAdmin);
    connect(m_boardAdminButton, &QPushButton::clicked, this, &MainWindow::showBoardAdmin);
    connect(m_moderatorButton, &QPushButton::clicked, this, &MainWindow::showModerator);
    connect(m_logoutButton, &QPushButton::clicked, this, [this]() {
        if (!m_session->isLoggedIn()) {
            showLogin();
            return;
        }
        if (!dialogs::confirm(this, QStringLiteral("退出登录"),
                              QStringLiteral("确认退出当前账号？本地令牌将立即清除。"),
                              QStringLiteral("退出登录")))
            return;

        m_api->logout(this, [this](const ApiResult&) {
            // 无论服务端返回如何，本地会话必须清空
            handleSessionCleared(QStringLiteral("已退出登录。"));
        });
    });
}

void MainWindow::start()
{
    if (m_session->isLoggedIn())
        showBoardList();
    else
        showLogin();
}

void MainWindow::switchTo(QWidget* page, bool recordHistory)
{
    if (!page)
        return;

    QWidget* current = m_stack->currentWidget();
    if (recordHistory && current && current != page && current != m_loginPage
        && current != m_registerPage) {
        m_history.append(current);
    }

    m_stack->setCurrentWidget(page);
    // 请求上下文绑定：切页后旧页面的迟到响应由 ApiClient 丢弃
    m_api->setActivePage(page);
    refreshNavBar();

    if (Page* typed = qobject_cast<Page*>(page))
        typed->onEnter();
}

void MainWindow::showLogin(const QString& hint)
{
    clearHistory();
    m_stack->setCurrentWidget(m_loginPage);
    m_api->setActivePage(m_loginPage);
    refreshNavBar();
    if (!hint.isEmpty())
        m_loginPage->showHint(hint);
    m_loginPage->onEnter();
}

void MainWindow::showRegister()
{
    switchTo(m_registerPage, true);
}

void MainWindow::showBoardList()
{
    if (!m_session->isLoggedIn()) {
        showLogin(QStringLiteral("请先登录后再浏览版块。"));
        return;
    }
    switchTo(m_boardListPage, true);
}

void MainWindow::showPostList(qint64 boardId, const QString& boardName)
{
    if (!m_session->isLoggedIn()) {
        showLogin(QStringLiteral("请先登录后再浏览帖子。"));
        return;
    }
    m_postListPage->setBoard(boardId, boardName);
    switchTo(m_postListPage, true);
}

void MainWindow::showPostDetail(qint64 postId)
{
    if (!m_session->isLoggedIn()) {
        showLogin(QStringLiteral("请先登录后再查看帖子。"));
        return;
    }
    m_postDetailPage->setPost(postId);
    switchTo(m_postDetailPage, true);
}

void MainWindow::showPostEditor(qint64 boardId, qint64 postId)
{
    if (!m_session->isLoggedIn()) {
        showLogin(QStringLiteral("请先登录后再发布内容。"));
        return;
    }
    m_postEditorPage->setTarget(boardId, postId);
    switchTo(m_postEditorPage, true);
}

void MainWindow::showSearch()
{
    if (!m_session->isLoggedIn()) {
        showLogin(QStringLiteral("请先登录后再使用搜索。"));
        return;
    }
    switchTo(m_searchPage, true);
}

void MainWindow::showProfile()
{
    if (!m_session->isLoggedIn()) {
        showLogin(QStringLiteral("请先登录后再进入个人中心。"));
        return;
    }
    switchTo(m_profilePage, true);
}

void MainWindow::showUserAdmin()
{
    if (!m_session->isLoggedIn()) {
        showLogin();
        return;
    }
    if (!m_session->isAdmin()) {
        notify(QStringLiteral("仅管理员可访问账号管理"));
        return;
    }
    switchTo(m_userAdminPage, true);
}

void MainWindow::showBoardAdmin()
{
    if (!m_session->isLoggedIn()) {
        showLogin();
        return;
    }
    if (!m_session->isAdmin()) {
        notify(QStringLiteral("仅管理员可访问版块管理"));
        return;
    }
    switchTo(m_boardAdminPage, true);
}

void MainWindow::showModerator()
{
    if (!m_session->isLoggedIn()) {
        showLogin();
        return;
    }
    if (!m_session->isAdmin()) {
        notify(QStringLiteral("仅管理员可进行版主授权"));
        return;
    }
    switchTo(m_moderatorPage, true);
}

void MainWindow::showModeration(qint64 postId)
{
    if (!m_session->isLoggedIn()) {
        showLogin();
        return;
    }
    if (!m_session->isAdmin() && m_session->assignments().isEmpty()) {
        notify(QStringLiteral("当前账号无内容处置权限"));
        return;
    }
    m_moderationPage->setPostId(postId);
    switchTo(m_moderationPage, true);
}

void MainWindow::goBack()
{
    // 回到上一层页面：出栈即切换，不再入栈，避免来回堆叠
    while (!m_history.isEmpty()) {
        QWidget* previous = m_history.takeLast();
        if (previous == m_stack->currentWidget())
            continue;
        m_switching = true;
        m_stack->setCurrentWidget(previous);
        m_api->setActivePage(previous);
        m_switching = false;
        refreshNavBar();
        if (Page* typed = qobject_cast<Page*>(previous))
            typed->onEnter();
        return;
    }
    showBoardList();
}

void MainWindow::notify(const QString& text)
{
    if (!m_statusLabel)
        return;
    m_statusLabel->setText(text);
    QTimer::singleShot(4000, this, [this]() {
        if (m_statusLabel)
            m_statusLabel->setText(QStringLiteral("就绪"));
    });
}

void MainWindow::refreshNavBar()
{
    const bool loggedIn = m_session->isLoggedIn();
    const bool admin = loggedIn && m_session->isAdmin();
    const bool moderator = loggedIn && !m_session->assignments().isEmpty();

    m_boardsButton->setVisible(loggedIn);
    m_searchButton->setVisible(loggedIn);
    m_profileButton->setVisible(loggedIn);
    m_userAdminButton->setVisible(admin);
    m_boardAdminButton->setVisible(admin);
    m_moderatorButton->setVisible(admin);
    m_logoutButton->setVisible(loggedIn);
    m_backButton->setVisible(loggedIn);
    m_backButton->setEnabled(loggedIn && !m_history.isEmpty());
    m_navBar->setVisible(true);

    if (loggedIn) {
        QString label = QStringLiteral("%1（%2）").arg(m_session->nickname(), m_session->roleCode());
        if (!admin && moderator)
            label += QStringLiteral(" · 版主");
        m_userLabel->setText(label);
        m_userLabel->setVisible(true);
    } else {
        m_userLabel->clear();
        m_userLabel->setVisible(false);
    }
}

void MainWindow::clearHistory()
{
    m_history.clear();
}

void MainWindow::handleSessionCleared(const QString& hint)
{
    m_session->clear();
    m_api->clearToken();
    clearHistory();
    showLogin(hint);
}

void MainWindow::ensureLoggedIn()
{
    if (!m_session->isLoggedIn())
        showLogin();
}

} // namespace bbs
