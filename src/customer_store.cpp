#include "customer_store.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <iomanip>

static std::string now_iso() {
    auto t = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(t);
    std::ostringstream os;
    os << std::put_time(std::gmtime(&tt), "%Y-%m-%dT%H:%M:%SZ");
    return os.str();
}

std::string CustomerStore::esc(const std::string& s) {
    std::string r;
    for (char c : s) {
        if (c == '|') r += "\\p";
        else if (c == '\\') r += "\\\\";
        else r += c;
    }
    return r;
}

std::string CustomerStore::unesc(const std::string& s) {
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

std::string CustomerStore::serialize(const Customer& c) const {
    std::ostringstream os;
    os << c.id << '|' << esc(c.name) << '|' << esc(c.phone) << '|'
       << esc(c.email) << '|' << esc(c.address) << '|' << esc(c.city) << '|'
       << esc(c.state) << '|' << esc(c.zip) << '|' << esc(c.notes) << '|'
       << c.created_at << '|' << (c.active ? '1' : '0');
    return os.str();
}

void CustomerStore::parse_line(const std::string& line) {
    if (line.empty() || line[0] == '#') return;
    std::vector<std::string> f;
    std::string tok;
    for (char ch : line) {
        if (ch == '|') { f.push_back(tok); tok.clear(); }
        else tok += ch;
    }
    f.push_back(tok);
    if (f.size() < 11) return;
    Customer c;
    try { c.id = std::stoi(f[0]); } catch (...) { return; }
    c.name = unesc(f[1]);
    c.phone = unesc(f[2]);
    c.email = unesc(f[3]);
    c.address = unesc(f[4]);
    c.city = unesc(f[5]);
    c.state = unesc(f[6]);
    c.zip = unesc(f[7]);
    c.notes = unesc(f[8]);
    c.created_at = f[9];
    c.active = (f[10] == "1");
    if (c.id >= next_id_) next_id_ = c.id + 1;
    customers_.push_back(std::move(c));
}

bool CustomerStore::load(const std::string& data_dir) {
    path_ = data_dir + "/customers.dat";
    std::ifstream f(path_);
    if (!f) return true;
    std::string line;
    while (std::getline(f, line)) parse_line(line);
    return true;
}

bool CustomerStore::save() {
    std::ofstream f(path_);
    if (!f) return false;
    std::lock_guard<std::mutex> lk(mu_);
    for (const auto& c : customers_) f << serialize(c) << '\n';
    return true;
}

std::vector<Customer> CustomerStore::all() const {
    std::lock_guard<std::mutex> lk(mu_);
    return customers_;
}

std::optional<Customer> CustomerStore::find(int id) const {
    std::lock_guard<std::mutex> lk(mu_);
    for (const auto& c : customers_)
        if (c.id == id) return c;
    return {};
}

Customer CustomerStore::add(Customer c) {
    std::lock_guard<std::mutex> lk(mu_);
    c.id = next_id_++;
    c.created_at = now_iso();
    customers_.push_back(c);
    return c;
}

bool CustomerStore::update(const Customer& c) {
    std::lock_guard<std::mutex> lk(mu_);
    for (auto& x : customers_) {
        if (x.id == c.id) { x = c; return true; }
    }
    return false;
}

bool CustomerStore::remove(int id) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = std::remove_if(customers_.begin(), customers_.end(),
        [id](const Customer& c){ return c.id == id; });
    if (it == customers_.end()) return false;
    customers_.erase(it, customers_.end());
    return true;
}
