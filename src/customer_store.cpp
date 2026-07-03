#include "customer_store.hpp"
#include "logger.hpp"
#include <stdexcept>

CustomerStore::CustomerStore(Database& db) : db_(db) {}

static Customer row_to_customer(Database::Stmt& st) {
    Customer c;
    c.id         = st.col_int(0);
    c.name       = st.col_text(1);
    c.phone      = st.col_text(2);
    c.email      = st.col_text(3);
    c.address    = st.col_text(4);
    c.city       = st.col_text(5);
    c.state      = st.col_text(6);
    c.zip        = st.col_text(7);
    c.notes      = st.col_text(8);
    c.active     = st.col_int(9) != 0;
    c.deleted    = st.col_int(10) != 0;
    c.created_at = st.col_text(11);
    c.updated_at = st.col_text(12);
    return c;
}

std::vector<Customer> CustomerStore::all() const {
    std::vector<Customer> result;
    try {
        Database::Stmt st(db_.handle(),
            "SELECT id,name,phone,email,address,city,state,zip,notes,active,deleted,created_at,updated_at "
            "FROM customers WHERE deleted=0 ORDER BY name COLLATE NOCASE");
        while (st.step()) result.push_back(row_to_customer(st));
    } catch (const std::exception& e) { LOGE("[customers] all: " << e.what()); }
    return result;
}

std::optional<Customer> CustomerStore::find(int id) const {
    try {
        Database::Stmt st(db_.handle(),
            "SELECT id,name,phone,email,address,city,state,zip,notes,active,deleted,created_at,updated_at "
            "FROM customers WHERE id=?");
        st.bind_int(1, id);
        if (!st.step()) return {};
        return row_to_customer(st);
    } catch (...) { return {}; }
}

Customer CustomerStore::add(Customer c) {
    try {
        Database::Stmt st(db_.handle(),
            "INSERT INTO customers(name,phone,email,address,city,state,zip,notes) "
            "VALUES(?,?,?,?,?,?,?,?) RETURNING id,created_at");
        st.bind_text(1,c.name); st.bind_text(2,c.phone); st.bind_text(3,c.email);
        st.bind_text(4,c.address); st.bind_text(5,c.city); st.bind_text(6,c.state);
        st.bind_text(7,c.zip); st.bind_text(8,c.notes);
        if (st.step()) { c.id = st.col_int(0); c.created_at = st.col_text(1); }
        // Update FTS
        Database::Stmt fts(db_.handle(),
            "INSERT INTO customers_fts(rowid,name,phone,email,address,city,notes) VALUES(?,?,?,?,?,?,?)");
        fts.bind_int(1,c.id); fts.bind_text(2,c.name); fts.bind_text(3,c.phone);
        fts.bind_text(4,c.email); fts.bind_text(5,c.address); fts.bind_text(6,c.city);
        fts.bind_text(7,c.notes); fts.step();
    } catch (const std::exception& e) { LOGE("[customers] add: " << e.what()); }
    return c;
}

bool CustomerStore::update(const Customer& c) {
    try {
        Database::Stmt st(db_.handle(),
            "UPDATE customers SET name=?,phone=?,email=?,address=?,city=?,state=?,zip=?,notes=?,"
            "updated_at=strftime('%Y-%m-%dT%H:%M:%SZ','now') WHERE id=?");
        st.bind_text(1,c.name); st.bind_text(2,c.phone); st.bind_text(3,c.email);
        st.bind_text(4,c.address); st.bind_text(5,c.city); st.bind_text(6,c.state);
        st.bind_text(7,c.zip); st.bind_text(8,c.notes); st.bind_int(9,c.id);
        st.step();
        // Update FTS
        Database::Stmt fd(db_.handle(), "DELETE FROM customers_fts WHERE rowid=?");
        fd.bind_int(1,c.id); fd.step();
        Database::Stmt fi(db_.handle(),
            "INSERT INTO customers_fts(rowid,name,phone,email,address,city,notes) VALUES(?,?,?,?,?,?,?)");
        fi.bind_int(1,c.id); fi.bind_text(2,c.name); fi.bind_text(3,c.phone);
        fi.bind_text(4,c.email); fi.bind_text(5,c.address); fi.bind_text(6,c.city);
        fi.bind_text(7,c.notes); fi.step();
        return true;
    } catch (const std::exception& e) { LOGE("[customers] update: " << e.what()); return false; }
}

bool CustomerStore::remove(int id) {
    try {
        Database::Stmt st(db_.handle(), "UPDATE customers SET deleted=1 WHERE id=?");
        st.bind_int(1,id); st.step();
        Database::Stmt fd(db_.handle(), "DELETE FROM customers_fts WHERE rowid=?");
        fd.bind_int(1,id); fd.step();
        return true;
    } catch (...) { return false; }
}

bool CustomerStore::hard_delete(int id) {
    try {
        Database::Stmt st(db_.handle(), "DELETE FROM customers WHERE id=?");
        st.bind_int(1,id); st.step(); return true;
    } catch (...) { return false; }
}

void CustomerStore::rebuild_fts() {
    try {
        db_.exec("DELETE FROM customers_fts");
        db_.exec("INSERT INTO customers_fts(rowid,name,phone,email,address,city,notes) "
                 "SELECT id,name,phone,email,address,city,notes FROM customers WHERE deleted=0");
    } catch (const std::exception& e) { LOGE("[customers] rebuild_fts: " << e.what()); }
}
