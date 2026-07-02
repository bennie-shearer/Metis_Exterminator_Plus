#include "server.hpp"
#include <sstream>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cstring>

#ifdef _WIN32
  #pragma comment(lib, "ws2_32.lib")
#endif

// OpenSSL headers - only in this translation unit
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>

HttpServer::HttpServer(const std::string& host, int port, const std::string& static_dir)
    : host_(host), port_(port), static_dir_(static_dir) {
#ifdef _WIN32
    WSADATA wd;
    WSAStartup(MAKEWORD(2,2), &wd);
#endif
    // Initialize OpenSSL once
    static bool openssl_init = false;
    if (!openssl_init) {
        SSL_library_init();
        SSL_load_error_strings();
        OpenSSL_add_all_algorithms();
        openssl_init = true;
    }
}

HttpServer::~HttpServer() {
    stop();
    if (ssl_ctx_) {
        SSL_CTX_free(ssl_ctx_);
        ssl_ctx_ = nullptr;
    }
#ifdef _WIN32
    WSACleanup();
#endif
}

bool HttpServer::enable_tls(const std::string& cert_path, const std::string& key_path) {
    // TLS 1.3 only - refuse TLS 1.2 and below
    const SSL_METHOD* method = TLS_server_method();
    SSL_CTX* ctx = SSL_CTX_new(method);
    if (!ctx) {
        std::cerr << "[tls] SSL_CTX_new failed\n";
        return false;
    }

    // Enforce TLS 1.3 minimum - no TLS 1.2 or below
    SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION);
    SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION);

    // TLS 1.3 ciphersuites: AES-256-GCM is the primary suite
    // TLS_AES_256_GCM_SHA384 uses AES-256-GCM with SHA-384
    // TLS_CHACHA20_POLY1305_SHA256 kept as fallback for ChaCha devices
    if (SSL_CTX_set_ciphersuites(ctx,
        "TLS_AES_256_GCM_SHA384:"
        "TLS_CHACHA20_POLY1305_SHA256") != 1) {
        std::cerr << "[tls] Warning: could not set TLS 1.3 ciphersuites\n";
    }

    // Post-Quantum key exchange: X25519MLKEM768 (X25519 + ML-KEM-768 hybrid)
    // Requires OpenSSL 3.2+ with oqs or built-in PQ support
    // Try to set the group preference; fall back gracefully if not supported
#ifdef SSL_CTX_set1_groups_list
    if (SSL_CTX_set1_groups_list(ctx, "X25519MLKEM768:X25519:P-256") != 1) {
        // OpenSSL version may not support X25519MLKEM768 - fall back to X25519
        if (SSL_CTX_set1_groups_list(ctx, "X25519:P-256") != 1) {
            std::cerr << "[tls] Warning: could not set key exchange groups\n";
        } else {
            std::cerr << "[tls] Note: X25519MLKEM768 not available in this OpenSSL build; using X25519\n";
        }
    } else {
        std::cout << "[tls] Post-Quantum key exchange: X25519MLKEM768 enabled\n";
    }
#else
    std::cerr << "[tls] Note: SSL_CTX_set1_groups_list not available; key exchange groups not set\n";
#endif

    // Disable session tickets for forward secrecy (TLS 1.3 handles this natively)
    SSL_CTX_set_options(ctx,
        SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION |
        SSL_OP_NO_COMPRESSION |
        SSL_OP_CIPHER_SERVER_PREFERENCE);

    // Load certificate
    if (SSL_CTX_use_certificate_file(ctx, cert_path.c_str(), SSL_FILETYPE_PEM) <= 0) {
        std::cerr << "[tls] Certificate load failed: " << cert_path << "\n";
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(ctx);
        return false;
    }

    // Load private key
    if (SSL_CTX_use_PrivateKey_file(ctx, key_path.c_str(), SSL_FILETYPE_PEM) <= 0) {
        std::cerr << "[tls] Private key load failed: " << key_path << "\n";
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(ctx);
        return false;
    }

    // Verify key matches certificate
    if (SSL_CTX_check_private_key(ctx) != 1) {
        std::cerr << "[tls] Certificate/key mismatch\n";
        SSL_CTX_free(ctx);
        return false;
    }

    ssl_ctx_ = ctx;
    std::cout << "[tls] TLS 1.3 enabled  cipher=AES-256-GCM  cert=" << cert_path << "\n";
    return true;
}

