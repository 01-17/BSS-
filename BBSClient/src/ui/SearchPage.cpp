#include "ui/SearchPage.h"

#include "model/JsonMapper.h"
#include "net/ApiClient.h"
#include "ui/StateView.h"
#include "ui/Theme.h"
#include "util/TimeFormat.h"
#include "util/Validators.h"

#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace bbs {

SearchPage::SearchPage(const PageContext& context, QWidget* parent)
    : Page(context, parent)
{
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 18);
    root->setSpacing(12);

    root->addWidget(theme::pageTitle(QStringLiteral("搜索帖子"), this));

    QFrame* filter = new QFrame(this);
    filter->setObjectName(QStringLiteral("card"));
    QGridLayout* grid = new QGridLayout(filter);
    grid->setContentsMargins(16, 14, 16, 14);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(10);

    m_keywordEdit = new QLineEdit(filter);
    m_keywordEdit->setPlaceholderText(QStringLiteral("关键词（标题或正文，可为空，最多 100 字符）"));
    m_keywordEdit->setMaxLength(200);
    grid->addWidget(m_keywordEdit, 0, 0, 1, 3);

    grid->addWidget(theme::metaLabel(QStringLiteral("版块"), filter), 1, 0);
    m_boardCombo = new QComboBox(filter);
    m_boardCombo->addItem(QStringLiteral("全部版块"), QVariant::fromValue<qint64>(-1));
    grid->addWidget(m_boardCombo, 1, 1);

    grid->addWidget(theme::metaLabel(QStringLiteral("排序"), filter), 1, 2);
    m_sortCombo = new QComboBox(filter);
    m_sortCombo->addItem(QStringLiteral("按时间（最新优先）"), QStringLiteral("time"));
    m_sortCombo->addItem(QStringLiteral("按热度（回复多优先）"), QStringLiteral("heat"));
    grid->addWidget(m_sortCombo, 1, 3);

    grid->addWidget(theme::metaLabel(QStringLiteral("开始日期"), filter), 2, 0);
    m_startDateEdit = new QLineEdit(filter);
    m_startDateEdit->setPlaceholderText(QStringLiteral("yyyy-MM-dd，可空"));
    grid->addWidget(m_startDateEdit, 2, 1);

    grid->addWidget(theme::metaLabel(QStringLiteral("结束日期"), filter), 2, 2);
    m_endDateEdit = new QLineEdit(filter);
    m_endDateEdit->setPlaceholderText(QStringLiteral("yyyy-MM-dd，可空"));
    grid->addWidget(m_endDateEdit, 2, 3);

    m_searchButton = theme::primaryButton(QStringLiteral("搜索"), filter);
    grid->addWidget(m_searchButton, 2, 4);

    root->addWidget(filter);

    m_errorLabel = theme::errorLabel(this);
    m_errorLabel->setVisible(false);
    root->addWidget(m_errorLabel);

    m_summaryLabel = theme::metaLabel(QString(), this);
    root->addWidget(m_summaryLabel);

    m_state = new StateView(this);
    root->addWidget(m_state, 1);

    QScrollArea* scroll = new QScrollArea(m_state);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    m_resultContainer = new QWidget(scroll);
    m_resultLayout = new QVBoxLayout(m_resultContainer);
    m_resultLayout->setContentsMargins(0, 0, 0, 0);
    m_resultLayout->setSpacing(10);
    m_resultLayout->addStretch(1);
    scroll->setWidget(m_resultContainer);
    m_state->setContentWidget(scroll);

    connect(m_searchButton, &QPushButton::clicked, this, &SearchPage::doSearch);
    connect(m_keywordEdit, &QLineEdit::returnPressed, this, &SearchPage::doSearch);
    connect(m_state, &StateView::retryRequested, this, &SearchPage::doSearch);
}

void SearchPage::onEnter()
{
    m_errorLabel->setVisible(false);
    if (m_boardCombo->count() <= 1)
        loadBoards();
    if (!m_state->isContentVisible())
        m_state->showEmpty(QStringLiteral("输入关键词或选择筛选条件后点击“搜索”。"));
}

void SearchPage::loadBoards()
{
    // 版块下拉仅用于筛选，失败不影响搜索（服务端会忽略无效参数前先做校验）
    m_context.api->listBoards(this, [this](const ApiResult& result) {
        if (!result.ok)
            return;
        QVector<BoardView> boards;
        QString error;
        if (!json::parseBoardList(result.data, &boards, &error))
            return;
        for (const BoardView& board : boards) {
            m_boardCombo->addItem(board.isOpen()
                                      ? board.name
                                      : QStringLiteral("%1（已停用）").arg(board.name),
                                  QVariant::fromValue<qint64>(board.boardId));
        }
    });
}

