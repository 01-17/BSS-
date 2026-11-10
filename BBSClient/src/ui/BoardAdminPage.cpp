#include "ui/BoardAdminPage.h"

#include "model/JsonMapper.h"
#include "net/ApiClient.h"
#include "session/SessionManager.h"
#include "ui/Dialogs.h"
#include "ui/StateView.h"
#include "ui/Theme.h"
#include "util/TimeFormat.h"
#include "util/Validators.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTextEdit>
#include <QVBoxLayout>

namespace bbs {

BoardAdminPage::BoardAdminPage(const PageContext& context, QWidget* parent)
    : Page(context, parent)
{
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 18);
    root->setSpacing(12);

    root->addWidget(theme::pageTitle(QStringLiteral("版块管理"), this));

    // ---- 新建 / 编辑表单 ----
    QFrame* formCard = new QFrame(this);
    formCard->setObjectName(QStringLiteral("card"));
    QVBoxLayout* formLayout = new QVBoxLayout(formCard);
    formLayout->setContentsMargins(18, 16, 18, 16);
    formLayout->setSpacing(8);

    m_formTitle = theme::sectionTitle(QStringLiteral("新建版块"), formCard);
    formLayout->addWidget(m_formTitle);

    m_nameEdit = new QLineEdit(formCard);
    m_nameEdit->setPlaceholderText(QStringLiteral("版块名称（1-30 个字符，不可重复）"));
    m_nameEdit->setMaxLength(60);
    formLayout->addWidget(m_nameEdit);

    m_descEdit = new QTextEdit(formCard);
    m_descEdit->setAcceptRichText(false);
    m_descEdit->setPlaceholderText(QStringLiteral("版块描述（0-200 个字符，可为空）"));
    m_descEdit->setFixedHeight(70);
    formLayout->addWidget(m_descEdit);

    m_formError = theme::errorLabel(formCard);
    m_formError->setVisible(false);
    formLayout->addWidget(m_formError);

    QHBoxLayout* formButtons = new QHBoxLayout;
    formButtons->addStretch(1);
    m_cancelEditButton = theme::normalButton(QStringLiteral("取消编辑"), formCard);
    m_cancelEditButton->setVisible(false);
    formButtons->addWidget(m_cancelEditButton);
    m_submitButton = theme::primaryButton(QStringLiteral("创建版块"), formCard);
    formButtons->addWidget(m_submitButton);
    formLayout->addLayout(formButtons);

    root->addWidget(formCard);

    m_summaryLabel = theme::metaLabel(QString(), this);
    root->addWidget(m_summaryLabel);

    // ---- 版块列表 ----
    m_state = new StateView(this);
    root->addWidget(m_state, 1);

    QScrollArea* scroll = new QScrollArea(m_state);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    m_listContainer = new QWidget(scroll);
    m_listLayout = new QVBoxLayout(m_listContainer);
    m_listLayout->setContentsMargins(0, 0, 0, 0);
    m_listLayout->setSpacing(10);
    m_listLayout->addStretch(1);
    scroll->setWidget(m_listContainer);
    m_state->setContentWidget(scroll);

    connect(m_submitButton, &QPushButton::clicked, this, &BoardAdminPage::submitForm);
    connect(m_cancelEditButton, &QPushButton::clicked, this, &BoardAdminPage::resetForm);
    connect(m_state, &StateView::retryRequested, this, &BoardAdminPage::load);
}

void BoardAdminPage::onEnter()
{
    if (!m_context.session->isAdmin()) {
        m_state->showError(QStringLiteral("仅管理员可访问版块管理"));
        return;
    }
    resetForm();
    load();
}

void BoardAdminPage::resetForm()
{
    m_editingBoardId = -1;
    m_nameEdit->clear();
    m_descEdit->clear();
    m_formError->setVisible(false);
    m_formTitle->setText(QStringLiteral("新建版块"));
    m_submitButton->setText(QStringLiteral("创建版块"));
    m_cancelEditButton->setVisible(false);
}

