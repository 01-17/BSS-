---
AIGC:
    Label: "1"
    ContentProducer: 001191440300708461136T1XGW3
    ProduceID: bcc488cf2549f9980544cdbd79c5ddb1_3bc7a852dc4a11f1ba6252540064ee0f
    ReservedCode1: IeCppAvkVPI93IQR+fD0qJmA1tPCu0v2wyUaMf3ewxEmLj7I6MkCZa3Xk8vjalcrlS4PZ6sEjbnQfaXhtYpXCjyYZseZIGhjJPZ13D8AiB7T45gEZm0BEpFMPE84uqpbFZ78ih8Ku57FSDwf2NzJKn4e7IethHli13OfZpsxOXCa4uCPEZRZCRnIZa4=
    ContentPropagator: 001191440300708461136T1XGW3
    PropagateID: bcc488cf2549f9980544cdbd79c5ddb1_3bc7a852dc4a11f1ba6252540064ee0f
    ReservedCode2: IeCppAvkVPI93IQR+fD0qJmA1tPCu0v2wyUaMf3ewxEmLj7I6MkCZa3Xk8vjalcrlS4PZ6sEjbnQfaXhtYpXCjyYZseZIGhjJPZ13D8AiB7T45gEZm0BEpFMPE84uqpbFZ78ih8Ku57FSDwf2NzJKn4e7IethHli13OfZpsxOXCa4uCPEZRZCRnIZa4=
---

# BBSClient — BBS 论坛桌面客户端（Qt/C++）

BBS 论坛课设项目的**客户端工程**。与 C++17 服务端工程 `BSS论坛`（`http / service / dao / sql` 四层）通过 **HTTP/1.1 + JSON(UTF-8)** 通信，客户端不持有数据库、不接触密码与令牌细节，只认 28 条接口与统一四字段响应信封。

**当前完成进度：全部完成（P01–P13 十三个页面全量交付）**——工程骨架、通信层（28 接口全覆盖）、数据模型、会话层、校验/时间工具、通用 UI 组件（四态视图 / 二次确认 / 主题）以及 P01–P13 全部页面的界面、交互与四态反馈均已实现完毕。已在本机用 `qmake + MSVC2019 x64 + Qt 5.15.2` 实际编译链接通过：**0 错误、0 警告**，产物 `build\bin\BBSClient.exe`（358 912 字节），并提供一键启动脚本 `run.bat`。

---

## 1. 技术约束与实现声明

| 项目 | 约束 | 本工程实现 |
|---|---|---|
| GUI 框架 | Qt 5.15 Widgets | `QT += core gui widgets network`，仅使用 Widgets，不用 QML |
| 语言标准 | C++17 | `CONFIG += c++17`、`win32:CONFIG += c++17` |
| 编译器 | MSVC2019 x64（MSVC 19.29） | 已实测编译链接通过 |
| 构建系统 | qmake（`.pro`） | 提供 `BBSClient.pro`（另附 `CMakeLists.txt` 供 Qt6/Qt5 双兼容备选） |
| 源码编码 | UTF-8（含中文字面量） | `QMAKE_CXXFLAGS += /utf-8` |
| 界面构建 | **纯 C++ 代码手写**，禁止 `.ui` 文件 | 工程内 **0 个 `.ui` 文件**，全部控件由构造函数 `new` 并布局 |
| 第三方库 | **除 Qt 外不引入任何第三方库** | 依赖仅 `QtCore / QtGui / QtWidgets / QtNetwork` |

---

## 2. 目录结构

