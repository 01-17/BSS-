---
AIGC:
    Label: "1"
    ContentProducer: 001191440300708461136T1XGW3
    ProduceID: bcc488cf2549f9980544cdbd79c5ddb1_48eef4f9dc2e11f1ba6252540064ee0f
    ReservedCode1: cBQxAJ6Srds+HPW/IaNWuLCZ7tNpOvTtCup7li21XrkBoP6CRvKr1E3nTJwH2Td83bWjN+lfXQixMVjJMOq/VrajXEBN0neNlfyYki5GwsbzdPSSEGXa5uxeICIzMeIQrF3j7Px83AqJmHgsorUeb3A9gemN+mAm2zJuEZ66/Lt2YGUndERFTDt9e6w=
    ContentPropagator: 001191440300708461136T1XGW3
    PropagateID: bcc488cf2549f9980544cdbd79c5ddb1_48eef4f9dc2e11f1ba6252540064ee0f
    ReservedCode2: cBQxAJ6Srds+HPW/IaNWuLCZ7tNpOvTtCup7li21XrkBoP6CRvKr1E3nTJwH2Td83bWjN+lfXQixMVjJMOq/VrajXEBN0neNlfyYki5GwsbzdPSSEGXa5uxeICIzMeIQrF3j7Px83AqJmHgsorUeb3A9gemN+mAm2zJuEZ66/Lt2YGUndERFTDt9e6w=
---

# BBS 论坛桌面应用系统 — 前端开发熟悉摘要

> 文档用途：面向前端（Qt/C++ 桌面客户端）开发者的设计交底摘要，覆盖架构与技术栈、页面/模块清单、交互与业务规则、HTTP API 约定、对前端有影响的数据结构、UI 规范与约束。
> 编制日期：2026-11-09　编制依据：课设目录下 9 份设计文档 + 代码包 `BSS--zb.zip`（已解压逐层核对）。
> 说明：凡标注「文档规定」的内容均可在对应文档章节溯源；凡标注「建议」的内容为本摘要给出的落地参考，不属于设计文档的强制约束。

---

## 1. 结论速览（先看这一节）

| 序号 | 结论 | 依据 |
|---|---|---|
| 1 | 系统为 **C/S 桌面应用**：Qt/C++ 桌面客户端 + C++17 服务端 + SQLite 3，通过 **HTTP/1.1 + JSON(UTF-8)** 通信，本机/局域网部署 | 概要设计说明书 §系统架构；SRS ╱CON-001 |
| 2 | 代码包 `BSS--zb` **只包含服务端工程**，无 `.ui/.qrc/.qml` 等任何前端文件，**客户端需从零实现** | 代码包目录实查（29 个 `.h`、24 个 `.cpp`、2 个 `.sql`，全部在 `common/dao/http/service` 四层内） |
| 3 | 服务端 **28 个 HTTP 接口已在 `http/ApiRouter.cpp` 全部注册并实现**，前端可直接联调，无需等待接口开发 | `ApiRouter::registerRoutes()` 实查，28 条路由与 API 文档 01–28 一一对应 |
| 4 | 鉴权采用 **Bearer 不透明令牌（64 位小写十六进制）**，有效期 8 小时，**只允许内存持有**，禁止落盘/写日志 | API 交接规范 §3.1.1；详细设计说明书 §通信与安全 |
| 5 | 所有响应为**统一四字段信封** `success/code/message/data`，前端按 `HTTP 状态 + code` 分流，禁止用中文 message 判断业务 | API 交接规范 §3.1.2 表 3-3 |
| 6 | 全部业务列表**不分页**，一次返回完整有序数组（`data.items`）；空结果为 `[]` | API 交接规范 §3.1.2、§3.3 |
| 7 | 数据库为 7 张表，**逻辑删除**（`status` 0/1），帖子删除后其回复随父帖隐藏；前端不得自行推断"已删除=不可查"以外的状态 | 类图与数据库设计说明书 §4 |
| 8 | 客户端硬性指标：**单请求 5 s 超时**、**写请求超时禁止自动重发**、**提交中禁用提交按钮**、**断线/超时/非法响应三类提示可区分** | SRS NFR-003；API §3.1.3 |
| 9 | 关键入口 **≤3 次界面操作**可达（发帖/回复/搜索/资料编辑），删除必须二次确认，论坛内容**纯文本呈现**（不解析 HTML） | SRS IF-001、NFR-005、NFR-002 |
| 10 | 管理侧有两套入口：**管理员**（版块增改启停、账号启停、版主授权）与**版主**（管辖版块内移除内容、置顶、加精）；版主是**动态权限**，`role_code` 仍为 `member` | SRS FR-017~FR-021；API §3.1.1 表 3-2 |

---

## 2. 项目整体架构与技术栈

### 2.1 架构形态

```
┌──────────────────────────┐        HTTP/1.1 + JSON(UTF-8)        ┌──────────────────────────┐
│  Qt/C++ 桌面客户端        │ ───────────────────────────────────► │  C++17 服务端（本工程）    │
│  页面层 + 通信模块        │ ◄─────────────────────────────────── │  http / service / dao 层  │
│  仅在内存持有令牌         │      统一四字段响应信封               │  单 DbWorker 串行访问      │
└──────────────────────────┘                                      └────────────┬─────────────┘
                                                                               │ SQLite3（仅服务端可访问）
                                                                        bbs.db（7 张表）
```

