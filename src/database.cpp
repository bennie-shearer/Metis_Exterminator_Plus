#include "database.hpp"
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

bool Database::open(const std::string& path, bool wal, int cache_kb, int busy_ms) {
    fs::create_directories(fs::path(path).parent_path());
    int rc = sqlite3_open(path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::cerr << "DB open error: " << sqlite3_errmsg(db_) << "\n";
        db_ = nullptr;
        return false;
    }
    sqlite3_busy_timeout(db_, busy_ms);
    if (wal) exec("PRAGMA journal_mode=WAL");
    exec("PRAGMA foreign_keys=ON");
    exec("PRAGMA synchronous=NORMAL");
    exec("PRAGMA cache_size=-" + std::to_string(cache_kb));
    return true;
}

void Database::close() {
    if (db_) { sqlite3_close(db_); db_ = nullptr; }
}

bool Database::exec(const std::string& sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::cerr << "DB exec error: " << (err ? err : "?") << "\n";
        sqlite3_free(err);
        return false;
    }
    return true;
}

bool Database::exec(const std::string& sql, std::function<void(sqlite3_stmt*)> row_cb) {
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "DB prepare error: " << sqlite3_errmsg(db_) << "\n";
        return false;
    }
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) row_cb(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

bool Database::create_schema() {
    static const char* ddl = R"SQL(
CREATE TABLE IF NOT EXISTS customers (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    name         TEXT NOT NULL,
    phone        TEXT DEFAULT '',
    email        TEXT DEFAULT '',
    address      TEXT DEFAULT '',
    city         TEXT DEFAULT '',
    state        TEXT DEFAULT '',
    zip          TEXT DEFAULT '',
    notes        TEXT DEFAULT '',
    active       INTEGER DEFAULT 1,
    deleted      INTEGER DEFAULT 0,
    created_at   TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    updated_at   TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);

CREATE TABLE IF NOT EXISTS jobs (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    customer_id         INTEGER NOT NULL REFERENCES customers(id) ON DELETE CASCADE,
    service_type        TEXT NOT NULL,
    pest_type           TEXT DEFAULT '',
    status              TEXT DEFAULT 'Scheduled',
    scheduled_date      TEXT DEFAULT '',
    scheduled_time      TEXT DEFAULT '',
    technician          TEXT DEFAULT '',
    address             TEXT DEFAULT '',
    notes               TEXT DEFAULT '',
    price               REAL DEFAULT 0.0,
    deleted             INTEGER DEFAULT 0,
    recurring           INTEGER DEFAULT 0,
    recur_interval_days INTEGER DEFAULT 0,
    time_start          TEXT DEFAULT '',
    time_end            TEXT DEFAULT '',
    created_at          TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    completed_at        TEXT DEFAULT ''
);

CREATE TABLE IF NOT EXISTS job_chemicals (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    job_id        INTEGER NOT NULL REFERENCES jobs(id) ON DELETE CASCADE,
    chemical_name TEXT NOT NULL,
    epa_reg       TEXT DEFAULT '',
    quantity_oz   REAL DEFAULT 0.0,
    unit          TEXT DEFAULT 'oz',
    applied_at    TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);

CREATE TABLE IF NOT EXISTS invoices (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    customer_id    INTEGER NOT NULL REFERENCES customers(id) ON DELETE CASCADE,
    job_id         INTEGER DEFAULT 0,
    invoice_number TEXT NOT NULL UNIQUE,
    status         TEXT DEFAULT 'Pending',
    subtotal       REAL DEFAULT 0.0,
    tax_rate       REAL DEFAULT 0.0,
    tax_amount     REAL DEFAULT 0.0,
    total          REAL DEFAULT 0.0,
    issued_date    TEXT DEFAULT '',
    due_date       TEXT DEFAULT '',
    paid_date      TEXT DEFAULT '',
    notes          TEXT DEFAULT '',
    created_at     TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);

CREATE TABLE IF NOT EXISTS invoice_items (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    invoice_id   INTEGER NOT NULL REFERENCES invoices(id) ON DELETE CASCADE,
    description  TEXT NOT NULL,
    unit_price   REAL DEFAULT 0.0,
    quantity     INTEGER DEFAULT 1
);

CREATE TABLE IF NOT EXISTS users (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    username      TEXT NOT NULL UNIQUE COLLATE NOCASE,
    password_hash TEXT NOT NULL,
    role          TEXT DEFAULT 'technician',
    active        INTEGER DEFAULT 1,
    failed_logins INTEGER DEFAULT 0,
    locked_until  TEXT DEFAULT '',
    api_key       TEXT DEFAULT '',
    created_at    TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);

CREATE TABLE IF NOT EXISTS sessions (
    token        TEXT PRIMARY KEY,
    user_id      INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    created_at   TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    expires_at   TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS audit_log (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    event_type TEXT NOT NULL,
    username   TEXT DEFAULT '',
    ip_address TEXT DEFAULT '',
    details    TEXT DEFAULT '',
    success    INTEGER DEFAULT 1,
    created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);

CREATE TABLE IF NOT EXISTS login_attempts (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    username   TEXT NOT NULL,
    ip_address TEXT DEFAULT '',
    success    INTEGER DEFAULT 0,
    created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);

CREATE INDEX IF NOT EXISTS idx_jobs_customer      ON jobs(customer_id);
CREATE INDEX IF NOT EXISTS idx_jobs_status        ON jobs(status);
CREATE INDEX IF NOT EXISTS idx_jobs_date          ON jobs(scheduled_date);
CREATE INDEX IF NOT EXISTS idx_jobs_deleted       ON jobs(deleted);
CREATE INDEX IF NOT EXISTS idx_invoices_customer  ON invoices(customer_id);
CREATE INDEX IF NOT EXISTS idx_invoices_status    ON invoices(status);
CREATE INDEX IF NOT EXISTS idx_sessions_expires   ON sessions(expires_at);
CREATE INDEX IF NOT EXISTS idx_audit_event        ON audit_log(event_type);
CREATE INDEX IF NOT EXISTS idx_audit_created      ON audit_log(created_at);
CREATE INDEX IF NOT EXISTS idx_login_attempts_usr ON login_attempts(username, created_at);
CREATE INDEX IF NOT EXISTS idx_customers_deleted  ON customers(deleted);

-- FTS5 full-text search on customers and jobs
CREATE VIRTUAL TABLE IF NOT EXISTS customers_fts USING fts5(
    name, phone, email, address, city, notes,
    content='customers', content_rowid='id'
);
CREATE VIRTUAL TABLE IF NOT EXISTS jobs_fts USING fts5(
    service_type, pest_type, technician, address, notes,
    content='jobs', content_rowid='id'
);
)SQL";
    exec("BEGIN");
    bool ok = exec(ddl);
    exec(ok ? "COMMIT" : "ROLLBACK");
    return ok;
}

// Stmt implementation
Database::Stmt::Stmt(sqlite3* db, const std::string& sql) {
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt_, nullptr);
    if (rc != SQLITE_OK)
        throw std::runtime_error(std::string("Stmt prepare: ") + sqlite3_errmsg(db));
}