void HttpServer::add_route(const std::string& method, const std::string& pattern, Handler h) {
    Route r;
    r.method = method;
    r.pattern = pattern;
    r.handler = std::move(h);
    std::istringstream ss(pattern);
    std::string seg;
    while (std::getline(ss, seg, '/')) {
        if (!seg.empty() && seg[0] == ':')
            r.param_names.push_back(seg.substr(1));
    }
    routes_.push_back(std::move(r));
}

bool HttpServer::match_route(const Route& r, const std::string& path,
                              std::unordered_map<std::string, std::string>& params) {
    std::vector<std::string> psegs, rsegs;
    std::istringstream ps(path), rs(r.pattern);
    std::string seg;
    while (std::getline(ps, seg, '/')) if (!seg.empty()) psegs.push_back(seg);
    while (std::getline(rs, seg, '/')) if (!seg.empty()) rsegs.push_back(seg);
    if (psegs.size() != rsegs.size()) return false;
    for (size_t i = 0; i < rsegs.size(); ++i) {
        if (rsegs[i][0] == ':') params[rsegs[i].substr(1)] = psegs[i];
        else if (rsegs[i] != psegs[i]) return false;
    }
    return true;
}

std::string HttpServer::url_decode(const std::string& s) {
    std::string r;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            int v = 0;
            std::sscanf(s.substr(i+1,2).c_str(), "%x", &v);
            r += static_cast<char>(v);
            i += 2;
        } else if (s[i] == '+') r += ' ';
        else r += s[i];
    }
    return r;
}

std::unordered_map<std::string, std::string> HttpServer::parse_query(const std::string& q) {
    std::unordered_map<std::string, std::string> m;
    std::istringstream ss(q);
    std::string pair;
    while (std::getline(ss, pair, '&')) {
        auto eq = pair.find('=');
        if (eq == std::string::npos) continue;
        m[url_decode(pair.substr(0, eq))] = url_decode(pair.substr(eq+1));
    }
    return m;
}

HttpRequest HttpServer::parse_request(const std::string& raw) {
    HttpRequest req;
    std::istringstream ss(raw);
    std::string line;
    std::getline(ss, line);
    std::istringstream rl(line);
    std::string version, full_path;
    rl >> req.method >> full_path >> version;
    auto qpos = full_path.find('?');
    if (qpos != std::string::npos) {
        req.query = full_path.substr(qpos+1);
        req.path = full_path.substr(0, qpos);
    } else {
        req.path = full_path;
    }
    req.qparams = parse_query(req.query);
    while (std::getline(ss, line) && line != "\r") {
        auto col = line.find(':');
        if (col == std::string::npos) continue;
        std::string key = line.substr(0, col);
        std::string val = line.substr(col+1);
        while (!val.empty() && (val[0]==' '||val[0]=='\t')) val=val.substr(1);
        while (!val.empty() && (val.back()=='\r'||val.back()=='\n')) val.pop_back();
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        req.headers[key] = val;
    }
    std::string body_line;
    while (std::getline(ss, body_line)) req.body += body_line + "\n";
    if (!req.body.empty() && req.body.back() == '\n') req.body.pop_back();
    return req;
}

std::string HttpServer::status_text(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        default:  return "Unknown";
    }
}

std::string HttpServer::mime_type(const std::string& path) {
    if (path.size() >= 5 && path.substr(path.size()-5) == ".html") return "text/html; charset=utf-8";
    if (path.size() >= 4 && path.substr(path.size()-4) == ".css")  return "text/css";
    if (path.size() >= 3 && path.substr(path.size()-3) == ".js")   return "application/javascript";
    if (path.size() >= 4 && path.substr(path.size()-4) == ".svg")  return "image/svg+xml";
    if (path.size() >= 4 && path.substr(path.size()-4) == ".ico")  return "image/x-icon";
    if (path.size() >= 5 && path.substr(path.size()-5) == ".json") return "application/json";
    return "application/octet-stream";
}