void BoardAdminPage::load()
{
    if (!m_context.isValid())
        return;

    m_state->showLoading(QStringLiteral("正在加载版块列表…"));
    m_context.api->listBoards(this, [this](const ApiResult& result) {
        if (!result.ok) {
            m_state->showError(result.displayMessage());
            return;
        }
        QVector<BoardView> boards;
        QString error;
        if (!json::parseBoardList(result.data, &boards, &error)) {
            m_state->showError(QStringLiteral("通信数据异常：%1").arg(error));
            return;
        }
        m_boards = boards;
        if (m_boards.isEmpty()) {
            m_state->showEmpty(QStringLiteral("当前还没有任何版块，可在上方新建。"));
            m_summaryLabel->clear();
            return;
        }
        rebuild();
        m_state->showContent();
    });
}

void BoardAdminPage::rebuild()
{
    while (m_listLayout->count() > 1) {
        QLayoutItem* item = m_listLayout->takeAt(0);
        if (QWidget* widget = item->widget())
            widget->deleteLater();
        delete item;
    }

    int openCount = 0;
    for (const BoardView& board : m_boards) {
        if (board.isOpen())
            ++openCount;

        QFrame* card = new QFrame(m_listContainer);
        card->setObjectName(QStringLiteral("card"));
        QHBoxLayout* cardLayout = new QHBoxLayout(card);
        cardLayout->setContentsMargins(16, 12, 16, 12);
        cardLayout->setSpacing(12);

        QVBoxLayout* infoLayout = new QVBoxLayout;
        infoLayout->setSpacing(4);

        QHBoxLayout* nameRow = new QHBoxLayout;
        QLabel* name = new QLabel(board.name, card);
        name->setTextFormat(Qt::PlainText);
        name->setStyleSheet(QStringLiteral("font-size:15px; font-weight:600;"));
        nameRow->addWidget(name);
        if (board.isOpen())
            nameRow->addWidget(
                theme::tagLabel(QStringLiteral("开放"), QStringLiteral("#16a34a"), card));
        else
            nameRow->addWidget(
                theme::tagLabel(QStringLiteral("已停用"), QStringLiteral("#9ca3af"), card));
        nameRow->addStretch(1);
        infoLayout->addLayout(nameRow);

        const QString desc = board.description.trimmed().isEmpty()
                                 ? QStringLiteral("（无描述）")
                                 : board.description;
        QLabel* descLabel = theme::metaLabel(desc, card);
        descLabel->setWordWrap(true);
        infoLayout->addWidget(descLabel);
        infoLayout->addWidget(theme::metaLabel(
            QStringLiteral("版块 ID %1 · 创建于 %2").arg(board.boardId).arg(
                timefmt::toLocalDisplay(board.createdAt)),
            card));

        cardLayout->addLayout(infoLayout, 1);

        QPushButton* edit = theme::normalButton(QStringLiteral("编辑"), card);
        edit->setFixedWidth(70);
        const BoardView snapshot = board;
        connect(edit, &QPushButton::clicked, this, [this, snapshot]() { startEdit(snapshot); });
        cardLayout->addWidget(edit);

        const qint64 boardId = board.boardId;
        const QString boardName = board.name;
        if (board.isOpen()) {
            QPushButton* disable = theme::dangerButton(QStringLiteral("停用"), card);
            disable->setFixedWidth(70);
            connect(disable, &QPushButton::clicked, this, [this, boardId, boardName]() {
                changeStatus(boardId, 0, boardName);
            });
            cardLayout->addWidget(disable);
        } else {
            QPushButton* enable = theme::primaryButton(QStringLiteral("启用"), card);
            enable->setFixedWidth(70);
            connect(enable, &QPushButton::clicked, this, [this, boardId, boardName]() {
                changeStatus(boardId, 1, boardName);
            });
            cardLayout->addWidget(enable);
        }

        m_listLayout->insertWidget(m_listLayout->count() - 1, card);
    }

    m_summaryLabel->setText(
        QStringLiteral("共 %1 个版块，其中开放 %2 个").arg(m_boards.size()).arg(openCount));
}

