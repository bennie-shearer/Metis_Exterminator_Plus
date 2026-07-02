#pragma once
#include <string>
#include <functional>
#include <vector>
#include <stdexcept>
#include <cstdint>
#include "../third_party/sqlite/sqlite3.h"

class Database {
public:
    explicit Database() : db_(nullptr) {}
    ~Database() { close(); }

    bool open(const std::string& path, bool wal = true, int cache_kb = 4096, int busy_ms = 5000);
    void close();
    bool is_open() const { return db_ != nullptr; }

    bool exec(const std::string& sql);
    bool exec(const std::string& sql, std::function<void(sqlite3_stmt*)> row_cb);

    sqlite3* handle() { return db_; }

    class Stmt {
    public:
        Stmt(sqlite3* db, const std::string& sql);
        ~Stmt();
        Stmt(const Stmt&) = delete;
        Stmt& operator=(const Stmt&) = delete;

        void bind_int(int idx, int val);
        void bind_int64(int idx, int64_t val);
        void bind_double(int idx, double val);
        void bind_text(int idx, const std::string& val);
        void bind_null(int idx);

        bool step();
        void reset();

        int    col_int(int idx)    const;
        int64_t col_int64(int idx) const;
        double col_double(int idx) const;
        std::string col_text(int idx) const;

    private:
        sqlite3_stmt* stmt_ = nullptr;
    };

    bool create_schema();

private:
    sqlite3* db_;
};
