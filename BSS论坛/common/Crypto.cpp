#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>

#include "Crypto.h"
#include "Error.h"

#pragma comment(lib, "bcrypt.lib")

namespace bbs {
namespace crypto {

namespace {

class AlgorithmHandle {
public:
    explicit AlgorithmHandle(LPCWSTR algorithm) {
        NTSTATUS status = BCryptOpenAlgorithmProvider(&handle_, algorithm, nullptr,
                                                      BCRYPT_ALG_HANDLE_HMAC_FLAG);
        if (status < 0) handle_ = nullptr;
    }
    ~AlgorithmHandle() {
        if (handle_) BCryptCloseAlgorithmProvider(handle_, 0);
    }
    AlgorithmHandle(const AlgorithmHandle&) = delete;
    AlgorithmHandle& operator=(const AlgorithmHandle&) = delete;

    BCRYPT_ALG_HANDLE get() const { return handle_; }
    bool valid() const { return handle_ != nullptr; }

private:
    BCRYPT_ALG_HANDLE handle_ = nullptr;
};

} // namespace

std::vector<std::uint8_t> randomBytes(std::size_t count) {
    std::vector<std::uint8_t> buffer(count);
    const NTSTATUS status = BCryptGenRandom(nullptr, buffer.data(),
                                            static_cast<ULONG>(buffer.size()),
                                            BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (status < 0) {
        throw errors::internal("安全随机数生成失败");
    }
    return buffer;
}

std::vector<std::uint8_t> deriveKey(const std::string& password,
                                    const std::vector<std::uint8_t>& salt,
                                    std::int64_t iterations) {
    AlgorithmHandle algorithm(BCRYPT_SHA256_ALGORITHM);
    if (!algorithm.valid()) {
        throw errors::internal("密码派生算法初始化失败");
    }
    std::vector<std::uint8_t> key(kDerivedKeyBytes);
    const NTSTATUS status = BCryptDeriveKeyPBKDF2(
        algorithm.get(),
        reinterpret_cast<PUCHAR>(const_cast<char*>(password.data())),
        static_cast<ULONG>(password.size()),
        const_cast<PUCHAR>(salt.data()), static_cast<ULONG>(salt.size()),
        static_cast<ULONGLONG>(iterations),
        key.data(), static_cast<ULONG>(key.size()), 0);
    if (status < 0) {
        throw errors::internal("密码派生计算失败");
    }
    return key;
}

std::string hexEncode(const std::vector<std::uint8_t>& bytes) {
    static const char kDigits[] = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (std::uint8_t b : bytes) {
        out.push_back(kDigits[b >> 4]);
        out.push_back(kDigits[b & 0x0F]);
    }
    return out;
}

bool hexDecode(const std::string& hex, std::vector<std::uint8_t>& bytes) {
    if (hex.size() % 2 != 0) return false;
    auto value = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    bytes.clear();
    bytes.reserve(hex.size() / 2);
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        const int hi = value(hex[i]);
        const int lo = value(hex[i + 1]);
        if (hi < 0 || lo < 0) return false;
        bytes.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
    }
    return true;
}

bool constantTimeEquals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        diff |= static_cast<unsigned char>(a[i] ^ b[i]);
    }
    return diff == 0;
}

std::string randomToken() {
    return hexEncode(randomBytes(32));
}

} // namespace crypto
} // namespace bbs
