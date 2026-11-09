#include "JsonUtil.h"

#include <set>
#include <vector>

#include "../common/Error.h"

namespace bbs {

namespace {

// 利用 SAX 遍历检测同一对象内的重复键（nlohmann::json 默认后者覆盖前者）
class DuplicateKeyDetector : public nlohmann::json_sax<json> {
public:
    bool duplicate = false;
    bool invalid = false;

    bool null() override { return true; }
    bool boolean(bool) override { return true; }
    bool number_integer(number_integer_t) override { return true; }
    bool number_unsigned(number_unsigned_t) override { return true; }
    bool number_float(number_float_t, const string_t&) override { return true; }
    bool string(string_t&) override { return true; }
    bool binary(binary_t&) override { return true; }

    bool start_object(std::size_t) override {
        keys_.emplace_back();
        return true;
    }
    bool key(string_t& value) override {
        if (keys_.empty()) return true;
        if (!keys_.back().insert(value).second) duplicate = true;
        return true;
    }
    bool end_object() override {
        if (!keys_.empty()) keys_.pop_back();
        return true;
    }
    bool start_array(std::size_t) override { return true; }
    bool end_array() override { return true; }
    bool parse_error(std::size_t, const std::string&,
                     const nlohmann::detail::exception&) override {
        invalid = true;
        return false;
    }

private:
    std::vector<std::set<std::string>> keys_;
};

} // namespace

json JsonUtil::parseObject(const std::string& text) {
    if (text.empty()) {
        throw errors::invalidArgument("请求体不能为空");
    }

    DuplicateKeyDetector detector;
    json::sax_parse(text, &detector);
    if (detector.invalid) {
        throw errors::invalidArgument("请求体不是合法的 JSON");
    }
    if (detector.duplicate) {
        throw errors::invalidArgument("请求体包含重复字段");
    }

    json parsed;
    try {
        parsed = json::parse(text);
    } catch (const json::exception&) {
        throw errors::invalidArgument("请求体不是合法的 JSON");
    }
    if (!parsed.is_object()) {
        throw errors::invalidArgument("请求体必须为 JSON 对象");
    }
    return parsed;
}

void JsonUtil::requireExactFields(const json& object,
                                  std::initializer_list<const char*> fields) {
    requireFields(object, fields, {});
}

void JsonUtil::requireFields(const json& object,
                             std::initializer_list<const char*> required,
                             std::initializer_list<const char*> optional) {
    for (const char* field : required) {
        if (!object.contains(field)) {
            throw errors::invalidArgument("缺少字段 " + std::string(field));
        }
    }
    for (auto it = object.begin(); it != object.end(); ++it) {
        bool known = false;
        for (const char* field : required) {
            if (it.key() == field) {
                known = true;
                break;
            }
        }
        if (!known) {
            for (const char* field : optional) {
                if (it.key() == field) {
                    known = true;
                    break;
                }
            }
        }
        if (!known) {
            throw errors::invalidArgument("字段 " + it.key() + " 不受支持");
        }
    }
}

Text JsonUtil::getString(const json& object, const char* field) {
    const auto& value = object.at(field);
    if (!value.is_string()) {
        throw errors::invalidArgument("字段 " + std::string(field) + " 必须为字符串");
    }
    return value.get<Text>();
}

Id JsonUtil::getId(const json& object, const char* field) {
    const auto& value = object.at(field);
    if (!value.is_number_integer() && !value.is_number_unsigned()) {
        throw errors::invalidArgument("字段 " + std::string(field) + " 必须为整数");
    }
    if (value.is_number_unsigned()) {
        const auto raw = value.get<std::uint64_t>();
        if (raw == 0 || raw > static_cast<std::uint64_t>(INT64_MAX)) {
            throw errors::invalidArgument("字段 " + std::string(field) + " 必须为正整数");
        }
        return static_cast<Id>(raw);
    }
    const auto raw = value.get<std::int64_t>();
    if (raw <= 0) {
        throw errors::invalidArgument("字段 " + std::string(field) + " 必须为正整数");
    }
    return raw;
}

int JsonUtil::getInt(const json& object, const char* field) {
    const auto& value = object.at(field);
    if (!value.is_number_integer()) {
        throw errors::invalidArgument("字段 " + std::string(field) + " 必须为整数");
    }
    const auto raw = value.get<std::int64_t>();
    if (raw < INT32_MIN || raw > INT32_MAX) {
        throw errors::invalidArgument("字段 " + std::string(field) + " 超出取值范围");
    }
    return static_cast<int>(raw);
}

bool JsonUtil::getBool(const json& object, const char* field) {
    const auto& value = object.at(field);
    if (!value.is_boolean()) {
        throw errors::invalidArgument("字段 " + std::string(field) + " 必须为布尔值");
    }
    return value.get<bool>();
}

Id JsonUtil::parsePathId(const std::string& text, const char* name) {
    if (text.empty()) {
        throw errors::invalidArgument(std::string("路径参数 ") + name + " 非法");
    }
    Id value = 0;
    for (char c : text) {
        if (c < '0' || c > '9') {
            throw errors::invalidArgument(std::string("路径参数 ") + name + " 必须为正整数");
        }
        const int digit = c - '0';
        if (value > (INT64_MAX - digit) / 10) {
            throw errors::invalidArgument(std::string("路径参数 ") + name + " 超出取值范围");
        }
        value = value * 10 + digit;
    }
    if (value <= 0) {
        throw errors::invalidArgument(std::string("路径参数 ") + name + " 必须为正整数");
    }
    return value;
}

} // namespace bbs
