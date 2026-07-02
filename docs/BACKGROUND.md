# Background - Metis Exterminator Plus

**Version 2.3.0**

---

## Table of Contents

1. [The History and Purpose of Software for Exterminator Environments](#1-the-history-and-purpose-of-software-for-exterminator-environments)
2. [The Evolution of Pest Control Business Systems](#2-the-evolution-of-pest-control-business-systems)
3. [How Metis Exterminator Plus Fits the Ecosystem](#3-how-metis-exterminator-plus-fits-the-ecosystem)
4. [Design Philosophy](#4-design-philosophy)
5. [Technology Choices](#5-technology-choices)
6. [The Metis Suite Context](#6-the-metis-suite-context)

---

## 1. The History and Purpose of Software for Exterminator Environments

Pest control is one of the oldest service trades, yet it was among the last to adopt dedicated software management. Through the 1980s and early 1990s, most exterminators operated on paper route cards, handwritten invoices, and appointment ledgers kept in ring binders.

The first dedicated pest control software appeared in the mid-1990s as DOS-based scheduling tools distributed by regional vendors. These systems handled route scheduling and chemical tracking but had no networking capability, relying instead on end-of-day printouts and physical filing.

The Windows era (1995-2005) brought the first graphical pest control management systems. Products such as ServSuite, PestPac, and Briostack introduced customer record management, recurring service scheduling, and basic invoicing. These were client-server applications deployed on local area networks. Data lived in proprietary binary formats or early relational databases.

The SaaS revolution (2008-present) shifted the market to cloud-hosted platforms. Modern exterminator software suites offer mobile field apps, GPS route optimization, digital signatures, chemical tracking for regulatory compliance, and accounting integrations. The tradeoff is vendor lock-in, monthly subscription costs, and complete dependence on internet connectivity and third-party infrastructure.

The core insight driving Metis Exterminator Plus is that a significant segment of pest control businesses — particularly single-owner operators and small regional companies — neither needs nor can afford enterprise SaaS overhead. They need a fast, reliable, locally-hosted system with a modern browser UI, military-grade security, and zero ongoing subscription cost.

---

## 2. The Evolution of Pest Control Business Systems

**Generation 1: Paper and Card Systems (pre-1990)**
Route cards, chemical logs in notebooks, carbon-copy invoices. The system was the technician's memory and the office manager's filing cabinet.

**Generation 2: DOS and Early Windows (1990-1998)**
Single-machine scheduling programs. Data lived on one PC. No internet dependency. Zero ongoing cost after purchase.

**Generation 3: Client-Server LAN (1998-2010)**
Multi-user systems over local area networks. Centralized SQL databases. Dedicated office server required. Chemical usage reporting and regulatory compliance tracking emerged.

**Generation 4: Cloud SaaS (2010-present)**
Browser-based or mobile-first platforms. Monthly subscription pricing. GPS tracking, mobile field apps, online customer portals, automated payment processing. High capability at high cost with complete internet dependency and external data custody.

**The Gap Metis Addresses**
A self-hosted, locally-run system with a modern browser client, enterprise-grade security (TLS 1.3, AES-256-GCM, Post-Quantum key exchange, bcrypt authentication), and zero subscription cost. Metis Exterminator Plus fills this gap for the Windows, Linux, and macOS desktop.

---

## 3. How Metis Exterminator Plus Fits the Ecosystem

Metis Exterminator Plus is positioned as a self-hosted alternative to Generation 4 SaaS platforms, delivering the core workflow of a pest control business without subscription fees, internet dependencies, or external data custody.

**Core workflow coverage in v2.3.0:**
- New customer intake and full contact management
- Job creation, scheduling, and technician assignment
- Service delivery tracking with status progression
- Invoice generation with configurable tax and line items
- Payment recording and outstanding balance tracking
- JSON data export for backup and migration
- Full system and audit logging viewable from the browser
- Kubernetes-ready health probes and Prometheus metrics

**Infrastructure readiness:**
- GPU acceleration hooks (Windows CUDA/OpenCL, Linux ROCm/OpenCL, macOS Metal)
- Kubernetes deployment stubs (liveness, readiness, metrics endpoints)
- Container runtime detection (containerd, podman — no Docker required)

**Comparison positioning:**
Metis Exterminator Plus competes with pen-and-paper and spreadsheet systems by offering a structured, searchable SQLite database with a modern UI. It competes with entry-level SaaS by eliminating subscription costs and internet dependency. It does not attempt to compete with enterprise platforms on GPS route optimization or mobile native apps.

---

## 4. Design Philosophy

Metis Exterminator Plus is built on the core principles of the Metis Suite:

**Zero external runtime dependencies.**
Every library the server needs ships with the project. OpenSSL, SQLite, bcrypt, and nlohmann/json are bundled. No package manager, no install wizard, no internet connection required to run.

**PSON-only configuration.**
No values that affect runtime behavior are hardcoded in source. Every port number, file path, tax rate, bcrypt cost factor, session timeout, TLS cert path, admin credentials, and infrastructure flag lives in `config/config.pson`. This makes the system transparent and auditable: the entire runtime configuration is one human-readable text file.

**Single VERSION file as authoritative source.**
The `VERSION` file at the project root is the sole source of version truth. CMake reads it, `configure_file` injects it into `version.hpp`, and every document references the same number.

**SQLite as the database engine.**
SQLite requires no server process, no installation, no configuration, and no network. A single file holds the entire database. WAL mode provides concurrent reader safety. The `.db` file is the backup.

**Exe-relative paths throughout.**
All file references are resolved relative to the executable. The project directory is self-contained and portable.

**Server-first client.**
The browser client communicates exclusively with the C++20 server via REST API over TLS. There is no offline/. This ensures all data is always in the SQLite database, properly backed up, and subject to server-side validation.

**Military-grade security by default.**
TLS 1.3 enforced (no TLS 1.2 or below), AES-256-GCM cipher suite, Post-Quantum key exchange (X25519MLKEM768), bcrypt-12 password hashing, 256-bit session tokens, server-side session expiry, and timing-safe password comparison — all active by default, all configurable from PSON.

**Infrastructure stubs by design.**
GPU acceleration, Kubernetes integration, and container runtime introspection are stubbed with clear hook points. They compile cleanly, document the intended integration surface, and activate via PSON flags when the implementation is complete.

---

## 5. Technology Choices

| Component | Choice | Rationale |
|---|---|---|
| Language | C++20 | Native performance, zero runtime, cross-platform |
| Build system | CMake + Ninja | CLion native, fast incremental builds |
| Database | SQLite (bundled) | Zero install, single file, WAL, ACID |
| Password hashing | bcrypt Solar Designer (bundled) | Proven, tunable cost, timing-safe |
| JSON | nlohmann/json (bundled) | Header-only, modern C++ |
| TLS | OpenSSL 4.0 (prebuilt static) | TLS 1.3, AES-256-GCM, X25519MLKEM768 |
| Configuration | PSON (custom) | Metis Suite standard, human-readable |
| Frontend | Vanilla JS + HTML + CSS | Zero build step, no framework, any browser |
| HTTP server | Custom C++20 | Zero external dependency, full control |
| Logging | Custom Logger (src/logger.hpp) | Dual stdout+file, flushed, timestamped |

---

## 6. The Metis Suite Context

Metis Exterminator Plus is one application in the Metis Suite — a family of zero-dependency C++20 HTTP server applications with vanilla-JS browser clients. Every Metis Suite application shares the same invariants: PSON configuration, single VERSION file, exe-relative paths, no external runtime dependencies, pure ASCII source, zero Docker/Doxygen/PyTest/GTest/Jupyter.

The Metis Suite spans domains including security tools, mainframe emulation, code analysis, church management, financial analysis, medical records, logistics, and congressional intelligence. Metis Exterminator Plus extends this family into field service management for the pest control trade.

The shared architecture means that an administrator familiar with any one Metis Suite application can operate any other. The configuration structure, startup sequence, logging format, and backup approach are identical across the entire family.