- **客户端不持有、不分发任何数据库文件**，所有读写经服务端 HTTP 接口完成（API 交接规范 §3.4、概要设计说明书 §架构）。
- 服务端线程模型：**thread-per-connection** 负责报文收发，业务与数据库操作由**单个工作线程串行执行**（`sql/bbs_seed.sql` 注释与详细设计说明书 §线程模型）。该模型意味着：并发写不会互相阻塞在业务层，但前端仍应按"一请求一响应"串行管理页面状态。

### 2.2 技术栈清单

| 层次 | 技术选型 | 前端关注点 |
|---|---|---|
| 客户端 | Qt（Widgets）/ C++（Windows 10/11） | 页面、路由式导航、异步 HTTP（`QNetworkAccessManager`）、表单校验、状态反馈 |
| 通信 | HTTP/1.1，`application/json; charset=utf-8`，字段名 **snake_case** | 统一封装请求构造、四字段解析、5 s 超时、错误分流 |
| 服务端 | C++17，自研 HTTP 解析（`http/HttpParser`、`HttpServer`、`ApiRouter`） | 严格校验未知/重复字段（多传即 400）；客户端不要发送多余字段 |
| 数据 | SQLite 3（`bbs.db`），UTF-8，`PRAGMA foreign_keys=ON`、`busy_timeout=2000` | 503 可能来自锁等待/数据库不可用，前端需可重试提示 |
| 部署 | 服务端监听 `0.0.0.0:8080`，客户端连 `127.0.0.1:8080`（`config.ini`） | 基址应做成可配置项，默认 `http://127.0.0.1:8080/api/v1` |
| 工程 | Git、Visio（`.vsdx` 源图）、Word（OLE 嵌入） | 客户端代码需按目录体现"交互/通信"职责（NFR-006） |

### 2.3 服务端分层（前端需理解的边界）

| 层 | 目录 | 职责 | 与前端的关系 |
|---|---|---|---|
| HTTP 协议层 | `http/` | 路由匹配、请求头/体校验、JSON 解析与序列化、统一信封、错误码 | **前端唯一交互面**：只认 28 条路由与四字段信封 |
| 业务层 | `service/` | 权限校验（`PermissionService`）、字段校验（`Validation`）、事务与业务规则 | 前端"隐藏入口"只是体验优化，**服务端始终独立鉴权** |
| 数据层 | `dao/` | 7 表 CRUD、投影查询、日志追加 | 前端不可见，但字段语义由该层决定 |
| 公共层 | `common/` | 配置、加密（PBKDF2）、日志、类型、工具 | 密码、盐、令牌相关细节前端一律不接触 |

---

## 3. 代码包现状核查（已解压实查）

