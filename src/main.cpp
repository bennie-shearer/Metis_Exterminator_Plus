#include "version.hpp"
#include "pson.hpp"
#include "server.hpp"
#include "router.hpp"
#include "database.hpp"
#include "auth.hpp"
#include "logger.hpp"
#include "customer_store.hpp"
#include "job_store.hpp"
#include "invoice_store.hpp"
#include "../infra/gpu/gpu_stub.hpp"
#include "../infra/kubernetes/k8s_stub.hpp"
#include "../infra/containers/container_stub.hpp"

#include <iostream>
#include <filesystem>
#include <csignal>
#include <memory>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;

static HttpServer* g_srv = nullptr;

static void on_signal(int) {
    if (g_srv) g_srv->stop();
}

static fs::path exe_dir() {
#ifdef _WIN32
    char buf[4096];
    GetModuleFileNameA(nullptr, buf, sizeof(buf));
    return fs::path(buf).parent_path();
#else
    return fs::canonical("/proc/self/exe").parent_path();
#endif
}

int main() {
    // Banner before logger is open
    std::cout << "=======================================================\n"
              << METIS_EXTERMINATOR_APP << " v" << METIS_EXTERMINATOR_VERSION << "\n"
              << "Dedicated to the Chones Family\n"
              << "Author: " << METIS_EXTERMINATOR_AUTHOR << "\n"
              << "License: " << METIS_EXTERMINATOR_LICENSE << "\n"
              << "=======================================================\n\n"
              << std::flush;

    fs::path base = exe_dir();
    fs::path cfg_path = base / "config" / "config.pson";
    if (!fs::exists(cfg_path)) cfg_path = base / ".." / "config" / "config.pson";

    Pson cfg;
    if (!cfg.load(cfg_path.string())) {
        std::cerr << "Warning: config not found at " << cfg_path << ", using defaults.\n" << std::flush;
    } else {
        std::cout << "[config] Loaded: " << cfg_path.string() << "\n" << std::flush;
    }

    // Open log file - all subsequent output goes to stdout AND file
    std::string log_file_rel = cfg.get_or("logging","file","logs/metis_exterminator.log");
    fs::path log_path = base / log_file_rel;
    fs::create_directories(log_path.parent_path());
    Logger::instance().open(log_path.string());

    LOGI("=== " << METIS_EXTERMINATOR_APP << " v" << METIS_EXTERMINATOR_VERSION << " starting ===");
    LOGI("Config: " << cfg_path.string());
    LOGI("Log file: " << fs::absolute(log_path).string());
    std::cout << "[LOG FILE]: " << fs::absolute(log_path).string() << "\n" << std::flush;

    // Infra detection
    LOGI("[infra] GPU enabled: "        << (cfg.get_or("infra","gpu_enabled","false")         == "true" ? "yes" : "no"));
    LOGI("[infra] Kubernetes enabled: " << (cfg.get_or("infra","kubernetes_enabled","false")  == "true" ? "yes" : "no"));
    LOGI("[infra] Containers enabled: " << (cfg.get_or("infra","containers_enabled","false")  == "true" ? "yes" : "no"));
    bool in_container = metis::infra::containers::running_in_container();
    if (in_container) LOGI("[infra] Running inside container");

    // Database
    std::string db_path_rel = cfg.get_or("database","path","data/metis_exterminator.db");
    fs::path db_path = base / db_path_rel;
    bool wal      = cfg.get_or("database","wal_mode","true") == "true";
    int  cache_kb = cfg.get_int("database","cache_size_kb", 4096);
    int  busy_ms  = cfg.get_int("database","busy_timeout_ms", 5000);

    Database db;
    if (!db.open(db_path.string(), wal, cache_kb, busy_ms)) {
        LOGE("Fatal: cannot open database: " << db_path.string());
        return 1;
    }
    if (!db.create_schema()) {
        LOGE("Fatal: schema creation failed");
        return 1;
    }
    LOGI("[db] SQLite: " << db_path.string());

    // Auth - always reset from PSON so config is single source of truth
    int bcrypt_cost        = cfg.get_int("app","bcrypt_cost", 12);
    int session_min        = cfg.get_int("app","session_timeout_minutes", 60);
    int purge_min          = cfg.get_int("session","purge_interval_minutes", 10);
    std::string admin_user = cfg.get_or("auth","admin_user","admin");
    std::string admin_pass = cfg.get_or("auth","admin_password","Admin#2026");
    std::string admin_role = cfg.get_or("auth","admin_role","admin");

    AuthManager auth(db, bcrypt_cost, session_min);
    bool reset_ok = auth.reset_admin(admin_user, admin_pass, admin_role);
    LOGI("[auth] Admin reset from PSON: " << (reset_ok ? "OK" : "FAILED")
         << "  user=" << admin_user
         << "  pass=" << admin_pass
         << "  role=" << admin_role
         << "  bcrypt_cost=" << bcrypt_cost);

    // Flat-file stores (compatibility / export-import)
    std::string d_dir_rel = cfg.get_or("server","data_dir","data");
    fs::path data_dir = base / d_dir_rel;
    fs::create_directories(data_dir);

    CustomerStore customers; customers.load(data_dir.string());
    JobStore      jobs;      jobs.load(data_dir.string());
    InvoiceStore  invoices;  invoices.load(data_dir.string());

    // HTTP server
    std::string host      = cfg.get_or("server","host","127.0.0.1");
    int         port      = cfg.get_int("server","port", 9100);
    std::string s_dir_rel = cfg.get_or("server","static_dir","web");
    fs::path static_dir   = base / s_dir_rel;

    HttpServer srv(host, port, static_dir.string());
    g_srv = &srv;

    // TLS 1.3 + AES-256-GCM + Post-Quantum (X25519MLKEM768)
    bool tls_on = cfg.get_or("server","tls_enabled","false") == "true";
    if (tls_on) {
        std::string cert = cfg.get_or("server","tls_cert","tls/server.crt");
        std::string key  = cfg.get_or("server","tls_key","tls/server.key");
        fs::path cert_path = base / cert;
        fs::path key_path  = base / key;
        if (srv.enable_tls(cert_path.string(), key_path.string())) {
            LOGI("[tls] TLS 1.3  AES-256-GCM  X25519MLKEM768  cert=" << cert_path.string());
        } else {
            LOGE("[tls] TLS setup FAILED - falling back to plain HTTP");
        }
    }

    register_routes(srv, customers, jobs, invoices, cfg, db, auth);

    // HTTP redirect/setup server on port+1 when TLS active
    std::unique_ptr<HttpServer> http_redirect;
    if (srv.tls_active()) {
        int redirect_port = port + 1;
        http_redirect = std::make_unique<HttpServer>(host, redirect_port, static_dir.string());
        std::string https_base = "https://" + host + ":" + std::to_string(port);
        http_redirect->add_route("GET", "/", [https_base](const HttpRequest&) {
            HttpResponse r; r.status = 301; r.headers["Location"] = https_base + "/";
            r.body = "<html><body>Redirecting to <a href=\"" + https_base + "\">" + https_base + "</a></body></html>";
            return r;
        });
        http_redirect->add_route("GET", "/setup", [https_base](const HttpRequest&) {
            HttpResponse r; r.content_type = "text/html";
            r.body = "<!DOCTYPE html><html><head><title>Metis TLS Setup</title></head>"
                     "<body style='font-family:sans-serif;max-width:600px;margin:2rem auto;padding:1rem'>"
                     "<h2>Metis Exterminator Plus - TLS Setup</h2>"
                     "<p>Server is running TLS 1.3 / AES-256-GCM / Post-Quantum.</p>"
                     "<ol><li>Click the link: <a href='" + https_base + "'>" + https_base + "</a></li>"
                     "<li>Chrome warning: click <strong>Advanced</strong> then <strong>Proceed to 127.0.0.1</strong></li>"
                     "<li>Sign in with credentials from config.pson auth section</li></ol></body></html>";
            return r;
        });
        std::thread([&http_redirect]{ http_redirect->run(); }).detach();
        LOGI("[server] HTTP setup guide: http://" << host << ":" << redirect_port << "/setup");
    }

    // Session purge background thread
    std::thread purge_thread([&auth, purge_min]{
        while (true) {
            std::this_thread::sleep_for(std::chrono::minutes(purge_min));
            auth.purge_expired_sessions();
        }
    });
    purge_thread.detach();

    std::signal(SIGINT,  on_signal);
    std::signal(SIGTERM, on_signal);

    std::string scheme = srv.tls_active() ? "https" : "http";
    LOGI("[server] Listening on " << scheme << "://" << host << ":" << port);
    if (srv.tls_active()) {
        LOGI("  Open: https://" << host << ":" << port);
        LOGI("  Cert warning: click Advanced -> Proceed to " << host);
        LOGI("  Setup guide: http://" << host << ":" << (port+1) << "/setup");
    } else {
        LOGI("  Open: http://" << host << ":" << port);
    }

    srv.run();

    LOGI("Shutting down cleanly...");
    customers.save();
    jobs.save();
    invoices.save();
    db.close();
    return 0;
}
