# Implementation Plan - Metis Exterminator Plus

**Version 3.1.0**

---

## Phase 1 — Foundation (COMPLETE: v3.1.0)

- [x] C++20 HTTP server (zero external dependencies)
- [x] PSON configuration system
- [x] Customer, Job, Invoice management
- [x] Vanilla-JS browser SPA
- [x] Dashboard with stats and upcoming jobs
- [x] CMake + Ninja build for Windows/Linux/macOS

## Phase 2 — Security and Reliability (COMPLETE: v3.1.0 - v3.1.0)

- [x] SQLite database with WAL mode
- [x] bcrypt-12 password hashing (Solar Designer implementation)
- [x] TLS 1.3 + AES-256-GCM + Post-Quantum (X25519MLKEM768)
- [x] Session-based authentication (256-bit tokens, SQLite-backed)
- [x] Infrastructure stubs: GPU, Kubernetes, containers
- [x] Prometheus metrics endpoint
- [x] Kubernetes liveness/readiness probes
- [x] Thread-safe dual stdout+file logger
- [x] System log and audit log browser views
- [x] All PSON-driven configuration (nothing hardcoded in source)
- [x] Server-only client — removed offline/localStorage data mode; server required
- [x] Full 17-document Metis Docs Model

## Phase 3 — Full SQLite Migration (Planned: v3.1.0)

- [x] Migrate CustomerStore, JobStore, InvoiceStore to SQLite
- [ ] Retire flat-file `.dat` stores
- [x] FTS5 full-text search
- [ ] `POST /api/users` — user management API
- [ ] `PUT /api/auth/password` — change password
- [x] Role-based API access control

## Phase 4 — Business Completeness (Planned: v3.1.0)

- [ ] Recurring service scheduling
- [ ] Automatic overdue invoice detection
- [ ] Chemical usage tracking
- [ ] CSV export for accounting
- [ ] Print-optimized invoice layout
- [ ] Email notification hooks

## Phase 5 — Infrastructure Activation (Planned: v3.1.0)

- [ ] GPU analytics (Windows CUDA/OpenCL, Linux ROCm, macOS Metal)
- [ ] Kubernetes deployment manifests
- [ ] Container image (containerd/podman)
- [ ] Log rotation
- [ ] Structured JSON logging

## Phase 6 — Commercial Readiness (Planned: v3.1.0)

- [ ] Payment processing integration
- [ ] QuickBooks export
- [ ] Customer self-service portal
- [ ] Multi-user role management UI
- [ ] License key system