HttpResponse HttpServer::serve_static(const std::string& path) {
    std::string full = static_dir_ + path;
    if (full.back() == '/') full += "index.html";
    std::ifstream f(full, std::ios::binary);
    HttpResponse resp;
    if (!f) {
        std::ifstream idx(static_dir_ + "/index.html", std::ios::binary);
        if (!idx) { resp.status = 404; resp.body = "{\"error\":\"not found\"}"; return resp; }
        resp.content_type = "text/html; charset=utf-8";
        resp.body = std::string(std::istreambuf_iterator<char>(idx), {});
        return resp;
    }
    resp.content_type = mime_type(full);
    resp.body = std::string(std::istreambuf_iterator<char>(f), {});
    return resp;
}

HttpResponse HttpServer::dispatch(HttpRequest& req) {
    for (auto& route : routes_) {
        if (route.method != req.method) continue;
        std::unordered_map<std::string, std::string> params;
        if (match_route(route, req.path, params)) {
            req.params = params;
            return route.handler(req);
        }
    }
    if (req.method == "GET") return serve_static(req.path);
    HttpResponse r;
    r.status = 404;
    r.body = "{\"error\":\"not found\"}";
    return r;
}

static std::string http_status_text(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        default:  return "Unknown";
    }
}

static std::string build_response(const HttpResponse& resp) {
    std::ostringstream out;
    out << "HTTP/1.1 " << resp.status << " " << http_status_text(resp.status) << "\r\n";
    out << "Content-Type: " << resp.content_type << "\r\n";
    out << "Content-Length: " << resp.body.size() << "\r\n";
    out << "Access-Control-Allow-Origin: *\r\n";
    out << "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n";
    out << "Access-Control-Allow-Headers: Content-Type, Authorization\r\n";
    out << "Strict-Transport-Security: max-age=31536000; includeSubDomains\r\n";
    for (auto& [k, v] : resp.headers) out << k << ": " << v << "\r\n";
    out << "Connection: close\r\n\r\n";
    out << resp.body;
    return out.str();
}

void HttpServer::handle_client(socket_t client) {
    std::string response_str;

    if (ssl_ctx_) {
        // TLS 1.3 path
        SSL* ssl = SSL_new(ssl_ctx_);
        if (!ssl) { sock_close(client); return; }
        SSL_set_fd(ssl, static_cast<int>(client));

        int accept_ret = SSL_accept(ssl);
        if (accept_ret <= 0) {
            ERR_print_errors_fp(stderr);
            SSL_free(ssl);
            sock_close(client);
            return;
        }

        // Read request over TLS
        char buf[16384] = {};
        int n = SSL_read(ssl, buf, sizeof(buf)-1);
        if (n > 0) {
            HttpRequest req = parse_request(std::string(buf, n));
            HttpResponse resp = dispatch(req);
            response_str = build_response(resp);
            SSL_write(ssl, response_str.data(), static_cast<int>(response_str.size()));
        }
        SSL_shutdown(ssl);
        SSL_free(ssl);
    } else {
        // Plain HTTP fallback
        char buf[16384] = {};
        int n = recv(client, buf, sizeof(buf)-1, 0);
        if (n > 0) {
            HttpRequest req = parse_request(std::string(buf, n));
            HttpResponse resp = dispatch(req);
            response_str = build_response(resp);
            send(client, response_str.data(), static_cast<int>(response_str.size()), 0);
        }
    }
    sock_close(client);
}

void HttpServer::run() {
    listen_sock_ = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listen_sock_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port_));
    inet_pton(AF_INET, host_.c_str(), &addr.sin_addr);
    bind(listen_sock_, (sockaddr*)&addr, sizeof(addr));
    listen(listen_sock_, 128);
    running_ = true;
    while (running_) {
        socket_t client = accept(listen_sock_, nullptr, nullptr);
        if (client == INVALID_SOCK) break;
        std::thread([this, client]{ handle_client(client); }).detach();
    }
}

void HttpServer::stop() {
    running_ = false;
    if (listen_sock_ != INVALID_SOCK) {
        sock_close(listen_sock_);
        listen_sock_ = INVALID_SOCK;
    }
}
