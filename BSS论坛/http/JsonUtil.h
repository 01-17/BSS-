// JSON 解析与字段取值：拒绝重复键、未知字段与类型不符
#pragma once

#include <initializer_list>
#include <string>

#include "../common/Types.h"
#include "../third_party/json.hpp"

namespace bbs {

using json = nlohmann::json;

class JsonUtil {
public:
    // 解析为对象；非法 JSON、重复键、非对象均抛 400
    static json parseObject(const std::string& text);

    // 校验字段集合：缺少必要字段或出现未知字段均抛 400
    static void requireExactFields(const json& object,
                                   std::initializer_list<const char*> fields);

    // 校验字段集合：required 必须存在，optional 可省略，其余字段均抛 400
    static void requireFields(const json& object,
                              std::initializer_list<const char*> required,
                              std::initializer_list<const char*> optional);

    static Text getString(const json& object, const char* field);
    static Id getId(const json& object, const char* field);
    static int getInt(const json& object, const char* field);
    static bool getBool(const json& object, const char* field);

    // 路径参数中的正整数
    static Id parsePathId(const std::string& text, const char* name);
};

} // namespace bbs