```
BBSClient/
├─ BBSClient.pro                # qmake 工程（主用构建入口）
├─ CMakeLists.txt               # 备选构建（Qt5/Qt6 兼容、AUTOMOC）
├─ run.bat                      # 一键启动脚本（注入 Qt 运行库 PATH + 服务端连通性提示）
├─ README.md
└─ src/
   ├─ main.cpp                  # 程序入口：QApplication + 主题 + 主窗口
   ├─ net/                      # 通信层（唯一 HTTP 出口）
   │  ├─ ApiClient.h/.cpp       # 统一请求入口：28 接口、H0/H1/J/N、5s 超时、401、迟到响应丢弃
   ├─ model/                    # 数据模型层
   │  ├─ Dtos.h                 # 全部 DTO（AuthorView/UserProfile/BoardView/PostSummary/PostDetail/...）
   │  └─ JsonMapper.h/.cpp      # JSON ⇄ DTO 映射（qint64 精确保留，不做 double 中转）
   ├─ session/
   │  └─ SessionManager.h/.cpp  # 令牌仅内存 + 当前用户 + 管理员/版主判定
   ├─ util/
   │  ├─ Validators.h/.cpp      # 字段校验器（与服务端 service/Validation 同口径）
   │  └─ TimeFormat.h/.cpp      # UTC 字符串 → 本地时间显示（yyyy-MM-dd HH:mm）
   └─ ui/                       # 页面层与通用组件（全部 C++ 手写）
      ├─ Page.h                 # 页面契约：Navigator / PageContext / Page::onEnter
      ├─ Theme.h/.cpp           # 全局 QSS 与配色、字号
      ├─ Dialogs.h/.cpp         # 二次确认、破坏性确认、理由输入、提示
      ├─ StateView.h/.cpp       # 四态视图：加载中 / 空 / 失败可重试 / 内容
      ├─ MainWindow.h/.cpp      # P03 主框架壳：顶部导航 + 页面栈 + 状态条 + 会话联动
      ├─ LoginPage.h/.cpp       # P01 登录
      ├─ RegisterPage.h/.cpp    # P02 注册
      ├─ BoardListPage.h/.cpp   # P04 版块列表
      ├─ PostListPage.h/.cpp    # P05 版块帖子列表
      ├─ PostDetailPage.h/.cpp  # P06 帖子详情 + 回复
      ├─ PostEditorPage.h/.cpp  # P07 发帖 / 编辑帖子
      ├─ SearchPage.h/.cpp      # P08 搜索
      ├─ ProfilePage.h/.cpp     # P09 个人中心
      ├─ UserAdminPage.h/.cpp   # P10 账号管理
      ├─ BoardAdminPage.h/.cpp  # P11 版块管理
      ├─ ModeratorPage.h/.cpp   # P12 版主授权
      └─ ModerationPage.h/.cpp  # P13 内容处置
```

代码规模 **45 个源文件、7 323 行**（`src` 下，不含构建产物）：`src/main.cpp` 37 行、`net` 789 行、`model` 914 行、`session` 125 行、`util` 302 行、`ui` 5 156 行。整体分四层：`net`（通信）→ `model`（数据）→ `session`（状态与权限）→ `ui`（展示与交互），`util` 横向复用；`ui/Page.h` 提供 `Navigator` 导航契约与 `PageContext` 依赖注入，页面之间不互相持有指针。

---

## 3. 构建与环境

### 3.1 环境要求

- Windows 10 / 11 x64
- Qt 5.15.x（MSVC 2019 64-bit 构建版本，含 Qt Network 模块）
- Visual Studio 2019（含 MSVC v142 x64 工具链）
- 无需额外安装任何第三方库

### 3.2 命令行构建（qmake + nmake，本机已实测通过）

以下为本机**实际执行并通过**的命令（Qt 5.15.2 由 Anaconda 提供，前缀 `D:\Users\Administrator\anaconda3\Library`；若用官方安装包，把 `QMAKE` 换成 `C:\Qt\5.15.2\msvc2019_64\bin\qmake.exe` 即可）：

```bat
:: 1) 打开 x64 原生工具命令提示符（或先执行 vcvars64.bat 注入 MSVC 环境）
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvars64.bat"

:: 2) 进入工程目录，用 Qt 5.15.2 的 qmake 生成 Makefile（release）
cd /d <工程目录>\BBSClient
"D:\Users\Administrator\anaconda3\Library\bin\qmake.exe" BBSClient.pro -spec win32-msvc "CONFIG+=release"

:: 3) 编译并链接
nmake            :: 或 jom -j8

:: 4) 产物
build\bin\BBSClient.exe
```

