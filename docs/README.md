# Metis Exterminator Plus

**Version 2.3.0** | MIT License | C++20 | Windows / Linux / macOS

A professional pest control business management system built on C++20 with a
vanilla-JavaScript browser client. Part of the Metis Suite family.

*Dedicated to the Chones Family.*

---

## Features

- Customer management with full contact records
- Job scheduling — pest type, service type, technician, address, status
- Invoice generation with configurable line items and tax rate
- SQLite database with WAL mode (ACID, single-file backup)
- bcrypt-12 password hashing (Solar Designer implementation)
- TLS 1.3 + AES-256-GCM + Post-Quantum (X25519MLKEM768) transport
- Session-based login with configurable expiry
- System log and audit log viewable in the browser
- Prometheus-compatible `/metrics` endpoint
- Kubernetes liveness (`/healthz`) and readiness (`/readyz`) probes
- GPU, Kubernetes, and container infrastructure stubs (ready for future activation)
- PSON-driven configuration — nothing operational is hardcoded in source
- JSON data export from the browser for backup

## Quick Start

```
cmake -S . -B build -G Ninja
cmake --build build
build\Metis_Exterminator_Plus.exe
```

Default credentials: read from `config/config.pson` — `auth.admin_user` / `auth.admin_password`.

Open your browser to the URL printed on startup (HTTPS if TLS is enabled).

## Project Structure

```
Metis_Exterminator_Plus/
  src/              C++20 server source
  web/              Vanilla-JS browser client
  config/           config.pson (single source of truth)
  third_party/      SQLite, bcrypt, nlohmann/json (bundled)
  openssl-prebuilt/ Pre-built OpenSSL static libs
  tls/              TLS certificate and key
  infra/            GPU, Kubernetes, container stubs
  docs/             All documentation
  VERSION           Authoritative version (MAJOR.MINOR.PATCH)
```

## Documentation

| Document | Purpose |
|---|---|
| API.md | REST endpoint reference |
| ARCHITECTURE.md | System design |
| BACKGROUND.md | History and design philosophy |
| BILLING.md | Invoice and payment workflow |
| BUILD_GUIDE.md | Build instructions |
| CHANGELOG.md | Version history |
| COMMERCIAL-TODO.md | Commercial feature roadmap |
| COMPARISON.md | Comparison with alternatives |
| CONFIGURATION.md | config.pson reference |
| DATA_MODEL.md | Database schema |
| HOWTO.md | Common task walkthroughs |
| IMPLEMENTATION-PLAN.md | Development phases |
| OPERATIONS.md | Deployment and maintenance |
| SUPPORT.md | Troubleshooting |
| TODO.md | Pending improvements |

## License

MIT — see `docs/LICENSE.txt`.

## Author

Bennie Shearer (Retired)

## Acknowledgments

| Organization | Website |
|---|---|
| CLion by JetBrains s.r.o. | https://www.jetbrains.com/clion/ |
| WebStorm by JetBrains s.r.o. | https://www.jetbrains.com/webstorm/ |
| Claude by Anthropic PBC | https://www.anthropic.com/ |
| OpenSSL Project | https://www.openssl.org/ |
| SQLite | https://sqlite.org/ |
