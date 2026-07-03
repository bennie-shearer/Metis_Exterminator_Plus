# API Reference - Metis Exterminator Plus

**Version 3.1.0** | Base URL: `https://127.0.0.1:9100`

All endpoints return `Content-Type: application/json` unless noted.
Protected endpoints require `Authorization: Bearer <token>` or `X-API-Key: <key>`.

---

## Authentication

### POST /api/auth/login
```json
{ "username": "admin", "password": "Admin#2026" }
```
Response: `{ "token": "<256-bit hex>" }`
Errors: 400 missing fields, 401 invalid credentials, 429 rate limited

### POST /api/auth/logout
```json
{ "token": "<token>" }
```
Response: 204

### GET /api/auth/me
Returns current user: `{ "id": 1, "username": "admin", "role": "admin" }`

### PUT /api/auth/password
```json
{ "old_password": "...", "new_password": "..." }
```
Response: `{ "status": "password changed" }`

### POST /api/auth/apikey
Generates API key for current user.
Response: `{ "api_key": "mep_<hex>" }`

---

## Users (admin only)

### GET /api/users
Returns array of `{ id, username, role, active, has_api_key }`.

### POST /api/users
```json
{ "username": "tech1", "password": "SecurePass#1", "role": "technician" }
```
Response: 201 `{ "status": "created", "username": "tech1", "role": "technician" }`
Errors: 409 username exists

### DELETE /api/users/:id
Deactivates user and kills all their sessions. Response: 204.

### PUT /api/users/:id/password
```json
{ "new_password": "NewPass#1" }
```
Admin force-set password. Response: `{ "status": "password updated" }`.

---

## Customers

### GET /api/customers
### GET /api/customers/:id
### POST /api/customers
```json
{ "name":"*", "phone":"", "email":"", "address":"", "city":"", "state":"", "zip":"", "notes":"" }
```
### PUT /api/customers/:id
### DELETE /api/customers/:id — soft-delete (sets deleted=1)

---

## Jobs

### GET /api/jobs
Query params: `status=Scheduled`, `customer_id=1`
### GET /api/jobs/:id
### POST /api/jobs
```json
{ "customer_id":1, "service_type":"*", "pest_type":"", "scheduled_date":"YYYY-MM-DD",
  "scheduled_time":"HH:MM", "technician":"", "address":"", "price":0.0,
  "status":"Scheduled", "notes":"", "recurring":false, "recur_interval_days":0 }
```
### PUT /api/jobs/:id
### DELETE /api/jobs/:id

---

## Invoices

### GET /api/invoices
### GET /api/invoices/:id — includes `items` array
### POST /api/invoices
```json
{ "customer_id":1, "issued_date":"YYYY-MM-DD", "due_date":"YYYY-MM-DD",
  "notes":"", "items":[{"description":"*","unit_price":0.0,"quantity":1}] }
```
### PUT /api/invoices/:id
### DELETE /api/invoices/:id
### POST /api/invoices/check-overdue
Marks Pending invoices past due date as Overdue.
Response: `{ "marked_overdue": 3 }`

---

## Search

### GET /api/search?q=
FTS5 full-text search across customers and jobs.
Response: `{ "customers": [...], "jobs": [...] }`

---

## Export

### GET /api/export/invoices.csv — CSV download
### GET /api/export/customers.csv — CSV download

---

## System

### GET /api/version
Returns app config: `{ version, app, author, company, currency, tax_name, tax_rate, theme }`

### GET /api/logs?lines=100
Returns last N log lines as JSON array of strings.

### GET /api/audit
Returns auth-related log lines from log file.

### GET /api/audit/events?limit=100 (admin only)
Returns audit events from SQLite: `[{ id, event, username, ip, details, success, created_at }]`

### POST /api/admin/rotate-log (admin only)
Rotates the log file. Response: `{ "status": "log rotated" }`

### GET /api/stats
Returns `{ customers, jobs_scheduled, jobs_completed, revenue, outstanding }`

### GET /healthz
Liveness probe. Returns 200.

### GET /readyz
Readiness probe. Returns 200 when data loaded.

### GET /metrics
Prometheus-compatible metrics (text/plain).

### POST /api/shutdown (admin only)
Flushes all data and stops the server cleanly.

---

## Chemical Tracking

### GET /api/jobs/:id/chemicals
Returns `[{ id, chemical_name, epa_reg, quantity_oz, unit, applied_at }]`

### POST /api/jobs/:id/chemicals
```json
{ "chemical_name":"Bifenthrin", "epa_reg":"279-3210", "quantity_oz":4.0, "unit":"oz" }
```

### DELETE /api/jobs/:id/chemicals/:cid — Response: 204

---

## Technician Time Tracking

### PUT /api/jobs/:id/time
```json
{ "time_start":"08:30", "time_end":"10:15" }
```
Sets `time_start`/`time_end`. Auto-sets status to `In Progress` when start time is recorded.

---

## Recurring Jobs

### POST /api/jobs/:id/spawn-recurring
Creates next occurrence of a recurring job using `recur_interval_days`.
Response: `{ "id": 42, "scheduled_date": "2026-08-02" }`

---

## Email Notifications

### POST /api/invoices/:id/send-email
Queues invoice email to customer's email address.
Response: `{ "status": "queued", "to": "...", "invoice": "INV-1001", "note": "..." }`
Configure `email.*` in config.pson. SMTP library integration required for actual delivery.

---

## Route Optimization

### GET /api/route/optimize?date=YYYY-MM-DD&technician=Name
Returns scheduled/in-progress jobs for the date and technician, sorted by address.
Response: `[{ id, customer_id, service_type, address, technician, scheduled_time }]`
