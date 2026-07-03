#include "auth.hpp"
#include "logger.hpp"
#include "../third_party/bcrypt/bcrypt.h"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <random>
#include <iostream>
#include <vector>
#include <algorithm>

static std::string now_iso() {
    auto t = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(t);
    std::ostringstream os;
    os << std::put_time(std::gmtime(&tt), "%Y-%m-%dT%H:%M:%SZ");
    return os.str();
}

static std::string now_plus_minutes(int minutes) {
    using namespace std::chrono;
    auto t = system_clock::now() + std::chrono::minutes(minutes);
    std::time_t tt = system_clock::to_time_t(t);
    std::ostringstream os;
    os << std::put_time(std::gmtime(&tt), "%Y-%m-%dT%H:%M:%SZ");
    return os.str();
}

AuthManager::AuthManager(Database& db, int bcrypt_cost, int session_minutes,
                         int max_attempts, int lockout_minutes, int rate_limit_per_min)
    : db_(db), bcrypt_cost_(bcrypt_cost), session_minutes_(session_minutes),
      max_attempts_(max_attempts), lockout_minutes_(lockout_minutes),
      rate_limit_per_min_(rate_limit_per_min) {}

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

bool AuthManager::is_locked(const std::string& username) {
    std::string now = now_iso();
    bool locked = false;
    db_.exec("SELECT locked_until FROM users WHERE username=? COLLATE NOCASE",
        [&](sqlite3_stmt* s) {
            std::string lu = reinterpret_cast<const char*>(sqlite3_column_text(s, 0));
            if (!lu.empty() && lu > now) locked = true;
        });
    // Also check sqlite3_bind
    try {
        Database::Stmt st(db_.handle(),
            "SELECT locked_until FROM users WHERE username=? COLLATE NOCASE");
        st.bind_text(1, username);
        if (st.step()) {
            std::string lu = st.col_text(0);
            locked = (!lu.empty() && lu > now);
        }
    } catch (...) {}
    return locked;
}

void AuthManager::record_failed_login(const std::string& username) {
    try {
        // Increment failed_logins
        Database::Stmt st(db_.handle(),
            "UPDATE users SET failed_logins = failed_logins + 1 WHERE username=? COLLATE NOCASE");
        st.bind_text(1, username);
        st.step();
        // Check if should lock
        int fails = 0;
        Database::Stmt st2(db_.handle(),
            "SELECT failed_logins FROM users WHERE username=? COLLATE NOCASE");
        st2.bind_text(1, username);
        if (st2.step()) fails = st2.col_int(0);
        if (fails >= max_attempts_) {
            std::string lockout = now_plus_minutes(lockout_minutes_);
            Database::Stmt st3(db_.handle(),
                "UPDATE users SET locked_until=? WHERE username=? COLLATE NOCASE");
            st3.bind_text(1, lockout);
            st3.bind_text(2, username);
            st3.step();
            LOGE("[auth] Account locked: " << username << " until " << lockout);
        }
    } catch (...) {}
}

void AuthManager::clear_failed_logins(const std::string& username) {
    try {
        Database::Stmt st(db_.handle(),
            "UPDATE users SET failed_logins=0, locked_until='' WHERE username=? COLLATE NOCASE");
        st.bind_text(1, username);
        st.step();
    } catch (...) {}
}

bool AuthManager::reset_admin(const std::string& username, const std::string& password, const std::string& role) {
    std::string hash = hash_password(password, bcrypt_cost_);
    if (hash.empty()) { LOGE("reset_admin: hash_password FAILED for user=" << username); return false; }
    std::lock_guard<std::mutex> lk(mu_);
    db_.exec("DELETE FROM sessions");
    db_.exec("DELETE FROM users");
    try {
        Database::Stmt st(db_.handle(), "INSERT INTO users(username,password_hash,role) VALUES(?,?,?)");
        st.bind_text(1, username); st.bind_text(2, hash); st.bind_text(3, role);
        st.step();
        LOGI("reset_admin: OK  user=" << username << "  role=" << role << "  hash_prefix=" << hash.substr(0,7));
        return true;
    } catch (const std::exception& e) { LOGE("reset_admin: INSERT failed: " << e.what()); return false; }
}

bool AuthManager::bootstrap_admin(const std::string& username, const std::string& password, const std::string& role) {
    std::lock_guard<std::mutex> lk(mu_);
    bool exists = false;
    db_.exec("SELECT COUNT(*) FROM users", [&](sqlite3_stmt* s){ exists = sqlite3_column_int(s,0)>0; });
    if (exists) return false;
    std::string hash = hash_password(password, bcrypt_cost_);
    if (hash.empty()) return false;
    try {
        Database::Stmt st(db_.handle(), "INSERT INTO users(username,password_hash,role) VALUES(?,?,?)");
        st.bind_text(1, username); st.bind_text(2, hash); st.bind_text(3, role);
        st.step(); return true;
    } catch (...) { return false; }
}

