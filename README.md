# 第 7 组 BBS 论坛（C/S 架构课设）

基于 **C++17 服务端 + Qt 桌面客户端 + SQLite** 的论坛系统。客户端与服务端通过 **HTTP/1.1 + JSON(UTF-8)** 通信，接口契约以 `day5/第7组_BBS论坛_HTTP_API接口交接规范.md` 为准，共 28 条接口。

- 服务端：Windows x64 控制台程序，C++17 / Winsock2 / SQLite3 / nlohmann-json / Windows CNG(BCrypt)
- 客户端：Qt 5.15.2 Widgets 桌面应用，全部请求异步、5 秒超时
- 数据库：SQLite 单文件 `data/bbs.db`，7 张表、12 个索引，服务端启动时自动建表并写入演示数据

## 目录结构

```
BBS/
├─ BBS论坛.slnx               # VS 解决方案（服务端）
├─ BBS论坛/                   # 服务端工程
│  ├─ main.cpp                # 启动入口：定位根目录、读配置、建库、启动 HTTP 服务
│  ├─ config.ini              # 监听地址、数据库路径、日志配置
│  ├─ common/                 # 类型、错误码、时间/UTF-8 工具、PBKDF2、日志、配置
│  ├─ dao/                    # 数据库连接、语句封装、6 个 Repository
│  ├─ service/                # 校验、权限、认证、版块/帖子/回复/管理服务
│  ├─ http/                   # HTTP 解析、JSON 校验、路由、服务器与工作线程
│  ├─ sql/                    # bbs_schema.sql 建表脚本、bbs_seed.sql 演示数据
│  ├─ third_party/            # sqlite3.c/h、json.hpp（随仓库提供）
│  └─ out/                    # 编译输出（Debug/Release）
├─ BBSClient/                 # Qt 客户端工程
│  ├─ BBSClient.pro           # qmake 工程文件
│  ├─ run.bat                 # 一键启动脚本
│  ├─ src/{model,net,session,ui,util}/
│  └─ build/bin/              # 客户端输出（含 windeployqt 部署的 Qt 运行库）
├─ tests/                     # 数据库回归测试（Python 标准库）
├─ Demo/                      # 演示脚本
└─ README.md
```

## 环境要求

| 组件 | 版本 / 位置 |
| --- | --- |
| 操作系统 | Windows 10 / 11 x64 |
| Visual Studio | 2022 及以上（本机为 VS 18 Community，v145 工具集） |
| Qt | 5.15.2 `win64_msvc2019_64`，本机安装在 `F:\Tools\Common_Tools\Qt\5.15.2\msvc2019_64` |
| Python（可选） | 3.8+，仅用于数据库回归测试 |

服务端第三方库已放在 `BBS论坛/third_party/`，无需额外下载。

## 编译与运行

### 1. 服务端

在 VS 中打开 `BBS论坛.slnx`，选择 `x64` + `Debug`/`Release`，直接生成并运行。

命令行方式：

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" `
  "F:\Code\BBS\BBS论坛.slnx" /p:Configuration=Release /p:Platform=x64 /m /nologo
```

产物：`BBS论坛\out\Release\BBS论坛.exe`。

首次运行会在工作目录自动创建 `data\bbs.db`、`data\backups\`（每日备份，保留 7 份）和 `logs\server.log`，并写入演示数据。启动成功后输出：

```
工作目录: F:\Code\BBS\BBS论坛
[INFO] 数据库已打开: data/bbs.db
[INFO] HTTP 服务已启动，监听 0.0.0.0:8080
服务运行中，按 Ctrl+C 停止
```

> 程序会从 exe 所在目录向上最多 4 层自动查找 `config.ini`，因此双击 `out\Release\BBS论坛.exe` 也能正常启动。

### 2. 客户端

```powershell
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
cd F:\Code\BBS\BBSClient
F:\Tools\Common_Tools\Qt\5.15.2\msvc2019_64\bin\qmake.exe BBSClient.pro -spec win32-msvc "CONFIG+=release"
nmake
F:\Tools\Common_Tools\Qt\5.15.2\msvc2019_64\bin\windeployqt.exe --release build\bin\BBSClient.exe
```

产物：`BBSClient\build\bin\BBSClient.exe`（已随 exe 部署 Qt5 DLL 与 `platforms\qwindows.dll`）。

也可以直接双击 `BBSClient\run.bat` 启动（会先探测 8080 端口是否在监听）。若 Qt 不在上述路径，修改 `run.bat` 与命令中的 `QT_DIR` 即可。

## 配置

`BBS论坛/config.ini`：

```ini
[server]
host=0.0.0.0
port=8080