> 说明：`.pro` 中已把 `DESTDIR / OBJECTS_DIR / MOC_DIR` 指向工程内 `build/` 子目录，源码目录保持干净；`build/`、工程根的 `Makefile`、`.qmake.stash` 均为 qmake 生成物，可随时删除后按上述命令重建。
>
> 清理重建（可选）：`nmake distclean` 或直接删除 `build\` 目录后重新执行第 2、3 步。

### 3.3 Qt Creator 构建

`文件 → 打开文件或项目 → 选择 BBSClient.pro`，选 MSVC2019 64bit 套件，直接构建运行。

### 3.4 本次编译验证记录

| 项目 | 结果 |
|---|---|
| 验证时间 | 2026-11-09（本机） |
| 构建方式 | `qmake (Qt 5.15.2) + nmake`，`-spec win32-msvc`、`CONFIG+=release`，MSVC 14.29（VS2019 Enterprise x64） |
| 结果 | **编译链接成功**，`link /OUT:build\bin\BBSClient.exe` 正常退出（退出码 0），产出 `BBSClient.exe`（358 912 字节） |
| 编译告警 | **0 错误、0 警告**（`/utf-8` 生效，中文注释与字面量无乱码；源码中无 `.ui` 文件，界面全部由 C++ 构建） |
| 参与编译单元 | 全部 `src` 源文件 + `moc` 产物共 33 个编译单元（含 `moc_ApiClient / moc_SessionManager / moc_Page / moc_StateView / moc_MainWindow / moc_LoginPage … moc_ModerationPage` 等 17 个 moc 文件） |
| 分层自检 | `net/model/session/util/ui` 五个模块均参与编译；`src` 下 45 个源文件、0 个空文件、0 处 TODO/FIXME |
| 运行库依赖 | 依赖 `Qt5Core_conda.dll / Qt5Gui_conda.dll / Qt5Widgets_conda.dll / Qt5Network_conda.dll`（Anaconda 版 Qt 的 DLL 名带 `_conda` 后缀），因此**需先注入 Qt bin 目录到 PATH** 才能双击运行，`run.bat` 已完成该处理 |
| 整数精度自检 | 以 `post_id=9223372036854775807`、`board_id=9007199254740993` 构造响应体实测：经 `protectIntegerTokens` + `readInt64` 还原后数值逐位一致；字符串内的 `a:12`、小数 `1.5`、指数 `1e9`、负数 `-42` 等边界未被误改 |

> 交付包内已清理构建中间文件（`build\obj`、`build\moc`、工程根 `Makefile`、`.qmake.stash`），仅保留可直接运行的可执行产物 `build\bin\BBSClient.exe`；如需重建，按 §3.2 命令执行即可（`qmake` 会重新生成 Makefile 与 moc 文件）。

---

## 4. 运行与联调

### 4.1 启动顺序

1. 先启动服务端 `BSS论坛`（监听 `0.0.0.0:8080`，SQLite 数据库 `bbs.db` 已由 `sql/bbs_schema.sql` + `sql/bbs_seed.sql` 初始化）；
2. 再启动客户端：工程根目录双击 **`run.bat`**（或命令行执行 `run.bat`），用演示账号登录。

### 4.1.1 run.bat 做了什么

`run.bat` 解决「Anaconda 版 Qt 的 DLL 不在系统 PATH、直接双击 exe 会报缺少 `Qt5Widgets_conda.dll`」的问题：

| 步骤 | 动作 |
|---|---|
| 环境注入 | `set "PATH=D:\Users\Administrator\anaconda3\Library\bin;%PATH%"`，并设 `QT_PLUGIN_PATH=D:\Users\Administrator\anaconda3\Library\plugins`（保证 `platforms\qwindows.dll` 可被找到） |
| 基址预设 | 若未定义 `BBS_API_BASE`，默认设为 `http://127.0.0.1:8080/api/v1`；已定义则不覆盖 |
| 前置检查 | exe 不存在 → 打印重建命令后 `exit /b 1`；Qt 运行库目录不存在 → 提示修改脚本顶部 `QT_DIR` 后退出 |
| 服务端探测 | 用 PowerShell `TcpClient` 以 **800 ms** 超时连接 `127.0.0.1:8080`（只建连、不发送数据，无副作用） |
| 未启动提示 | 未连通时输出：客户端仍可启动，但所有数据操作会失败、界面只显示「无法连接服务器」，并提示先启动服务端 / 用 `bbs_schema.sql` + `bbs_seed.sql` 初始化 `bbs.db` / 核对 `config.ini` 端口；随后 `choice` 询问「仍要启动客户端吗？[Y=启动 / N=退出]」，选 N 退出（`exit /b 2`） |
| 启动 | 已连通或用户选 Y → `start "" build\bin\BBSClient.exe` |

