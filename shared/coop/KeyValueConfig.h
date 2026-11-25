// Tiny key=value configuration file parser.
// Format: lines of "key = value", '#' starts a comment. Used by both the
// dedicated server and the in-game client (net settings, autosave interval...).
#pragma once

#include <string>
#include <unordered_map>

namespace coop {

class KeyValueConfig {
public:
    // Returns false if the file could not be opened (entries stay empty).
    bool Load(const std::string& path);

    // Writes the current entries back to disk (server "config save" support).
    bool Save(const std::string& path) const;

    std::string Get(const std::string& key, const std::string& fallback = "") const;
    int GetInt(const std::string& key, int fallback) const;
    float GetFloat(const std::string& key, float fallback) const;
    bool GetBool(const std::string& key, bool fallback) const;

    void Set(const std::string& key, const std::string& value);

private:
    std::unordered_map<std::string, std::string> entries_;
};

} // namespace coop
