#pragma once

// P04 版块列表
// 数据源：GET /boards（需登录，返回全部未删除版块，含已停用）。
// 交互：点击版块进入帖子列表；管理员额外提供"版块管理"入口（登录后 2 次点击可达）。

#include "ui/Page.h"

#include <QVector>

class QPushButton;
class QVBoxLayout;
class QWidget;

namespace bbs {

class StateView;

class BoardListPage : public Page
{
    Q_OBJECT
public:
    explicit BoardListPage(const PageContext& context, QWidget* parent = nullptr);

    void onEnter() override;

private:
    void load();
    void rebuildCards(const QVector<BoardView>& boards);
    void updateAdminEntry();

    StateView* m_state = nullptr;
    QWidget* m_listContainer = nullptr;
    QVBoxLayout* m_listLayout = nullptr;
    QPushButton* m_adminButton = nullptr;
};

} // namespace bbs
