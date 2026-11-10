#pragma once

// P08 帖子搜索
// 数据源：GET /posts/search?keyword=&board_id=&start_date=&end_date=&sort=time|heat
// 参数口径与服务端一致：keyword 0-100（trim 后计数）、日期 yyyy-MM-dd 且为真实日期、
// 开始日期不得晚于结束日期；空参数不传（service 端视为不限）。

#include "ui/Page.h"

#include <QVector>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QVBoxLayout;
class QWidget;

namespace bbs {

class StateView;

class SearchPage : public Page
{
    Q_OBJECT
public:
    explicit SearchPage(const PageContext& context, QWidget* parent = nullptr);

    void onEnter() override;

private:
    void loadBoards();
    void doSearch();
    void renderResults(const QVector<PostSummary>& posts);

    QComboBox* m_boardCombo = nullptr;
    QLineEdit* m_keywordEdit = nullptr;
    QLineEdit* m_startDateEdit = nullptr;
    QLineEdit* m_endDateEdit = nullptr;
    QComboBox* m_sortCombo = nullptr;
    QLabel* m_errorLabel = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QPushButton* m_searchButton = nullptr;
    StateView* m_state = nullptr;
    QWidget* m_resultContainer = nullptr;
    QVBoxLayout* m_resultLayout = nullptr;
};

} // namespace bbs
