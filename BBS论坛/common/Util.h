// 通用工具：UTC 时间、本地日期换算、UTF-8 校验与文本规范化、URL 百分号解码
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace bbs {
namespace util {

// 当前 UTC 时间，格式 YYYY-MM-DD HH:MM:SS
std::string nowUtc();

// 当前时间加指定秒数后的 UTC 文本
std::string utcAfterSeconds(std::int64_t seconds);

// 内部 UTC 文本转 JSON 输出格式 YYYY-MM-DDTHH:MM:SSZ
// 入参非法时原样返回，避免序列化阶段抛错
std::string toJsonTime(const std::string& utc);

// 校验 YYYY-MM-DD 为真实公历日期
bool validDate(const std::string& date);

// 本地起始日零时转 UTC 文本；失败返回 std::nullopt
std::optional<std::string> localDateStartUtc(const std::string& date);

// 本地结束日次日零时转 UTC 文本；失败返回 std::nullopt
std::optional<std::string> localDateNextDayUtc(const std::string& date);

// 严格校验 UTF-8（拒绝过长编码、代理区、超出 U+10FFFF）
bool isValidUtf8(const std::string& text);

// 按 Unicode 码点计数
std::size_t codePointCount(const std::string& text);

// 去除 Unicode 首尾空白（保留内部空白与换行）
std::string trimUnicode(const std::string& text);

// 是否含嵌入 NUL
bool containsNul(const std::string& text);

// 用户名校验：4–20 个 ASCII 字母、数字或下划线
bool isUsername(const std::string& text);

// ASCII 小写化
std::string toLowerAscii(const std::string& text);

// ASCII 大小写不敏感比较
bool equalsNoCaseAscii(const std::string& a, const std::string& b);

// URL 百分号解码：只解码一次，不把加号转换为空格
// 非法转义或解码后非 UTF-8 返回 false
bool percentDecode(const std::string& input, std::string& output);

} // namespace util
} // namespace bbs
