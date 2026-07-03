#pragma once
#include "database.hpp"
#include <string>
#include <vector>
#include <optional>

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
    std::string updated_at;
    bool active  = true;
    bool deleted = false;
};

class CustomerStore {
public:
    explicit CustomerStore(Database& db);
    bool load(const std::string& /*data_dir*/) { return true; }  // no-op: SQLite
    bool save()                                { return true; }  // no-op: SQLite

    std::vector<Customer> all() const;
    std::optional<Customer> find(int id) const;
    Customer add(Customer c);
    bool update(const Customer& c);
    bool remove(int id);          // soft-delete
    bool hard_delete(int id);     // permanent

    void rebuild_fts();

private:
    Database& db_;
};
