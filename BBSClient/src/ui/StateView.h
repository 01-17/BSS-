#pragma once

// 四态视图：加载中 / 空数据 / 失败（可重试） / 成功内容。
// 设计文档 IF-001 要求所有数据页面必须具备四态反馈，禁止出现"白屏无提示"。
// 使用方式：
//   StateView* view = new StateView;
//   view->setContentWidget(myListWidget);   // 只注入一次
//   view->showLoading();                    // 请求发起
//   view->showEmpty("该版块还没有帖子");      // 成功但空
//   view->showError(result.displayMessage()); // 失败
//   view->showContent();                    // 成功有数据
// 失败态的"重试"按钮通过 retryRequested() 通知页面重新发请求。

#include <QWidget>

class QLabel;
class QPushButton;
class QStackedLayout;
class QProgressBar;

namespace bbs {

class StateView : public QWidget
{
    Q_OBJECT
public:
    explicit StateView(QWidget* parent = nullptr);

    // 注入成功态内容控件（StateView 接管其父子关系，但不参与布局外的改动）
    void setContentWidget(QWidget* content);

    void showLoading(const QString& text = QStringLiteral("正在加载…"));
    void showEmpty(const QString& text);
    void showError(const QString& text);
    void showContent();

    bool isContentVisible() const;

signals:
    void retryRequested();

private:
    void setCurrentWidget(QWidget* widget);

    QStackedLayout* m_stack = nullptr;
    QWidget* m_loadingPage = nullptr;
    QWidget* m_emptyPage = nullptr;
    QWidget* m_errorPage = nullptr;
    QWidget* m_contentPage = nullptr;

    QLabel* m_loadingLabel = nullptr;
    QLabel* m_emptyLabel = nullptr;
    QLabel* m_errorLabel = nullptr;
    QProgressBar* m_progress = nullptr;
    QPushButton* m_retryButton = nullptr;
};

} // namespace bbs
