#include "router.hpp"
#include "logger.hpp"
#include "../infra/kubernetes/k8s_stub.hpp"
#include "version.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <memory>
#include <thread>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <string>
#include <functional>

static std::string jstr(const std::string& s) {
    std::string r = "\"";
    for (char c : s) {
        if (c == '"') r += "\\\"";
        else if (c == '\\') r += "\\\\";
        else if (c == '\n') r += "\\n";
        else if (c == '\r') r += "\\r";
        else if (c == '\t') r += "\\t";
        else r += c;
    }
    r += '"';
    return r;
}

static std::string customer_json(const Customer& c) {
    std::ostringstream o;
    o << "{"
      << "\"id\":" << c.id << ","
      << "\"name\":" << jstr(c.name) << ","
      << "\"phone\":" << jstr(c.phone) << ","
      << "\"email\":" << jstr(c.email) << ","
      << "\"address\":" << jstr(c.address) << ","
      << "\"city\":" << jstr(c.city) << ","
      << "\"state\":" << jstr(c.state) << ","
      << "\"zip\":" << jstr(c.zip) << ","
      << "\"notes\":" << jstr(c.notes) << ","
      << "\"created_at\":" << jstr(c.created_at) << ","
      << "\"active\":" << (c.active ? "true" : "false")
      << "}";
    return o.str();
}

static std::string job_json(const Job& j) {
    std::ostringstream o;
    o << "{"
      << "\"id\":" << j.id << ","
      << "\"customer_id\":" << j.customer_id << ","
      << "\"service_type\":" << jstr(j.service_type) << ","
      << "\"pest_type\":" << jstr(j.pest_type) << ","
      << "\"status\":" << jstr(j.status) << ","
      << "\"scheduled_date\":" << jstr(j.scheduled_date) << ","
      << "\"scheduled_time\":" << jstr(j.scheduled_time) << ","
      << "\"technician\":" << jstr(j.technician) << ","
      << "\"address\":" << jstr(j.address) << ","
      << "\"notes\":" << jstr(j.notes) << ","
      << "\"price\":" << j.price << ","
      << "\"created_at\":" << jstr(j.created_at) << ","
      << "\"completed_at\":" << jstr(j.completed_at)
      << "}";
    return o.str();
}

static std::string invoice_item_json(const InvoiceItem& ii) {
    std::ostringstream o;
    o << "{"
      << "\"description\":" << jstr(ii.description) << ","
      << "\"unit_price\":" << ii.unit_price << ","
      << "\"quantity\":" << ii.quantity
      << "}";
    return o.str();
}

static std::string invoice_json(const Invoice& inv) {
    std::ostringstream o;
    o << "{"
      << "\"id\":" << inv.id << ","
      << "\"customer_id\":" << inv.customer_id << ","
      << "\"job_id\":" << inv.job_id << ","
      << "\"invoice_number\":" << jstr(inv.invoice_number) << ","
      << "\"status\":" << jstr(inv.status) << ","
      << "\"items\":[";
    for (size_t i = 0; i < inv.items.size(); ++i) {
        if (i) o << ",";
        o << invoice_item_json(inv.items[i]);
    }
    o << "],"
      << "\"subtotal\":" << inv.subtotal << ","
      << "\"tax_rate\":" << inv.tax_rate << ","
      << "\"tax_amount\":" << inv.tax_amount << ","
      << "\"total\":" << inv.total << ","
      << "\"issued_date\":" << jstr(inv.issued_date) << ","
      << "\"due_date\":" << jstr(inv.due_date) << ","
      << "\"paid_date\":" << jstr(inv.paid_date) << ","
      << "\"notes\":" << jstr(inv.notes) << ","
      << "\"created_at\":" << jstr(inv.created_at)
      << "}";
    return o.str();
}

static std::string json_str(const std::string& raw, const std::string& key) {
    std::string search = "\"" + key + "\":\"";
    auto pos = raw.find(search);
    if (pos == std::string::npos) return {};
    pos += search.size();
    std::string val;
    while (pos < raw.size() && raw[pos] != '"') {
        if (raw[pos] == '\\' && pos+1 < raw.size()) { ++pos; }
        val += raw[pos++];
    }
    return val;
}

static double json_num(const std::string& raw, const std::string& key) {
    std::string search = "\"" + key + "\":";
    auto pos = raw.find(search);
    if (pos == std::string::npos) return 0.0;
    pos += search.size();
    while (pos < raw.size() && (raw[pos] == ' ' || raw[pos] == '\t')) ++pos;
    std::string num;
    while (pos < raw.size() && (std::isdigit(raw[pos]) || raw[pos] == '.' || raw[pos] == '-')) num += raw[pos++];
    try { return std::stod(num); } catch (...) { return 0.0; }
}

static bool json_bool(const std::string& raw, const std::string& key) {
    std::string st = "\"" + key + "\":true";
    return raw.find(st) != std::string::npos;
}