> 换机器部署时，只需改脚本顶部 `QT_DIR`（指向本机 Qt 5.15.2 的 `Library` 或 `msvc2019_64` 目录）即可复用。

### 4.2 联调基址（可配置）

| 项 | 值 | 说明 |
|---|---|---|
| 默认基址 | `http://127.0.0.1:8080/api/v1` | 与 API 规范一致 |
| 环境变量覆盖 | `BBS_API_BASE` | 例：`set BBS_API_BASE=http://192.168.1.10:8080/api/v1` |
| 代码内设置 | `ApiClient::setBaseUrl()` | 基址末尾斜杠自动去除 |

> 服务端 `config.ini` 的监听端口若改动，客户端基址需同步调整；客户端不做端口扫描或自动发现。

### 4.3 演示账号（来源 `sql/bbs_seed.sql`）

| 用户名 | 密码 | 角色 / 状态 | 用途 |
|---|---|---|---|
| `admin07` | `AdminDemo2026!` | admin，正常 | 版块管理、账号管理、版主授权、内容处置 |
| `moderator07` | `ModDemo2026!` | member + 「技术交流」版主 | 版主巡查、移除内容、置顶/加精 |
| `alice07` | `AliceDemo2026!` | member，正常 | 作者侧发帖/编辑/删除、普通浏览 |
| `bob07` | `BobDemo2026!` | member，正常 | 越权验证（编辑他人内容应 403） |
| `disabled07` | `DisabledDemo2026!` | member，**停用** | 登录失败提示「账号已停用」 |

种子数据同时覆盖：停用版块（站务公告）、已移除帖子、已删除回复、1 条置顶、1 条加精、1 项版主授权 —— 可直接用于四态与显隐验证。

---

## 5. 通信层实现要点（src/net）

| 规范要求 | 实现方式 |
|---|---|
| 统一基址与路径 | `m_baseUrl + path`，路径区分大小写，ID 用 `qint64` 直接拼路径 |
| H0 / H1 | `RequestOptions::auth`；H0 省略 `Authorization`，H1 发 `Authorization: Bearer <token>` |
| J / N | `RequestOptions::hasBody`；J 设 `Content-Type: application/json; charset=utf-8` 并发 JSON 体，N 发零字节体且不带 Content-Type（GET/DELETE 一律 N） |
| 四字段信封 | `parseEnvelope()` 严格校验 `success/code/message/data` 四字段类型，缺字段或非 JSON 对象 → `InvalidResponse`（提示"通信数据异常"，不伪造业务码） |
| 64 位整数精度 | Qt5 的 `QJsonValue` 以 double 承载数字，> 2^53 的 ID 会丢精度。`json::protectIntegerTokens()` 在 `fromJson` 之前把 JSON 文本中「值位置」的纯整数 token 改写为等值字符串（字符串字面量与键名不动、小数与指数形式不动），再由 `readInt64/readInt` 严格还原为 `qint64`；未经保护的超大 double 一律按类型不符拒绝，避免静默错误 |
| 5 s 超时 | 每请求一个单次 `QTimer`（5000 ms），超时 `abort()` 并标记，由 `finished` 统一收口为 `FailureKind::Timeout` |
| 写请求超时 | 文案区分为「提交超时，请刷新查看结果」，**不自动重发**，编辑内容保留，由用户刷新核实后自行决定是否重提 |
| HTTP 状态 + code 双通道 | 先取 HTTP 状态，再解析 `code`；`401` 直接清内存令牌并发 `sessionExpired()` |
| 401 清会话 | `ApiClient::sessionExpired` → `MainWindow` 清 `SessionManager` 并回登录页（改密接口 401 同样处理） |
| 迟到响应 | 请求记录发起时的 `activePage`（`QPointer<QWidget>`），回调前比对当前页；页面已切换或 owner 已析构 → **丢弃响应**，不覆盖当前界面 |
| 令牌安全 | 令牌仅存 `QString m_token` 私有成员，不写 QSettings / 日志 / URL / 界面 |

