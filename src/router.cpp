#include "router.hpp"
#include "logger.hpp"
#include "../infra/kubernetes/k8s_stub.hpp"
#include "version.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <chrono>
#include <sstream>
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

    std::string prefix = cfg.get_or("app", "invoice_prefix", "INV");
    int next_inv_num = cfg.get_int("app", "next_invoice_number", 1000);
    double tax_rate = cfg.get_double("app", "tax_rate", 0.0875);

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
        double tax_rate         = cfg.get_double("tax", "default_rate", 0.0875);
        LOGI("[version] app.name='" << app_name << "' app.version='" << app_version << "' company='" << company << "'");
        r.body = "{"
                 "\"version\":" + jstr(app_version) + ","
                 "\"app\":" + jstr(app_name) + ","
                 "\"author\":\"" METIS_EXTERMINATOR_AUTHOR "\","
                 "\"company\":" + jstr(company) + ","
                 "\"currency\":" + jstr(currency) + ","
                 "\"tax_name\":" + jstr(tax_name) + ","
                 "\"tax_rate\":" + std::to_string(tax_rate) +
                 "}";
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
        LOGI("[login] attempt  user='" << user << "'  body_len=" << req.body.size()
             << "  pass_len=" << pass.size());
        if (user.empty() || pass.empty()) {
            LOGE("[login] rejected: empty fields  body='" << req.body << "'");
            r.status = 400; r.body = "{\"error\":\"username and password required\"}"; return r;
        }
        auto token = auth.login(user, pass);
        if (!token) {
            LOGE("[login] FAILED 401  user='" << user << "'");
            r.status = 401; r.body = "{\"error\":\"invalid credentials\"}"; return r;
        }
        LOGI("[login] SUCCESS  user='" << user << "'");
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

    srv.add_route("POST", "/api/invoices", [&invoices, prefix, &next_inv_num, tax_rate](const HttpRequest& req) mutable {
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
    srv.add_route("POST", "/api/shutdown", [&srv, &customers, &jobs, &invoices, &db](const HttpRequest&) {
        HttpResponse r;
        LOGI("[shutdown] Clean shutdown requested from browser");
        r.body = "{\"status\":\"shutting down\",\"message\":\"Server is stopping. You may close this tab.\"}";
        std::thread([&srv, &customers, &jobs, &invoices, &db](){
            std::this_thread::sleep_for(std::chrono::milliseconds(400));
            LOGI("[shutdown] Flushing data stores...");
            customers.save();
            jobs.save();
            invoices.save();
            db.close();
            LOGI("[shutdown] All data flushed. Server stopped.");
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
}