解压路径：`C:\Users\Administrator\Desktop\课设\BSS--zb\BSS论坛\`

```
BSS论坛.vcxproj / main.cpp / config.ini
├─ common/   Config、Crypto、Error、Logger、Types、Util
├─ dao/      DatabaseManager、User/Board/Post/Reply/ModeratorAssignment/ModerationLog Repository、Entities、Projections
├─ http/     HttpServer、HttpParser、HttpMessage、ApiRouter、JsonUtil、JsonWriter
├─ service/  Auth/Board/Post/Reply/Moderation/Permission Service、Validation
├─ sql/      bbs_schema.sql、bbs_seed.sql
└─ third_party/  json.hpp、sqlite3.h/.c
```

核查结论：

1. **无任何前端代码**（无 `.ui`、`.qrc`、`.qml`、无 `client/` 或 `ui/` 目录），前端 100% 需新建工程实现。
2. **28 条路由已注册**：`POST /auth/register`、`POST /auth/login`、`POST /auth/logout`、`GET|PUT /users/me`、`PUT /users/me/password`、`GET /boards`、`GET /boards/{id}/posts`、`POST /boards`、`PUT /boards/{id}`、`PUT /boards/{id}/status`、`POST /boards/{id}/posts`、`GET /posts/search`、`GET /posts/{id}`、`PUT|DELETE /posts/{id}`、`GET|POST /posts/{id}/replies`、`PUT|DELETE /replies/{id}`、`GET /admin/users`、`PUT /admin/users/{id}/status`、`PUT|DELETE /admin/boards/{board_id}/moderators/{user_id}`、`POST /moderation/posts/{id}/removal`、`POST /moderation/replies/{id}/removal`、`PUT /moderation/posts/{id}/pinned`、`PUT /moderation/posts/{id}/featured`。
3. **演示种子数据已就绪**（`bbs_seed.sql`，幂等）：2 角色、5 账号、3 版块（其中「站务公告」为停用）、5 帖子（含 1 条已移除、1 条置顶、1 条加精）、6 回复（含 1 条已删除）、1 项版主授权、7 条管理记录。**前端可直接按此数据准备空态/非空态/停用态/已删除态的展示用例**。

演示账号（来源 `bbs_seed.sql` 头部注释）：

| 用户名 | 密码 | 角色/状态 | 用途 |
|---|---|---|---|
| `admin07` | `AdminDemo2026!` | admin，正常 | 管理员全流程（版块、账号、授权） |
| `moderator07` | `ModDemo2026!` | member + 「技术交流」版主 | 版主巡查、置顶/加精 |
| `alice07` | `AliceDemo2026!` | member，正常 | 作者侧编辑/删除、普通浏览 |
| `bob07` | `BobDemo2026!` | member，正常 | 越权校验（编辑他人内容应 403） |
| `disabled07` | `DisabledDemo2026!` | member，停用 | 停用账号登录失败提示 |

---

## 4. 前端需实现的页面/模块清单

> 下表「接口」列给出该页面**必须**调用的接口编号（编号与 API 交接规范 §3.3 一致，见本文 §6.3）。页面命名与分区为**建议**，功能范围来自 SRS IF-001 与 FR 清单，属文档规定。

| 编号 | 页面/模块 | 核心内容与控件 | 依赖接口 | 角色可见性 |
|---|---|---|---|---|
| P01 | 登录页 | 用户名、密码（遮蔽输入）、登录按钮、跳注册、连接失败提示 | 02 | 未登录 |
| P02 | 注册页 | 用户名、密码、确认密码、提交、返回登录；注册成功提示并转登录 | 01 | 未登录 |
| P03 | 主框架（首页壳） | 顶部栏（当前昵称、角色标识、退出）、左侧版块导航、内容区、全局状态提示条（加载中/成功/失败） | 04、07、03 | 已登录 |
| P04 | 版块列表 | 版块名、简介、状态标识（**开放/停用**，停用须可见但不误导）、进入版块、管理员"新建版块"入口 | 07 | 已登录（新建仅 admin） |
| P05 | 版块帖子列表 | 帖子标题、作者昵称、发布时间、可见回复数、置顶/加精标记；排序＝置顶优先 + 发布时间倒序；空列表提示 + 发帖入口 | 08、07 | 已登录（发帖需版块开放） |
| P06 | 帖子详情页 | 标题、作者、时间、正文（纯文本）、编辑/删除入口（仅作者）、回复列表（按时间升序）、回复输入框、返回列表 | 13、18、19、16、17、27、28、25 | 已登录；管理入口按权限显示 |
| P07 | 发帖/编辑帖子页（或对话框） | 标题（1–100）、正文（1–5000，多行纯文本）、字数计数、提交/取消；错误定位到字段并保留输入 | 12、14 | 已登录（作者本人可编辑） |
| P08 | 搜索页 | 关键词（0–100）、版块下拉（含"全部"）、起始日/结束日（日期选择器）、排序（时间/热度）、搜索、重置条件；空结果 + 重设入口 | 20、07 | 已登录 |
| P09 | 个人中心 | 用户名（只读）、昵称、基础角色、账号状态、注册时间、当前版主授权列表；昵称修改；密码修改（当前密码+新密码） | 04、05、06 | 已登录 |
| P10 | 账号管理（管理员） | 账号列表：用户名、昵称、角色、状态、授权；停用/恢复按钮（**管理员账号不可操作**） | 21、22 | admin |
| P11 | 版块管理（管理员） | 版块列表、新建/修改（名称、简介）、停用/启用 | 07、09、10、11 | admin |
| P12 | 版主授权（管理员） | 选择账号 + 版块 → 授予；已有授权列表 → 撤销；重复授权/无可撤销的冲突提示 | 21、07、23、24 | admin |
| P13 | 内容处置（版主/管理员） | 帖子/回复"移除"：原因输入（1–200）+ 二次确认；帖子置顶/撤销置顶、加精/撤销加精开关 | 25、26、27、28 | moderator / admin |

**入口与导航约束（NFR-005，文档规定）**：从已登录首页出发，进入①发帖编辑、②回复输入、③搜索输入、④个人资料编辑，各自**最多 3 次界面操作**（列表选择计入，文字输入与最终提交不计）。

---

## 5. 页面交互与业务规则要点

### 5.1 认证与会话

| 规则 | 内容 | 前端落地 |
|---|---|---|
| 令牌传递 | 仅 `Authorization: Bearer <token>`；H0 接口（注册/登录）**不得**携带 Authorization，携带即 400 | 有活动会话时先调用 03 退出，再切登录/注册页 |
| 令牌生命周期 | 自签发起 8 小时，`expires_at` 固定；正常访问不续期 | 展示"登录已过期"提示；不实现静默续期 |
| 令牌存续范围 | 仅在内存，禁止写 URL、日志、界面、配置文件 | 通信模块私有成员持有；退出即清空 |
| 会话失效场景 | 退出、到期、改密、账号停用、服务端重启 | 任一 401 → 清理本地会话 → 回登录页（改密成功后按 401 处理） |
| 登录失败提示 | 凭据错误 → "账号或密码错误"（401）；凭据正确但账号停用 → "账号已停用"（401） | 两种文案区分，不可混用 |
| 注册规则 | 固定创建 `member`；昵称由服务端取用户名；重复用户名 409 | 注册成功后可自动跳登录页并回填用户名 |

### 5.2 列表、排序与可见性

| 规则 | 内容 |
|---|---|
| 版块默认列表排序 | `is_pinned` 降序 → `created_at` 降序 → `post_id` 降序（**置顶优先**） |
| 搜索排序 `time` | `created_at` 降序 → `post_id` 降序 |
| 搜索排序 `heat` | `reply_count` 降序 → 再按 `time` 规则；**置顶不影响搜索顺序**，加精仅显示标记 |
| 回复顺序 | 按创建时间升序，同刻按 `reply_id` 升序（服务端已排好，前端不得二次乱序） |
| 可见性 | 列表/检索只返回 `status=1` 的帖子；回复可见要求**自身与父帖均可见**；帖子被删后详情、检索、新增回复全部不可用（404） |
| 停用版块 | 版块停用后：旧帖**仍可读**、本人内容仍可管理、版主仍可巡查；**禁止新发帖与新回复**（写请求返回 409）；停用版块可出现于版块列表与搜索结果，**必须在列表中给出状态标识** |
| 计数 | `reply_count` 为查询时可见回复数，非负；`PostDetail.reply_count` 必等于 `replies.length`；回复增删后**重新查询时**热度随之变化 |
| 分页 | 无分页参数，全量返回数组；空结果为 `[]`，前端渲染空态 |

### 5.3 内容编辑与删除

| 规则 | 内容 |
|---|---|
| 作者判定 | 仅作者本人可编辑/删除自己的帖子与回复；**管理员与版主同样受限**（越权 403）。管理侧走"移除"而非"删除" |
| 帖子可编辑字段 | 仅 `title`、`content`；保存后 `updated_at` 更新 |
| 回复可编辑字段 | 仅 `content`（1–1000） |
| 删除语义 | 逻辑删除，前端仅需移除条目并刷新；**删除前必须二次确认**（SRS IF-001、FR-011、FR-013） |
| 管理移除 | 版主/管理员填写原因（1–200）后移除；成功返回 `status=0`；须二次确认 |
| 提交失败保留内容 | 400/403/404/409/500/503 及超时场景**一律保留编辑内容**，并定位错误字段 |
| 重复操作 | 管理动作 `changed=false` 表示目标已是期望状态，不报错、不追日志；前端提示"状态未变化"即可 |

### 5.4 权限与入口显隐

| 角色 | 可见入口 |
|---|---|
| 未登录 | 仅登录、注册 |
| 普通用户（member） | 版块/帖子浏览、搜索、本人发帖回复、本人内容编辑删除、个人中心 |
| 版主（member + 有效授权） | 上述 + 管辖版块内的移除/置顶/加精（`UserProfile.moderator_assignments` 用于判显；**回复的版块归属靠父帖判断**） |
| 管理员（admin） | 全站内容管理 + 版块管理 + 账号管理 + 版主授权；**不可被停用、不可被授权为版主、不可被业务接口创建** |

> 显隐只是体验优化：服务端对每个请求独立鉴权（SRS FR-017）；前端隐藏入口后仍需处理 403 返回。

### 5.5 通信与异常（NFR-003，硬性）

| 场景 | 前端行为 |
|---|---|
| 单请求超时 | 从发出起 **5 s**；超时后停止等待 |
| 写请求超时 | 显示 **"提交超时，请刷新查看结果"**，保留编辑文本，**禁止自动重发**；恢复后先刷新列表/详情核实，再由用户决定是否重提 |
| 读请求超时 | 提示超时并提供"重试/刷新"按钮 |
| 断线 | 与超时分别提示（文案可区分）；连接恢复后刷新当前列表或详情 |
| 提交中 | 禁用对应提交按钮，防止重复提交 |
| 非法响应 | HTTP 状态、`success`、`code` 互相矛盾 → 提示"通信数据异常"，保留编辑内容；不伪造成业务码 |
| 迟到响应 | 只归属原请求，**不得覆盖已切换的页面**（按请求对象关联当前页面） |
| 401 | 清理本地会话、回登录页；改密接口收到 401 同样重新登录 |
| 503 | 提示"数据库暂不可用，请稍后重试"（可与网络故障复用同一重试入口） |

---

## 6. HTTP API 接口约定

### 6.1 基址与请求头

- 基址：`http://127.0.0.1:8080/api/v1`（端口以部署配置为准，建议做成可配置项）。
- 路径**区分大小写**；`{xxx}` 为路径变量，替换为**正十进制 ID**（1–9223372036854775807，不接受符号、小数、指数、前后空白）。
- 路由优先匹配固定路径：`/posts/search` 先于 `/posts/{post_id}`。

