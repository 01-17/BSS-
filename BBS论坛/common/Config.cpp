#include "Config.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>

#include "Util.h"

namespace bbs {

bool Config::load(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) return false;

    std::string section;
    std::string line;
    while (std::getline(in, line)) {
        line = util::trimUnicode(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        if (line.front() == '[' && line.back() == ']') {
            section = util::toLowerAscii(line.substr(1, line.size() - 2));
            continue;
        }
        const std::size_t pos = line.find('=');
        if (pos == std::string::npos) continue;
        std::string key = util::toLowerAscii(util::trimUnicode(line.substr(0, pos)));
        std::string value = util::trimUnicode(line.substr(pos + 1));
        if (!key.empty()) values_[section][key] = value;
    }
    return true;
}

std::string Config::get(const std::string& section, const std::string& key,
                        const std::string& fallback) const {
    auto sec = values_.find(util::toLowerAscii(section));
    if (sec == values_.end()) return fallback;
    auto item = sec->second.find(util::toLowerAscii(key));
    if (item == sec->second.end()) return fallback;
    return item->second;
}

int Config::getInt(const std::string& section, const std::string& key, int fallback) const {
    const std::string text = get(section, key, "");
    if (text.empty()) return fallback;
    char* end = nullptr;
    const long value = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0') return fallback;
    return static_cast<int>(value);
}

} // namespace bbs