void SearchPage::doSearch()
{
    if (!m_context.isValid())
        return;

    const validators::Check keyword = validators::keyword(m_keywordEdit->text());
    if (!keyword.ok) {
        m_errorLabel->setText(keyword.error);
        m_errorLabel->setVisible(true);
        return;
    }

    const QString startDate = validators::trimUnicode(m_startDateEdit->text());
    if (!startDate.isEmpty() && !validators::isValidDate(startDate)) {
        m_errorLabel->setText(QStringLiteral("开始日期格式应为 yyyy-MM-dd 的真实日期"));
        m_errorLabel->setVisible(true);
        return;
    }
    const QString endDate = validators::trimUnicode(m_endDateEdit->text());
    if (!endDate.isEmpty() && !validators::isValidDate(endDate)) {
        m_errorLabel->setText(QStringLiteral("结束日期格式应为 yyyy-MM-dd 的真实日期"));
        m_errorLabel->setVisible(true);
        return;
    }
    if (!startDate.isEmpty() && !endDate.isEmpty() && startDate > endDate) {
        m_errorLabel->setText(QStringLiteral("开始日期不能晚于结束日期"));
        m_errorLabel->setVisible(true);
        return;
    }

    m_errorLabel->setVisible(false);
    m_searchButton->setEnabled(false);
    m_state->showLoading(QStringLiteral("正在搜索…"));

    const qint64 boardId = m_boardCombo->currentData().toLongLong();
    const QString sort = m_sortCombo->currentData().toString();

    m_context.api->searchPosts(keyword.normalized, boardId, startDate, endDate, sort, this,
                              [this](const ApiResult& result) {
        m_searchButton->setEnabled(true);
        if (!result.ok) {
            m_state->showError(result.displayMessage());
            m_summaryLabel->clear();
            return;
        }
        QVector<PostSummary> posts;
        QString error;
        if (!json::parsePostList(result.data, &posts, &error)) {
            m_state->showError(QStringLiteral("通信数据异常：%1").arg(error));
            return;
        }
        if (posts.isEmpty()) {
            m_summaryLabel->setText(QStringLiteral("未找到匹配的帖子"));
            m_state->showEmpty(QStringLiteral("没有符合条件的帖子，试试更换关键词或放宽日期范围。"));
            return;
        }
        renderResults(posts);
        m_state->showContent();
    });
}

void SearchPage::renderResults(const QVector<PostSummary>& posts)
{
    while (m_resultLayout->count() > 1) {
        QLayoutItem* item = m_resultLayout->takeAt(0);
        if (QWidget* widget = item->widget())
            widget->deleteLater();
        delete item;
    }

    m_summaryLabel->setText(
        QStringLiteral("共找到 %1 篇帖子（%2）")
            .arg(posts.size())
            .arg(m_sortCombo->currentText()));

    for (const PostSummary& post : posts) {
        QFrame* card = new QFrame(m_resultContainer);
        card->setObjectName(QStringLiteral("card"));
        QVBoxLayout* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(16, 12, 16, 12);
        cardLayout->setSpacing(6);

        QHBoxLayout* titleRow = new QHBoxLayout;
        if (post.isPinned)
            titleRow->addWidget(theme::tagLabel(QStringLiteral("置顶"), QStringLiteral("#dc2626"),
                                               card));
        if (post.isFeatured)
            titleRow->addWidget(theme::tagLabel(QStringLiteral("精华"), QStringLiteral("#d97706"),
                                               card));
        QLabel* title = new QLabel(post.title, card);
        title->setTextFormat(Qt::PlainText);
        title->setWordWrap(true);
        title->setStyleSheet(QStringLiteral("font-size:15px; font-weight:600;"));
        titleRow->addWidget(title, 1);
        QPushButton* open = theme::normalButton(QStringLiteral("查看"), card);
        open->setFixedWidth(70);
        titleRow->addWidget(open);
        cardLayout->addLayout(titleRow);

        cardLayout->addWidget(theme::metaLabel(
            QStringLiteral("%1 · 回复 %2 · 发布 %3")
                .arg(post.author.nickname)
                .arg(post.replyCount)
                .arg(timefmt::toLocalDisplay(post.createdAt)),
            card));

        const qint64 postId = post.postId;
        connect(open, &QPushButton::clicked, this, [this, postId]() {
            if (m_context.nav)
                m_context.nav->showPostDetail(postId);
        });

        m_resultLayout->insertWidget(m_resultLayout->count() - 1, card);
    }
}

} // namespace bbs