| 记号 | 含义 | 要求 |
|---|---|---|
| **H0** | 未登录接口 | 省略 `Authorization`；携带则 400 |
| **H1** | 受保护接口 | 必须 `Authorization: Bearer <token>` |
| **J** | 有请求体 | `Content-Type: application/json`（可附 `charset=utf-8`），体必须是 JSON 对象 |
| **N** | 无请求体 | 省略 `Content-Type`，发送零字节体（GET/DELETE 一律不带体） |

其他约定：`Accept: application/json` 可发；未知或重复的 Query 键、未知或重复的 JSON 键、必填值缺失、N 接口带体 → **400**；H1 缺失/格式非法/令牌无效 → **401**；请求体不接受表单与其他媒体类型。

### 6.2 响应信封与错误映射

成功：

```json
{ "success": true, "code": "OK", "message": "操作成功", "data": { /* 接口规定对象 */ } }
```

失败：

```json
{ "success": false, "code": "FORBIDDEN", "message": "无权管理该版块内容", "data": null }
```

| HTTP | code | 含义 | 前端处理 |
|---|---|---|---|
| 400 | `INVALID_ARGUMENT` | 报文/字段/参数/日期区间非法，或改密时当前密码错误 | 定位字段、保留输入、提示具体原因 |
| 401 | `UNAUTHENTICATED` | 登录凭据错误、账号停用、会话不可用 | 登录页展示对应文案；受保护页面清会话回登录 |
| 403 | `FORBIDDEN` | 无作者/管理员/版块管理权限，或对管理员执行受限操作 | 提示无权限并刷新权限相关入口 |
| 404 | `RESOURCE_UNAVAILABLE` | 路径未注册；目标不存在/已删除/不可访问 | 刷新列表或退出失效详情页 |
| 405 | `METHOD_NOT_ALLOWED` | 路由存在但不支持该方法（含 `Allow` 头） | 属实现缺陷，记录日志 |
| 409 | `DATA_CONFLICT` | 唯一性/授权关联/当前状态冲突（如版块停用后发帖、重复授权） | 提示冲突原因并刷新当前状态 |
| 500 | `INTERNAL_ERROR` | 服务端未预期错误 | 通用失败提示 + 重试入口 |
| 503 | `DATABASE_UNAVAILABLE` | 数据库不可用/锁等待/IO 问题 | "稍后重试"提示 |

