#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

class Logger {
public:
    static Logger& instance() {
        static Logger L;
        return L;
    }

    void open(const std::string& path) {
        std::lock_guard<std::mutex> lk(mu_);
        file_.open(path, std::ios::app);
        path_ = path;
    }

    void log(const std::string& level, const std::string& msg) {
        std::string line = now_str() + " [" + level + "] " + msg;
        std::lock_guard<std::mutex> lk(mu_);
        std::cout << line << "\n" << std::flush;
        if (file_.is_open()) {
            file_ << line << "\n" << std::flush;
        }
    }

    void info(const std::string& msg)  { log("INFO ", msg); }
    void warn(const std::string& msg)  { log("WARN ", msg); }
    void error(const std::string& msg) { log("ERROR", msg); }

private:
    std::mutex mu_;
    std::ofstream file_;
    std::string path_;

    static std::string now_str() {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::ostringstream os;
        os << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
        return os.str();
    }
};

#define LOG_INFO(msg)  Logger::instance().info(msg)
#define LOG_WARN(msg)  Logger::instance().warn(msg)
#define LOG_ERROR(msg) Logger::instance().error(msg)

// Stream-style helper
#define LOGSTREAM(level, expr) \
    do { std::ostringstream _ls; _ls << expr; Logger::instance().log(level, _ls.str()); } while(0)

#define LOGI(expr) LOGSTREAM("INFO ", expr)
#define LOGW(expr) LOGSTREAM("WARN ", expr)
#define LOGE(expr) LOGSTREAM("ERROR", expr)
