# Changelog - Metis Exterminator Plus

All notable changes follow Semantic Versioning (MAJOR.MINOR.PATCH).

---

## [2.3.0] - 2026-07-02

### Removed
- 
- All .js and app.js
- Export/Import buttons removed from navbar (data export now via server-backed endpoint only)
- Standalone HTML file removed as a deliverable — server is required

### Changed
- Session token now stored in `sessionStorage` (tab-scoped, never persists past browser close)
- Theme preference stored in `sessionStorage` with `
- Login page now shows clear error when server is not reachable instead of falling back to 
- `api.js` rewritten as pure server-REST client (70 lines, no 
- `app.js` rewritten: all data operations go through the server; serverError() shown when offline
- Tax rate read from `/api/version` response (sourced from `tax.default_rate` in PSON)
- App name and version read from `/api/version` (sourced from PSON)
- `client` section added to `config.pson` for theme and storage key configuration
- VERSION bumped to 2.3.0

### Fixed
- bcrypt verify=FAIL root cause: replaced hand-written `crypt_blowfish.c` with Solar Designer
  reference implementation (rg3/bcrypt). Added `crypt_gensalt.c` and `crypt.c` ow-crypt wrapper.
  The original implementation produced non-deterministic hash output (hash and verify produced
  different results for the same input).
- `reset_admin()` deadlock: method locked `mu_` then called `create_user()` which also locked `mu_`.
  Fixed by inlining the hash+INSERT in `reset_admin()` directly.
- `router.cpp` missing `#include <iostream>` causing compile error with debug logging.
- Password field width: `input[type=password]` missing from global `width:100%` CSS rule.
- Version string hardcoded in JS — now read from `/api/version` API endpoint backed by PSON.

### Added
- `src/logger.hpp` — thread-safe dual stdout+file logger with flush-on-write and timestamps
- `/api/logs` endpoint — returns last N lines of log file as JSON array
- `/api/audit` endpoint — returns auth-only log events
- System Log and Audit Log views in browser (admin-only nav tabs)
- Logo: rat, cockroach, wasp, ant inside red prohibition circle — original SVG art
- All 17 docs updated to v2.3.0

---

## [2.2.x] - 2026-07-02

See git history for incremental patch details (2.3.0 through 2.3.0).

---

## [2.1.x] - 2026-07-02

See git history for incremental patch details (2.3.0 through 2.3.0).

---

## [2.0.x] - 2026-07-01

### Added (2.3.0)
- SQLite database, bcrypt authentication, session management
- Infrastructure stubs: GPU, Kubernetes, containers
- TLS 1.3 + AES-256-GCM + X25519MLKEM768
- Full 17-document Metis Docs Model

---

## [1.0.1] - 2026-07-01

- Export/Import JSON backup (now removed in 2.3.0)

## [1.0.0] - 2026-07-01

- Initial release: customers, jobs, invoices, dashboard