约定要点：列表恒在 `data.items`；创建返回 **201**，其余成功 200；`data={}` 表示空对象（退出、改密）；`changed=false` 表示状态未变。

### 6.3 28 个接口清单

| # | 方法 路径 | 头 | 权限 | 请求 | 成功 data |
|---|---|---|---|---|---|
| 01 | `POST /auth/register` | H0+J | 未登录 | `username`、`password` | `RegisteredUser`（201） |
| 02 | `POST /auth/login` | H0+J | 未登录 | `username`、`password` | `LoginResult`（token、expires_at、user） |
| 03 | `POST /auth/logout` | H1+N | 登录用户 | — | `{}`（前端清理本地令牌） |
| 04 | `GET /users/me` | H1+N | 登录用户 | — | `UserProfile` |
| 05 | `PUT /users/me` | H1+J | 登录用户 | `nickname` | `UserProfile` |
| 06 | `PUT /users/me/password` | H1+J | 登录用户 | `current_password`、`new_password` | `{}`（成功后旧会话失效） |
| 07 | `GET /boards` | H1+N | 登录用户 | — | `{items:[BoardView]}`（含停用版块） |
| 08 | `GET /boards/{board_id}/posts` | H1+N | 登录用户 | — | `{items:[PostSummary]}`（置顶优先 + 时间倒序） |
| 09 | `POST /boards` | H1+J | 管理员 | `name`、`description?` | `{changed, board}`（201） |
| 10 | `PUT /boards/{board_id}` | H1+J | 管理员 | `name`、`description`（必填，可 `""`） | `{changed, board}` |
| 11 | `PUT /boards/{board_id}/status` | H1+J | 管理员 | `status`（0/1） | `{changed, board}` |
| 12 | `POST /boards/{board_id}/posts` | H1+J | 登录用户 | `title`、`content` | `PostDetail`（201，`reply_count=0`、`replies=[]`） |
| 13 | `GET /posts/{post_id}` | H1+N | 登录用户 | — | `PostDetail` |
| 14 | `PUT /posts/{post_id}` | H1+J | 作者本人 | `title`、`content` | `PostDetail` |
| 15 | `DELETE /posts/{post_id}` | H1+N | 作者本人 | — | `{post_id, status:0}` |
| 16 | `GET /posts/{post_id}/replies` | H1+N | 登录用户 | — | `{items:[ReplyView]}`（时间升序） |
| 17 | `POST /posts/{post_id}/replies` | H1+J | 登录用户 | `content` | `ReplyView`（201） |
| 18 | `PUT /replies/{reply_id}` | H1+J | 作者本人 | `content` | `ReplyView` |
| 19 | `DELETE /replies/{reply_id}` | H1+N | 作者本人 | — | `{reply_id, status:0}` |
| 20 | `GET /posts/search` | H1+N | 登录用户 | Query：`keyword?`、`board_id?`、`start_date?`、`end_date?`、`sort?` | `{items:[PostSummary]}` |
| 21 | `GET /admin/users` | H1+N | 管理员 | — | `{items:[UserProfile]}` |
| 22 | `PUT /admin/users/{user_id}/status` | H1+J | 管理员 | `status`（0/1） | `{changed, user_id, status}` |
| 23 | `PUT /admin/boards/{board_id}/moderators/{user_id}` | H1+N | 管理员 | — | `{assigned:true, assignment}` |
| 24 | `DELETE /admin/boards/{board_id}/moderators/{user_id}` | H1+N | 管理员 | — | `{assigned:false, user_id, board_id}` |
| 25 | `POST /moderation/posts/{post_id}/removal` | H1+J | 版主/管理员 | `reason`（1–200） | `{post_id, status:0}` |
| 26 | `POST /moderation/replies/{reply_id}/removal` | H1+J | 版主/管理员 | `reason`（1–200） | `{reply_id, status:0}` |
| 27 | `PUT /moderation/posts/{post_id}/pinned` | H1+J | 版主/管理员 | `is_pinned`（boolean） | `{changed, post_id, is_pinned, updated_at}` |
| 28 | `PUT /moderation/posts/{post_id}/featured` | H1+J | 版主/管理员 | `is_featured`（boolean） | `{changed, post_id, is_featured, updated_at}` |

搜索接口要点：

- 三个条件（关键词、版块、日期）**同时生效**；全部省略时返回全部可见帖子。
- `keyword` 对标题与正文做**区分大小写**的普通子串匹配，**不检索回复**；`%`、`_` 按字面字符处理（需百分号编码，如 `100%25_`）。
- `board_id` 指定时必须存在（否则 404）；开放与停用版块均可筛选；停用版块**不从搜索结果中排除**。
- `start_date` / `end_date` 为**部署地本地日历日期** `YYYY-MM-DD`，**含首尾两天**；**客户端不要把日期先转 UTC 再发送**；起始日晚于结束日 → 400。
- `sort` 缺省 `time`；显式空串非法（400）。

### 6.4 字段校验口径（与服务端保持同口径）

| 字段 | 规则 |
|---|---|
| `username` | 4–20 个 **ASCII 字母/数字/下划线**；登录与唯一性**大小写不敏感**（`User_01` 与 `user_01` 冲突）；保留原大小写显示 |
| `password` / `current_password` / `new_password` | 8–64 个 Unicode 码点，**保留首尾空白**，不做规范化 |
| `nickname` / 版块 `name` | 1–30 个码点（版块名 `BINARY` 比较，大小写敏感唯一） |
| `description` | 0–200 个码点（创建可省略＝`""`；修改必填，可用 `""` 清空） |
| `title` | 1–100 个码点 |
| 帖子 `content` | 1–5000 个码点 |
| 回复 `content` | 1–1000 个码点 |
| `keyword` | 0–100 个码点（去首尾空白后为空则跳过条件） |
| `reason` | 1–200 个码点（移除内容必填） |
| `status` | 整数 0/1（账号、版块、内容语义各不相同，见 §7.3） |
| `is_pinned` / `is_featured` | JSON **布尔** true/false，不接受 0/1 或字符串 |

