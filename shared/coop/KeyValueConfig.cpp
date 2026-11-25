#include "KeyValueConfig.h"

#include <fstream>
#include <sstream>
#include <cctype>

namespace coop {

namespace {
std::string Trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace((unsigned char)s[b])) ++b;
    while (e > b && std::isspace((unsigned char)s[e - 1])) --e;
    return s.substr(b, e - b);
}
} // namespace

bool KeyValueConfig::Load(const std::string& path) {
    std::ifstream in(path);
    if (!in) return false;
    std::string line;
    while (std::getline(in, line)) {
        size_t hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = Trim(line.substr(0, eq));
        std::string val = Trim(line.substr(eq + 1));
        if (!key.empty()) entries_[key] = val;
    }
    return true;
}

bool KeyValueConfig::Save(const std::string& path) const {
    std::ofstream out(path, std::ios::trunc);
    if (!out) return false;
    for (const auto& kv : entries_) out << kv.first << " = " << kv.second << "\n";
    return true;
}

std::string KeyValueConfig::Get(const std::string& key, const std::string& fallback) const {
    auto it = entries_.find(key);
    return it == entries_.end() ? fallback : it->second;
}

int KeyValueConfig::GetInt(const std::string& key, int fallback) const {
    auto it = entries_.find(key);
    if (it == entries_.end()) return fallback;
    try { return std::stoi(it->second); } catch (...) { return fallback; }
}

float KeyValueConfig::GetFloat(const std::string& key, float fallback) const {
    auto it = entries_.find(key);
    if (it == entries_.end()) return fallback;
    try { return std::stof(it->second); } catch (...) { return fallback; }
}

bool KeyValueConfig::GetBool(const std::string& key, bool fallback) const {
    auto it = entries_.find(key);
    if (it == entries_.end()) return fallback;
    const std::string& v = it->second;
    if (v == "1" || v == "true" || v == "yes") return true;
    if (v == "0" || v == "false" || v == "no") return false;
    return fallback;
}

void KeyValueConfig::Set(const std::string& key, const std::string& value) {
    entries_[key] = value;
}

} // namespace coop
