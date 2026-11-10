cmake_minimum_required(VERSION 3.16)

project(BBSClient VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# ---------------------------------------------------------------------------
# Qt 查找：优先 Qt6，回退 Qt5（同一份源码同时兼容两者）
#  - Qt5 需要 Network + Widgets；Qt6 的 Network 中已含 QtNetwork
# ---------------------------------------------------------------------------
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Network)

message(STATUS "BBSClient: 使用 Qt ${QT_VERSION_MAJOR}（${Qt${QT_VERSION_MAJOR}_VERSION}）")

set(BBS_SOURCES
    src/main.cpp

    # 网络层：全应用唯一 HTTP 出口
    src/net/ApiClient.cpp

    # 数据层：DTO 与 JSON 映射（Dtos.h 为纯声明头，无同名 .cpp）
    src/model/JsonMapper.cpp

    # 会话层：令牌仅在内存
    src/session/SessionManager.cpp

    # 工具层：校验口径与时间格式化
    src/util/Validators.cpp
    src/util/TimeFormat.cpp

    # 界面层：公共控件与 P01-P13
    src/ui/Theme.cpp
    src/ui/Dialogs.cpp
    src/ui/StateView.cpp
    src/ui/MainWindow.cpp
    src/ui/LoginPage.cpp
    src/ui/RegisterPage.cpp
    src/ui/BoardListPage.cpp
    src/ui/PostListPage.cpp
    src/ui/PostDetailPage.cpp
    src/ui/PostEditorPage.cpp
    src/ui/SearchPage.cpp
    src/ui/ProfilePage.cpp
    src/ui/UserAdminPage.cpp
    src/ui/BoardAdminPage.cpp
    src/ui/ModeratorPage.cpp
    src/ui/ModerationPage.cpp
)

set(BBS_HEADERS
    src/net/ApiClient.h

    src/model/Dtos.h
    src/model/JsonMapper.h

    src/session/SessionManager.h

    src/util/Validators.h
    src/util/TimeFormat.h

    src/ui/Page.h
    src/ui/Theme.h
    src/ui/Dialogs.h
    src/ui/StateView.h
    src/ui/MainWindow.h
    src/ui/LoginPage.h
    src/ui/RegisterPage.h
    src/ui/BoardListPage.h
    src/ui/PostListPage.h
    src/ui/PostDetailPage.h
    src/ui/PostEditorPage.h
    src/ui/SearchPage.h
    src/ui/ProfilePage.h
    src/ui/UserAdminPage.h
    src/ui/BoardAdminPage.h
    src/ui/ModeratorPage.h
    src/ui/ModerationPage.h
)

# Qt5 的 moc 不会自动处理 .cpp 中 Q_OBJECT 的情况，这里依赖 AUTOMOC；
# 所有含 Q_OBJECT 的类均声明在 .h 中，头文件一并纳入 target。
add_executable(BBSClient WIN32 ${BBS_SOURCES} ${BBS_HEADERS})

target_include_directories(BBSClient PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(BBSClient PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
)

if (MSVC)
    target_compile_options(BBSClient PRIVATE /W3 /utf-8)
else()
    target_compile_options(BBSClient PRIVATE -Wall -Wextra)
endif()

# 便于 IDE 直接打开 .h/.cpp 时定位资源（本项目不使用 .ui / .qrc，界面全部代码构建）
source_group("net" FILES src/net/ApiClient.cpp src/net/ApiClient.h)
source_group("model" FILES src/model/Dtos.h
    src/model/JsonMapper.cpp src/model/JsonMapper.h)
source_group("session" FILES src/session/SessionManager.cpp src/session/SessionManager.h)
source_group("util" FILES src/util/Validators.cpp src/util/Validators.h
    src/util/TimeFormat.cpp src/util/TimeFormat.h)
