#include "ui/PostEditorPage.h"

#include "model/JsonMapper.h"
#include "net/ApiClient.h"
#include "ui/Dialogs.h"
#include "ui/StateView.h"
#include "ui/Theme.h"
#include "util/Validators.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

namespace bbs {

PostEditorPage::PostEditorPage(const PageContext& context, QWidget* parent)
    : Page(context, parent)
{
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 18);
    root->setSpacing(12);

    QHBoxLayout* header = new QHBoxLayout;
    m_cancelButton = theme::normalButton(QStringLiteral("返回"), this);
    header->addWidget(m_cancelButton);
    m_titleLabel = theme::pageTitle(QStringLiteral("发帖"), this);
    header->addWidget(m_titleLabel);
    header->addStretch(1);
    m_submitButton = theme::primaryButton(QStringLiteral("发布"), this);
    header->addWidget(m_submitButton);
    root->addLayout(header);

    m_state = new StateView(this);
    root->addWidget(m_state, 1);

    m_form = new QFrame(m_state);
    m_form->setObjectName(QStringLiteral("card"));
    QVBoxLayout* formLayout = new QVBoxLayout(m_form);
    formLayout->setContentsMargins(18, 16, 18, 16);
    formLayout->setSpacing(8);

    formLayout->addWidget(theme::sectionTitle(QStringLiteral("标题"), m_form));
    m_titleEdit = new QLineEdit(m_form);
    m_titleEdit->setPlaceholderText(QStringLiteral("1-100 个字符"));
    m_titleEdit->setMaxLength(200);
    formLayout->addWidget(m_titleEdit);
    m_titleCounter = theme::metaLabel(QStringLiteral("0 / 100"), m_form);
    formLayout->addWidget(m_titleCounter);

    formLayout->addWidget(theme::sectionTitle(QStringLiteral("正文"), m_form));
    m_contentEdit = new QTextEdit(m_form);
    m_contentEdit->setAcceptRichText(false); // 纯文本
    m_contentEdit->setPlaceholderText(QStringLiteral("1-5000 个字符，仅支持纯文本"));
    m_contentEdit->setMinimumHeight(320);
    formLayout->addWidget(m_contentEdit);
    m_contentCounter = theme::metaLabel(QStringLiteral("0 / 5000"), m_form);
    formLayout->addWidget(m_contentCounter);

    m_errorLabel = theme::errorLabel(m_form);
    m_errorLabel->setVisible(false);
    formLayout->addWidget(m_errorLabel);

    m_state->setContentWidget(m_form);

    connect(m_cancelButton, &QPushButton::clicked, this, &PostEditorPage::backToPrevious);
    connect(m_submitButton, &QPushButton::clicked, this, &PostEditorPage::submit);
    connect(m_titleEdit, &QLineEdit::textChanged, this, &PostEditorPage::updateCounters);
    connect(m_contentEdit, &QTextEdit::textChanged, this, &PostEditorPage::updateCounters);
}

void PostEditorPage::setTarget(qint64 boardId, qint64 postId)
{
    m_boardId = boardId;
    m_postId = postId;
    m_loadedForEdit = false;
}

void PostEditorPage::onEnter()
{
    m_errorLabel->setVisible(false);
    setBusy(false);

    if (m_postId > 0) {
        m_titleLabel->setText(QStringLiteral("编辑帖子"));
        m_submitButton->setText(QStringLiteral("保存修改"));
        loadForEdit();
        return;
    }

    m_titleLabel->setText(QStringLiteral("发帖"));
    m_submitButton->setText(QStringLiteral("发布"));
    m_titleEdit->clear();
    m_contentEdit->clear();
    updateCounters();
    m_state->showContent();
}

void PostEditorPage::loadForEdit()
{
    if (!m_context.isValid() || m_postId <= 0) {
        m_state->showError(QStringLiteral("未指定要编辑的帖子"));
        return;
    }
    m_state->showLoading(QStringLiteral("正在加载帖子内容…"));
    const qint64 postId = m_postId;
    m_context.api->fetchPost(postId, this, [this, postId](const ApiResult& result) {
        if (postId != m_postId)
            return;
        if (!result.ok) {
            m_state->showError(result.displayMessage());
            return;
        }
        PostDetail detail;
        QString error;
        if (!json::parsePostDetail(result.data, &detail, &error)) {
            m_state->showError(QStringLiteral("通信数据异常：%1").arg(error));
            return;
        }
        m_boardId = detail.boardId;
        m_titleEdit->setText(detail.title);
        m_contentEdit->setPlainText(detail.content);
        m_loadedForEdit = true;
        updateCounters();
        m_state->showContent();
    });
}

