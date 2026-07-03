# Changelog - Metis Exterminator Plus

All notable changes follow Semantic Versioning (MAJOR.MINOR.PATCH).

---

## [3.0.0] - 2026-07-02

### Security
- Rate limiting on `POST /api/auth/login` (configurable via `security.rate_limit_login_per_minute`)
- Account lockout after `auth.max_login_attempts` failed logins; auto-clear on success
- CORS restriction to `security.allowed_origins` from PSON
- API key authentication (`X-API-Key` header) for machine-to-machine access
- Per-user role enforcement across all REST endpoints (`admin` vs `technician`)
- Audit log entries written to SQLite `audit_log` table on every login success/failure
- `PUT /api/auth/password` — change own password with old password verification
- `PUT /api/users/:id/password` — admin force-set password for any user
- `POST /api/auth/apikey` — generate API key for current user

### Data Layer
- SQLite schema expanded: `job_chemicals`, `audit_log`, `login_attempts` tables
- `customers` and `jobs` get `deleted` column (soft-delete foundation)
- FTS5 indexes on `customers_fts` and `jobs_fts` for full-text search
- `GET /api/search?q=` — FTS5 search across customers and jobs
- `GET /api/export/invoices.csv` — CSV export
- `GET /api/export/customers.csv` — CSV export

### Business Features
- `POST /api/invoices/check-overdue` — auto-marks Pending invoices past due date as Overdue
- Chemical tracking schema (`job_chemicals` table) ready for UI
- Recurring job columns (`recurring`, `recur_interval_days`) in schema
- Technician time tracking columns (`time_start`, `time_end`) in schema

### User Management API
- `GET /api/users` — list all users (admin only)
- `POST /api/users` — create user with role (admin only)
- `DELETE /api/users/:id` — deactivate user (admin only, cannot deactivate self)

### UI / Client
- Calendar view — monthly grid showing scheduled jobs
- Full-text search view — search bar wired to `/api/search`
- CSV download buttons on Customers and Invoices views
- Overdue check button on Invoices view
- User management UI in Admin view (list, deactivate, set-password)
- Change-password modal wired to real `PUT /api/auth/password` endpoint
- New user creation wired to `POST /api/users`
- Password strength indicator on all password fields
- Dark/light mode respects `client.theme` PSON setting on first load

### Infrastructure
- `POST /api/admin/rotate-log` — manual log rotation (admin only)
- `GET /api/audit/events` — audit events from SQLite (admin only)
- `security`, `email`, `features` sections added to `config/config.pson`
- `client.theme` returned from `/api/version` for client-side theme init

### Fixed
- PSON parser: `#` in quoted values no longer stripped as comment (was truncating `Admin#2026`)
- `AuthManager` constructor now accepts all PSON security parameters

### Docs
- All 17 Metis Docs Model documents updated to v3.1.0
- OpenAPI specification added (API.md)
- Linux systemd deployment guide added (OPERATIONS.md)
- Windows NSSM deployment guide added (OPERATIONS.md)
- CONFIGURATION.md updated with all new PSON sections

---

## [2.3.x] - 2026-07-02 (see git history)
## [2.2.x] - 2026-07-02 (see git history)
## [2.1.x] - 2026-07-02 (see git history)
## [2.0.0] - 2026-07-01
## [1.0.0] - 2026-07-01 — Initial release

## [3.1.0] - 2026-07-02

### Data Layer
- **Migrated CustomerStore, JobStore, InvoiceStore fully to SQLite** — flat `.dat` files retired
- FTS5 indexes rebuilt on every startup
- Soft-delete: `deleted=1` column on customers and jobs (UI shows only active records)
- New `job_chemicals`, `job_technician_time` columns fully wired

### Business Features
- **Chemical usage tracking** — GET/POST/DELETE `/api/jobs/:id/chemicals`; UI in job rows (Chem button)
- **Technician time tracking** — PUT `/api/jobs/:id/time`; UI clock button on each job row; auto-marks In Progress when start time recorded
- **Recurring job spawning** — POST `/api/jobs/:id/spawn-recurring`; creates next occurrence; button shown on recurring jobs
- **Route optimization** — GET `/api/route/optimize?date=&technician=`; sorts jobs by address; full Route view with technician filter and print button
- **Email notifications** — POST `/api/invoices/:id/send-email`; logs intent; email button on invoice rows; SMTP integration ready via `email.*` config
- **Print invoice** — print-optimized popup window with full invoice layout; opens browser print dialog automatically
- **Print/PDF calendar** — Print button on calendar view; `@media print` CSS hides nav/buttons, preserves calendar grid

### UI / Client
- **Pagination helper** — `paginate()` and `paginationHtml()` functions ready for all list views (PAGE_SIZE=25)
- **Print-optimized invoice layout** — `printInvoice(id)` opens professional invoice in new window with green branding, item table, totals, and auto-print
- **Route view** — new nav tab; date picker + technician filter + refresh + print button
- Calendar print: `window.print()` on calendar view produces clean PDF-ready output

### API
- `GET /api/jobs/:id/chemicals` — list chemicals for job
- `POST /api/jobs/:id/chemicals` — add chemical record
- `DELETE /api/jobs/:id/chemicals/:cid` — remove chemical record
- `PUT /api/jobs/:id/time` — update time_start/time_end
- `POST /api/jobs/:id/spawn-recurring` — create next recurring job
- `POST /api/invoices/:id/send-email` — trigger email notification
- `GET /api/route/optimize` — optimized job list by address

### Documentation
- All 17 docs updated to v3.1.0
- API.md updated with all new endpoints
- CONFIGURATION.md updated
