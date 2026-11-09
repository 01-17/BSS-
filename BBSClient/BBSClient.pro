#-------------------------------------------------------------------------------
# BBSClient.pro
# BBS 论坛桌面客户端（Qt 5.15 Widgets / C++17 / qmake / MSVC2019 x64）
#
# 说明：
#   1) 界面全部由 C++ 代码构建，工程不使用任何 .ui 文件，因此无需 QT += uic 相关配置；
#   2) 除 Qt 外不链接任何第三方库（无 QtNetwork 之外的依赖）；
#   3) 源文件内含大量中文字面量，MSVC 必须加 /utf-8，否则会按本地代码页误解析；
#   4) 构建产物与中间文件统一输出到 build 子目录，保持源码目录整洁。
#
# 构建（Qt 5.15 + MSVC2019 x64 命令行）：
#   mkdir build && cd build
#   qmake ..\BBSClient.pro -spec win32-msvc "CONFIG+=release"
#   nmake            # 或 jom
#   release\BBSClient.exe
# 也可直接用 Qt Creator 打开本文件构建运行。
#-------------------------------------------------------------------------------

QT += core gui widgets network

TEMPLATE = app
TARGET = BBSClient

CONFIG += c++17
win32:CONFIG += c++17
CONFIG -= debug_and_release
CONFIG += warn_on

# 中文字面量按 UTF-8 解析（MSVC 专用编译选项）
QMAKE_CXXFLAGS += /utf-8

DEFINES += QT_DEPRECATED_WARNINGS

INCLUDEPATH += $$PWD/src

# 输出目录
DESTDIR     = $$PWD/build/bin
OBJECTS_DIR = $$PWD/build/obj
MOC_DIR     = $$PWD/build/moc
RCC_DIR     = $$PWD/build/rcc
UI_DIR      = $$PWD/build/ui

#-------------------------------------------------------------------------------
# 源文件
#-------------------------------------------------------------------------------
SOURCES += \
    src/main.cpp \
    \
    src/net/ApiClient.cpp \
    \
    src/model/JsonMapper.cpp \
    \
    src/session/SessionManager.cpp \
    \
    src/util/Validators.cpp \
    src/util/TimeFormat.cpp \
    \
    src/ui/Theme.cpp \
    src/ui/Dialogs.cpp \
    src/ui/StateView.cpp \
    src/ui/MainWindow.cpp \
    src/ui/LoginPage.cpp \
    src/ui/RegisterPage.cpp \
    src/ui/BoardListPage.cpp \
    src/ui/PostListPage.cpp \
    src/ui/PostDetailPage.cpp \
    src/ui/PostEditorPage.cpp \
    src/ui/SearchPage.cpp \
    src/ui/ProfilePage.cpp \
    src/ui/UserAdminPage.cpp \
    src/ui/BoardAdminPage.cpp \
    src/ui/ModeratorPage.cpp \
    src/ui/ModerationPage.cpp

#-------------------------------------------------------------------------------
# 头文件（含 Q_OBJECT 的类全部声明在头文件中，由 moc 处理）
#-------------------------------------------------------------------------------
HEADERS += \
    src/net/ApiClient.h \
    \
    src/model/Dtos.h \
    src/model/JsonMapper.h \
    \
    src/session/SessionManager.h \
    \
    src/util/Validators.h \
    src/util/TimeFormat.h \
    \
    src/ui/Page.h \
    src/ui/Theme.h \
    src/ui/Dialogs.h \
    src/ui/StateView.h \
    src/ui/MainWindow.h \
    src/ui/LoginPage.h \
    src/ui/RegisterPage.h \
    src/ui/BoardListPage.h \
    src/ui/PostListPage.h \
    src/ui/PostDetailPage.h \
    src/ui/PostEditorPage.h \
    src/ui/SearchPage.h \
    src/ui/ProfilePage.h \
    src/ui/UserAdminPage.h \
    src/ui/BoardAdminPage.h \
    src/ui/ModeratorPage.h \
    src/ui/ModerationPage.h

# Windows 下作为 GUI 子系统程序（不弹出控制台窗口）
win32: CONFIG += windows
