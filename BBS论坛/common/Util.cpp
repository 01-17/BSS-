#include "Util.h"

#include <cstdio>
#include <cstring>
#include <ctime>

namespace bbs {
namespace util {

namespace {

std::string formatUtc(std::time_t t) {
    std::tm tmUtc{};
    if (gmtime_s(&tmUtc, &t) != 0) return "";
    char buf[32]{};
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                  tmUtc.tm_year + 1900, tmUtc.tm_mon + 1, tmUtc.tm_mday,
                  tmUtc.tm_hour, tmUtc.tm_min, tmUtc.tm_sec);
    return buf;
}

// 本地日期零时转 time_t；日期分量已由调用方校验
std::optional<std::time_t> localMidnight(const std::string& date) {
    int year = 0, month = 0, day = 0;
    if (std::sscanf(date.c_str(), "%4d-%2d-%2d", &year, &month, &day) != 3) {
        return std::nullopt;
    }
    std::tm tmLocal{};
    tmLocal.tm_year = year - 1900;
    tmLocal.tm_mon = month - 1;
    tmLocal.tm_mday = day;
    tmLocal.tm_hour = 0;
    tmLocal.tm_min = 0;
    tmLocal.tm_sec = 0;
    tmLocal.tm_isdst = -1; // 由系统判定夏令时
    std::time_t t = std::mktime(&tmLocal);
    if (t == static_cast<std::time_t>(-1)) return std::nullopt;
    return t;
}

bool isUnicodeSpace(char32_t cp) {
    if (cp == 0x09 || cp == 0x0A || cp == 0x0B || cp == 0x0C || cp == 0x0D) return true;
    if (cp == 0x20 || cp == 0x85 || cp == 0xA0) return true;
    if (cp == 0x1680) return true;
    if (cp >= 0x2000 && cp <= 0x200A) return true;
    if (cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F) return true;
    return cp == 0x3000;
}

// 解码一个码点；调用前须保证 input 为合法 UTF-8
std::size_t decodeCodePoint(const std::string& input, std::size_t pos, char32_t& cp) {
    const unsigned char c = static_cast<unsigned char>(input[pos]);
    if (c < 0x80) {
        cp = c;
        return 1;
    }
    if ((c >> 5) == 0x06) {
        cp = ((c & 0x1Fu) << 6) | (static_cast<unsigned char>(input[pos + 1]) & 0x3Fu);
        return 2;
    }
    if ((c >> 4) == 0x0E) {
        cp = ((c & 0x0Fu) << 12) |
             ((static_cast<unsigned char>(input[pos + 1]) & 0x3Fu) << 6) |
             (static_cast<unsigned char>(input[pos + 2]) & 0x3Fu);
        return 3;
    }
    cp = ((c & 0x07u) << 18) |
         ((static_cast<unsigned char>(input[pos + 1]) & 0x3Fu) << 12) |
         ((static_cast<unsigned char>(input[pos + 2]) & 0x3Fu) << 6) |
         (static_cast<unsigned char>(input[pos + 3]) & 0x3Fu);
    return 4;
}

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

} // namespace

std::string nowUtc() {
    return formatUtc(std::time(nullptr));
}

std::string utcAfterSeconds(std::int64_t seconds) {
    return formatUtc(std::time(nullptr) + static_cast<std::time_t>(seconds));
}

std::string toJsonTime(const std::string& utc) {
    if (utc.size() != 19 || utc[10] != ' ') return utc;
    std::string out = utc;
    out[10] = 'T';
    out.push_back('Z');
    return out;
}

bool validDate(const std::string& date) {
    if (date.size() != 10) return false;
    for (std::size_t i = 0; i < date.size(); ++i) {
        if (i == 4 || i == 7) {
            if (date[i] != '-') return false;
        } else if (date[i] < '0' || date[i] > '9') {
            return false;
        }
    }
    const int year = (date[0] - '0') * 1000 + (date[1] - '0') * 100 +
                     (date[2] - '0') * 10 + (date[3] - '0');
    const int month = (date[5] - '0') * 10 + (date[6] - '0');
    const int day = (date[8] - '0') * 10 + (date[9] - '0');
    if (year < 1970 || year > 9999) return false;
    if (month < 1 || month > 12) return false;
    static const int monthDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int limit = monthDays[month - 1];
    const bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    if (month == 2 && leap) limit = 29;
    return day >= 1 && day <= limit;
}

std::optional<std::string> localDateStartUtc(const std::string& date) {
    if (!validDate(date)) return std::nullopt;
    auto t = localMidnight(date);
    if (!t) return std::nullopt;
    std::string text = formatUtc(*t);
    if (text.empty()) return std::nullopt;
    return text;
}

std::optional<std::string> localDateNextDayUtc(const std::string& date) {
    if (!validDate(date)) return std::nullopt;
    auto t = localMidnight(date);
    if (!t) return std::nullopt;
    *t += 24 * 60 * 60; // 本地日边界按固定 24 小时推进，避免夏令时跳变
    std::string text = formatUtc(*t);
    if (text.empty()) return std::nullopt;
    return text;
}

bool isValidUtf8(const std::string& text) {
    std::size_t i = 0;
    const std::size_t n = text.size();
    while (i < n) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        std::size_t extra = 0;
        char32_t cp = 0;
        if (c < 0x80) {
            ++i;
            continue;
        } else if ((c >> 5) == 0x06) {
            extra = 1;
            cp = c & 0x1Fu;
        } else if ((c >> 4) == 0x0E) {
            extra = 2;
            cp = c & 0x0Fu;
        } else if ((c >> 3) == 0x1E) {
            extra = 3;
            cp = c & 0x07u;
        } else {
            return false;
        }
        if (i + extra >= n) return false;
        for (std::size_t k = 1; k <= extra; ++k) {
            const unsigned char cc = static_cast<unsigned char>(text[i + k]);
            if ((cc >> 6) != 0x02) return false;
            cp = (cp << 6) | (cc & 0x3Fu);
        }
        // 过长编码与非法范围
        if (extra == 1 && cp < 0x80) return false;
        if (extra == 2 && cp < 0x800) return false;
        if (extra == 3 && cp < 0x10000) return false;
        if (cp > 0x10FFFF) return false;
        if (cp >= 0xD800 && cp <= 0xDFFF) return false;
        i += extra + 1;
    }
    return true;
}

