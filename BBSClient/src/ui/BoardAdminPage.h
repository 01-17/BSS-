#pragma once

// P11 版块管理（仅管理员）
// 数据源：GET /boards（返回全部版块，含已停用）
// 操作：POST /boards（name 必填、description 可选）、PUT /boards/{board_id}（name+description）、
//       PUT /boards/{board_id}/status（body 仅 {status}）。
// 服务端三接口均 requireAdmin；停用版块后该版块下禁止发帖（客户端据此禁用"发帖"入口）。

#include "ui/Page.h"

#include <QVector>

class QLabel;
class QLineEdit;
class QPushButton;
class QTextEdit;
class QVBoxLayout;
class QWidget;

namespace bbs {

class StateView;

class BoardAdminPage : public Page
{
    Q_OBJECT
public:
    explicit BoardAdminPage(const PageContext& context, QWidget* parent = nullptr);

    void onEnter() override;

private:
    void load();
    void rebuild();
    void resetForm();
    void submitForm();
    void startEdit(const BoardView& board);
    void changeStatus(qint64 boardId, int status, const QString& name);

    QVector<BoardView> m_boards;
    qint64 m_editingBoardId = -1;

    QLineEdit* m_nameEdit = nullptr;
    QTextEdit* m_descEdit = nullptr;
    QLabel* m_formTitle = nullptr;
    QLabel* m_formError = nullptr;
    QPushButton* m_submitButton = nullptr;
    QPushButton* m_cancelEditButton = nullptr;
    QLabel* m_summaryLabel = nullptr;
    StateView* m_state = nullptr;
    QWidget* m_listContainer = nullptr;
    QVBoxLayout* m_listLayout = nullptr;
};

} // namespace bbs
