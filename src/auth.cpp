#include "auth.hpp"
#include "logger.hpp"
#include "../third_party/bcrypt/bcrypt.h"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <iostream>

static std::string now_plus_minutes(int minutes) {
    using namespace std::chrono;
    auto t = system_clock::now() + std::chrono::minutes(minutes);
    std::time_t tt = system_clock::to_time_t(t);
    std::ostringstream os;
    os << std::put_time(std::gmtime(&tt), "%Y-%m-%dT%H:%M:%SZ");
    return os.str();
}

static std::string now_iso() {
    auto t = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(t);
    std::ostringstream os;
    os << std::put_time(std::gmtime(&tt), "%Y-%m-%dT%H:%M:%SZ");
    return os.str();
}

AuthManager::AuthManager(Database& db, int bcrypt_cost, int session_minutes)
    : db_(db), bcrypt_cost_(bcrypt_cost), session_minutes_(session_minutes) {}

std::string AuthManager::generate_token() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dist;
    std::ostringstream os;
    for (int i = 0; i < 4; ++i)
        os << std::hex << std::setfill('0') << std::setw(16) << dist(gen);
    return os.str();
}

std::string AuthManager::hash_password(const std::string& pw, int cost) {
    char salt[BCRYPT_HASHSIZE];
    char hash[BCRYPT_HASHSIZE];
    if (bcrypt_gensalt(cost, salt) != 0) return {};
    if (bcrypt_hashpw(pw.c_str(), salt, hash) != 0) return {};
    return std::string(hash);
}

bool AuthManager::verify_password(const std::string& pw, const std::string& hash) {
    return bcrypt_checkpw(pw.c_str(), hash.c_str()) == 0;
}

bool AuthManager::bootstrap_admin(const std::string& username, const std::string& password, const std::string& role) {
    std::lock_guard<std::mutex> lk(mu_);
    bool exists = false;
    db_.exec("SELECT COUNT(*) FROM users", [&](sqlite3_stmt* s){
        exists = sqlite3_column_int(s, 0) > 0;
    });
    if (exists) return false;
    return create_user(username, password, role);
}

bool AuthManager::reset_admin(const std::string& username, const std::string& password, const std::string& role) {
    // Do NOT call create_user() here - it also locks mu_ causing deadlock.
    // Inline the hash + insert instead.
    std::string hash = hash_password(password, bcrypt_cost_);
    if (hash.empty()) {
        LOGE("reset_admin: hash_password FAILED for user=" << username);
        return false;
    }
    std::lock_guard<std::mutex> lk(mu_);
    db_.exec("DELETE FROM sessions");
    db_.exec("DELETE FROM users");
    try {
        Database::Stmt st(db_.handle(), "INSERT INTO users(username,password_hash,role) VALUES(?,?,?)");
        st.bind_text(1, username);
        st.bind_text(2, hash);
        st.bind_text(3, role);
        st.step();
        LOGI("reset_admin: OK  user=" << username << "  role=" << role
             << "  hash_prefix=" << hash.substr(0,7));
        return true;
    } catch (const std::exception& e) {
        LOGE("reset_admin: INSERT failed: " << e.what());
        return false;
    }
}

bool AuthManager::create_user(const std::string& username, const std::string& password, const std::string& role) {
    std::string hash = hash_password(password, bcrypt_cost_);
    if (hash.empty()) return false;
    try {
        Database::Stmt st(db_.handle(), "INSERT INTO users(username,password_hash,role) VALUES(?,?,?)");
        st.bind_text(1, username);
        st.bind_text(2, hash);
        st.bind_text(3, role);
        st.step();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "create_user: " << e.what() << "\n";
        return false;
    }
}

std::optional<std::string> AuthManager::login(const std::string& username, const std::string& password) {
    std::lock_guard<std::mutex> lk(mu_);
    int user_id = 0;
    std::string stored_hash;
    bool active = false;
    try {
        Database::Stmt st(db_.handle(), "SELECT id,password_hash,active FROM users WHERE username=? COLLATE NOCASE");
        st.bind_text(1, username);
        if (!st.step()) {
            LOGE("login: user NOT FOUND in DB: '" << username << "'");
            return {};
        }
        user_id     = st.col_int(0);
        stored_hash = st.col_text(1);
        active      = st.col_int(2) != 0;
        LOGI("login: found  user='" << username << "'  id=" << user_id
             << "  active=" << active
             << "  hash_prefix=" << stored_hash.substr(0,7));
    } catch (...) { LOGE("login: DB query exception for user='" << username << "'"); return {}; }

    if (!active) { LOGE("login: user INACTIVE: " << username); return {}; }
    bool pw_ok = verify_password(password, stored_hash);
    LOGI("login: bcrypt verify=" << (pw_ok ? "OK" : "FAIL")
         << "  user='" << username << "'  pass_len=" << password.size());
    if (!pw_ok) return {};

    std::string token = generate_token();
    std::string expires = now_plus_minutes(session_minutes_);
    try {
        Database::Stmt st(db_.handle(), "INSERT INTO sessions(token,user_id,expires_at) VALUES(?,?,?)");
        st.bind_text(1, token);
        st.bind_int(2, user_id);
        st.bind_text(3, expires);
        st.step();
    } catch (...) { return {}; }
    return token;
}

std::optional<User> AuthManager::validate_session(const std::string& token) {
    std::lock_guard<std::mutex> lk(mu_);
    std::string now = now_iso();
    User u;
    bool found = false;
    try {
        Database::Stmt st(db_.handle(),
            "SELECT u.id,u.username,u.role,u.active "
            "FROM sessions s JOIN users u ON s.user_id=u.id "
            "WHERE s.token=? AND s.expires_at>?");
        st.bind_text(1, token);
        st.bind_text(2, now);
        if (!st.step()) return {};
        u.id       = st.col_int(0);
        u.username = st.col_text(1);
        u.role     = st.col_text(2);
        u.active   = st.col_int(3) != 0;
        found = true;
    } catch (...) {}
    if (!found || !u.active) return {};
    return u;
}

bool AuthManager::logout(const std::string& token) {
    std::lock_guard<std::mutex> lk(mu_);
    try {
        Database::Stmt st(db_.handle(), "DELETE FROM sessions WHERE token=?");
        st.bind_text(1, token);
        st.step();
        return true;
    } catch (...) { return false; }
}

void AuthManager::purge_expired_sessions() {
    std::lock_guard<std::mutex> lk(mu_);
    std::string now = now_iso();
    try {
        Database::Stmt st(db_.handle(), "DELETE FROM sessions WHERE expires_at<?");
        st.bind_text(1, now);
        st.step();
    } catch (...) {}
}