void PostEditorPage::updateCounters()
{
    m_titleCounter->setText(
        QStringLiteral("%1 / 100").arg(validators::codePointCount(m_titleEdit->text())));
    m_contentCounter->setText(
        QStringLiteral("%1 / 5000").arg(validators::codePointCount(m_contentEdit->toPlainText())));
}

void PostEditorPage::backToPrevious()
{
    if (!m_context.nav)
        return;
    if (!m_state->isContentVisible()) {
        m_context.nav->goBack();
        return;
    }
    const bool dirty = !m_titleEdit->text().isEmpty() || !m_contentEdit->toPlainText().isEmpty();
    if (dirty && !dialogs::confirm(this, QStringLiteral("放弃编辑"),
                                   QStringLiteral("当前内容尚未提交，确认离开并放弃？"),
                                   QStringLiteral("放弃")))
        return;
    if (m_postId > 0)
        m_context.nav->showPostDetail(m_postId);
    else
        m_context.nav->showPostList(m_boardId, QString());
}

void PostEditorPage::setBusy(bool busy)
{
    m_submitButton->setEnabled(!busy);
    m_cancelButton->setEnabled(!busy);
    m_titleEdit->setReadOnly(busy);
    m_contentEdit->setReadOnly(busy);
    m_submitButton->setText(busy ? QStringLiteral("提交中…")
                                 : (m_postId > 0 ? QStringLiteral("保存修改")
                                                 : QStringLiteral("发布")));
}

void PostEditorPage::submit()
{
    if (!m_context.isValid())
        return;

    const validators::Check title = validators::postTitle(m_titleEdit->text());
    if (!title.ok) {
        m_errorLabel->setText(title.error);
        m_errorLabel->setVisible(true);
        return;
    }
    const validators::Check content = validators::postContent(m_contentEdit->toPlainText());
    if (!content.ok) {
        m_errorLabel->setText(content.error);
        m_errorLabel->setVisible(true);
        return;
    }
    if (m_postId == 0 && m_boardId <= 0) {
        m_errorLabel->setText(QStringLiteral("未指定版块，请返回版块列表重新进入"));
        m_errorLabel->setVisible(true);
        return;
    }

    m_errorLabel->setVisible(false);
    setBusy(true);

    const QString titleValue = title.normalized;
    const QString contentValue = content.normalized;

    if (m_postId == 0) {
        const qint64 boardId = m_boardId;
        m_context.api->createPost(boardId, titleValue, contentValue, this,
                                  [this, boardId](const ApiResult& result) {
            setBusy(false);
            if (!result.ok) {
                // 超时保留输入、不自动重发
                m_errorLabel->setText(
                    result.isTimeout()
                        ? QStringLiteral("提交超时：内容已保留，请检查网络后手动重新提交")
                        : result.displayMessage());
                m_errorLabel->setVisible(true);
                return;
            }
            PostDetail detail;
            QString error;
            if (!json::parsePostDetail(result.data, &detail, &error)) {
                m_errorLabel->setText(QStringLiteral("通信数据异常：%1").arg(error));
                m_errorLabel->setVisible(true);
                return;
            }
            if (m_context.nav) {
                m_context.nav->notify(QStringLiteral("帖子已发布"));
                m_context.nav->showPostDetail(detail.postId);
            }
            Q_UNUSED(boardId)
        });
        return;
    }

    const qint64 postId = m_postId;
    m_context.api->updatePost(postId, titleValue, contentValue, this,
                              [this](const ApiResult& result) {
        setBusy(false);
        if (!result.ok) {
            m_errorLabel->setText(
                result.isTimeout()
                    ? QStringLiteral("提交超时：内容已保留，请检查网络后手动重新提交")
                    : result.displayMessage());
            m_errorLabel->setVisible(true);
            return;
        }
        PostDetail detail;
        QString error;
        if (!json::parsePostDetail(result.data, &detail, &error)) {
            m_errorLabel->setText(QStringLiteral("通信数据异常：%1").arg(error));
            m_errorLabel->setVisible(true);
            return;
        }
        if (m_context.nav) {
            m_context.nav->notify(QStringLiteral("修改已保存"));
            m_context.nav->showPostDetail(detail.postId);
        }
    });
}

} // namespace bbs
