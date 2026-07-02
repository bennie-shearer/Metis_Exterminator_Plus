#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>

struct PsonValue {
    std::string str;
    std::vector<std::string> arr;
    bool is_array = false;
};

struct PsonSection {
    std::unordered_map<std::string, PsonValue> keys;
};

class Pson {
public:
    bool load(const std::string& path);
    std::optional<std::string> get(const std::string& section, const std::string& key) const;
    std::optional<std::vector<std::string>> get_array(const std::string& section, const std::string& key) const;
    std::string get_or(const std::string& section, const std::string& key, const std::string& def) const;
    int get_int(const std::string& section, const std::string& key, int def) const;
    double get_double(const std::string& section, const std::string& key, double def) const;

private:
    std::unordered_map<std::string, PsonSection> sections_;
};
