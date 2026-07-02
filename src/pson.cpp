#include "pson.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <stdexcept>

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

bool Pson::load(const std::string& path) {
    std::ifstream f(path);
    if (!f) return false;

    std::string cur_section;
    std::string cur_key;
    bool in_array = false;
    std::string line;

    while (std::getline(f, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;

        if (in_array) {
            if (t == "]") {
                in_array = false;
                cur_key.clear();
            } else {
                std::string val = t;
                if (!val.empty() && val.front() == '"') val = val.substr(1);
                if (!val.empty() && val.back() == '"') val.pop_back();
                if (!val.empty() && val.back() == ',') val.pop_back();
                sections_[cur_section].keys[cur_key].arr.push_back(trim(val));
            }
            continue;
        }

        if (t.back() == '{') {
            cur_section = trim(t.substr(0, t.size() - 1));
            continue;
        }
        if (t == "}") {
            cur_section.clear();
            continue;
        }

        auto eq = t.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(t.substr(0, eq));
        std::string val = trim(t.substr(eq + 1));

        if (val == "[") {
            sections_[cur_section].keys[key].is_array = true;
            cur_key = key;
            in_array = true;
            continue;
        }

        if (!val.empty() && val.front() == '"') {
            // Quoted string: strip quotes only, preserve all content including #
            val = val.substr(1);
            if (!val.empty() && val.back() == '"') val.pop_back();
        } else {
            // Unquoted: strip inline # comments and whitespace
            auto hash = val.find('#');
            if (hash != std::string::npos) val = trim(val.substr(0, hash));
        }

        sections_[cur_section].keys[key].str = val;
    }
    return true;
}

std::optional<std::string> Pson::get(const std::string& sec, const std::string& key) const {
    auto si = sections_.find(sec);
    if (si == sections_.end()) return {};
    auto ki = si->second.keys.find(key);
    if (ki == si->second.keys.end()) return {};
    return ki->second.str;
}

std::optional<std::vector<std::string>> Pson::get_array(const std::string& sec, const std::string& key) const {
    auto si = sections_.find(sec);
    if (si == sections_.end()) return {};
    auto ki = si->second.keys.find(key);
    if (ki == si->second.keys.end()) return {};
    return ki->second.arr;
}

std::string Pson::get_or(const std::string& sec, const std::string& key, const std::string& def) const {
    auto v = get(sec, key);
    return v ? *v : def;
}

int Pson::get_int(const std::string& sec, const std::string& key, int def) const {
    auto v = get(sec, key);
    if (!v) return def;
    try { return std::stoi(*v); } catch (...) { return def; }
}

double Pson::get_double(const std::string& sec, const std::string& key, double def) const {
    auto v = get(sec, key);
    if (!v) return def;
    try { return std::stod(*v); } catch (...) { return def; }
}
