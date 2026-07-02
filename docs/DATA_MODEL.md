# Data Model - Metis Exterminator Plus

**Version 2.3.0**

---

## SQLite Schema

### customers

| Column | Type | Description |
|---|---|---|
| id | INTEGER PK AUTOINCREMENT | Unique customer ID |
| name | TEXT NOT NULL | Full name or business name |
| phone | TEXT | Phone number |
| email | TEXT | Email address |
| address | TEXT | Street address |
| city | TEXT | City |
| state | TEXT | State/province (2 letters) |
| zip | TEXT | Postal code |
| notes | TEXT | Free-form notes |
| active | INTEGER | 1 = active, 0 = inactive |
| created_at | TEXT | ISO 8601 UTC timestamp |

### jobs

| Column | Type | Description |
|---|---|---|
| id | INTEGER PK AUTOINCREMENT | Unique job ID |
| customer_id | INTEGER FK | References customers(id), CASCADE DELETE |
| service_type | TEXT | One of: Initial Inspection, General Treatment, Follow-up Treatment, Preventive Service, Emergency Service, Exclusion Service |
| pest_type | TEXT | Pest being treated |
| status | TEXT | Scheduled, In Progress, Completed, Cancelled |
| scheduled_date | TEXT | YYYY-MM-DD |
| scheduled_time | TEXT | HH:MM (24h) |
| technician | TEXT | Assigned technician name |
| address | TEXT | Service address (may differ from customer address) |
| notes | TEXT | Job notes |
| price | REAL | Job price before tax |
| created_at | TEXT | ISO 8601 UTC timestamp |
| completed_at | TEXT | ISO 8601 UTC timestamp (empty if not completed) |

### invoices

| Column | Type | Description |
|---|---|---|
| id | INTEGER PK AUTOINCREMENT | Unique invoice ID |
| customer_id | INTEGER FK | References customers(id), CASCADE DELETE |
| job_id | INTEGER | Associated job ID (0 if standalone) |
| invoice_number | TEXT UNIQUE | Generated: PREFIX-NNNNN (e.g. INV-01000) |
| status | TEXT | Pending, Paid, Overdue |
| subtotal | REAL | Sum of line items |
| tax_rate | REAL | Tax rate at time of invoice (decimal) |
| tax_amount | REAL | Calculated tax |
| total | REAL | subtotal + tax_amount |
| issued_date | TEXT | YYYY-MM-DD |
| due_date | TEXT | YYYY-MM-DD |
| paid_date | TEXT | YYYY-MM-DD (empty if unpaid) |
| notes | TEXT | Invoice notes |
| created_at | TEXT | ISO 8601 UTC timestamp |

### invoice_items

| Column | Type | Description |
|---|---|---|
| id | INTEGER PK AUTOINCREMENT | Unique item ID |
| invoice_id | INTEGER FK | References invoices(id), CASCADE DELETE |
| description | TEXT | Line item description |
| unit_price | REAL | Price per unit |
| quantity | INTEGER | Quantity (minimum 1) |

### users

| Column | Type | Description |
|---|---|---|
| id | INTEGER PK AUTOINCREMENT | Unique user ID |
| username | TEXT UNIQUE COLLATE NOCASE | Login username |
| password_hash | TEXT | bcrypt hash |
| role | TEXT | admin or technician |
| active | INTEGER | 1 = active, 0 = disabled |
| created_at | TEXT | ISO 8601 UTC timestamp |

### sessions

| Column | Type | Description |
|---|---|---|
| token | TEXT PK | 256-bit hex session token |
| user_id | INTEGER FK | References users(id), CASCADE DELETE |
| created_at | TEXT | ISO 8601 UTC timestamp |
| expires_at | TEXT | ISO 8601 UTC timestamp |

---

## Indexes

| Index | Table | Columns | Purpose |
|---|---|---|---|
| idx_jobs_customer | jobs | customer_id | Fast customer job lookup |
| idx_jobs_status | jobs | status | Status filter queries |
| idx_jobs_date | jobs | scheduled_date | Date-sorted job lists |
| idx_invoices_customer | invoices | customer_id | Customer invoice history |
| idx_invoices_status | invoices | status | Overdue/pending queries |
| idx_sessions_expires | sessions | expires_at | Expired session purge |

---

## Flat-File Format (Legacy / Export)

Flat-file stores use pipe-delimited records with escape sequences:
- `|` encoded as `\p`
- `\` encoded as `\\`
- `^` (invoices) encoded as `\c`

These files (`customers.dat`, `jobs.dat`, `invoices.dat`) in the `data/` directory serve as the export/import target for the . They will be deprecated in a future version when the client switches to direct SQLite export.

---

## Pest Types (Enumeration)

Ants, Bed Bugs, Bees/Wasps, Cockroaches, Fleas, Flies, Mice/Rats, Mosquitoes, Moths, Silverfish, Spiders, Termites, Ticks, Wildlife, Other

## Service Types (Enumeration)

Initial Inspection, General Treatment, Follow-up Treatment, Preventive Service, Emergency Service, Exclusion Service
