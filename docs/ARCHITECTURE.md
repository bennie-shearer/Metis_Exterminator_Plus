# Architecture - Metis Exterminator Plus

**Version 3.1.0**

---

## System Overview

```
Browser Client (Vanilla JS)
        |
        | HTTP REST (JSON)
        v
C++20 HTTP Server (port 9100)
        |
        +---> Router (register_routes)
        |         |
        |         +---> CustomerStore   ---> customers.dat (flat-file)
        |         +---> JobStore        ---> jobs.dat
        |         +---> InvoiceStore    ---> invoices.dat
        |         +---> AuthManager     ---> SQLite (sessions, users)
        |         +---> Database        ---> SQLite (schema, queries)
        |         +---> K8s stubs       ---> /healthz /readyz /metrics
        |
        +---> Static file server ---> web/
        |
        +---> PSON config reader ---> config/config.pson
```

---

## Component Map

### src/main.cpp
Entry point. Reads config.pson, opens the database, bootstraps admin credentials, loads flat-file stores, registers routes, starts the HTTP server, and installs a signal handler for clean shutdown.

### src/server.hpp / server.cpp
Single-threaded accept loop with per-connection detached threads. Parses raw HTTP/1.1 requests, dispatches to registered route handlers, serves static files as fallback, and writes HTTP/1.1 responses. Supports GET, POST, PUT, DELETE, OPTIONS. No external networking library.

### src/router.hpp / router.cpp
Registers all REST endpoints on the HttpServer instance. Thin layer: extracts JSON fields from request bodies, calls store or auth methods, serializes responses to JSON strings. Also registers infrastructure endpoints (/healthz, /readyz, /metrics).

### src/pson.hpp / pson.cpp
PSON configuration parser. Reads `section { key = "value" }` files with comment support (#). Used by every component for runtime configuration.

### src/database.hpp / database.cpp
SQLite wrapper. Opens the database, applies PRAGMA settings from config, creates the schema on first run. Provides a `Stmt` RAII class for prepared statements. Schema includes: customers, jobs, invoices, invoice_items, users, sessions.

### src/auth.hpp / auth.cpp
Session-based authentication. Hashes passwords with bcrypt (bundled). Generates 256-bit hex session tokens. Validates sessions against the SQLite sessions table. Purges expired sessions periodically.

### src/customer_store.hpp / customer_store.cpp
### src/job_store.hpp / job_store.cpp
### src/invoice_store.hpp / invoice_store.cpp
Flat-file data stores (pipe-delimited, escape-safe). Loaded at startup, flushed on change and at shutdown. Will be retired in v3.1.0 when full SQLite migration is complete.

### third_party/sqlite/
SQLite 3 amalgamation (sqlite3.c, sqlite3.h). Compiled as a C source file with SQLITE_THREADSAFE=1.

### third_party/bcrypt/
bcrypt wrapper library by Ricardo Garcia (CC0). Provides `bcrypt_gensalt`, `bcrypt_hashpw`, `bcrypt_checkpw`. Requires crypt_blowfish/ subfolder.

### third_party/nlohmann/
nlohmann/json single-header library. Available for future use; current JSON serialization uses hand-written `jstr()` helpers to avoid a heavy include in hot paths.

### openssl-prebuilt/
Prebuilt OpenSSL static libraries for Windows, Linux, and macOS. CMake selects the correct platform directory.

### infra/gpu/gpu_stub.hpp
Stub for GPU acceleration. Provides `detect()` and `is_enabled()`. Platform-specific detection hooks documented in comments. Activated by `infra.gpu_enabled = true` in config.pson.

### infra/kubernetes/k8s_stub.hpp
Stub for Kubernetes integration. Provides `liveness_probe()`, `readiness_probe()`, and `prometheus_metrics()`. The last function is live: `/metrics` returns real counts.

### infra/containers/container_stub.hpp
Stub for container runtime introspection. Detects whether the process is running inside a container via environment variables and cgroup inspection on Linux.

### web/
Static browser client. `index.html` is the SPA shell. `css/app.css` provides the green-toned Metis design. `js/api.js` is the API abstraction layer (auto-detects server vs standalone . `js/app.js` is the full application logic.

---

## Data Flow: Creating an Invoice

```
User fills invoice form in browser
    --> POST /api/invoices (JSON body)
        --> router.cpp: parse items array, calculate subtotal/tax/total
            --> InvoiceStore::add() assigns invoice_number, writes to invoices.dat
                --> invoices.save() flushes to disk
                    --> JSON response returned to browser
                        --> loadInvoices() refreshes the table
```

---

## Threading Model

- Main thread: accept() loop
- Per-connection: detached std::thread (handle_client)
- Background: detached std::thread for session purge (every 10 minutes)
- Store access: std::mutex per store (CustomerStore, JobStore, InvoiceStore)
- Database access: SQLite SQLITE_THREADSAFE=1 (serialized mode)

---

## Security Notes

- Passwords: bcrypt with configurable cost factor (default 12)
- Sessions: 256-bit random hex token, stored in SQLite, expiry enforced server-side
- TLS: optional via config, self-signed cert included for development
- CORS: permissive (all origins) for localhost development; restrict in production
- Input validation: service_type and name fields validated; SQL injection impossible (prepared statements)