void BoardAdminPage::startEdit(const BoardView& board)
{
    m_editingBoardId = board.boardId;
    m_nameEdit->setText(board.name);
    m_descEdit->setPlainText(board.description);
    m_formTitle->setText(QStringLiteral("编辑版块（ID %1）").arg(board.boardId));
    m_submitButton->setText(QStringLiteral("保存修改"));
    m_cancelEditButton->setVisible(true);
    m_formError->setVisible(false);
    m_nameEdit->setFocus();
}

void BoardAdminPage::submitForm()
{
    if (!m_context.isValid())
        return;

    const validators::Check name = validators::boardName(m_nameEdit->text());
    if (!name.ok) {
        m_formError->setText(name.error);
        m_formError->setVisible(true);
        return;
    }
    const validators::Check desc = validators::boardDescription(m_descEdit->toPlainText());
    if (!desc.ok) {
        m_formError->setText(desc.error);
        m_formError->setVisible(true);
        return;
    }

    m_formError->setVisible(false);
    m_submitButton->setEnabled(false);
    m_submitButton->setText(QStringLiteral("提交中…"));

    const qint64 editingId = m_editingBoardId;
    if (editingId < 0) {
        m_context.api->createBoard(name.normalized, desc.normalized, this,
                                   [this](const ApiResult& result) {
            m_submitButton->setEnabled(true);
            m_submitButton->setText(QStringLiteral("创建版块"));
            BoardMutationResult mutation;
            QString error;
            if (!result.ok) {
                m_formError->setText(result.displayMessage());
                m_formError->setVisible(true);
                return;
            }
            if (!json::parseBoardMutation(result.data, &mutation, &error)) {
                m_formError->setText(QStringLiteral("通信数据异常：%1").arg(error));
                m_formError->setVisible(true);
                return;
            }
            resetForm();
            if (m_context.nav)
                m_context.nav->notify(QStringLiteral("版块创建成功"));
            load();
        });
        return;
    }

    m_context.api->updateBoard(editingId, name.normalized, desc.normalized, this,
                               [this](const ApiResult& result) {
        m_submitButton->setEnabled(true);
        m_submitButton->setText(QStringLiteral("保存修改"));
        if (!result.ok) {
            m_formError->setText(result.displayMessage());
            m_formError->setVisible(true);
            return;
        }
        BoardMutationResult mutation;
        QString error;
        if (!json::parseBoardMutation(result.data, &mutation, &error)) {
            m_formError->setText(QStringLiteral("通信数据异常：%1").arg(error));
            m_formError->setVisible(true);
            return;
        }
        resetForm();
        if (m_context.nav)
            m_context.nav->notify(mutation.changed ? QStringLiteral("版块信息已更新")
                                                   : QStringLiteral("内容未变化，无需更新"));
        load();
    });
}

void BoardAdminPage::changeStatus(qint64 boardId, int status, const QString& name)
{
    if (!m_context.isValid())
        return;

    const QString action = status == 0 ? QStringLiteral("停用") : QStringLiteral("启用");
    if (!dialogs::confirmDestructive(
            this, QStringLiteral("%1版块").arg(action),
            QStringLiteral("确认%1版块“%2”？%3")
                .arg(action, name,
                     status == 0 ? QStringLiteral("停用后该版块禁止发帖，已有帖子仍可浏览。")
                                 : QStringLiteral("启用后该版块恢复发帖。")),
            action))
        return;

    m_context.api->setBoardStatus(boardId, status, this, [this](const ApiResult& result) {
        if (!result.ok) {
            dialogs::warn(this, QStringLiteral("操作未生效"), result.displayMessage());
            return;
        }
        BoardMutationResult mutation;
        QString error;
        if (!json::parseBoardMutation(result.data, &mutation, &error)) {
            dialogs::warn(this, QStringLiteral("通信数据异常"), error);
            return;
        }
        if (m_context.nav) {
            m_context.nav->notify(mutation.changed ? QStringLiteral("版块状态已更新")
                                                   : QStringLiteral("版块状态未发生变化"));
        }
        load();
    });
}

} // namespace bbs