错误映射（与 API 规范表 3-3 一致）：

| HTTP | code | 客户端处理 |
|---|---|---|
| 400 | `INVALID_ARGUMENT` | 定位字段、保留输入、展示服务端原因 |
| 401 | `UNAUTHENTICATED` | 登录页展示「账号或密码错误 / 账号已停用」；受保护页清会话回登录 |
| 403 | `FORBIDDEN` | 提示无权限，刷新权限相关入口 |
| 404 | `RESOURCE_UNAVAILABLE` | 刷新列表或退出失效详情 |
| 405 | `METHOD_NOT_ALLOWED` | 按实现缺陷处理（记录日志） |
| 409 | `DATA_CONFLICT` | 提示冲突原因并刷新当前状态 |
| 500 | `INTERNAL_ERROR` | 通用失败 + 重试入口 |
| 503 | `DATABASE_UNAVAILABLE` | 「数据库暂不可用，请稍后重试」 |

---

## 6. 页面清单与进度（P01–P13，全部完成）

编号与接口编号沿用《HTTP API 接口交接规范》。**进度：P01–P13 十三个页面已全部实现完成**——界面、交互、四态反馈（加载中 / 空 / 失败可重试 / 内容）、权限显隐、二次确认与提交禁用均已落地，无占位页面、无空实现。下表的「状态」指**代码落地状态**（编译通过），运行期行为以接服务端联调为准。

| 编号 | 页面 | 主要接口 | 代码文件 | 状态 |
|---|---|---|---|---|
| P01 | 登录页 | 02 | `LoginPage` | 已实现 |
| P02 | 注册页 | 01 | `RegisterPage` | 已实现 |
| P03 | 主框架（首页壳） | 03、04、07 | `MainWindow` | 已实现 |
| P04 | 版块列表 | 07（+09 新建入口） | `BoardListPage` | 已实现 |
| P05 | 版块帖子列表 | 08、07（+12 发帖入口） | `PostListPage` | 已实现 |
| P06 | 帖子详情 / 回复 | 13、18、19、16、17（+25~28 管理入口） | `PostDetailPage` | 已实现 |
| P07 | 发帖 / 编辑帖子 | 12、14 | `PostEditorPage` | 已实现 |
| P08 | 搜索 | 20、07 | `SearchPage` | 已实现 |
| P09 | 个人中心 | 04、05、06 | `ProfilePage` | 已实现 |
| P10 | 账号管理（admin） | 21、22 | `UserAdminPage` | 已实现 |
| P11 | 版块管理（admin） | 07、09、10、11 | `BoardAdminPage` | 已实现 |
| P12 | 版主授权（admin） | 21、07、23、24 | `ModeratorPage` | 已实现 |
| P13 | 内容处置（moderator/admin） | 25、26、27、28 | `ModerationPage` | 已实现 |

28 个接口在 `ApiClient` 中**全部有封装方法**（01~28 一一对应），无遗漏、无占位空实现。

导航约束（NFR-005）：经顶部导航条，发帖（版块列表 → 进入版块 → 发帖）、回复、搜索、资料编辑均在 **≤3 次界面操作**内可达；导航条按 `role_code == admin` 与 `moderator_assignments` **动态显隐**，不硬编码角色。

---

## 7. 关键业务规则落地