通用口径：长度按 **Unicode 码点**计数；除密码外，必填文本先**去除首尾空白**再判非空与计数；拒绝内嵌 NUL；以服务端校验为准。

### 6.5 类型与时间格式

| 类型 | 约定 |
|---|---|
| ID | JSON 为十进制整数，64 位（`std::int64_t`）；**必须用可保 64 位精度的解析方式**，禁止经 double 中转 |
| 时间 | 服务端返回 UTC 字符串 `YYYY-MM-DDTHH:MM:SSZ`（整秒）；展示时**转本地时区**；搜索日期参数用本地日历日期，不转 UTC |
| 布尔 | `is_pinned`、`is_featured`、`changed`、`assigned` 为 JSON 布尔 |
| 状态 | `status` 为数字 0/1，不自动转换 |
| 空值 | 响应中 `data=null` 仅出现在错误结构；**可选请求字段可省略，传显式 null 非法** |
| 文本 | 纯文本呈现，客户端**不解析/执行 HTML 或脚本** |

---

## 7. 类图与数据库设计中对前端有影响的数据结构

### 7.1 公开响应对象（前端 DTO 全清单）

**用户与版块**

| 对象 | 字段 | 前端用途 |
|---|---|---|
| `AuthorView` | `user_id`、`nickname` | 帖子/回复作者；昵称随账号当前值变化 |
| `RegisteredUser` | `user_id`、`username`、`nickname`、`role_code`（固定 `member`） | 注册成功回显 |
| `UserProfile` | `user_id`、`username`、`nickname`、`role_code`、`status`、`registered_at`、`moderator_assignments[]` | 个人中心、账号管理列表；`moderator_assignments` 按 `board_id` 升序，无授权为 `[]` |
| `LoginResult` | `token`、`expires_at`、`user`(UserProfile) | 登录后内存保存令牌与资料 |
| `BoardView` | `board_id`、`name`、`description`、`status`、`created_at`、`updated_at` | 版块列表/导航；停用版块也出现在列表 |
| `ModeratorAssignment` | `user_id`、`board_id`、`assigned_at` | 授权展示；撤权后从数组消失 |

**帖子与回复**

| 对象 | 字段 | 前端用途 |
|---|---|---|
| `PostSummary` | `post_id`、`board_id`、`author`(AuthorView)、`title`、`status`、`is_pinned`、`is_featured`、`created_at`、`updated_at`、`reply_count` | 列表/搜索结果；`reply_count` 为可见回复数 |
| `PostDetail` | PostSummary 全部字段（平铺）+ `content` + `replies[]`(ReplyView) | 详情页；`replies` 已排序，`reply_count == replies.length` |
| `ReplyView` | `reply_id`、`post_id`、`author`、`content`、`status`、`created_at`、`updated_at` | 回复列表；回显时 `status=1` |

**管理结果**

| 用途 | data |
|---|---|
| 版块管理 | `changed`、`board`(BoardView) |
| 账号启停 | `changed`、`user_id`、`status` |
| 授予版主 | `assigned=true`、`assignment`(ModeratorAssignment) |
| 撤销版主 | `assigned=false`、`user_id`、`board_id` |
| 删帖/移除帖子 | `post_id`、`status=0` |
| 删回复/移除回复 | `reply_id`、`status=0` |
| 置顶/加精 | `changed`、`post_id`、`is_pinned`/`is_featured`、`updated_at` |
| 退出/改密 | `{}` |

### 7.2 数据库 7 表 → 前端语义映射

| 表 | 关键字段 | 前端可见影响 |
|---|---|---|
| `roles` | `role_id`、`role_code`（`member`/`admin`）、`role_name` | 仅体现为 `role_code`；**版主不是角色**，由授权动态判定 |
| `users` | `username`(唯一, NOCASE)、`nickname`、`role_id`、`status`、`session_version`、`registered_at` | `session_version` 递增导致**旧令牌全部失效**（改密/停用后必须重新登录）；密码哈希/盐/迭代次数**永不下发** |
| `boards` | `name`(唯一, BINARY)、`description`、`status`、`created_at`、`updated_at` | 停用仅暂停新增，历史可读 |
| `posts` | `board_id`、`author_id`、`title`、`content`、`status`、`is_pinned`、`is_featured`、时间戳 | 前端拿到的是 0/1 数字状态 + 布尔标记（服务端已转换） |
| `replies` | `post_id`、`author_id`、`content`、`status`、时间戳 | 单条回复删除不影响其他回复 |
| `board_moderators` | `(user_id, board_id)` 联合主键、`assigned_at` | 一人多版块；同账号+版块不可重复授权（409） |
| `moderation_logs` | `actor_id`、`action`（14 类）、四类目标外键、`reason`、`created_at` | **无查询接口**，前端不展示日志列表 |

### 7.3 状态枚举语义（易混淆，务必区分）

| 字段 | 0 | 1 | 说明 |
|---|---|---|---|
| `users.status` | 停用 | 正常 | 停用后已有会话失效、旧内容按原状态显示；恢复需重新登录 |
| `boards.status` | 停用 | 开放 | 停用：旧帖可读、本人可管理、版主可巡查；禁新发帖/新回复 |
| `posts.status` / `replies.status` | 逻辑删除（不可见） | 可见 | 帖子删除后其回复**行保留但随父帖隐藏** |
| `posts.is_pinned` / `is_featured` | 否 | 是 | 置顶只改版块默认列表顺序；加精只显示标识 |

