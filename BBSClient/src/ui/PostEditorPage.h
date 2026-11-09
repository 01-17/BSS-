#pragma once

// P07 发帖 / 编辑帖子
// 新建：POST /boards/{board_id}/posts；编辑：PUT /posts/{post_id}。
// 约束（NFR-003）：提交中禁用按钮并锁定输入区；超时/失败保留全部输入且不自动重发；
// 标题 1-100、正文 1-5000（口径与服务端一致），纯文本提交。

#include "ui/Page.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QTextEdit;

namespace bbs {

class StateView;

class PostEditorPage : public Page
{
    Q_OBJECT
public:
    explicit PostEditorPage(const PageContext& context, QWidget* parent = nullptr);

    // postId == 0 表示新建；否则为编辑（boardId 可由详情自动纠正）
    void setTarget(qint64 boardId, qint64 postId);
    void onEnter() override;

private:
    void loadForEdit();
    void submit();
    void setBusy(bool busy);
    void updateCounters();
    void backToPrevious();

    qint64 m_boardId = 0;
    qint64 m_postId = 0;
    bool m_loadedForEdit = false;

    QLabel* m_titleLabel = nullptr;
    QLineEdit* m_titleEdit = nullptr;
    QTextEdit* m_contentEdit = nullptr;
    QLabel* m_titleCounter = nullptr;
    QLabel* m_contentCounter = nullptr;
    QLabel* m_errorLabel = nullptr;
    QPushButton* m_submitButton = nullptr;
    QPushButton* m_cancelButton = nullptr;
    StateView* m_state = nullptr;
    QWidget* m_form = nullptr;
};

} // namespace bbs
