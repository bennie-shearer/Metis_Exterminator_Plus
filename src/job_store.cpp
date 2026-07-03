#include "job_store.hpp"
#include "logger.hpp"
#include <stdexcept>

JobStore::JobStore(Database& db) : db_(db) {}

static Job row_to_job(Database::Stmt& st) {
    Job j;
    j.id                 = st.col_int(0);
    j.customer_id        = st.col_int(1);
    j.service_type       = st.col_text(2);
    j.pest_type          = st.col_text(3);
    j.status             = st.col_text(4);
    j.scheduled_date     = st.col_text(5);
    j.scheduled_time     = st.col_text(6);
    j.technician         = st.col_text(7);
    j.address            = st.col_text(8);
    j.notes              = st.col_text(9);
    j.price              = st.col_double(10);
    j.created_at         = st.col_text(11);
    j.completed_at       = st.col_text(12);
    j.deleted            = st.col_int(13) != 0;
    j.recurring          = st.col_int(14) != 0;
    j.recur_interval_days= st.col_int(15);
    j.time_start         = st.col_text(16);
    j.time_end           = st.col_text(17);
    return j;
}

static const char* JOB_SELECT =
    "SELECT id,customer_id,service_type,pest_type,status,scheduled_date,scheduled_time,"
    "technician,address,notes,price,created_at,completed_at,deleted,recurring,"
    "recur_interval_days,time_start,time_end FROM jobs";

std::vector<Job> JobStore::all() const {
    std::vector<Job> result;
    try {
        Database::Stmt st(db_.handle(),
            std::string(JOB_SELECT) + " WHERE deleted=0 ORDER BY scheduled_date DESC");
        while (st.step()) result.push_back(row_to_job(st));
    } catch (const std::exception& e) { LOGE("[jobs] all: " << e.what()); }
    return result;
}

std::vector<Job> JobStore::for_customer(int customer_id) const {
    std::vector<Job> result;
    try {
        Database::Stmt st(db_.handle(),
            std::string(JOB_SELECT) + " WHERE customer_id=? AND deleted=0 ORDER BY scheduled_date DESC");
        st.bind_int(1, customer_id);
        while (st.step()) result.push_back(row_to_job(st));
    } catch (...) {}
    return result;
}

std::optional<Job> JobStore::find(int id) const {
    try {
        Database::Stmt st(db_.handle(), std::string(JOB_SELECT) + " WHERE id=?");
        st.bind_int(1, id);
        if (!st.step()) return {};
        return row_to_job(st);
    } catch (...) { return {}; }
}

Job JobStore::add(Job j) {
    try {
        Database::Stmt st(db_.handle(),
            "INSERT INTO jobs(customer_id,service_type,pest_type,status,scheduled_date,"
            "scheduled_time,technician,address,notes,price,recurring,recur_interval_days,"
            "time_start,time_end) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?) RETURNING id,created_at");
        st.bind_int(1,j.customer_id); st.bind_text(2,j.service_type);
        st.bind_text(3,j.pest_type); st.bind_text(4,j.status.empty()?"Scheduled":j.status);
        st.bind_text(5,j.scheduled_date); st.bind_text(6,j.scheduled_time);
        st.bind_text(7,j.technician); st.bind_text(8,j.address);
        st.bind_text(9,j.notes); st.bind_double(10,j.price);
        st.bind_int(11,j.recurring?1:0); st.bind_int(12,j.recur_interval_days);
        st.bind_text(13,j.time_start); st.bind_text(14,j.time_end);
        if (st.step()) { j.id = st.col_int(0); j.created_at = st.col_text(1); }
        // FTS
        Database::Stmt fi(db_.handle(),
            "INSERT INTO jobs_fts(rowid,service_type,pest_type,technician,address,notes) VALUES(?,?,?,?,?,?)");
        fi.bind_int(1,j.id); fi.bind_text(2,j.service_type); fi.bind_text(3,j.pest_type);
        fi.bind_text(4,j.technician); fi.bind_text(5,j.address); fi.bind_text(6,j.notes);
        fi.step();
    } catch (const std::exception& e) { LOGE("[jobs] add: " << e.what()); }
    return j;
}

bool JobStore::update(const Job& j) {
    try {
        Database::Stmt st(db_.handle(),
            "UPDATE jobs SET service_type=?,pest_type=?,status=?,scheduled_date=?,"
            "scheduled_time=?,technician=?,address=?,notes=?,price=?,recurring=?,"
            "recur_interval_days=?,time_start=?,time_end=?,completed_at=? WHERE id=?");
        st.bind_text(1,j.service_type); st.bind_text(2,j.pest_type);
        st.bind_text(3,j.status); st.bind_text(4,j.scheduled_date);
        st.bind_text(5,j.scheduled_time); st.bind_text(6,j.technician);
        st.bind_text(7,j.address); st.bind_text(8,j.notes);
        st.bind_double(9,j.price); st.bind_int(10,j.recurring?1:0);
        st.bind_int(11,j.recur_interval_days); st.bind_text(12,j.time_start);
        st.bind_text(13,j.time_end); st.bind_text(14,j.completed_at);
        st.bind_int(15,j.id); st.step();
        // FTS
        Database::Stmt fd(db_.handle(), "DELETE FROM jobs_fts WHERE rowid=?");
        fd.bind_int(1,j.id); fd.step();
        Database::Stmt fi(db_.handle(),
            "INSERT INTO jobs_fts(rowid,service_type,pest_type,technician,address,notes) VALUES(?,?,?,?,?,?)");
        fi.bind_int(1,j.id); fi.bind_text(2,j.service_type); fi.bind_text(3,j.pest_type);
        fi.bind_text(4,j.technician); fi.bind_text(5,j.address); fi.bind_text(6,j.notes);
        fi.step();
        return true;
    } catch (const std::exception& e) { LOGE("[jobs] update: " << e.what()); return false; }
}

bool JobStore::remove(int id) {
    try {
        Database::Stmt st(db_.handle(), "UPDATE jobs SET deleted=1 WHERE id=?");
        st.bind_int(1,id); st.step();
        Database::Stmt fd(db_.handle(), "DELETE FROM jobs_fts WHERE rowid=?");
        fd.bind_int(1,id); fd.step();
        return true;
    } catch (...) { return false; }
}

bool JobStore::hard_delete(int id) {
    try {
        Database::Stmt st(db_.handle(), "DELETE FROM jobs WHERE id=?");
        st.bind_int(1,id); st.step(); return true;
    } catch (...) { return false; }
}

void JobStore::rebuild_fts() {
    try {
        db_.exec("DELETE FROM jobs_fts");
        db_.exec("INSERT INTO jobs_fts(rowid,service_type,pest_type,technician,address,notes) "
                 "SELECT id,service_type,pest_type,technician,address,notes FROM jobs WHERE deleted=0");
    } catch (const std::exception& e) { LOGE("[jobs] rebuild_fts: " << e.what()); }
}
