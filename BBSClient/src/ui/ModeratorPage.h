#pragma once

// P12 版主授权（仅管理员）
// 数据源：GET /boards + GET /admin/users（后者每个账号带 moderator_assignments，
//         据此汇总出"某版块当前有哪些版主"，服务端未提供版主列表接口）。
// 操作：PUT /admin/boards/{board_id}/moderators/{user_id}   授予（无请求体）
//       DELETE /admin/boards/{board_id}/moderators/{user_id} 撤销（无请求体）
// 服务端约束：目标账号必须是启用状态的普通成员；管理员不接受版主授权；重复授予/撤销返回 409。

#include "ui/Page.h"

#include <QVector>

class QComboBox;
class QLabel;
class QVBoxLayout;
class QWidget;

namespace bbs {

class StateView;

class ModeratorPage : public Page
{
    Q_OBJECT
public:
    explicit ModeratorPage(const PageContext& context, QWidget* parent = nullptr);

    void onEnter() override;

private:
    void loadAll();
    void rebuildBoardCombo();
    void rebuildUserCombo();
    void rebuildAssignmentList();
    void grant();
    void revoke(qint64 userId, const QString& nickname);

    QVector<BoardView> m_boards;
    QVector<UserProfile> m_users;

    QComboBox* m_boardCombo = nullptr;
    QComboBox* m_userCombo = nullptr;
    QLabel* m_hintLabel = nullptr;
    QLabel* m_assignmentSummary = nullptr;
    StateView* m_state = nullptr;
    QWidget* m_assignmentContainer = nullptr;
    QVBoxLayout* m_assignmentLayout = nullptr;
};

} // namespace bbs