### 7.4 类图（业务视图）到前端职责的对应

| 类图中的概念 | 前端职责落点 |
|---|---|
| 用户 / 角色 / 版主授权 | 当前用户状态、入口显隐、个人中心授权列表 |
| 版块 / 帖子 / 回复 | 三级导航（版块 → 帖子列表 → 详情 + 回复） |
| 管理记录（ModerationLog） | 前端**无读取接口**，仅在"移除需填原因"这一交互上体现 |
| 权限判定（服务端 `PermissionService`） | 前端仅做展示层判显，实际以 403 为准 |

> 说明（文档规定与建议的区分）：设计文档在客户端侧只规定到**职责与通信要求**（异步调用、5 s 超时、令牌内存持有、按 HTTP+code 分流、页面状态刷新），**未细化到客户端类级设计**；`详细设计说明书` 第 2 章的 20 个核心类属服务端分层类（Service/Repository/Validation 等）。因此 P01–P13 的页面划分与下列类划分均为**建议**。

---

## 8. UI / 交互规范与约束

### 8.1 界面需求（SRS IF-001，文档规定）

1. 必须提供：登录注册、版块与帖子列表、详情与回复、搜索、个人中心、**角色对应的管理入口**。
2. **加载中、空结果、成功、失败**四种状态必须有可区分反馈（不可仅用颜色区分文字提示）。
3. **删除前要求确认**（删帖、删回复、移除违规内容、停用/恢复账号、撤销授权建议同样确认）。
4. 支持**中文输入与显示**；论坛内容以**纯文本**呈现（不渲染富文本/HTML）。
5. 接口错误提示**不得暴露**数据库连接信息、SQL 语句、内部调用细节、密码/哈希/盐/令牌。

### 8.2 非功能约束（对前端有直接约束力）

| 编号 | 要求 | 前端检查点 |
|---|---|---|
| NFR-001 | 版块列表、详情、搜索、发帖各测 10 次，≥9 次 **2 s 内**完成（客户端发出请求至页面显示结果） | 列表渲染不得阻塞主线程；数据到达即刷新 |
| NFR-002 | 密码以哈希保存；响应/日志不含密码、哈希、盐；**客户端遮蔽密码输入**；令牌不得在界面与日志显示 | 密码框 `EchoMode=Password`；日志脱敏（不打印 Authorization） |
| NFR-003 | 5 s 超时；写超时提示"提交超时，请刷新查看结果"并保留文本；**禁止自动重发**；断线/超时分别提示 | 通信模块统一超时策略 |
| NFR-004 | 无效输入、失效会话、越权、引用失效不产生写入 | 失败后不得本地"乐观插入"数据 |
| NFR-005 | 四类关键入口 ≤3 次界面操作 | 首页提供直达入口（发帖、搜索、我的） |
| NFR-006 | 职责可按工程目录辨认 | 建议 `ui/` 与 `net/` 分离（建议） |
| NFR-007 | Windows 10/11 可启动、连接、登录、浏览、写入并刷新 | 记录 Qt 与编译器版本 |

### 8.3 交互细节约束（源自 FR 条款，属强制）

| 场景 | 约束 |
|---|---|
| 发帖 | 版块停用时拒绝写入（409），**保留可修正的编辑内容**；空白/超长拒绝并定位字段 |
| 编辑他人内容 | 403，原内容保持不变 |
| 帖子已被删除时打开详情 | 404 → 提示"内容不可用"并返回/刷新列表 |
| 无搜索结果 | 显示空态 + **"重设条件"入口** |
| 日期逆序 | 400 → 提示"起始日期不得晚于结束日期"（建议前端提交前先做本地校验） |
| 重复授权 / 无可撤销授权 | 409 / 404 → 提示并刷新授权列表 |
| 对管理员账号操作 | 403 → 管理员条目应**禁用**停用按钮与授权入口 |
| 停用账号仍可被撤权 | 撤销入口对停用账号保持可用 |
| 退出时断线 | 可直接清理本地令牌（服务端令牌按有效期失效） |
| 改密成功 | 旧令牌与小版本会话全部失效 → 回登录页重新登录 |

---

## 9. 建议的客户端实现要点（非文档强制）

1. **工程分层建议**：`ui/`（页面与对话框）、`net/`（`ApiClient`：请求构造、信封解析、超时、错误映射）、`model/`（DTO + JSON 转换）、`session/`（当前用户与令牌、权限判显）。满足 NFR-006 的"职责可按目录辨认"。
2. **单一请求入口**：所有请求经一个 `ApiClient::request(method, path, needAuth, jsonBody, callback)`，集中处理 H0/H1/J/N 头组合、5 s 超时、四字段校验、401 清理会话、错误文案映射，避免各页面各自拼装。
3. **请求-页面绑定**：为每个请求生成自增上下文 ID，回调时校验其归属页面/控件是否仍存在，防止迟到响应覆盖新页面（API §3.1.3）。
4. **状态机**：每个列表/详情维护 `Idle → Loading → Loaded / Empty / Error` 四态，与 IF-001 的四类反馈一一对应。
5. **提交保护**：提交按钮点击后立即置灰并显示"提交中…"，收到响应（含超时）后恢复；写请求超时后按钮恢复但**不自动重提交**。
6. **时间显示**：统一封装 `UTC ISO → 本地时间` 格式化（`yyyy-MM-dd HH:mm`），搜索日期控件输出 `yyyy-MM-dd` 原文，不做时区换算。
7. **ID 精度**：C++ 侧用 `qint64`/`std::int64_t` 解析，严禁经 `double`。
8. **输入校验复用**：按 §6.4 口径写一套前端校验器，与服务端同口径（码点计数用 `QString::size()` 的 UTF-16 需注意补充平面字符；建议用码点迭代计数）。
9. **权限判显**：`role_code=="admin"` → 显示版块/账号/授权管理；`moderator_assignments` 非空 → 对应版块的巡查入口（帖子详情按 `board_id` 匹配；回复按其父帖 `board_id` 匹配）。
10. **联调用例基线**：直接用 `bbs_seed.sql` 的 5 账号 / 3 版块（含 1 个停用）/ 5 帖子（含置顶、加精、已移除）/ 6 回复（含 1 条已删除）验证各状态渲染。