[database]
path=data/bbs.db
backup_dir=data/backups

[log]
file=logs/server.log
level=INFO
```

所有路径为相对工作目录的路径。

## 演示账号

| 账号 | 密码 | 角色 / 状态 |
| --- | --- | --- |
| `admin07` | `AdminDemo2026!` | 管理员，正常 |
| `moderator07` | `ModDemo2026!` | 版主（技术交流版块），正常 |
| `alice07` | `AliceDemo2026!` | 普通用户，正常 |
| `bob07` | `BobDemo2026!` | 普通用户，正常 |
| `disabled07` | `DisabledDemo2026!` | 普通用户，已停用 |

密码存储为 PBKDF2-HMAC-SHA256（16 字节盐、600000 次迭代、32 字节摘要），数据库中只保存盐与摘要的十六进制串。

## 接口概览

- 基址：`http://127.0.0.1:8080/api/v1`
- JSON 字段统一 snake_case，UTF-8 编码
- 受保护接口需 `Authorization: Bearer <token>`；令牌为 64 位小写十六进制，有效期 8 小时，改密或账号停用使该账号全部令牌立即失效
- 统一响应信封：

```json
{ "success": true, "code": "OK", "message": "操作成功", "data": {} }
```

| 业务码 | HTTP | 含义 |
| --- | --- | --- |
| `INVALID_ARGUMENT` | 400 | 报文/字段/参数/日期非法，或当前密码错误 |
| `UNAUTHENTICATED` | 401 | 凭据错误、账号停用、会话不可用 |
| `FORBIDDEN` | 403 | 缺少作者、管理员或版块管理权限 |
| `RESOURCE_UNAVAILABLE` | 404 | 路径未注册或目标不存在/已删除 |
| `METHOD_NOT_ALLOWED` | 405 | 方法不被支持，响应头带 `Allow` |
| `DATA_CONFLICT` | 409 | 唯一性、授权关联或状态冲突 |
| `INTERNAL_ERROR` | 500 | 未预期错误 |
| `DATABASE_UNAVAILABLE` | 503 | 数据库无法访问或锁等待超时 |

接口分组：

| 分组 | 路由 |
| --- | --- |
| 认证 | `POST /auth/register`、`POST /auth/login`、`POST /auth/logout` |
| 个人资料 | `GET /users/me`、`PUT /users/me`、`PUT /users/me/password` |
| 版块 | `GET /boards`、`GET /boards/{board_id}/posts`、`POST /boards`、`PUT /boards/{board_id}`、`PUT /boards/{board_id}/status`、`POST /boards/{board_id}/posts` |
| 帖子 | `GET /posts/search`、`GET/PUT/DELETE /posts/{post_id}` |
| 回复 | `GET/POST /posts/{post_id}/replies`、`PUT/DELETE /replies/{reply_id}` |
| 管理 | `GET /admin/users`、`PUT /admin/users/{user_id}/status`、`PUT/DELETE /admin/boards/{board_id}/moderators/{user_id}` |
| 版务处置 | `POST /moderation/posts/{post_id}/removal`、`POST /moderation/replies/{reply_id}/removal`、`PUT /moderation/posts/{post_id}/pinned`、`PUT /moderation/posts/{post_id}/featured` |

权限要点：管理员可管理全站并编辑任意帖子；版主仅对被授权版块生效，且只能编辑自己的帖子；普通用户只能编辑自己的内容。

## 数据库

7 张表：`roles`、`users`、`boards`、`posts`、`replies`、`board_moderators`、`moderation_logs`。

- 软删除：`status=0` 表示停用（账号/版块）或已删除（帖子/回复）
- 发布时间与更新时间均为 UTC 文本 `YYYY-MM-DD HH:MM:SS`，接口序列化为 `YYYY-MM-DDTHH:MM:SSZ`
- 帖子列表排序 `is_pinned DESC, created_at DESC, post_id DESC`，回复按 `created_at ASC`
- 搜索使用 `instr()` 做字面量、区分大小写的子串匹配
- 管理动作共 14 种，写入 `moderation_logs`

详细表结构与约束见 `BBS论坛/sql/bbs_schema.sql`。

## 测试

数据库回归测试（内存库运行，不影响运行中的服务）：

```powershell
cd F:\Code\BBS
python -m unittest discover -s tests -v
```
