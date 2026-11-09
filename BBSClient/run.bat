@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

rem ============================================================
rem  BBSClient 启动脚本
rem  1) 把 Qt 5.15.2 运行库目录加入 PATH（本机为 anaconda 内的 Qt 5.15.2）
rem  2) 指定 Qt 插件目录（platforms/qwindows.dll 等）
rem  3) 检查服务端 127.0.0.1:8080 是否在监听，给出提示后启动客户端
rem ============================================================

set "QT_DIR=D:\Users\Administrator\anaconda3\Library"
set "QT_BIN=%QT_DIR%\bin"
set "QT_PLUGINS=%QT_DIR%\plugins"
set "APP=%~dp0build\bin\BBSClient.exe"
set "HOST=127.0.0.1"
set "PORT=8080"

echo ============================================================
echo  BBS 论坛桌面客户端 BBSClient
echo ============================================================
echo.

if not exist "%APP%" (
    echo [错误] 未找到可执行文件：
    echo        %APP%
    echo.
    echo        请先在工程目录下执行编译：
    echo          call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
    echo          cd /d "%~dp0"
    echo          "%QT_BIN%\qmake.exe" BBSClient.pro -spec win32-msvc "CONFIG+=release"
    echo          nmake
    echo.
    pause
    exit /b 1
)

if not exist "%QT_BIN%\Qt5Core_conda.dll" (
    echo [错误] 未找到 Qt 运行库目录：
    echo        %QT_BIN%
    echo        请修改本脚本中的 QT_DIR 变量，指向本机 Qt 5.15.2 安装目录的 Library 或 msvc2019_64。
    echo.
    pause
    exit /b 1
)

set "PATH=%QT_BIN%;%PATH%"
set "QT_PLUGIN_PATH=%QT_PLUGINS%"
if not defined BBS_API_BASE set "BBS_API_BASE=http://127.0.0.1:8080/api/v1"

echo [1/2] Qt 运行库：%QT_BIN%
echo       插件目录：%QT_PLUGINS%
echo       接口基址：%BBS_API_BASE%
echo.

rem ---------- 服务端连通性探测（800ms 超时，不产生副作用）----------
set "SRV_UP=0"
for /f "delims=" %%r in ('powershell -NoProfile -Command "try{$c=New-Object Net.Sockets.TcpClient;$a=$c.BeginConnect('%HOST%',%PORT%,$null,$null);if($a.AsyncWaitHandle.WaitOne(800) -and $c.Connected){'1'}else{'0'};$c.Close()}catch{'0'}"') do set "SRV_UP=%%r"

echo [2/2] 服务端 %HOST%:%PORT% 状态检查...
if "%SRV_UP%"=="1" (
    echo       服务端已在监听，正常启动客户端。
    echo.
    start "" "%APP%"
    exit /b 0
)

echo.
echo ------------------------------------------------------------
echo  [提示] 未检测到服务端 %HOST%:%PORT%
echo.
echo  客户端将启动，但所有需要数据的操作都会失败，界面只会显示
echo  「无法连接服务器」，无法登录与浏览。
echo.
echo  请先启动服务端 BSS论坛（同一台机器上运行，监听 8080 端口，
echo  首次启动前用 sql\bbs_schema.sql 与 sql\bbs_seed.sql 初始化 bbs.db），
echo  或确认服务端 config.ini 中的端口与客户端基址 %BBS_API_BASE% 一致。
echo ------------------------------------------------------------
echo.
choice /c YN /n /m "仍要启动客户端吗？[Y=启动 / N=退出] "
if errorlevel 2 (
    echo 已取消启动。
    exit /b 2
)

echo.
start "" "%APP%"
exit /b 0
