#pragma once

// P05 版块内帖子列表
// 数据源：GET /boards/{board_id}/posts（需登录，返回该版块全部未删除帖子）。
// 排序由服务端决定（置顶优先、其次更新时间倒序），前端不再二次排序，
// 避免与服务端顺序不一致造成"看起来漏帖"的误解。

#include "ui/Page.h"

#include <QVector>

class QLabel;
class QPushButton;
class QVBoxLayout;
class QWidget;

namespace bbs {

class StateView;

class PostListPage : public Page
{
    Q_OBJECT
public:
    explicit PostListPage(const PageContext& context, QWidget* parent = nullptr);

    void setBoard(qint64 boardId, const QString& boardName);
    void onEnter() override;

private:
    void load();
    void loadBoardStatus();
    void rebuildList(const QVector<PostSummary>& posts);
    void updatePostingAvailability();

    qint64 m_boardId = 0;
    QString m_boardName;
    bool m_boardOpen = true; // 未知状态时先乐观放行，服务端仍会兜底校验

    QLabel* m_titleLabel = nullptr;
    QLabel* m_hintLabel = nullptr;
    QPushButton* m_postButton = nullptr;
    QPushButton* m_refreshButton = nullptr;
    StateView* m_state = nullptr;
    QWidget* m_listContainer = nullptr;
    QVBoxLayout* m_listLayout = nullptr;
};

} // namespace bbs
