#pragma once
#include "database.hpp"
#include <string>
#include <vector>
#include <optional>

struct InvoiceItem {
    int id = 0;
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
    double subtotal   = 0.0;
    double tax_rate   = 0.0;
    double tax_amount = 0.0;
    double total      = 0.0;
    std::string issued_date;
    std::string due_date;
    std::string paid_date;
    std::string notes;
    std::string created_at;
};

class InvoiceStore {
public:
    explicit InvoiceStore(Database& db);
    bool load(const std::string& /*data_dir*/) { return true; }
    bool save()                                { return true; }

    std::vector<Invoice> all() const;
    std::vector<Invoice> for_customer(int customer_id) const;
    std::optional<Invoice> find(int id) const;
    Invoice add(Invoice inv, const std::string& prefix, int& next_num);
    bool update(const Invoice& inv);
    bool remove(int id);

private:
    Database& db_;
    void load_items(Invoice& inv) const;
};
