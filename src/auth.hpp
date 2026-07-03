#pragma once
#include <string>
#include <optional>
#include <mutex>
#include <unordered_map>
#include <chrono>
#include "database.hpp"

struct User {
    int id = 0;
    std::string username;
    std::string role;
    std::string api_key;
    bool active = true;
};

class AuthManager {
public:
    explicit AuthManager(Database& db, int bcrypt_cost = 12, int session_minutes = 60,
                         int max_attempts = 5, int lockout_minutes = 15,
                         int rate_limit_per_min = 10);

    bool bootstrap_admin(const std::string& username, const std::string& password, const std::string& role = "admin");
    bool reset_admin(const std::string& username, const std::string& password, const std::string& role = "admin");
    bool create_user(const std::string& username, const std::string& password, const std::string& role);
    bool change_password(int user_id, const std::string& old_password, const std::string& new_password);
    bool set_password(int user_id, const std::string& new_password);  // admin override
    bool deactivate_user(int user_id);
    std::vector<User> list_users();

    std::optional<std::string> login(const std::string& username, const std::string& password,
                                     const std::string& ip = "");
    std::optional<User> validate_session(const std::string& token);
    std::optional<User> validate_api_key(const std::string& key);
    bool logout(const std::string& token);
    void purge_expired_sessions();

    std::string generate_api_key(int user_id);

    // Rate limiting check (returns false if rate limit exceeded)
    bool check_rate_limit(const std::string& ip);

    void write_audit(const std::string& event, const std::string& username,
                     const std::string& ip, const std::string& details, bool success);

private:
    Database& db_;
    int bcrypt_cost_;
    int session_minutes_;
    int max_attempts_;
    int lockout_minutes_;
    int rate_limit_per_min_;
    mutable std::mutex mu_;

    // In-memory rate limit: ip -> list of attempt timestamps
    std::unordered_map<std::string, std::vector<std::chrono::steady_clock::time_point>> rate_map_;

    static std::string generate_token();
    static std::string hash_password(const std::string& pw, int cost);
    static bool verify_password(const std::string& pw, const std::string& hash);

    bool is_locked(const std::string& username);
    void record_failed_login(const std::string& username);
    void clear_failed_logins(const std::string& username);
};
