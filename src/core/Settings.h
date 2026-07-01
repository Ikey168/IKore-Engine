#pragma once

#include <cstddef>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

/**
 * @file Settings.h
 * @brief Persistent key/value settings store (issue #58).
 *
 * Milestone 9 (In-Engine Editor & Tooling). A small, typed settings store behind
 * the settings menu: set/get bool/int/float/string values, serialize them to a
 * simple "key=value" text form, and load them back, so settings persist across
 * sessions. saveToFile()/loadFromFile() do the round trip to disk; serialize()/
 * load() are the pure string form, unit-testable without touching the filesystem.
 *
 * Header-only, std only. Values are stored as strings and coerced on read, with a
 * caller-supplied default for missing or unparyable keys.
 */
namespace IKore {

class Settings {
public:
    void setBool(const std::string& key, bool value) { m_values[key] = value ? "true" : "false"; }
    void setInt(const std::string& key, int value) { m_values[key] = std::to_string(value); }
    void setFloat(const std::string& key, float value) { m_values[key] = floatToString(value); }
    void setString(const std::string& key, const std::string& value) { m_values[key] = value; }

    bool has(const std::string& key) const { return m_values.count(key) > 0; }
    void clear() { m_values.clear(); }
    std::size_t size() const { return m_values.size(); }

    bool getBool(const std::string& key, bool defaultValue = false) const {
        auto it = m_values.find(key);
        if (it == m_values.end()) return defaultValue;
        const std::string& v = it->second;
        return v == "true" || v == "1" || v == "on" || v == "yes";
    }

    int getInt(const std::string& key, int defaultValue = 0) const {
        auto it = m_values.find(key);
        if (it == m_values.end()) return defaultValue;
        try {
            return std::stoi(it->second);
        } catch (...) {
            return defaultValue;
        }
    }

    float getFloat(const std::string& key, float defaultValue = 0.0f) const {
        auto it = m_values.find(key);
        if (it == m_values.end()) return defaultValue;
        try {
            return std::stof(it->second);
        } catch (...) {
            return defaultValue;
        }
    }

    std::string getString(const std::string& key, const std::string& defaultValue = "") const {
        auto it = m_values.find(key);
        return it == m_values.end() ? defaultValue : it->second;
    }

    /// Serialize to "key=value" lines, one per key, in sorted key order.
    std::string serialize() const {
        std::string out;
        for (const auto& kv : m_values) out += kv.first + "=" + kv.second + "\n";
        return out;
    }

    /**
     * @brief Parse "key=value" lines, merging into the current values.
     *
     * Blank lines and lines beginning with '#' or ';' are ignored. Whitespace
     * around the key and value is trimmed. Existing keys are overwritten; keys not
     * present in @p text are left as-is, so defaults set beforehand survive.
     */
    void load(const std::string& text) {
        std::istringstream in(text);
        std::string line;
        while (std::getline(in, line)) {
            const std::string trimmed = trim(line);
            if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') continue;
            const std::size_t eq = trimmed.find('=');
            if (eq == std::string::npos) continue;
            const std::string key = trim(trimmed.substr(0, eq));
            const std::string value = trim(trimmed.substr(eq + 1));
            if (!key.empty()) m_values[key] = value;
        }
    }

    /// Write the serialized settings to @p path (truncating). Returns success.
    bool saveToFile(const std::string& path) const {
        std::ofstream out(path, std::ios::trunc);
        if (!out) return false;
        out << serialize();
        return static_cast<bool>(out);
    }

    /// Load and merge settings from @p path. Returns false if the file is absent.
    bool loadFromFile(const std::string& path) {
        std::ifstream in(path);
        if (!in) return false;
        std::stringstream ss;
        ss << in.rdbuf();
        load(ss.str());
        return true;
    }

private:
    static std::string floatToString(float value) {
        std::ostringstream os;
        os << value;
        return os.str();
    }

    static std::string trim(const std::string& s) {
        const std::size_t a = s.find_first_not_of(" \t\r\n");
        const std::size_t b = s.find_last_not_of(" \t\r\n");
        return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
    }

    std::map<std::string, std::string> m_values;
};

} // namespace IKore
