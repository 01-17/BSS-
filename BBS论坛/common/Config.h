// 配置文件读取：config.ini（[section] 与 key=value）
#pragma once

#include <map>
#include <string>

namespace bbs {

class Config {
public:
    // 加载失败返回 false，不抛异常
    bool load(const std::string& path);

    std::string get(const std::string& section, const std::string& key,
                    const std::string& fallback) const;
    int getInt(const std::string& section, const std::string& key, int fallback) const;

private:
    std::map<std::string, std::map<std::string, std::string>> values_;
};

} // namespace bbs
