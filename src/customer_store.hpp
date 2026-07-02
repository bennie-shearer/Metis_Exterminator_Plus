#pragma once
#include <string>
#include <vector>
#include <optional>
#include <mutex>

struct Customer {
    int id = 0;
    std::string name;
    std::string phone;
    std::string email;
    std::string address;
    std::string city;
    std::string state;
    std::string zip;
    std::string notes;
    std::string created_at;
    bool active = true;
};

class CustomerStore {
public:
    bool load(const std::string& data_dir);
    bool save();

    std::vector<Customer> all() const;
    std::optional<Customer> find(int id) const;
    Customer add(Customer c);
    bool update(const Customer& c);
    bool remove(int id);

private:
    std::string path_;
    std::vector<Customer> customers_;
    mutable std::mutex mu_;
    int next_id_ = 1;

    void parse_line(const std::string& line);
    std::string serialize(const Customer& c) const;
    static std::string esc(const std::string& s);
    static std::string unesc(const std::string& s);
};
