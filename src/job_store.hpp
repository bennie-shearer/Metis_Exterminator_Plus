#pragma once
#include "database.hpp"
#include <string>
#include <vector>
#include <optional>

struct Job {
    int id = 0;
    int customer_id = 0;
    std::string service_type;
    std::string pest_type;
    std::string status;
    std::string scheduled_date;
    std::string scheduled_time;
    std::string technician;
    std::string address;
    std::string notes;
    double price = 0.0;
    std::string created_at;
    std::string completed_at;
    bool deleted = false;
    bool recurring = false;
    int recur_interval_days = 0;
    std::string time_start;
    std::string time_end;
};

class JobStore {
public:
    explicit JobStore(Database& db);
    bool load(const std::string& /*data_dir*/) { return true; }
    bool save()                                { return true; }

    std::vector<Job> all() const;
    std::vector<Job> for_customer(int customer_id) const;
    std::optional<Job> find(int id) const;
    Job add(Job j);
    bool update(const Job& j);
    bool remove(int id);      // soft-delete
    bool hard_delete(int id); // permanent

    void rebuild_fts();

private:
    Database& db_;
};