| 规则 | 落地位置与做法 |
|---|---|
| 字段校验与服务端同口径 | `util/Validators`：`trimUnicode` + Unicode 码点计数；username 4–20 ASCII `[A-Za-z0-9_]`；password 8–64 **不 trim**；nickname/版块名 1–30；description 0–200；title 1–100；帖子正文 1–5000；回复 1–1000；reason 1–200；keyword 0–100；拒绝内嵌 NUL |
| 数据库日期 vs UTC | `util/TimeFormat`：响应中的 `yyyy-MM-ddTHH:mm:ssZ` 一律转**本地时区**展示；搜索的 `start_date/end_date` 直接发本地日历日期，**不转 UTC** |
| 排序不做二次处理 | 列表/回复顺序完全采用服务端返回顺序（置顶优先、回复时间升序） |
| 停用版块 | 列表中给出「停用」标识；停用版块可读旧帖但发帖/回复入口禁用（服务端返回 409 时另作提示） |
| 作者本人判定 | 编辑/删除入口仅对 `author.user_id == 当前用户` 与 `status == 1` 的内容显示；管理员与版主**不使用删除**，走「移除」 |
| 二次确认 | 删除帖子/回复、移除内容、停用/启用版块、停用账号、撤销版主一律经 `dialogs::confirmDestructive`（默认焦点在「取消」） |
| 理由必填 | `dialogs::promptReason` 校验 1–200 码点，取消则不发请求 |
| 提交中禁用 | 提交按钮在请求期间 `setEnabled(false)`，结束后恢复 |
| 幂等提示 | 接口返回 `changed=false` 时提示「状态未变化」，不视为错误 |
| 纯文本呈现 | 帖子正文、回复、标题一律以 `QLabel`/`QPlainTextEdit` 纯文本显示，不解析 HTML |

---

## 8. 与文档的对照索引

| 实现内容 | 依据文档 / 服务端源码 |
|---|---|
| 28 接口路径、方法、头组合、请求与响应字段 | 《HTTP API 接口交接规范》§3.2~§3.3；`http/ApiRouter.cpp` `registerRoutes()` |
| 四字段信封与错误码表 | 同规范 §3.1.2 表 3-3；`http/JsonWriter.cpp` |
| DTO 字段与类型 | 同规范 §3.2.2；`dao/Projections.h` |
| 校验口径 | `service/Validation.cpp`、`common/Util.cpp` |
| 权限模型（admin / 版主动态授权） | `service/PermissionService.cpp`；SRS FR-017~FR-021 |
| 页面范围、四态反馈、二次确认、≤3 次操作 | SRS IF-001、NFR-002/003/005；概要设计说明书客户端模块划分 |
| 令牌仅内存、8 小时、401 语义 | 详细设计说明书 §通信与安全 |
| 本工程的完整设计说明（架构、类职责、时序、接口映射、校验口径、验收自查） | 同目录上一级的《BBS论坛桌面客户端设计说明书.docx》 |

---

## 9. 交付物清单

| 交付物 | 位置 | 说明 |
|---|---|---|
| 客户端源码工程 | `output\BBSClient\` | 45 个源文件、7 323 行；`BBSClient.pro` 为构建入口 |
| 一键启动脚本 | `output\BBSClient\run.bat` | 注入 Qt 运行库 PATH + 服务端连通性提示 |
| 设计说明书 | `output\BBS论坛桌面客户端设计说明书.docx` | 逐章对应本工程真实代码（架构 / 类职责 / 页面与跳转 / 四态 / 时序 / 28 接口映射 / 校验口径 / 构建部署 / V-01~V-13 自查） |
| 可执行产物 | `output\BBSClient\build\bin\BBSClient.exe` | 本机实测编译产出，可直接经 `run.bat` 启动；中间文件已清理，可由 §3.2 命令重建 |

---

## 10. 后续可做的联调验证（不属于本次交付范围）

1. 接服务端 `bbs_seed.sql` 做一次 P01→P13 全链路走查（含停用账号、停用版块、已删内容、越权 403）；
2. 补充网络异常演练：杀服务端（连接失败）、停用数据库（503）、弱网（5 s 超时），核对文案与内容保留；
3. 界面细节打磨：列表行高、长文本换行、窄窗口下的布局挤压；
4. 若课程要求截图与测试记录，可在联调后补充《客户端测试用例与截图》附录。
*（内容由AI生成，仅供参考）*