std::size_t codePointCount(const std::string& text) {
    std::size_t count = 0;
    for (unsigned char c : text) {
        if ((c & 0xC0) != 0x80) ++count;
    }
    return count;
}

std::string trimUnicode(const std::string& text) {
    if (text.empty() || !isValidUtf8(text)) return text;
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end) {
        char32_t cp = 0;
        const std::size_t len = decodeCodePoint(text, begin, cp);
        if (!isUnicodeSpace(cp)) break;
        begin += len;
    }
    while (end > begin) {
        // 从尾部向前定位一个完整码点的起始字节
        std::size_t start = end - 1;
        while (start > begin && (static_cast<unsigned char>(text[start]) & 0xC0) == 0x80) {
            --start;
        }
        char32_t cp = 0;
        decodeCodePoint(text, start, cp);
        if (!isUnicodeSpace(cp)) break;
        end = start;
    }
    return text.substr(begin, end - begin);
}

bool containsNul(const std::string& text) {
    return text.find('\0') != std::string::npos;
}

bool isUsername(const std::string& text) {
    if (text.size() < 4 || text.size() > 20) return false;
    for (char c : text) {
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                        (c >= '0' && c <= '9') || c == '_';
        if (!ok) return false;
    }
    return true;
}

std::string toLowerAscii(const std::string& text) {
    std::string out = text;
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

bool equalsNoCaseAscii(const std::string& a, const std::string& b) {
    return toLowerAscii(a) == toLowerAscii(b);
}

bool percentDecode(const std::string& input, std::string& output) {
    output.clear();
    output.reserve(input.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        const char c = input[i];
        if (c == '%') {
            if (i + 2 >= input.size()) return false;
            const int hi = hexValue(input[i + 1]);
            const int lo = hexValue(input[i + 2]);
            if (hi < 0 || lo < 0) return false;
            output.push_back(static_cast<char>((hi << 4) | lo));
            i += 2;
        } else {
            output.push_back(c);
        }
    }
    return isValidUtf8(output);
}

} // namespace util
} // namespace bbs