void register_routes(HttpServer& srv,
                     CustomerStore& customers,
                     JobStore& jobs,
                     InvoiceStore& invoices,
                     const Pson& cfg,
                     Database& db,
                     AuthManager& auth) {

    std::string prefix      = cfg.get_or("app", "invoice_prefix", "INV");
    int next_inv_num        = cfg.get_int("app", "next_invoice_number", 1000);
    double tax_rate         = cfg.get_double("tax", "default_rate", 0.0875);
    std::string allowed_org = cfg.get_or("security", "allowed_origins", "*");
    bool api_key_enabled    = cfg.get_or("security", "api_key_enabled", "false") == "true";
    std::string api_key_hdr = cfg.get_or("security", "api_key_header", "X-API-Key");

    // CORS header helper — stored on heap so route lambdas can capture by value
    auto cors_fn = std::make_shared<std::function<void(HttpResponse&)>>(
        [allowed_org](HttpResponse& r) {
            r.headers["Access-Control-Allow-Origin"]  = allowed_org;
            r.headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
            r.headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization, X-API-Key";
        });
    auto cors = [cors_fn](HttpResponse& r) { (*cors_fn)(r); };

    // Auth middleware — heap-allocated so all route lambdas capture the shared_ptr by value
    using GetUserFn = std::function<std::optional<User>(const HttpRequest&)>;
    auto get_user_fn = std::make_shared<GetUserFn>(
        [&auth, api_key_enabled, api_key_hdr](const HttpRequest& req) -> std::optional<User> {
            std::string auth_hdr;
            if (req.headers.count("authorization")) auth_hdr = req.headers.at("authorization");
            if (auth_hdr.substr(0,7) == "Bearer ") {
                return auth.validate_session(auth_hdr.substr(7));
            }
            if (api_key_enabled && req.headers.count(api_key_hdr)) {
                return auth.validate_api_key(req.headers.at(api_key_hdr));
            }
            return {};
        });
    // Convenience wrapper — capture shared_ptr by value (safe across route lifetimes)
    auto get_user = [get_user_fn](const HttpRequest& req) { return (*get_user_fn)(req); };

    srv.add_route("OPTIONS", "/api/customers", [](const HttpRequest&) {
        HttpResponse r; r.status = 204; r.body = ""; return r;
    });
    srv.add_route("OPTIONS", "/api/customers/:id", [](const HttpRequest&) {
        HttpResponse r; r.status = 204; r.body = ""; return r;
    });
    srv.add_route("OPTIONS", "/api/jobs", [](const HttpRequest&) {
        HttpResponse r; r.status = 204; r.body = ""; return r;
    });
    srv.add_route("OPTIONS", "/api/jobs/:id", [](const HttpRequest&) {
        HttpResponse r; r.status = 204; r.body = ""; return r;
    });
    srv.add_route("OPTIONS", "/api/invoices", [](const HttpRequest&) {
        HttpResponse r; r.status = 204; r.body = ""; return r;
    });
    srv.add_route("OPTIONS", "/api/invoices/:id", [](const HttpRequest&) {
        HttpResponse r; r.status = 204; r.body = ""; return r;
    });

    srv.add_route("GET", "/api/version", [&cfg](const HttpRequest&) {
        HttpResponse r;
        std::string app_name    = cfg.get_or("app", "name",    METIS_EXTERMINATOR_APP);
        std::string app_version = cfg.get_or("app", "version", METIS_EXTERMINATOR_VERSION);
        std::string company     = cfg.get_or("app", "company", "");
        std::string currency    = cfg.get_or("app", "currency", "USD");
        std::string tax_name    = cfg.get_or("tax", "name",    "Sales Tax");
        std::string theme       = cfg.get_or("client", "theme", "dark");
        double tax_rate         = cfg.get_double("tax", "default_rate", 0.0875);
        LOGI("[version] app.name='" << app_name << "' app.version='" << app_version << "' company='" << company << "'");
        r.body  = "{";
        r.body += "\"version\":"  + jstr(app_version) + ",";
        r.body += "\"app\":"      + jstr(app_name)    + ",";
        r.body += "\"author\":\"" METIS_EXTERMINATOR_AUTHOR "\",";
        r.body += "\"company\":"  + jstr(company)     + ",";
        r.body += "\"currency\":" + jstr(currency)    + ",";
        r.body += "\"tax_name\":" + jstr(tax_name)    + ",";
        r.body += "\"tax_rate\":" + std::to_string(tax_rate) + ",";
        r.body += "\"theme\":"    + jstr(theme);
        r.body += "}";
        return r;
    });

    // Health probes (Kubernetes liveness/readiness)
    srv.add_route("GET", "/healthz", [](const HttpRequest&) {
        auto p = metis::infra::k8s::liveness_probe();
        HttpResponse r;
        r.body = "{\"status\":\"" + p.message + "\"}";
        return r;
    });
    srv.add_route("GET", "/readyz", [&customers, &jobs](const HttpRequest&) {
        auto p = metis::infra::k8s::readiness_probe();
        HttpResponse r;
        r.body = "{\"status\":\"" + p.message + "\"}";
        return r;
    });
    srv.add_route("GET", "/metrics", [&customers, &jobs, &invoices](const HttpRequest&) {
        auto all_c = customers.all();
        auto all_j = jobs.all();
        auto all_i = invoices.all();
        HttpResponse r;
        r.content_type = "text/plain; version=0.0.4";
        r.body = metis::infra::k8s::prometheus_metrics(
            static_cast<int>(all_c.size()),
            static_cast<int>(all_j.size()),
            static_cast<int>(all_i.size()));
        return r;
    });

    // System log endpoint - returns last N lines of the log file
    srv.add_route("GET", "/api/logs", [&cfg](const HttpRequest& req) {
        HttpResponse r;
        std::string log_file = cfg.get_or("logging","file","logs/metis_exterminator.log");
        int lines = 100;
        if (req.qparams.count("lines")) {
            try { lines = std::stoi(req.qparams.at("lines")); } catch(...) {}
        }
        std::ifstream f(log_file);
        if (!f) {
            r.body = "{\"error\":\"log file not found\",\"path\":" + jstr(log_file) + "}";
            r.status = 404; return r;
        }
        std::vector<std::string> all_lines;
        std::string line;
        while (std::getline(f, line)) all_lines.push_back(line);
        int start = std::max(0, (int)all_lines.size() - lines);
        std::string body = "[";
        for (int i = start; i < (int)all_lines.size(); ++i) {
            if (i > start) body += ",";
            body += jstr(all_lines[i]);
        }
        body += "]";
        r.body = body;
        return r;
    });

    // Audit log endpoint - returns auth events from log
    srv.add_route("GET", "/api/audit", [&cfg](const HttpRequest& req) {
        HttpResponse r;
        std::string log_file = cfg.get_or("logging","file","logs/metis_exterminator.log");
        std::ifstream f(log_file);
        if (!f) { r.status = 404; r.body = "{\"error\":\"log not found\"}"; return r; }
        std::string body = "[";
        bool first = true;
        std::string line;
        while (std::getline(f, line)) {
            if (line.find("[login]") != std::string::npos ||
                line.find("[auth]")  != std::string::npos ||
                line.find("reset_admin") != std::string::npos) {
                if (!first) body += ",";
                body += jstr(line);
                first = false;
            }
        }
        body += "]";
        r.body = body;
        return r;
    });

    // Auth endpoints
    srv.add_route("OPTIONS", "/api/auth/login",  [](const HttpRequest&){ HttpResponse r; r.status=204; r.body=""; return r; });
    srv.add_route("OPTIONS", "/api/auth/logout", [](const HttpRequest&){ HttpResponse r; r.status=204; r.body=""; return r; });
    srv.add_route("OPTIONS", "/api/auth/me",     [](const HttpRequest&){ HttpResponse r; r.status=204; r.body=""; return r; });

    srv.add_route("POST", "/api/auth/login", [&auth](const HttpRequest& req) {
        HttpResponse r;
        std::string user = json_str(req.body, "username");
        std::string pass = json_str(req.body, "password");
        std::string ip   = req.headers.count("x-forwarded-for") ?
                           req.headers.at("x-forwarded-for") :
                           (req.headers.count("x-real-ip") ? req.headers.at("x-real-ip") : "local");
        LOGI("[login] attempt  user='" << user << "'  body_len=" << req.body.size()
             << "  pass_len=" << pass.size() << "  ip=" << ip);
        if (user.empty() || pass.empty()) {
            LOGE("[login] rejected: empty fields  body='" << req.body << "'");
            r.status = 400; r.body = "{\"error\":\"username and password required\"}"; return r;
        }
        // Rate limit check
        if (!auth.check_rate_limit(ip)) {
            LOGE("[login] RATE LIMITED ip=" << ip);
            r.status = 429; r.body = "{\"error\":\"too many login attempts, please wait\"}"; return r;
        }
        auto token = auth.login(user, pass, ip);
        if (!token) {
            LOGE("[login] FAILED 401  user='" << user << "'");
            auth.write_audit("login_failed", user, ip, "invalid credentials", false);
            r.status = 401; r.body = "{\"error\":\"invalid credentials\"}"; return r;
        }
        LOGI("[login] SUCCESS  user='" << user << "'");
        auth.write_audit("login_success", user, ip, "", true);
        r.body = "{\"token\":" + jstr(*token) + "}";
        return r;
    });

    srv.add_route("POST", "/api/auth/logout", [&auth](const HttpRequest& req) {
        HttpResponse r;
        std::string token = json_str(req.body, "token");
        if (!token.empty()) auth.logout(token);
        r.status = 204; r.body = ""; return r;
    });

    srv.add_route("GET", "/api/auth/me", [&auth](const HttpRequest& req) {
        HttpResponse r;
        std::string auth_hdr;
        if (req.headers.count("authorization")) auth_hdr = req.headers.at("authorization");
        if (auth_hdr.substr(0,7) == "Bearer ") auth_hdr = auth_hdr.substr(7);
        if (auth_hdr.empty()) { r.status=401; r.body="{\"error\":\"no token\"}"; return r; }
        auto u = auth.validate_session(auth_hdr);
        if (!u) { r.status=401; r.body="{\"error\":\"invalid or expired session\"}"; return r; }
        r.body = "{\"id\":" + std::to_string(u->id) + ",\"username\":" + jstr(u->username) + ",\"role\":" + jstr(u->role) + "}";
        return r;
    });

    srv.add_route("GET", "/api/customers", [&customers](const HttpRequest& req) {
        HttpResponse r;
        auto all = customers.all();
        std::string q = req.qparams.count("q") ? req.qparams.at("q") : "";
        std::string body = "[";
        bool first = true;
        for (const auto& c : all) {
            if (!q.empty()) {
                std::string nm = c.name, ph = c.phone, em = c.email;
                std::transform(nm.begin(), nm.end(), nm.begin(), ::tolower);
                std::transform(q.begin(), q.end(), q.begin(), ::tolower);
                if (nm.find(q) == std::string::npos &&
                    ph.find(q) == std::string::npos &&
                    em.find(q) == std::string::npos) continue;
            }
            if (!first) body += ",";
            body += customer_json(c);
            first = false;
        }
        body += "]";
        r.body = body;
        return r;
    });

    srv.add_route("GET", "/api/customers/:id", [&customers](const HttpRequest& req) {
        HttpResponse r;
        int id = std::stoi(req.params.at("id"));
        auto c = customers.find(id);
        if (!c) { r.status = 404; r.body = "{\"error\":\"not found\"}"; return r; }
        r.body = customer_json(*c);
        return r;
    });

    srv.add_route("POST", "/api/customers", [&customers](const HttpRequest& req) {
        HttpResponse r;
        Customer c;
        c.name    = json_str(req.body, "name");
        c.phone   = json_str(req.body, "phone");
        c.email   = json_str(req.body, "email");
        c.address = json_str(req.body, "address");
        c.city    = json_str(req.body, "city");
        c.state   = json_str(req.body, "state");
        c.zip     = json_str(req.body, "zip");
        c.notes   = json_str(req.body, "notes");
        c.active  = true;
        if (c.name.empty()) { r.status = 400; r.body = "{\"error\":\"name required\"}"; return r; }
        auto added = customers.add(c);
        customers.save();
        r.status = 201;
        r.body = customer_json(added);
        return r;
    });

    srv.add_route("PUT", "/api/customers/:id", [&customers](const HttpRequest& req) {
        HttpResponse r;
        int id = std::stoi(req.params.at("id"));
        auto existing = customers.find(id);
        if (!existing) { r.status = 404; r.body = "{\"error\":\"not found\"}"; return r; }
        Customer c = *existing;
        c.name    = json_str(req.body, "name");
        c.phone   = json_str(req.body, "phone");
        c.email   = json_str(req.body, "email");
        c.address = json_str(req.body, "address");
        c.city    = json_str(req.body, "city");
        c.state   = json_str(req.body, "state");
        c.zip     = json_str(req.body, "zip");
        c.notes   = json_str(req.body, "notes");
        customers.update(c);
        customers.save();
        r.body = customer_json(c);
        return r;
    });

    srv.add_route("DELETE", "/api/customers/:id", [&customers](const HttpRequest& req) {
        HttpResponse r;
        int id = std::stoi(req.params.at("id"));
        if (!customers.remove(id)) { r.status = 404; r.body = "{\"error\":\"not found\"}"; return r; }
        customers.save();
        r.status = 204; r.body = "";
        return r;
    });

    srv.add_route("GET", "/api/jobs", [&jobs](const HttpRequest& req) {
        HttpResponse r;
        std::vector<Job> all;
        if (req.qparams.count("customer_id")) {
            int cid = std::stoi(req.qparams.at("customer_id"));
            all = jobs.for_customer(cid);
        } else {
            all = jobs.all();
        }
        std::string status_f = req.qparams.count("status") ? req.qparams.at("status") : "";
        std::string body = "[";
        bool first = true;
        for (const auto& j : all) {
            if (!status_f.empty() && j.status != status_f) continue;
            if (!first) body += ",";
            body += job_json(j);
            first = false;
        }
        body += "]";
        r.body = body;
        return r;
    });

    srv.add_route("GET", "/api/jobs/:id", [&jobs](const HttpRequest& req) {
        HttpResponse r;
        int id = std::stoi(req.params.at("id"));
        auto j = jobs.find(id);
        if (!j) { r.status = 404; r.body = "{\"error\":\"not found\"}"; return r; }
        r.body = job_json(*j);
        return r;
    });

    srv.add_route("POST", "/api/jobs", [&jobs](const HttpRequest& req) {
        HttpResponse r;
        Job j;
        j.customer_id    = static_cast<int>(json_num(req.body, "customer_id"));
        j.service_type   = json_str(req.body, "service_type");
        j.pest_type      = json_str(req.body, "pest_type");
        j.status         = json_str(req.body, "status");
        j.scheduled_date = json_str(req.body, "scheduled_date");
        j.scheduled_time = json_str(req.body, "scheduled_time");
        j.technician     = json_str(req.body, "technician");
        j.address        = json_str(req.body, "address");
        j.notes          = json_str(req.body, "notes");
        j.price          = json_num(req.body, "price");
        if (j.service_type.empty()) { r.status = 400; r.body = "{\"error\":\"service_type required\"}"; return r; }
        auto added = jobs.add(j);
        jobs.save();
        r.status = 201;
        r.body = job_json(added);
        return r;
    });

    srv.add_route("PUT", "/api/jobs/:id", [&jobs](const HttpRequest& req) {
        HttpResponse r;
        int id = std::stoi(req.params.at("id"));
        auto existing = jobs.find(id);
        if (!existing) { r.status = 404; r.body = "{\"error\":\"not found\"}"; return r; }
        Job j = *existing;
        j.service_type   = json_str(req.body, "service_type");
        j.pest_type      = json_str(req.body, "pest_type");
        j.status         = json_str(req.body, "status");
        j.scheduled_date = json_str(req.body, "scheduled_date");
        j.scheduled_time = json_str(req.body, "scheduled_time");
        j.technician     = json_str(req.body, "technician");
        j.address        = json_str(req.body, "address");
        j.notes          = json_str(req.body, "notes");
        j.price          = json_num(req.body, "price");
        if (!json_str(req.body, "completed_at").empty()) j.completed_at = json_str(req.body, "completed_at");
        jobs.update(j);
        jobs.save();
        r.body = job_json(j);
        return r;
    });

    srv.add_route("DELETE", "/api/jobs/:id", [&jobs](const HttpRequest& req) {
        HttpResponse r;
        int id = std::stoi(req.params.at("id"));
        if (!jobs.remove(id)) { r.status = 404; r.body = "{\"error\":\"not found\"}"; return r; }
        jobs.save();
        r.status = 204; r.body = "";
        return r;
    });

    srv.add_route("GET", "/api/invoices", [&invoices](const HttpRequest& req) {
        HttpResponse r;
        std::vector<Invoice> all;
        if (req.qparams.count("customer_id")) {
            int cid = std::stoi(req.qparams.at("customer_id"));
            all = invoices.for_customer(cid);
        } else {
            all = invoices.all();
        }
        std::string body = "[";
        bool first = true;
        for (const auto& inv : all) {
            if (!first) body += ",";
            body += invoice_json(inv);
            first = false;
        }
        body += "]";
        r.body = body;
        return r;
    });

    srv.add_route("GET", "/api/invoices/:id", [&invoices](const HttpRequest& req) {
        HttpResponse r;
        int id = std::stoi(req.params.at("id"));
        auto inv = invoices.find(id);
        if (!inv) { r.status = 404; r.body = "{\"error\":\"not found\"}"; return r; }
        r.body = invoice_json(*inv);
        return r;
    });

    srv.add_route("POST", "/api/invoices", [&invoices, prefix, next_inv_num, tax_rate](const HttpRequest& req) mutable {
        HttpResponse r;
        Invoice inv;
        inv.customer_id  = static_cast<int>(json_num(req.body, "customer_id"));
        inv.job_id       = static_cast<int>(json_num(req.body, "job_id"));
        inv.issued_date  = json_str(req.body, "issued_date");
        inv.due_date     = json_str(req.body, "due_date");
        inv.notes        = json_str(req.body, "notes");
        inv.tax_rate     = tax_rate;
        auto pos = req.body.find("\"items\":[");
        if (pos != std::string::npos) {
            pos += 9;
            int depth = 1;
            std::string items_raw;
            while (pos < req.body.size() && depth > 0) {
                if (req.body[pos] == '[') depth++;
                else if (req.body[pos] == ']') { depth--; if (depth == 0) break; }
                items_raw += req.body[pos++];
            }
            size_t ip = 0;
            while (ip < items_raw.size()) {
                auto ob = items_raw.find('{', ip);
                if (ob == std::string::npos) break;
                auto cb = items_raw.find('}', ob);
                if (cb == std::string::npos) break;
                std::string item_s = items_raw.substr(ob, cb - ob + 1);
                InvoiceItem ii;
                ii.description = json_str(item_s, "description");
                ii.unit_price  = json_num(item_s, "unit_price");
                ii.quantity    = static_cast<int>(json_num(item_s, "quantity"));
                if (ii.quantity <= 0) ii.quantity = 1;
                if (!ii.description.empty()) inv.items.push_back(ii);
                ip = cb + 1;
            }
        }
        for (const auto& ii : inv.items) inv.subtotal += ii.unit_price * ii.quantity;
        inv.tax_amount = inv.subtotal * inv.tax_rate;
        inv.total = inv.subtotal + inv.tax_amount;
        auto added = invoices.add(inv, prefix, next_inv_num);
        invoices.save();
        r.status = 201;
        r.body = invoice_json(added);
        return r;
    });

    srv.add_route("PUT", "/api/invoices/:id", [&invoices](const HttpRequest& req) {
        HttpResponse r;
        int id = std::stoi(req.params.at("id"));
        auto existing = invoices.find(id);
        if (!existing) { r.status = 404; r.body = "{\"error\":\"not found\"}"; return r; }
        Invoice inv = *existing;
        std::string new_status = json_str(req.body, "status");
        if (!new_status.empty()) inv.status = new_status;
        std::string paid = json_str(req.body, "paid_date");
        if (!paid.empty()) inv.paid_date = paid;
        std::string due = json_str(req.body, "due_date");
        if (!due.empty()) inv.due_date = due;
        std::string notes = json_str(req.body, "notes");
        if (!notes.empty()) inv.notes = notes;
        invoices.update(inv);
        invoices.save();
        r.body = invoice_json(inv);
        return r;
    });

    srv.add_route("DELETE", "/api/invoices/:id", [&invoices](const HttpRequest& req) {
        HttpResponse r;
        int id = std::stoi(req.params.at("id"));
        if (!invoices.remove(id)) { r.status = 404; r.body = "{\"error\":\"not found\"}"; return r; }
        invoices.save();
        r.status = 204; r.body = "";
        return r;
    });

    // Shutdown — flushes data and stops the server cleanly
    srv.add_route("OPTIONS", "/api/shutdown", [](const HttpRequest&){ HttpResponse r; r.status=204; r.body=""; return r; });
    srv.add_route("POST", "/api/shutdown", [&srv, &db](const HttpRequest&) {
        HttpResponse r;
        LOGI("[shutdown] Clean shutdown requested from browser");
        r.body = "{\"status\":\"shutting down\",\"message\":\"Server is stopping. You may close this tab.\"}";
        std::thread([&srv, &db](){
            std::this_thread::sleep_for(std::chrono::milliseconds(400));
            LOGI("[shutdown] Closing SQLite database...");
            db.close();
            LOGI("[shutdown] Done. Server stopped.");
            srv.stop();
        }).detach();
        return r;
    });

    srv.add_route("GET", "/api/stats", [&customers, &jobs, &invoices](const HttpRequest&) {
        HttpResponse r;
        auto all_jobs = jobs.all();
        auto all_inv = invoices.all();
        int scheduled = 0, completed = 0;
        for (const auto& j : all_jobs) {
            if (j.status == "Scheduled") scheduled++;
            else if (j.status == "Completed") completed++;
        }
        double revenue = 0.0, outstanding = 0.0;
        for (const auto& inv : all_inv) {
            if (inv.status == "Paid") revenue += inv.total;
            else if (inv.status == "Pending") outstanding += inv.total;
        }
        int cust_count = static_cast<int>(customers.all().size());
        std::ostringstream o;
        o << "{"
          << "\"customers\":" << cust_count << ","
          << "\"jobs_scheduled\":" << scheduled << ","
          << "\"jobs_completed\":" << completed << ","
          << "\"revenue\":" << revenue << ","
          << "\"outstanding\":" << outstanding
          << "}";
        r.body = o.str();
        return r;
    });

    // ═══════════════════════════════════════════════════════════
    // USER MANAGEMENT (admin only)
    // ═══════════════════════════════════════════════════════════

    srv.add_route("OPTIONS", "/api/users",    [](const HttpRequest&){ HttpResponse r; r.status=204; r.body=""; return r; });
    srv.add_route("OPTIONS", "/api/users/:id",[](const HttpRequest&){ HttpResponse r; r.status=204; r.body=""; return r; });

    // GET /api/users — list all users (admin only)
    srv.add_route("GET", "/api/users", [&auth, get_user](const HttpRequest& req) {
        HttpResponse r;
        auto u = get_user(req);
        if (!u || u->role != "admin") { r.status=403; r.body="{\"error\":\"forbidden\"}"; return r; }
        auto users = auth.list_users();
        std::string body = "[";
        bool first = true;
        for (const auto& usr : users) {
            if (!first) body += ",";
            body += "{\"id\":" + std::to_string(usr.id) +
                    ",\"username\":" + jstr(usr.username) +
                    ",\"role\":" + jstr(usr.role) +
                    ",\"active\":" + (usr.active ? "true" : "false") +
                    ",\"has_api_key\":" + (usr.api_key.empty() ? "false" : "true") +
                    "}";
            first = false;
        }
        body += "]";
        r.body = body;
        return r;
    });

    // POST /api/users — create user (admin only)
    srv.add_route("POST", "/api/users", [&auth, get_user](const HttpRequest& req) {
        HttpResponse r;
        auto u = get_user(req);
        if (!u || u->role != "admin") { r.status=403; r.body="{\"error\":\"forbidden\"}"; return r; }
        std::string username = json_str(req.body, "username");
        std::string password = json_str(req.body, "password");
        std::string role     = json_str(req.body, "role");
        if (username.empty() || password.empty()) {
            r.status=400; r.body="{\"error\":\"username and password required\"}"; return r;
        }
        if (role.empty()) role = "technician";
        if (role != "admin" && role != "technician") {
            r.status=400; r.body="{\"error\":\"role must be admin or technician\"}"; return r;
        }
        if (password.size() < 8) { r.status=400; r.body="{\"error\":\"password must be at least 8 characters\"}"; return r; }
        if (auth.create_user(username, password, role)) {
            LOGI("[users] Created user: " << username << " role=" << role);
            r.status=201; r.body="{\"status\":\"created\",\"username\":" + jstr(username) + ",\"role\":" + jstr(role) + "}";
        } else {
            r.status=409; r.body="{\"error\":\"username already exists\"}";
        }
        return r;
    });

    // DELETE /api/users/:id — deactivate user (admin only)
    srv.add_route("DELETE", "/api/users/:id", [&auth, get_user](const HttpRequest& req) {
        HttpResponse r;
        auto u = get_user(req);
        if (!u || u->role != "admin") { r.status=403; r.body="{\"error\":\"forbidden\"}"; return r; }
        int target_id = std::stoi(req.params.at("id"));
        if (target_id == u->id) { r.status=400; r.body="{\"error\":\"cannot deactivate yourself\"}"; return r; }
        auth.deactivate_user(target_id);
        r.status=204; r.body="";
        return r;
    });

    // ═══════════════════════════════════════════════════════════
    // CHANGE PASSWORD
    // ═══════════════════════════════════════════════════════════
    srv.add_route("OPTIONS", "/api/auth/password", [](const HttpRequest&){ HttpResponse r; r.status=204; r.body=""; return r; });
    srv.add_route("PUT", "/api/auth/password", [&auth, get_user](const HttpRequest& req) {
        HttpResponse r;
        auto u = get_user(req);
        if (!u) { r.status=401; r.body="{\"error\":\"unauthorized\"}"; return r; }
        std::string old_pw  = json_str(req.body, "old_password");
        std::string new_pw  = json_str(req.body, "new_password");
        if (old_pw.empty() || new_pw.empty()) {
            r.status=400; r.body="{\"error\":\"old_password and new_password required\"}"; return r;
        }
        if (new_pw.size() < 8) {
            r.status=400; r.body="{\"error\":\"new_password must be at least 8 characters\"}"; return r;
        }
        if (auth.change_password(u->id, old_pw, new_pw)) {
            LOGI("[auth] Password changed: " << u->username);
            r.body="{\"status\":\"password changed\"}";
        } else {
            r.status=401; r.body="{\"error\":\"current password incorrect\"}";
        }
        return r;
    });

    // PUT /api/users/:id/password — admin force-set password
    srv.add_route("PUT", "/api/users/:id/password", [&auth, get_user](const HttpRequest& req) {
        HttpResponse r;
        auto u = get_user(req);
        if (!u || u->role != "admin") { r.status=403; r.body="{\"error\":\"forbidden\"}"; return r; }
        int target_id = std::stoi(req.params.at("id"));
        std::string new_pw = json_str(req.body, "new_password");
        if (new_pw.size() < 8) { r.status=400; r.body="{\"error\":\"password must be at least 8 characters\"}"; return r; }
        auth.set_password(target_id, new_pw);
        r.body="{\"status\":\"password updated\"}";
        return r;
    });

    // ═══════════════════════════════════════════════════════════
    // API KEY GENERATION
    // ═══════════════════════════════════════════════════════════
    srv.add_route("POST", "/api/auth/apikey", [&auth, get_user](const HttpRequest& req) {
        HttpResponse r;
        auto u = get_user(req);
        if (!u) { r.status=401; r.body="{\"error\":\"unauthorized\"}"; return r; }
        std::string key = auth.generate_api_key(u->id);
        r.body="{\"api_key\":" + jstr(key) + "}";
        LOGI("[auth] API key generated for: " << u->username);
        return r;
    });

    // ═══════════════════════════════════════════════════════════
    // FULL-TEXT SEARCH
    // ═══════════════════════════════════════════════════════════
    srv.add_route("GET", "/api/search", [&db, get_user](const HttpRequest& req) {
        HttpResponse r;
        auto u = get_user(req);
        if (!u) { r.status=401; r.body="{\"error\":\"unauthorized\"}"; return r; }
        std::string q = req.qparams.count("q") ? req.qparams.at("q") : "";
        if (q.empty()) { r.body="{\"customers\":[],\"jobs\":[]}"; return r; }
        std::string q_fts = q + "*";
        std::string cust_body = "[";
        bool first = true;
        try {
            Database::Stmt st(db.handle(),
                "SELECT c.id,c.name,c.phone,c.email,c.city,c.state FROM customers c "
                "JOIN customers_fts f ON c.id=f.rowid WHERE customers_fts MATCH ? AND c.deleted=0 LIMIT 20");
            st.bind_text(1, q_fts);
            while (st.step()) {
                if (!first) cust_body += ",";
                cust_body += "{\"id\":" + std::to_string(st.col_int(0)) +
                             ",\"name\":" + jstr(st.col_text(1)) +
                             ",\"phone\":" + jstr(st.col_text(2)) +
                             ",\"email\":" + jstr(st.col_text(3)) +
                             ",\"city\":" + jstr(st.col_text(4)) +
                             ",\"state\":" + jstr(st.col_text(5)) + "}";
                first = false;
            }
        } catch (...) {}
        cust_body += "]";
        std::string jobs_body = "["; first = true;
        try {
            Database::Stmt st(db.handle(),
                "SELECT j.id,j.customer_id,j.service_type,j.pest_type,j.status,j.scheduled_date FROM jobs j "
                "JOIN jobs_fts f ON j.id=f.rowid WHERE jobs_fts MATCH ? AND j.deleted=0 LIMIT 20");
            st.bind_text(1, q_fts);
            while (st.step()) {
                if (!first) jobs_body += ",";
                jobs_body += "{\"id\":" + std::to_string(st.col_int(0)) +
                             ",\"customer_id\":" + std::to_string(st.col_int(1)) +
                             ",\"service_type\":" + jstr(st.col_text(2)) +
                             ",\"pest_type\":" + jstr(st.col_text(3)) +
                             ",\"status\":" + jstr(st.col_text(4)) +
                             ",\"scheduled_date\":" + jstr(st.col_text(5)) + "}";
                first = false;
            }
        } catch (...) {}
        jobs_body += "]";
        r.body = "{\"customers\":" + cust_body + ",\"jobs\":" + jobs_body + "}";
        return r;
    });

    // ═══════════════════════════════════════════════════════════
    // CSV EXPORT
    // ═══════════════════════════════════════════════════════════
    srv.add_route("GET", "/api/export/invoices.csv", [&invoices, &customers, get_user, &auth](const HttpRequest& req) {
        HttpResponse r;
        // Accept token via query param for window.open downloads
        auto u = get_user(req);
        if (!u && req.qparams.count("token")) u = auth.validate_session(req.qparams.at("token"));
        if (!u) { r.status=401; r.body="unauthorized"; return r; }
        auto all_inv = invoices.all();
        auto all_cust = customers.all();
        auto cust_name = [&](int id) -> std::string {
            for (auto& c : all_cust) if (c.id == id) return c.name;
            return std::to_string(id);
        };
        std::ostringstream csv;
        csv << "Invoice Number,Customer,Issued Date,Due Date,Status,Subtotal,Tax,Total,Paid Date\n";
        for (const auto& inv : all_inv) {
            csv << inv.invoice_number << ","
                << "\"" << cust_name(inv.customer_id) << "\","
                << inv.issued_date << "," << inv.due_date << ","
                << inv.status << ","
                << std::fixed << std::setprecision(2) << inv.subtotal << ","
                << inv.tax_amount << "," << inv.total << ","
                << inv.paid_date << "\n";
        }
        r.content_type = "text/csv";
        r.headers["Content-Disposition"] = "attachment; filename=\"invoices.csv\"";
        r.body = csv.str();
        return r;
    });

    srv.add_route("GET", "/api/export/customers.csv", [&customers, get_user, &auth](const HttpRequest& req) {
        HttpResponse r;
        auto u = get_user(req);
        if (!u && req.qparams.count("token")) u = auth.validate_session(req.qparams.at("token"));
        if (!u) { r.status=401; r.body="unauthorized"; return r; }
        auto all = customers.all();
        std::ostringstream csv;
        csv << "ID,Name,Phone,Email,Address,City,State,ZIP,Notes\n";
        for (const auto& c : all) {
            csv << c.id << ","
                << "\"" << c.name << "\","
                << c.phone << "," << c.email << ","
                << "\"" << c.address << "\","
                << c.city << "," << c.state << "," << c.zip << ","
                << "\"" << c.notes << "\"\n";
        }
        r.content_type = "text/csv";
        r.headers["Content-Disposition"] = "attachment; filename=\"customers.csv\"";
        r.body = csv.str();
        return r;
    });

    // ═══════════════════════════════════════════════════════════
    // OVERDUE INVOICE DETECTION
    // ═══════════════════════════════════════════════════════════
    srv.add_route("POST", "/api/invoices/check-overdue", [&invoices, get_user](const HttpRequest& req) {
        HttpResponse r;
        auto u = get_user(req);
        if (!u) { r.status=401; r.body="{\"error\":\"unauthorized\"}"; return r; }
        auto all = invoices.all();
        std::string today = std::string(10, ' ');
        auto t = std::chrono::system_clock::now();
        std::time_t tt = std::chrono::system_clock::to_time_t(t);
        std::strftime(today.data(), 11, "%Y-%m-%d", std::gmtime(&tt));
        int marked = 0;
        for (auto& inv : all) {
            if (inv.status == "Pending" && !inv.due_date.empty() && inv.due_date < today) {
                inv.status = "Overdue";
                invoices.update(inv);
                marked++;
            }
        }
        if (marked > 0) { invoices.save(); LOGI("[invoices] Marked " << marked << " invoices overdue"); }
        r.body = "{\"marked_overdue\":" + std::to_string(marked) + "}";
        return r;
    });

    // ═══════════════════════════════════════════════════════════
    // AUDIT LOG from SQLite
    // ═══════════════════════════════════════════════════════════
    srv.add_route("GET", "/api/audit/events", [&db, get_user](const HttpRequest& req) {
        HttpResponse r;
        auto u = get_user(req);
        if (!u || u->role != "admin") { r.status=403; r.body="{\"error\":\"forbidden\"}"; return r; }
        int limit = 100;
        if (req.qparams.count("limit")) {
            try { limit = std::stoi(req.qparams.at("limit")); } catch (...) {}
        }
        std::string body = "[";
        bool first = true;
        try {
            Database::Stmt st(db.handle(),
                "SELECT id,event_type,username,ip_address,details,success,created_at "
                "FROM audit_log ORDER BY created_at DESC LIMIT ?");
            st.bind_int(1, limit);
            while (st.step()) {
                if (!first) body += ",";
                body += "{\"id\":" + std::to_string(st.col_int(0)) +
                        ",\"event\":" + jstr(st.col_text(1)) +
                        ",\"username\":" + jstr(st.col_text(2)) +
                        ",\"ip\":" + jstr(st.col_text(3)) +
                        ",\"details\":" + jstr(st.col_text(4)) +
                        ",\"success\":" + (st.col_int(5) ? "true" : "false") +
                        ",\"created_at\":" + jstr(st.col_text(6)) + "}";
                first = false;
            }
        } catch (...) {}
        body += "]";
        r.body = body;
        return r;
    });

    // ═══════════════════════════════════════════════════════════
    // LOG ROTATION (manual trigger)
    // ═══════════════════════════════════════════════════════════
    srv.add_route("POST", "/api/admin/rotate-log", [&cfg, get_user](const HttpRequest& req) {
        HttpResponse r;
        auto u = get_user(req);
        if (!u || u->role != "admin") { r.status=403; r.body="{\"error\":\"forbidden\"}"; return r; }
        std::string log_file = cfg.get_or("logging","file","logs/metis_exterminator.log");
        int max_files = cfg.get_int("logging","max_files", 5);
        // Rotate: rename .log -> .log.1, .log.1 -> .log.2, etc.
        for (int i = max_files-1; i >= 1; --i) {
            std::string src  = log_file + "." + std::to_string(i);
            std::string dest = log_file + "." + std::to_string(i+1);
            std::rename(src.c_str(), dest.c_str());
        }
        std::rename(log_file.c_str(), (log_file + ".1").c_str());
        Logger::instance().open(log_file);
        LOGI("[admin] Log rotated by " << u->username);
        r.body="{\"status\":\"log rotated\"}";
        return r;
    });

    // ═══════════════════════════════════════════════════════════
    // CHEMICAL TRACKING
    // ═══════════════════════════════════════════════════════════
    srv.add_route("OPTIONS", "/api/jobs/:id/chemicals",      [](const HttpRequest&){ HttpResponse r; r.status=204; r.body=""; return r; });
    srv.add_route("OPTIONS", "/api/jobs/:id/chemicals/:cid", [](const HttpRequest&){ HttpResponse r; r.status=204; r.body=""; return r; });

    srv.add_route("GET", "/api/jobs/:id/chemicals", [&db, get_user](const HttpRequest& req) {
        HttpResponse r;
        auto u = get_user(req);
        if (!u) { r.status=401; r.body="{\"error\":\"unauthorized\"}"; return r; }
        int job_id = std::stoi(req.params.at("id"));
        std::string body = "["; bool first = true;
        try {
            Database::Stmt st(db.handle(),
                "SELECT id,chemical_name,epa_reg,quantity_oz,unit,applied_at FROM job_chemicals WHERE job_id=? ORDER BY applied_at");
            st.bind_int(1, job_id);
            while (st.step()) {
                if (!first) body += ",";
                body += "{\"id\":" + std::to_string(st.col_int(0)) +
                        ",\"chemical_name\":" + jstr(st.col_text(1)) +
                        ",\"epa_reg\":" + jstr(st.col_text(2)) +
                        ",\"quantity_oz\":" + std::to_string(st.col_double(3)) +
                        ",\"unit\":" + jstr(st.col_text(4)) +
                        ",\"applied_at\":" + jstr(st.col_text(5)) + "}";
                first = false;
            }
        } catch (...) {}
        body += "]"; r.body = body; return r;
    });

    srv.add_route("POST", "/api/jobs/:id/chemicals", [&db, get_user](const HttpRequest& req) {
        HttpResponse r;
        auto u = get_user(req);
        if (!u) { r.status=401; r.body="{\"error\":\"unauthorized\"}"; return r; }
        int job_id = std::stoi(req.params.at("id"));
        std::string name = json_str(req.body, "chemical_name");
        std::string epa  = json_str(req.body, "epa_reg");
        std::string unit = json_str(req.body, "unit");
        double qty       = json_num(req.body, "quantity_oz");
        if (name.empty()) { r.status=400; r.body="{\"error\":\"chemical_name required\"}"; return r; }
        if (unit.empty()) unit = "oz";
        try {
            Database::Stmt st(db.handle(),
                "INSERT INTO job_chemicals(job_id,chemical_name,epa_reg,quantity_oz,unit) VALUES(?,?,?,?,?) RETURNING id");
            st.bind_int(1,job_id); st.bind_text(2,name); st.bind_text(3,epa);
            st.bind_double(4,qty); st.bind_text(5,unit);
            int new_id = 0; if (st.step()) new_id = st.col_int(0);
            r.status=201; r.body="{\"id\":" + std::to_string(new_id) + ",\"status\":\"added\"}";
        } catch (const std::exception& e) { r.status=500; r.body="{\"error\":" + jstr(e.what()) + "}"; }
        return r;
    });

    srv.add_route("DELETE", "/api/jobs/:id/chemicals/:cid", [&db, get_user](const HttpRequest& req) {
        HttpResponse r;
        auto u = get_user(req);
        if (!u) { r.status=401; r.body="{\"error\":\"unauthorized\"}"; return r; }
        int cid = std::stoi(req.params.at("cid"));
        try { Database::Stmt st(db.handle(), "DELETE FROM job_chemicals WHERE id=?"); st.bind_int(1,cid); st.step(); } catch (...) {}
        r.status=204; r.body=""; return r;
    });

    // ═══════════════════════════════════════════════════════════
    // TECHNICIAN TIME TRACKING
    // ═══════════════════════════════════════════════════════════
    srv.add_route("OPTIONS", "/api/jobs/:id/time", [](const HttpRequest&){ HttpResponse r; r.status=204; r.body=""; return r; });
    srv.add_route("PUT", "/api/jobs/:id/time", [&jobs, get_user](const HttpRequest& req) {
        HttpResponse r;
        auto u = get_user(req);
        if (!u) { r.status=401; r.body="{\"error\":\"unauthorized\"}"; return r; }
        int job_id = std::stoi(req.params.at("id"));
        auto jopt = jobs.find(job_id);
        if (!jopt) { r.status=404; r.body="{\"error\":\"job not found\"}"; return r; }
        Job j = *jopt;
        std::string ts = json_str(req.body, "time_start");
        std::string te = json_str(req.body, "time_end");
        if (!ts.empty()) j.time_start = ts;
        if (!te.empty()) { j.time_end = te; if (j.status == "Scheduled") j.status = "In Progress"; }
        jobs.update(j);
        r.body = "{\"status\":\"updated\",\"time_start\":" + jstr(j.time_start) + ",\"time_end\":" + jstr(j.time_end) + "}";
        return r;
    });

    // ═══════════════════════════════════════════════════════════
    // RECURRING JOB SPAWNING
    // ═══════════════════════════════════════════════════════════
    srv.add_route("OPTIONS", "/api/jobs/:id/spawn-recurring", [](const HttpRequest&){ HttpResponse r; r.status=204; r.body=""; return r; });
    srv.add_route("POST", "/api/jobs/:id/spawn-recurring", [&jobs, get_user](const HttpRequest& req) {
        HttpResponse r;
        auto u = get_user(req);
        if (!u) { r.status=401; r.body="{\"error\":\"unauthorized\"}"; return r; }
        int job_id = std::stoi(req.params.at("id"));
        auto jopt = jobs.find(job_id);
        if (!jopt) { r.status=404; r.body="{\"error\":\"job not found\"}"; return r; }
        Job src = *jopt;
        if (!src.recurring || src.recur_interval_days <= 0) {
            r.status=400; r.body="{\"error\":\"job is not recurring\"}"; return r;
        }
        std::tm tm = {}; std::istringstream ss(src.scheduled_date);
        ss >> std::get_time(&tm, "%Y-%m-%d");
        std::time_t t = std::mktime(&tm); t += src.recur_interval_days * 86400LL;
        std::tm* nt = std::gmtime(&t); char buf[11]; std::strftime(buf, sizeof(buf), "%Y-%m-%d", nt);
        Job next = src; next.id=0; next.scheduled_date=buf;
        next.status="Scheduled"; next.completed_at=""; next.time_start=""; next.time_end="";
        next = jobs.add(next);
        LOGI("[jobs] Spawned recurring #" << next.id << " from #" << job_id << " date=" << buf);
        r.status=201; r.body="{\"id\":" + std::to_string(next.id) + ",\"scheduled_date\":" + jstr(next.scheduled_date) + "}";
        return r;
    });

    // ═══════════════════════════════════════════════════════════
    // EMAIL NOTIFICATION (logs intent; SMTP library integration ready)
    // ═══════════════════════════════════════════════════════════
    srv.add_route("OPTIONS", "/api/invoices/:id/send-email", [](const HttpRequest&){ HttpResponse r; r.status=204; r.body=""; return r; });
    srv.add_route("POST", "/api/invoices/:id/send-email", [&cfg, get_user, &invoices, &customers](const HttpRequest& req) {
        HttpResponse r;
        auto u = get_user(req);
        if (!u) { r.status=401; r.body="{\"error\":\"unauthorized\"}"; return r; }
        bool email_enabled = cfg.get_or("email","enabled","false") == "true";
        int inv_id = std::stoi(req.params.at("id"));
        auto iopt = invoices.find(inv_id);
        if (!iopt) { r.status=404; r.body="{\"error\":\"invoice not found\"}"; return r; }
        auto copt = customers.find(iopt->customer_id);
        std::string to_email = copt ? copt->email : "";
        LOGI("[email] Invoice " << iopt->invoice_number << " to=" << to_email << " enabled=" << (email_enabled?"true":"false"));
        if (to_email.empty()) { r.status=400; r.body="{\"error\":\"customer has no email address\"}"; return r; }
        r.body = "{\"status\":\"queued\","
                 "\"to\":" + jstr(to_email) + ","
                 "\"invoice\":" + jstr(iopt->invoice_number) + ","
                 "\"note\":" + jstr(email_enabled ? "Email queued for delivery" : "Email disabled in config.pson (email.enabled=false)") + "}";
        return r;
    });

    // ═══════════════════════════════════════════════════════════
    // ROUTE OPTIMIZATION (address-sorted; hooks for full geocoding)
    // ═══════════════════════════════════════════════════════════
    srv.add_route("GET", "/api/route/optimize", [&jobs, get_user](const HttpRequest& req) {
        HttpResponse r;
        auto u = get_user(req);
        if (!u) { r.status=401; r.body="{\"error\":\"unauthorized\"}"; return r; }
        std::string date = req.qparams.count("date") ? req.qparams.at("date") : "";
        std::string tech = req.qparams.count("technician") ? req.qparams.at("technician") : "";
        auto all = jobs.all();
        std::vector<Job> route;
        for (const auto& j : all) {
            if (j.deleted) continue;
            if (!date.empty() && j.scheduled_date != date) continue;
            if (!tech.empty() && j.technician != tech) continue;
            if (j.status == "Scheduled" || j.status == "In Progress") route.push_back(j);
        }
        std::sort(route.begin(), route.end(), [](const Job& a, const Job& b){ return a.address < b.address; });
        std::string body = "["; bool first = true;
        for (const auto& j : route) {
            if (!first) body += ",";
            body += "{\"id\":" + std::to_string(j.id) +
                    ",\"customer_id\":" + std::to_string(j.customer_id) +
                    ",\"service_type\":" + jstr(j.service_type) +
                    ",\"address\":" + jstr(j.address) +
                    ",\"technician\":" + jstr(j.technician) +
                    ",\"scheduled_time\":" + jstr(j.scheduled_time) + "}";
            first = false;
        }
        body += "]"; r.body = body;
        LOGI("[route] " << route.size() << " jobs date=" << date << " tech=" << tech);
        return r;
    });
}
