// 业务字段校验与规范化（服务层统一口径）
#pragma once

#include <cstddef>
#include <string>

#include "../common/Types.h"

namespace bbs {
namespace validation {

// 通用文本：校验 UTF-8、拒绝 NUL、去 Unicode 首尾空白后按码点计数
Text text(const Text& value, const char* field, std::size_t minLength,
          std::size_t maxLength);

// 密码：保留原始字符与空白，只校验码点长度
Text password(const Text& value, const char* field);

// 用户名：去首尾空白后须为 4–20 个 ASCII 字母、数字或下划线
Text username(const Text& value);

// 版块名称：去首尾空白后 1–30 个码点
Text boardName(const Text& value);

// 简介：去首尾空白后 0–200 个码点
Text boardDescription(const Text& value);

// 搜索关键词：去首尾空白后 0–100 个码点
Text keyword(const Text& value);

// 状态值：仅接受 0 或 1
int status(const int& value, const char* field);

} // namespace validation
} // namespace bbs
