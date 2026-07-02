#pragma once
#include <string>
#include <functional>
#include <unordered_map>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOMINMAX
  #define NOMINMAX
  #endif
  #include <bcrypt.h>
  #include <winsock2.h>
  #include <ws2tcpip.h>
  using socket_t = SOCKET;
  #define INVALID_SOCK INVALID_SOCKET
  #define sock_close(s) closesocket(s)
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  using socket_t = int;
  #define INVALID_SOCK (-1)
  #define sock_close(s) close(s)
#endif

// Forward-declare OpenSSL types so we don't pull openssl headers into every TU
struct ssl_ctx_st;
struct ssl_st;
typedef struct ssl_ctx_st SSL_CTX;
typedef struct ssl_st     SSL;

struct HttpRequest {
    std::string method;
    std::string path;
    std::string query;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    std::unordered_map<std::string, std::string> params;
    std::unordered_map<std::string, std::string> qparams;
};

struct HttpResponse {
    int status = 200;
    std::string content_type = "application/json";
    std::string body;
    std::unordered_map<std::string, std::string> headers;
};

using Handler = std::function<HttpResponse(const HttpRequest&)>;

class HttpServer {
public:
    HttpServer(const std::string& host, int port, const std::string& static_dir);
    ~HttpServer();

    // Call before run() to enable TLS 1.3 + AES-256-GCM + X25519MLKEM768
    bool enable_tls(const std::string& cert_path, const std::string& key_path);
    bool tls_active() const { return ssl_ctx_ != nullptr; }

    void add_route(const std::string& method, const std::string& pattern, Handler h);
    void run();
    void stop();

private:
    std::string host_;
    int port_;
    std::string static_dir_;
    socket_t listen_sock_ = INVALID_SOCK;
    std::atomic<bool> running_{false};
    SSL_CTX* ssl_ctx_ = nullptr;

    struct Route {
        std::string method;
        std::string pattern;
        std::vector<std::string> param_names;
        Handler handler;
    };
    std::vector<Route> routes_;

    void handle_client(socket_t client);
    bool match_route(const Route& r, const std::string& path,
                     std::unordered_map<std::string, std::string>& params);
    HttpResponse serve_static(const std::string& path);
    HttpResponse dispatch(HttpRequest& req);
    static std::string status_text(int code);
    static std::string mime_type(const std::string& path);
    static std::string url_decode(const std::string& s);
    static std::unordered_map<std::string, std::string> parse_query(const std::string& q);
    static HttpRequest parse_request(const std::string& raw);
};
