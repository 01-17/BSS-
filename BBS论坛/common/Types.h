// 服务端公共类型定义：标识、文本、时间及值域枚举
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace bbs {

using Id = std::int64_t;      // 正整数标识
using RowCount = std::int64_t; // 受影响行数
using Text = std::string;     // UTF-8 文本
using Time = std::string;     // 内部 UTC 秒级文本 YYYY-MM-DD HH:MM:SS

// 账号状态：0 停用、1 正常
enum class AccountStatus { Disabled = 0, Active = 1 };

// 版块状态：0 停用、1 开放
enum class BoardStatus { Disabled = 0, Open = 1 };

// 内容状态：0 删除、1 可见
enum class ContentStatus { Deleted = 0, Visible = 1 };

// 搜索排序
enum class SearchSort { Time, Heat };

// 搜索筛选条件（外部输入，日期为部署地本地日期）
struct SearchFilter {
    Text keyword;
    std::optional<Id> boardId;
    std::optional<Text> startDate;
    std::optional<Text> endDate;
    SearchSort sort = SearchSort::Time;
};

// 会话快照：令牌、用户、会话版本、到期时刻
struct SessionContext {
    Text token;
    Id userId = 0;
    std::int64_t sessionVersion = 0;
    Time expiresAt;
};

// 经核验的当前主体
struct CurrentUser {
    Id userId = 0;
    Text roleCode;
    std::int64_t sessionVersion = 0;

    bool isAdmin() const { return roleCode == "admin"; }
};

} // namespace bbs