bool AuthManager::create_user(const std::string& username, const std::string& password, const std::string& role) {
    std::string hash = hash_password(password, bcrypt_cost_);
    if (hash.empty()) return false;
    std::lock_guard<std::mutex> lk(mu_);
    try {
        Database::Stmt st(db_.handle(), "INSERT INTO users(username,password_hash,role) VALUES(?,?,?)");
        st.bind_text(1, username); st.bind_text(2, hash); st.bind_text(3, role);
        st.step();
        write_audit("user_created", username, "", "role=" + role, true);
        return true;
    } catch (const std::exception& e) { LOGE("create_user: " << e.what()); return false; }
}

bool AuthManager::change_password(int user_id, const std::string& old_password, const std::string& new_password) {
    std::lock_guard<std::mutex> lk(mu_);
    try {
        Database::Stmt st(db_.handle(), "SELECT password_hash,username FROM users WHERE id=?");
        st.bind_int(1, user_id);
        if (!st.step()) return false;
        std::string stored = st.col_text(0);
        std::string uname  = st.col_text(1);
        if (!verify_password(old_password, stored)) return false;
        std::string new_hash = hash_password(new_password, bcrypt_cost_);
        if (new_hash.empty()) return false;
        Database::Stmt st2(db_.handle(), "UPDATE users SET password_hash=? WHERE id=?");
        st2.bind_text(1, new_hash); st2.bind_int(2, user_id); st2.step();
        write_audit("password_changed", uname, "", "user_id=" + std::to_string(user_id), true);
        return true;
    } catch (...) { return false; }
}

bool AuthManager::set_password(int user_id, const std::string& new_password) {
    std::string new_hash = hash_password(new_password, bcrypt_cost_);
    if (new_hash.empty()) return false;
    std::lock_guard<std::mutex> lk(mu_);
    try {
        Database::Stmt st(db_.handle(), "UPDATE users SET password_hash=?,failed_logins=0,locked_until='' WHERE id=?");
        st.bind_text(1, new_hash); st.bind_int(2, user_id); st.step();
        return true;
    } catch (...) { return false; }
}

bool AuthManager::deactivate_user(int user_id) {
    std::lock_guard<std::mutex> lk(mu_);
    try {
        Database::Stmt st(db_.handle(), "UPDATE users SET active=0 WHERE id=?");
        st.bind_int(1, user_id); st.step();
        // Kill all sessions for this user
        Database::Stmt st2(db_.handle(), "DELETE FROM sessions WHERE user_id=?");
        st2.bind_int(1, user_id); st2.step();
        return true;
    } catch (...) { return false; }
}

std::vector<User> AuthManager::list_users() {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<User> users;
    try {
        Database::Stmt st(db_.handle(), "SELECT id,username,role,active,api_key FROM users ORDER BY id");
        while (st.step()) {
            User u;
            u.id       = st.col_int(0);
            u.username = st.col_text(1);
            u.role     = st.col_text(2);
            u.active   = st.col_int(3) != 0;
            u.api_key  = st.col_text(4);
            users.push_back(u);
        }
    } catch (...) {}
    return users;
}

std::string AuthManager::generate_api_key(int user_id) {
    std::string key = "mep_" + generate_token();
    std::lock_guard<std::mutex> lk(mu_);
    try {
        Database::Stmt st(db_.handle(), "UPDATE users SET api_key=? WHERE id=?");
        st.bind_text(1, key); st.bind_int(2, user_id); st.step();
    } catch (...) {}
    return key;
}

bool AuthManager::check_rate_limit(const std::string& ip) {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lk(mu_);
    auto& attempts = rate_map_[ip];
    // Remove attempts older than 1 minute
    attempts.erase(std::remove_if(attempts.begin(), attempts.end(),
        [&](const auto& t){ return std::chrono::duration_cast<std::chrono::seconds>(now - t).count() > 60; }),
        attempts.end());
    if ((int)attempts.size() >= rate_limit_per_min_) return false;
    attempts.push_back(now);
    return true;
}

