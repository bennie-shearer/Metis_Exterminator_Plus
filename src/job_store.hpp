#pragma once
#include <string>
#include <vector>
#include <optional>
#include <mutex>

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
};

class JobStore {
public:
    bool load(const std::string& data_dir);
    bool save();

    std::vector<Job> all() const;
    std::vector<Job> for_customer(int customer_id) const;
    std::optional<Job> find(int id) const;
    Job add(Job j);
    bool update(const Job& j);
    bool remove(int id);

private:
    std::string path_;
    std::vector<Job> jobs_;
    mutable std::mutex mu_;
    int next_id_ = 1;

    void parse_line(const std::string& line);
    std::string serialize(const Job& j) const;
    static std::string esc(const std::string& s);
    static std::string unesc(const std::string& s);
};
