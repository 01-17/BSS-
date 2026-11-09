#include "Validation.h"

#include <string>

#include "../common/Error.h"
#include "../common/Util.h"

namespace bbs {
namespace validation {

namespace {

std::string lengthMessage(const char* field, std::size_t minLength,
                          std::size_t maxLength) {
    return "字段 " + std::string(field) + " 长度必须为 " + std::to_string(minLength) +
           "-" + std::to_string(maxLength) + " 个字符";
}

void ensureValidUtf8(const Text& value, const char* field) {
    if (!util::isValidUtf8(value)) {
        throw errors::invalidArgument("字段 " + std::string(field) + " 不是合法的 UTF-8 文本");
    }
    if (util::containsNul(value)) {
        throw errors::invalidArgument("字段 " + std::string(field) + " 不能包含空字符");
    }
}

} // namespace

Text text(const Text& value, const char* field, std::size_t minLength,
          std::size_t maxLength) {
    ensureValidUtf8(value, field);
    const Text normalized = util::trimUnicode(value);
    const std::size_t length = util::codePointCount(normalized);
    if (length < minLength || length > maxLength) {
        throw errors::invalidArgument(lengthMessage(field, minLength, maxLength));
    }
    return normalized;
}

Text password(const Text& value, const char* field) {
    ensureValidUtf8(value, field);
    const std::size_t length = util::codePointCount(value);
    if (length < 8 || length > 64) {
        throw errors::invalidArgument(lengthMessage(field, 8, 64));
    }
    return value;
}

Text username(const Text& value) {
    ensureValidUtf8(value, "username");
    const Text normalized = util::trimUnicode(value);
    if (!util::isUsername(normalized)) {
        throw errors::invalidArgument(
            "字段 username 必须为 4-20 个 ASCII 字母、数字或下划线");
    }
    return normalized;
}

Text boardName(const Text& value) {
    return text(value, "name", 1, 30);
}

Text boardDescription(const Text& value) {
    return text(value, "description", 0, 200);
}

Text keyword(const Text& value) {
    return text(value, "keyword", 0, 100);
}

int status(const int& value, const char* field) {
    if (value != 0 && value != 1) {
        throw errors::invalidArgument("字段 " + std::string(field) + " 只能为 0 或 1");
    }
    return value;
}

} // namespace validation
} // namespace bbs