std::optional<std::string> AuthManager::login(const std::string& username,
                                               const std::string& password,
                                               const std::string& ip) {
    std::lock_guard<std::mutex> lk(mu_);
    int user_id = 0;
    std::string stored_hash;
    bool active = false;
    try {
        Database::Stmt st(db_.handle(),
            "SELECT id,password_hash,active FROM users WHERE username=? COLLATE NOCASE");
        st.bind_text(1, username);
        if (!st.step()) {
            LOGE("login: user NOT FOUND: '" << username << "'");
            return {};
        }
        user_id     = st.col_int(0);
        stored_hash = st.col_text(1);
        active      = st.col_int(2) != 0;
        LOGI("login: found  user='" << username << "'  id=" << user_id
             << "  active=" << active << "  hash_prefix=" << stored_hash.substr(0,7));
    } catch (...) { return {}; }

    if (!active) { LOGE("login: INACTIVE: " << username); return {}; }

    // Check lockout
    {
        std::string now = now_iso();
        std::string locked_until;
        try {
            Database::Stmt st(db_.handle(), "SELECT locked_until FROM users WHERE id=?");
            st.bind_int(1, user_id);
            if (st.step()) locked_until = st.col_text(0);
        } catch (...) {}
        if (!locked_until.empty() && locked_until > now) {
            LOGE("login: LOCKED until " << locked_until << " user=" << username);
            return {};
        }
    }

    bool pw_ok = verify_password(password, stored_hash);
    LOGI("login: bcrypt verify=" << (pw_ok ? "OK" : "FAIL")
         << "  user='" << username << "'  pass_len=" << password.size());

    if (!pw_ok) {
        // Increment failed login counter
        int fails = 0;
        try {
            Database::Stmt st(db_.handle(),
                "UPDATE users SET failed_logins=failed_logins+1 WHERE id=? RETURNING failed_logins");
            st.bind_int(1, user_id);
            if (st.step()) fails = st.col_int(0);
        } catch (...) {}
        if (fails >= max_attempts_) {
            std::string lockout = now_plus_minutes(lockout_minutes_);
            try {
                Database::Stmt st(db_.handle(), "UPDATE users SET locked_until=? WHERE id=?");
                st.bind_text(1, lockout); st.bind_int(2, user_id); st.step();
            } catch (...) {}
            LOGE("login: LOCKED " << username << " after " << fails << " failures until " << lockout);
        }
        return {};
    }

    // Clear failed logins on success
    try {
        Database::Stmt st(db_.handle(), "UPDATE users SET failed_logins=0,locked_until='' WHERE id=?");
        st.bind_int(1, user_id); st.step();
    } catch (...) {}

    std::string token   = generate_token();
    std::string expires = now_plus_minutes(session_minutes_);
    try {
        Database::Stmt st(db_.handle(), "INSERT INTO sessions(token,user_id,expires_at) VALUES(?,?,?)");
        st.bind_text(1, token); st.bind_int(2, user_id); st.bind_text(3, expires);
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
            "SELECT u.id,u.username,u.role,u.active,u.api_key "
            "FROM sessions s JOIN users u ON s.user_id=u.id "
            "WHERE s.token=? AND s.expires_at>?");
        st.bind_text(1, token); st.bind_text(2, now);
        if (!st.step()) return {};
        u.id       = st.col_int(0);
        u.username = st.col_text(1);
        u.role     = st.col_text(2);
        u.active   = st.col_int(3) != 0;
        u.api_key  = st.col_text(4);
        found = true;
    } catch (...) {}
    if (!found || !u.active) return {};
    return u;
}

std::optional<User> AuthManager::validate_api_key(const std::string& key) {
    if (key.empty()) return {};
    std::lock_guard<std::mutex> lk(mu_);
    User u;
    try {
        Database::Stmt st(db_.handle(),
            "SELECT id,username,role,active FROM users WHERE api_key=? AND active=1");
        st.bind_text(1, key);
        if (!st.step()) return {};
        u.id       = st.col_int(0);
        u.username = st.col_text(1);
        u.role     = st.col_text(2);
        u.active   = st.col_int(3) != 0;
        u.api_key  = key;
    } catch (...) { return {}; }
    return u;
}

bool AuthManager::logout(const std::string& token) {
    std::lock_guard<std::mutex> lk(mu_);
    try {
        Database::Stmt st(db_.handle(), "DELETE FROM sessions WHERE token=?");
        st.bind_text(1, token); st.step(); return true;
    } catch (...) { return false; }
}

void AuthManager::purge_expired_sessions() {
    std::lock_guard<std::mutex> lk(mu_);
    std::string now = now_iso();
    try {
        Database::Stmt st(db_.handle(), "DELETE FROM sessions WHERE expires_at<?");
        st.bind_text(1, now); st.step();
    } catch (...) {}
}

void AuthManager::write_audit(const std::string& event, const std::string& username,
                               const std::string& ip, const std::string& details, bool success) {
    try {
        Database::Stmt st(db_.handle(),
            "INSERT INTO audit_log(event_type,username,ip_address,details,success) VALUES(?,?,?,?,?)");
        st.bind_text(1, event); st.bind_text(2, username);
        st.bind_text(3, ip); st.bind_text(4, details);
        st.bind_int(5, success ? 1 : 0);
        st.step();
    } catch (...) {}
}
