#pragma once

// 页面层公共契约：
//  - Navigator：所有页面只通过该接口跳转，页面之间不直接互相持有；
//  - PageContext：页面拿到的唯一依赖集合（网络、会话、导航）；
//  - Page：所有页面的基类，onEnter() 在页面被切换为当前页时调用，
//    用于触发本页数据加载；页面自身不缓存跨页状态，避免脏数据。

#include <QString>
#include <QWidget>

#include "model/Dtos.h"

namespace bbs {

class ApiClient;
class SessionManager;

class Navigator
{
public:
    virtual ~Navigator() = default;

    virtual void showLogin(const QString& hint = QString()) = 0;
    virtual void showRegister() = 0;

    virtual void showBoardList() = 0;
    virtual void showPostList(qint64 boardId, const QString& boardName) = 0;
    virtual void showPostDetail(qint64 postId) = 0;
    // postId == 0 表示新建，否则为编辑
    virtual void showPostEditor(qint64 boardId, qint64 postId) = 0;
    virtual void showSearch() = 0;

    virtual void showProfile() = 0;

    virtual void showUserAdmin() = 0;
    virtual void showBoardAdmin() = 0;
    virtual void showModerator() = 0;
    virtual void showModeration(qint64 postId) = 0;

    // 返回上一页（主框架维护一个简易页面栈）
    virtual void goBack() = 0;

    // 顶部状态条提示（不打断操作）
    virtual void notify(const QString& text) = 0;
};

struct PageContext
{
    ApiClient* api = nullptr;
    SessionManager* session = nullptr;
    Navigator* nav = nullptr;

    bool isValid() const { return api != nullptr && session != nullptr && nav != nullptr; }
};

class Page : public QWidget
{
    Q_OBJECT
public:
    explicit Page(const PageContext& context, QWidget* parent = nullptr)
        : QWidget(parent)
        , m_context(context)
    {
    }

    // 页面被激活时调用：发起数据加载或刷新
    virtual void onEnter() {}

protected:
    PageContext m_context;
};

} // namespace bbs
