// BBS 论坛客户端入口。
// 启动顺序：QApplication → 读取基址（BBS_API_BASE 可覆盖）→ 应用全局样式 → 主框架壳 → 登录页。

#include "net/ApiClient.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QFont>
#include <QString>

int main(int argc, char* argv[])
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    // 高分屏缩放：Qt5 需要显式开启，Qt6 默认开启且该属性已废弃
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);
#endif

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("BBSClient"));
    app.setOrganizationName(QStringLiteral("BBS论坛课设"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));

    // 全局字体：优先中文可读字体，缺失时由系统回落
    QFont font(QStringLiteral("Microsoft YaHei UI"));
    font.setPointSize(10);
    app.setFont(font);

    app.setStyleSheet(bbs::theme::styleSheet());

    bbs::MainWindow window;
    window.show();
    window.start();

    return app.exec();
}
