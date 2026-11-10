#pragma once

// 网络层：全应用唯一的 HTTP 出口。
// 严格实现《HTTP API 接口交接规范》：
//  - 基址 http://127.0.0.1:8080/api/v1
//  - 头组合：H0（无鉴权）/ H1（Authorization: Bearer <64hex>）/ J（Content-Type: application/json）/ N（无请求体）
//  - 统一四字段信封 {success, code, message, data}
//  - 5 秒超时；HTTP 状态与业务 code 双通道错误映射
//  - 401 立即清空会话令牌并通知上层
//  - 请求上下文绑定：owner 页面已切走时丢弃迟到响应，避免覆盖当前页面

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;
class QWidget;

namespace bbs {

enum class FailureKind {
    None,
    Network,         // 连接失败 / DNS / 传输中断
    Timeout,         // 5 秒超时
    InvalidResponse, // 非 JSON、信封字段缺失、success 与状态码矛盾
    Business         // 服务端明确返回 success=false
};

struct ApiResult
{
    bool ok = false;
    int httpStatus = 0;
    QString code;
    QString message;
    QJsonObject data;
    FailureKind kind = FailureKind::None;

    // 该请求是否为写请求（非 GET）。NFR-003 要求写请求超时的文案与读请求区分：
    // 写超时必须提示"结果未知、请刷新核实"，且严禁自动重发。
    bool write = false;

    // 供界面直接展示的错误文案（服务端 message 优先，其次按 code 兜底）
    QString displayMessage() const;
    bool isTimeout() const { return kind == FailureKind::Timeout; }
};

class ApiClient : public QObject
{
    Q_OBJECT
public:
    using Callback = std::function<void(const ApiResult&)>;

    explicit ApiClient(QObject* parent = nullptr);

    // 服务端基址，默认 http://127.0.0.1:8080/api/v1（可由 BBS_API_BASE 环境变量覆盖）
    void setBaseUrl(const QString& baseUrl);
    QString baseUrl() const { return m_baseUrl; }

    // 令牌仅保存在内存，不落盘、不写日志
    void setToken(const QString& token);
    void clearToken();
    bool hasToken() const { return !m_token.isEmpty(); }

    // 当前前台页面指针：用于判定迟到响应是否已被页面切换淘汰
    void setActivePage(QWidget* page);
    QWidget* activePage() const { return m_activePage; }

    // ---- 1~3 认证与账号（P01/P02/主框架） ----
    void registerAccount(const QString& username, const QString& password, QObject* owner,
                         const Callback& callback);
    void login(const QString& username, const QString& password, QObject* owner,
               const Callback& callback);
    void logout(QObject* owner, const Callback& callback);

    // ---- 4~6 个人资料（P09） ----
    void fetchMe(QObject* owner, const Callback& callback);
    void updateNickname(const QString& nickname, QObject* owner, const Callback& callback);
    void changePassword(const QString& currentPassword, const QString& newPassword,
                        QObject* owner, const Callback& callback);

    // ---- 7~8 版块浏览（P04） ----
    void listBoards(QObject* owner, const Callback& callback);
    void listBoardPosts(qint64 boardId, QObject* owner, const Callback& callback);

    // ---- 9~11 版块管理（P11，仅管理员） ----
    void createBoard(const QString& name, const QString& description, QObject* owner,
                     const Callback& callback);
    void updateBoard(qint64 boardId, const QString& name, const QString& description,
                     QObject* owner, const Callback& callback);
    void setBoardStatus(qint64 boardId, int status, QObject* owner, const Callback& callback);

    // ---- 12~15 帖子（P05/P06/P07） ----
    void createPost(qint64 boardId, const QString& title, const QString& content, QObject* owner,
                    const Callback& callback);
    void fetchPost(qint64 postId, QObject* owner, const Callback& callback);
    void updatePost(qint64 postId, const QString& title, const QString& content, QObject* owner,
                    const Callback& callback);
    void deletePost(qint64 postId, QObject* owner, const Callback& callback);

    // ---- 16~19 回复（P06） ----
    void listReplies(qint64 postId, QObject* owner, const Callback& callback);
    void createReply(qint64 postId, const QString& content, QObject* owner,
                     const Callback& callback);
    void updateReply(qint64 replyId, const QString& content, QObject* owner,
                     const Callback& callback);
    void deleteReply(qint64 replyId, QObject* owner, const Callback& callback);

    // ---- 20 搜索（P08） ----
    // boardId < 0 表示不限版块；startDate / endDate 为空表示不限（yyyy-MM-dd）
    void searchPosts(const QString& keyword, qint64 boardId, const QString& startDate,
                     const QString& endDate, const QString& sort, QObject* owner,
                     const Callback& callback);

    // ---- 21~22 账号管理（P10，仅管理员） ----
    void adminListUsers(QObject* owner, const Callback& callback);
    void adminSetUserStatus(qint64 userId, int status, QObject* owner, const Callback& callback);

    // ---- 23~24 版主授权（P12，仅管理员） ----
    void adminGrantModerator(qint64 boardId, qint64 userId, QObject* owner,
                             const Callback& callback);
    void adminRevokeModerator(qint64 boardId, qint64 userId, QObject* owner,
                              const Callback& callback);

    // ---- 25~28 内容处置（P13，管理员或该版块版主） ----
    void moderateRemovePost(qint64 postId, const QString& reason, QObject* owner,
                            const Callback& callback);
    void moderateRemoveReply(qint64 replyId, const QString& reason, QObject* owner,
                             const Callback& callback);
    void moderateSetPinned(qint64 postId, bool pinned, QObject* owner, const Callback& callback);
    void moderateSetFeatured(qint64 postId, bool featured, QObject* owner,
                             const Callback& callback);

signals:
    // 任意请求收到 401：令牌已清空，上层需回到登录页
    void sessionExpired();

private:
    struct RequestOptions
    {
        bool auth = true;   // H1 / H0
        bool hasBody = false; // J / N
        QJsonObject body;
        QList<QPair<QString, QString>> query;
    };

    struct Pending
    {
        // 用 QPointer 弱引用：owner 页面在请求返回前被销毁时自动置空，避免野指针
        QPointer<QObject> owner;
        QPointer<QWidget> page;
        Callback callback;
        QTimer* timer = nullptr;
        bool write = false;
    };

    quint64 send(const QString& method, const QString& path, const RequestOptions& options,
                 QObject* owner, const Callback& callback);
    void finish(quint64 id, QNetworkReply* reply);
    void parseEnvelope(int status, const QByteArray& payload, ApiResult* result) const;

    QNetworkAccessManager* m_nam = nullptr;
    QString m_baseUrl;
    QString m_token;
    QWidget* m_activePage = nullptr;
    quint64 m_nextId = 1;
    QHash<quint64, Pending> m_pending;
    QHash<quint64, bool> m_timedOut;
};

} // namespace bbs
