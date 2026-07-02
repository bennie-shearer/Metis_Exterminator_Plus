#pragma once
#include <string>
#include <optional>
#include <mutex>
#include "database.hpp"

struct User {
    int id = 0;
    std::string username;
    std::string role;
    bool active = true;
};

class AuthManager {
public:
    explicit AuthManager(Database& db, int bcrypt_cost = 12, int session_minutes = 60);

    bool bootstrap_admin(const std::string& username, const std::string& password, const std::string& role = "admin");
    bool reset_admin(const std::string& username, const std::string& password, const std::string& role = "admin");
    bool create_user(const std::string& username, const std::string& password, const std::string& role);
    std::optional<std::string> login(const std::string& username, const std::string& password);
    std::optional<User> validate_session(const std::string& token);
    bool logout(const std::string& token);
    void purge_expired_sessions();

private:
    Database& db_;
    int bcrypt_cost_;
    int session_minutes_;
    mutable std::mutex mu_;

    static std::string generate_token();
    static std::string hash_password(const std::string& pw, int cost);
    static bool verify_password(const std::string& pw, const std::string& hash);
};
