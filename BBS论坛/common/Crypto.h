// 密码派生与随机令牌：PBKDF2-HMAC-SHA256（Windows CNG）
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace bbs {
namespace crypto {

// 密码派生参数（与数据库 password_iterations 保持一致）
constexpr std::int64_t kPasswordIterations = 600000;
constexpr std::size_t kSaltBytes = 16;
constexpr std::size_t kDerivedKeyBytes = 32;

// 生成密码学安全随机字节
std::vector<std::uint8_t> randomBytes(std::size_t count);

// PBKDF2-HMAC-SHA256，输出 32 字节
std::vector<std::uint8_t> deriveKey(const std::string& password,
                                    const std::vector<std::uint8_t>& salt,
                                    std::int64_t iterations);

// 小写十六进制编码
std::string hexEncode(const std::vector<std::uint8_t>& bytes);

// 十六进制解码；输入非法返回 false
bool hexDecode(const std::string& hex, std::vector<std::uint8_t>& bytes);

// 常量时间比较，避免旁路泄露
bool constantTimeEquals(const std::string& a, const std::string& b);

// 生成 32 字节随机会话令牌（64 位小写十六进制）
std::string randomToken();

} // namespace crypto
} // namespace bbs