---

## 10. 前端验收自查清单（对应 V-01~V-13）

- [ ] 注册→登录→退出闭环；重复用户名、非法账号、错误密码、停用账号各自提示正确（V-01）
- [ ] 个人资料修改与改密：错填当前密码不生效；改密后旧会话失效并可重新登录（V-02）
- [ ] 版块列表含停用标识；管理员可建/改/停用/启用；重名 409（V-03）
- [ ] 发帖后详情可达；作者可编辑/删除；停用版块发帖被拒且内容保留（V-04）
- [ ] 回复按时间升序；本人回复可编辑/删除；对已删帖子回复失败（V-05）
- [ ] 搜索：关键词+版块+日期组合、时间/热度排序、空结果空态、逆序日期提示、已删帖不出现、停用版块旧帖可检索（V-06）
- [ ] 越权路径返回 403；版主入口仅在管辖版块出现；管理员全站可管理（V-07）
- [ ] 置顶影响版块默认顺序；加精仅显示标记；撤销后展示同步（V-08）
- [ ] 中文输入/显示、空结果、错误反馈、删除确认、关键入口步数走查（V-09）
- [ ] 断线/超时/数据库不可用三类提示可区分，编辑内容保留，超时不自动重发（V-10）
- [ ] 四类操作 2 s 内完成率 ≥90%（V-11）
- [ ] 密码遮蔽、令牌不入日志/界面（V-12）
- [ ] Windows 10/11 启动→连接→登录→浏览→写入→刷新（V-13）

---

## 11. 待确认与风险点（建议与后端/组长对齐）

| 编号 | 事项 | 影响 | 建议 |
|---|---|---|---|
| Q1 | 列表不分页，1000 帖量级需一次渲染 | 列表性能（NFR-001） | 前端做延迟渲染/虚拟列表或分批刷新（不改协议） |
| Q2 | `keyword` 区分大小写（`instr` 字面匹配） | 用户预期偏差 | 界面提示"区分大小写"，或前端明确不做大小写归一 |
| Q3 | 管理记录（`moderation_logs`）无查询接口 | 无法在界面展示处置历史 | 按文档不实现日志页面；如需展示须新增接口（超出当前约定） |
| Q4 | 无头像、富文本、附件、私信 | 界面不可自行扩展此类控件 | 严格按纯文本 + 昵称展示 |
| Q5 | `UserProfile.moderator_assignments` 只含版块 ID，不含版块名 | 授权列表需二次映射名称 | 结合 `GET /boards` 建立 `board_id → name` 对照 |
| Q6 | 版块详情字段在 08 接口中未随列表返回 | 帖子列表页标题栏需名称 | 由 P03 已加载的版块列表缓存提供；404 时提示返回版块列表 |
| Q7 | 管理员账号不可停用/不可授权，但仍在账号列表中 | 按钮禁用逻辑 | 按 `role_code=="admin"` 禁用并给出原因提示 |
| Q8 | 服务端重启令牌全失效 | 联调期频繁掉登录 | 客户端统一 401 兜底，提示"会话已失效，请重新登录" |
| Q9 | 日期筛选按**部署地**本地日历 | 跨时区部署易错 | 课程为单机部署，直接发本地日期；文档一致即可 |
| Q10 | 客户端类级设计文档缺失 | 类划分口径不统一 | 采用本文 §9 建议分层，评审时与组内确认 |

---

## 附：文档溯源索引

| 文档 | 本摘要引用章节 |
|---|---|
| 软件需求规格说明书 | §3.4 FR-001~FR-021、§3.5 DR、§3.6 IF-001~IF-003 与表 3-5 字段校验、§3.7 NFR-001~007、§4 验证 V-01~V-13 |
| 概要设计说明书 | 架构与分层、六大功能模块、七表数据模型、客户端职责、接口划分、三人分工 |
| 详细设计说明书 | 类型与命名（snake_case）、统一通信标准、异常与超时分支、事务边界、性能目标、客户端职责 |
| 类图与数据库设计说明书 | §3 概念 E-R、§4 七表字段与约束、表 4-8 管理动作映射、§4.9 删除策略、§5 索引与查询 |
| HTTP API 接口交接规范 | §3.1 统一规范（H0/H1/J/N、表 3-3 错误码、§3.1.3 超时与刷新）、§3.2 对象与校验、§3.3 接口 01–28、§3.4 内部模块职责 |
| 需求规格说明书组内评审记录 | 分工确认（陶彦恒：Qt/C++ 桌面界面、通信模块、界面测试）、R-01~R-05 整改 |
| 代码包 BSS--zb | 目录结构、`ApiRouter` 28 路由、`config.ini`、`bbs_schema.sql`、`bbs_seed.sql` 演示数据 |
*（内容由AI生成，仅供参考）*
