#include "invoice_store.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <iomanip>

static std::string now_iso_inv() {
    auto t = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(t);
    std::ostringstream os;
    os << std::put_time(std::gmtime(&tt), "%Y-%m-%dT%H:%M:%SZ");
    return os.str();
}

std::string InvoiceStore::esc(const std::string& s) {
    std::string r;
    for (char c : s) {
        if (c == '|') r += "\\p";
        else if (c == '^') r += "\\c";
        else if (c == '\\') r += "\\\\";
        else r += c;
    }
    return r;
}

std::string InvoiceStore::unesc(const std::string& s) {
    std::string r;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            if (s[i+1] == 'p') { r += '|'; ++i; }
            else if (s[i+1] == 'c') { r += '^'; ++i; }
            else if (s[i+1] == '\\') { r += '\\'; ++i; }
            else r += s[i];
        } else r += s[i];
    }
    return r;
}

std::string InvoiceStore::serialize(const Invoice& inv) const {
    std::ostringstream os;
    std::string items_str;
    for (size_t i = 0; i < inv.items.size(); ++i) {
        if (i > 0) items_str += '^';
        items_str += esc(inv.items[i].description) + '~'
                   + std::to_string(inv.items[i].unit_price) + '~'
                   + std::to_string(inv.items[i].quantity);
    }
    os << inv.id << '|' << inv.customer_id << '|' << inv.job_id << '|'
       << esc(inv.invoice_number) << '|' << esc(inv.status) << '|'
       << esc(items_str) << '|'
       << inv.subtotal << '|' << inv.tax_rate << '|' << inv.tax_amount << '|'
       << inv.total << '|' << esc(inv.issued_date) << '|' << esc(inv.due_date) << '|'
       << esc(inv.paid_date) << '|' << esc(inv.notes) << '|' << inv.created_at;
    return os.str();
}

void InvoiceStore::parse_line(const std::string& line) {
    if (line.empty() || line[0] == '#') return;
    std::vector<std::string> f;
    std::string tok;
    for (char ch : line) {
        if (ch == '|') { f.push_back(tok); tok.clear(); }
        else tok += ch;
    }
    f.push_back(tok);
    if (f.size() < 15) return;
    Invoice inv;
    try { inv.id = std::stoi(f[0]); } catch (...) { return; }
    try { inv.customer_id = std::stoi(f[1]); } catch (...) {}
    try { inv.job_id = std::stoi(f[2]); } catch (...) {}
    inv.invoice_number = unesc(f[3]);
    inv.status = unesc(f[4]);
    std::string items_raw = unesc(f[5]);
    std::istringstream iss(items_raw);
    std::string item_tok;
    while (std::getline(iss, item_tok, '^')) {
        if (item_tok.empty()) continue;
        std::vector<std::string> ip;
        std::string ipart;
        for (char c : item_tok) {
            if (c == '~') { ip.push_back(ipart); ipart.clear(); }
            else ipart += c;
        }
        ip.push_back(ipart);
        if (ip.size() >= 3) {
            InvoiceItem ii;
            ii.description = unesc(ip[0]);
            try { ii.unit_price = std::stod(ip[1]); } catch (...) {}
            try { ii.quantity = std::stoi(ip[2]); } catch (...) { ii.quantity = 1; }
            inv.items.push_back(ii);
        }
    }
    try { inv.subtotal = std::stod(f[6]); } catch (...) {}
    try { inv.tax_rate = std::stod(f[7]); } catch (...) {}
    try { inv.tax_amount = std::stod(f[8]); } catch (...) {}
    try { inv.total = std::stod(f[9]); } catch (...) {}
    inv.issued_date = unesc(f[10]);
    inv.due_date = unesc(f[11]);
    inv.paid_date = unesc(f[12]);
    inv.notes = unesc(f[13]);
    inv.created_at = f[14];
    if (inv.id >= next_id_) next_id_ = inv.id + 1;
    invoices_.push_back(std::move(inv));
}

bool InvoiceStore::load(const std::string& data_dir) {
    path_ = data_dir + "/invoices.dat";
    std::ifstream f(path_);
    if (!f) return true;
    std::string line;
    while (std::getline(f, line)) parse_line(line);
    return true;
}

bool InvoiceStore::save() {
    std::ofstream f(path_);
    if (!f) return false;
    std::lock_guard<std::mutex> lk(mu_);
    for (const auto& inv : invoices_) f << serialize(inv) << '\n';
    return true;
}

std::vector<Invoice> InvoiceStore::all() const {
    std::lock_guard<std::mutex> lk(mu_);
    return invoices_;
}

std::vector<Invoice> InvoiceStore::for_customer(int cid) const {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<Invoice> out;
    for (const auto& inv : invoices_)
        if (inv.customer_id == cid) out.push_back(inv);
    return out;
}

std::optional<Invoice> InvoiceStore::find(int id) const {
    std::lock_guard<std::mutex> lk(mu_);
    for (const auto& inv : invoices_)
        if (inv.id == id) return inv;
    return {};
}

Invoice InvoiceStore::add(Invoice inv, const std::string& prefix, int& next_num) {
    std::lock_guard<std::mutex> lk(mu_);
    inv.id = next_id_++;
    inv.created_at = now_iso_inv();
    std::ostringstream num;
    num << prefix << "-" << std::setfill('0') << std::setw(5) << next_num++;
    inv.invoice_number = num.str();
    if (inv.status.empty()) inv.status = "Pending";
    invoices_.push_back(inv);
    return inv;
}

bool InvoiceStore::update(const Invoice& inv) {
    std::lock_guard<std::mutex> lk(mu_);
    for (auto& x : invoices_) {
        if (x.id == inv.id) { x = inv; return true; }
    }
    return false;
}

bool InvoiceStore::remove(int id) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = std::remove_if(invoices_.begin(), invoices_.end(),
        [id](const Invoice& inv){ return inv.id == id; });
    if (it == invoices_.end()) return false;
    invoices_.erase(it, invoices_.end());
    return true;
}