Database::Stmt::~Stmt() { if (stmt_) sqlite3_finalize(stmt_); }

void Database::Stmt::bind_int(int idx, int val)            { sqlite3_bind_int(stmt_, idx, val); }
void Database::Stmt::bind_int64(int idx, int64_t val)      { sqlite3_bind_int64(stmt_, idx, val); }
void Database::Stmt::bind_double(int idx, double val)      { sqlite3_bind_double(stmt_, idx, val); }
void Database::Stmt::bind_null(int idx)                    { sqlite3_bind_null(stmt_, idx); }
void Database::Stmt::bind_text(int idx, const std::string& val) {
    sqlite3_bind_text(stmt_, idx, val.c_str(), -1, SQLITE_TRANSIENT);
}

bool Database::Stmt::step() {
    int rc = sqlite3_step(stmt_);
    return rc == SQLITE_ROW;
}

void Database::Stmt::reset() { sqlite3_reset(stmt_); }

int         Database::Stmt::col_int(int idx)    const { return sqlite3_column_int(stmt_, idx); }
int64_t     Database::Stmt::col_int64(int idx)  const { return sqlite3_column_int64(stmt_, idx); }
double      Database::Stmt::col_double(int idx)  const { return sqlite3_column_double(stmt_, idx); }
std::string Database::Stmt::col_text(int idx)    const {
    const char* t = reinterpret_cast<const char*>(sqlite3_column_text(stmt_, idx));
    return t ? t : "";
}
