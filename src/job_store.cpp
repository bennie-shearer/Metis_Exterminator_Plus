#include "job_store.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <iomanip>

static std::string now_iso_j() {
    auto t = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(t);
    std::ostringstream os;
    os << std::put_time(std::gmtime(&tt), "%Y-%m-%dT%H:%M:%SZ");
    return os.str();
}

std::string JobStore::esc(const std::string& s) {
    std::string r;
    for (char c : s) {
        if (c == '|') r += "\\p";
        else if (c == '\\') r += "\\\\";
        else r += c;
    }
    return r;
}

std::string JobStore::unesc(const std::string& s) {
    std::string r;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            if (s[i+1] == 'p') { r += '|'; ++i; }
            else if (s[i+1] == '\\') { r += '\\'; ++i; }
            else r += s[i];
        } else r += s[i];
    }
    return r;
}

std::string JobStore::serialize(const Job& j) const {
    std::ostringstream os;
    os << j.id << '|' << j.customer_id << '|'
       << esc(j.service_type) << '|' << esc(j.pest_type) << '|'
       << esc(j.status) << '|' << esc(j.scheduled_date) << '|'
       << esc(j.scheduled_time) << '|' << esc(j.technician) << '|'
       << esc(j.address) << '|' << esc(j.notes) << '|'
       << j.price << '|' << j.created_at << '|' << esc(j.completed_at);
    return os.str();
}

void JobStore::parse_line(const std::string& line) {
    if (line.empty() || line[0] == '#') return;
    std::vector<std::string> f;
    std::string tok;
    for (char ch : line) {
        if (ch == '|') { f.push_back(tok); tok.clear(); }
        else tok += ch;
    }
    f.push_back(tok);
    if (f.size() < 13) return;
    Job j;
    try { j.id = std::stoi(f[0]); } catch (...) { return; }
    try { j.customer_id = std::stoi(f[1]); } catch (...) {}
    j.service_type = unesc(f[2]);
    j.pest_type = unesc(f[3]);
    j.status = unesc(f[4]);
    j.scheduled_date = unesc(f[5]);
    j.scheduled_time = unesc(f[6]);
    j.technician = unesc(f[7]);
    j.address = unesc(f[8]);
    j.notes = unesc(f[9]);
    try { j.price = std::stod(f[10]); } catch (...) {}
    j.created_at = f[11];
    j.completed_at = unesc(f[12]);
    if (j.id >= next_id_) next_id_ = j.id + 1;
    jobs_.push_back(std::move(j));
}

bool JobStore::load(const std::string& data_dir) {
    path_ = data_dir + "/jobs.dat";
    std::ifstream f(path_);
    if (!f) return true;
    std::string line;
    while (std::getline(f, line)) parse_line(line);
    return true;
}

bool JobStore::save() {
    std::ofstream f(path_);
    if (!f) return false;
    std::lock_guard<std::mutex> lk(mu_);
    for (const auto& j : jobs_) f << serialize(j) << '\n';
    return true;
}

std::vector<Job> JobStore::all() const {
    std::lock_guard<std::mutex> lk(mu_);
    return jobs_;
}

std::vector<Job> JobStore::for_customer(int cid) const {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<Job> out;
    for (const auto& j : jobs_)
        if (j.customer_id == cid) out.push_back(j);
    return out;
}

std::optional<Job> JobStore::find(int id) const {
    std::lock_guard<std::mutex> lk(mu_);
    for (const auto& j : jobs_)
        if (j.id == id) return j;
    return {};
}

Job JobStore::add(Job j) {
    std::lock_guard<std::mutex> lk(mu_);
    j.id = next_id_++;
    j.created_at = now_iso_j();
    if (j.status.empty()) j.status = "Scheduled";
    jobs_.push_back(j);
    return j;
}

bool JobStore::update(const Job& j) {
    std::lock_guard<std::mutex> lk(mu_);
    for (auto& x : jobs_) {
        if (x.id == j.id) { x = j; return true; }
    }
    return false;
}

bool JobStore::remove(int id) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = std::remove_if(jobs_.begin(), jobs_.end(),
        [id](const Job& j){ return j.id == id; });
    if (it == jobs_.end()) return false;
    jobs_.erase(it, jobs_.end());
    return true;
}
