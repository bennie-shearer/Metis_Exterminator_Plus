#pragma once
#include <string>
#include <vector>
#include <optional>
#include <mutex>

struct InvoiceItem {
    std::string description;
    double unit_price = 0.0;
    int quantity = 1;
};

struct Invoice {
    int id = 0;
    int customer_id = 0;
    int job_id = 0;
    std::string invoice_number;
    std::string status;
    std::vector<InvoiceItem> items;
    double subtotal = 0.0;
    double tax_rate = 0.0;
    double tax_amount = 0.0;
    double total = 0.0;
    std::string issued_date;
    std::string due_date;
    std::string paid_date;
    std::string notes;
    std::string created_at;
};

class InvoiceStore {
public:
    bool load(const std::string& data_dir);
    bool save();

    std::vector<Invoice> all() const;
    std::vector<Invoice> for_customer(int customer_id) const;
    std::optional<Invoice> find(int id) const;
    Invoice add(Invoice inv, const std::string& prefix, int& next_num);
    bool update(const Invoice& inv);
    bool remove(int id);

private:
    std::string path_;
    std::vector<Invoice> invoices_;
    mutable std::mutex mu_;
    int next_id_ = 1;

    void parse_line(const std::string& line);
    std::string serialize(const Invoice& inv) const;
    static std::string esc(const std::string& s);
    static std::string unesc(const std::string& s);
};
