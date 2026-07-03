#include "invoice_store.hpp"
#include "logger.hpp"
#include <sstream>
#include <stdexcept>

InvoiceStore::InvoiceStore(Database& db) : db_(db) {}

void InvoiceStore::load_items(Invoice& inv) const {
    try {
        Database::Stmt st(db_.handle(),
            "SELECT id,description,unit_price,quantity FROM invoice_items WHERE invoice_id=? ORDER BY id");
        st.bind_int(1, inv.id);
        while (st.step()) {
            InvoiceItem it;
            it.id          = st.col_int(0);
            it.description = st.col_text(1);
            it.unit_price  = st.col_double(2);
            it.quantity    = st.col_int(3);
            inv.items.push_back(it);
        }
    } catch (...) {}
}

static Invoice row_to_invoice(Database::Stmt& st) {
    Invoice inv;
    inv.id             = st.col_int(0);
    inv.customer_id    = st.col_int(1);
    inv.job_id         = st.col_int(2);
    inv.invoice_number = st.col_text(3);
    inv.status         = st.col_text(4);
    inv.subtotal       = st.col_double(5);
    inv.tax_rate       = st.col_double(6);
    inv.tax_amount     = st.col_double(7);
    inv.total          = st.col_double(8);
    inv.issued_date    = st.col_text(9);
    inv.due_date       = st.col_text(10);
    inv.paid_date      = st.col_text(11);
    inv.notes          = st.col_text(12);
    inv.created_at     = st.col_text(13);
    return inv;
}

static const char* INV_SELECT =
    "SELECT id,customer_id,job_id,invoice_number,status,subtotal,tax_rate,tax_amount,"
    "total,issued_date,due_date,paid_date,notes,created_at FROM invoices";

std::vector<Invoice> InvoiceStore::all() const {
    std::vector<Invoice> result;
    try {
        Database::Stmt st(db_.handle(), std::string(INV_SELECT) + " ORDER BY created_at DESC");
        while (st.step()) {
            auto inv = row_to_invoice(st);
            load_items(inv);
            result.push_back(inv);
        }
    } catch (const std::exception& e) { LOGE("[invoices] all: " << e.what()); }
    return result;
}

std::vector<Invoice> InvoiceStore::for_customer(int customer_id) const {
    std::vector<Invoice> result;
    try {
        Database::Stmt st(db_.handle(),
            std::string(INV_SELECT) + " WHERE customer_id=? ORDER BY created_at DESC");
        st.bind_int(1, customer_id);
        while (st.step()) {
            auto inv = row_to_invoice(st);
            load_items(inv);
            result.push_back(inv);
        }
    } catch (...) {}
    return result;
}

std::optional<Invoice> InvoiceStore::find(int id) const {
    try {
        Database::Stmt st(db_.handle(), std::string(INV_SELECT) + " WHERE id=?");
        st.bind_int(1, id);
        if (!st.step()) return {};
        auto inv = row_to_invoice(st);
        load_items(inv);
        return inv;
    } catch (...) { return {}; }
}

Invoice InvoiceStore::add(Invoice inv, const std::string& prefix, int& next_num) {
    // Build invoice number
    if (inv.invoice_number.empty()) {
        std::ostringstream os;
        os << prefix << "-" << next_num++;
        inv.invoice_number = os.str();
    }
    // Compute totals
    inv.subtotal = 0.0;
    for (const auto& it : inv.items)
        inv.subtotal += it.unit_price * it.quantity;
    inv.tax_amount = inv.subtotal * inv.tax_rate;
    inv.total      = inv.subtotal + inv.tax_amount;
    if (inv.status.empty()) inv.status = "Pending";
    try {
        Database::Stmt st(db_.handle(),
            "INSERT INTO invoices(customer_id,job_id,invoice_number,status,subtotal,"
            "tax_rate,tax_amount,total,issued_date,due_date,notes) "
            "VALUES(?,?,?,?,?,?,?,?,?,?,?) RETURNING id,created_at");
        st.bind_int(1,inv.customer_id); st.bind_int(2,inv.job_id);
        st.bind_text(3,inv.invoice_number); st.bind_text(4,inv.status);
        st.bind_double(5,inv.subtotal); st.bind_double(6,inv.tax_rate);
        st.bind_double(7,inv.tax_amount); st.bind_double(8,inv.total);
        st.bind_text(9,inv.issued_date); st.bind_text(10,inv.due_date);
        st.bind_text(11,inv.notes);
        if (st.step()) { inv.id = st.col_int(0); inv.created_at = st.col_text(1); }
        // Insert items
        for (auto& it : inv.items) {
            Database::Stmt si(db_.handle(),
                "INSERT INTO invoice_items(invoice_id,description,unit_price,quantity) "
                "VALUES(?,?,?,?) RETURNING id");
            si.bind_int(1,inv.id); si.bind_text(2,it.description);
            si.bind_double(3,it.unit_price); si.bind_int(4,it.quantity);
            if (si.step()) it.id = si.col_int(0);
        }
    } catch (const std::exception& e) { LOGE("[invoices] add: " << e.what()); }
    return inv;
}

bool InvoiceStore::update(const Invoice& inv) {
    // Recompute totals
    double sub = 0.0;
    for (const auto& it : inv.items) sub += it.unit_price * it.quantity;
    double tax = sub * inv.tax_rate;
    double tot = sub + tax;
    try {
        Database::Stmt st(db_.handle(),
            "UPDATE invoices SET status=?,subtotal=?,tax_rate=?,tax_amount=?,total=?,"
            "issued_date=?,due_date=?,paid_date=?,notes=? WHERE id=?");
        st.bind_text(1,inv.status); st.bind_double(2,sub);
        st.bind_double(3,inv.tax_rate); st.bind_double(4,tax);
        st.bind_double(5,tot); st.bind_text(6,inv.issued_date);
        st.bind_text(7,inv.due_date); st.bind_text(8,inv.paid_date);
        st.bind_text(9,inv.notes); st.bind_int(10,inv.id);
        st.step();
        // Replace items
        Database::Stmt di(db_.handle(), "DELETE FROM invoice_items WHERE invoice_id=?");
        di.bind_int(1,inv.id); di.step();
        for (const auto& it : inv.items) {
            Database::Stmt si(db_.handle(),
                "INSERT INTO invoice_items(invoice_id,description,unit_price,quantity) VALUES(?,?,?,?)");
            si.bind_int(1,inv.id); si.bind_text(2,it.description);
            si.bind_double(3,it.unit_price); si.bind_int(4,it.quantity);
            si.step();
        }
        return true;
    } catch (const std::exception& e) { LOGE("[invoices] update: " << e.what()); return false; }
}

bool InvoiceStore::remove(int id) {
    try {
        Database::Stmt st(db_.handle(), "DELETE FROM invoices WHERE id=?");
        st.bind_int(1,id); st.step(); return true;
    } catch (...) { return false; }
}
