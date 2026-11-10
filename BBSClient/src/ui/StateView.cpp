#include "ui/StateView.h"

#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedLayout>
#include <QVBoxLayout>

namespace bbs {

namespace {

QWidget* makeMessagePage(const QString& objectName, QLabel** label, QWidget* parent)
{
    QWidget* page = new QWidget(parent);
    page->setObjectName(objectName);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 48, 24, 48);
    layout->setSpacing(16);
    layout->setAlignment(Qt::AlignCenter);

    QLabel* text = new QLabel(page);
    text->setAlignment(Qt::AlignCenter);
    text->setWordWrap(true);
    text->setObjectName(QStringLiteral("stateHint"));
    layout->addWidget(text);

    *label = text;
    return page;
}

} // namespace

StateView::StateView(QWidget* parent)
    : QWidget(parent)
{
    m_stack = new QStackedLayout(this);
    m_stack->setContentsMargins(0, 0, 0, 0);

    // 加载中
    m_loadingPage = new QWidget(this);
    {
        QVBoxLayout* layout = new QVBoxLayout(m_loadingPage);
        layout->setContentsMargins(24, 48, 24, 48);
        layout->setSpacing(12);
        layout->setAlignment(Qt::AlignCenter);

        m_progress = new QProgressBar(m_loadingPage);
        m_progress->setRange(0, 0); // 不确定进度
        m_progress->setFixedWidth(220);
        m_progress->setTextVisible(false);
        layout->addWidget(m_progress, 0, Qt::AlignHCenter);

        m_loadingLabel = new QLabel(QStringLiteral("正在加载…"), m_loadingPage);
        m_loadingLabel->setObjectName(QStringLiteral("stateHint"));
        m_loadingLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(m_loadingLabel, 0, Qt::AlignHCenter);
    }

    // 空数据
    m_emptyPage = makeMessagePage(QStringLiteral("emptyPage"), &m_emptyLabel, this);

    // 失败（含重试）
    m_errorPage = makeMessagePage(QStringLiteral("errorPage"), &m_errorLabel, this);
    m_retryButton = new QPushButton(QStringLiteral("重试"), m_errorPage);
    m_retryButton->setObjectName(QStringLiteral("primaryButton"));
    m_retryButton->setCursor(Qt::PointingHandCursor);
    m_retryButton->setFixedWidth(120);
    // QLayout 基类只提供两参 addWidget，这里取回具体布局类型后再带对齐参数添加
    if (QBoxLayout* errorLayout = qobject_cast<QBoxLayout*>(m_errorPage->layout()))
        errorLayout->addWidget(m_retryButton, 0, Qt::AlignHCenter);
    connect(m_retryButton, &QPushButton::clicked, this, &StateView::retryRequested);

    // 成功内容（占位空控件，后续通过 setContentWidget 替换）
    m_contentPage = new QWidget(this);
    new QVBoxLayout(m_contentPage);
    m_contentPage->layout()->setContentsMargins(0, 0, 0, 0);

    m_stack->addWidget(m_loadingPage);
    m_stack->addWidget(m_emptyPage);
    m_stack->addWidget(m_errorPage);
    m_stack->addWidget(m_contentPage);
    m_stack->setCurrentWidget(m_loadingPage);
}

void StateView::setContentWidget(QWidget* content)
{
    if (!content)
        return;

    QLayout* layout = m_contentPage->layout();
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* old = item->widget()) {
            if (old != content)
                old->deleteLater();
        }
        delete item;
    }
    content->setParent(m_contentPage);
    layout->addWidget(content);
}

void StateView::showLoading(const QString& text)
{
    m_loadingLabel->setText(text.isEmpty() ? QStringLiteral("正在加载…") : text);
    setCurrentWidget(m_loadingPage);
}

void StateView::showEmpty(const QString& text)
{
    m_emptyLabel->setText(text);
    setCurrentWidget(m_emptyPage);
}

void StateView::showError(const QString& text)
{
    m_errorLabel->setText(text.isEmpty() ? QStringLiteral("请求失败，请稍后重试") : text);
    setCurrentWidget(m_errorPage);
}

void StateView::showContent()
{
    setCurrentWidget(m_contentPage);
}

bool StateView::isContentVisible() const
{
    return m_stack->currentWidget() == m_contentPage;
}

void StateView::setCurrentWidget(QWidget* widget)
{
    m_stack->setCurrentWidget(widget);
}

} // namespace bbs
