# API Reference - Metis Exterminator Plus

**Version 2.3.0** | Base URL: `http://2.3.0.1:9100`

All request and response bodies are JSON. All endpoints support CORS preflight (OPTIONS).

---

## Authentication

### POST /api/auth/login

Authenticate and receive a session token.

**Request**
```json
{ "username": "admin", "password": "Admin#2026" }
```

**Response 200**
```json
{ "token": "a1b2c3d2.3.0." }
```

**Response 401**
```json
{ "error": "invalid credentials" }
```

### POST /api/auth/logout

Invalidate a session token.

**Request**
```json
{ "token": "a1b2c3d2.3.0." }
```

**Response 204** (no body)

### GET /api/auth/me

Validate the current session and return user info.

**Header:** `Authorization: Bearer <token>`

**Response 200**
```json
{ "id": 1, "username": "admin", "role": "admin" }
```

---

## Customers

### GET /api/customers

Returns all customers. Optional query parameter: `q` (search by name, phone, or email).

**Response 200** - array of customer objects

### GET /api/customers/:id

**Response 200** - single customer object  
**Response 404** `{ "error": "not found" }`

### POST /api/customers

**Request**
```json
{
  "name": "John Smith",
  "phone": "(208) 555-0100",
  "email": "john@example.com",
  "address": "123 Main St",
  "city": "Coeur d Alene",
  "state": "ID",
  "zip": "83814",
  "notes": ""
}
```
**Response 201** - created customer object

### PUT /api/customers/:id

Same fields as POST. **Response 200** - updated customer object.

### DELETE /api/customers/:id

**Response 204** (no body)

---

## Jobs

### GET /api/jobs

Optional query parameters: `customer_id`, `status`.

**Response 200** - array of job objects

### GET /api/jobs/:id

**Response 200** - single job object

### POST /api/jobs

**Request**
```json
{
  "customer_id": 1,
  "service_type": "General Treatment",
  "pest_type": "Ants",
  "status": "Scheduled",
  "scheduled_date": "2026-07-15",
  "scheduled_time": "09:00",
  "technician": "Mike Johnson",
  "address": "123 Main St",
  "price": 125.00,
  "notes": ""
}
```
**Response 201** - created job object

### PUT /api/jobs/:id

Same fields as POST plus optional `completed_at`. **Response 200** - updated job object.

### DELETE /api/jobs/:id

**Response 204**

---

## Invoices

### GET /api/invoices

Optional query parameter: `customer_id`.

**Response 200** - array of invoice objects (includes `items` array)

### GET /api/invoices/:id

**Response 200** - single invoice object

### POST /api/invoices

**Request**
```json
{
  "customer_id": 1,
  "job_id": 5,
  "issued_date": "2026-07-15",
  "due_date": "2026-08-14",
  "notes": "",
  "items": [
    { "description": "General ant treatment", "unit_price": 125.00, "quantity": 1 },
    { "description": "Bait stations (4)", "unit_price": 12.50, "quantity": 4 }
  ]
}
```
Tax is calculated automatically from `app.tax_rate` in config.pson.

**Response 201** - created invoice with `invoice_number`, `subtotal`, `tax_amount`, `total`

### PUT /api/invoices/:id

Updatable fields: `status`, `paid_date`, `due_date`, `notes`.

**Response 200** - updated invoice object

### DELETE /api/invoices/:id

**Response 204**

---

## Dashboard

### GET /api/stats

**Response 200**
```json
{
  "customers": 42,
  "jobs_scheduled": 8,
  "jobs_completed": 127,
  "revenue": 15840.00,
  "outstanding": 1250.00
}
```

---

## Infrastructure

### GET /api/version

```json
{ "version": "2.3.0", "app": "Metis Exterminator Plus", "author": "Bennie Shearer" }
```

### GET /healthz

Kubernetes liveness probe. Returns `200` when the server is alive.

```json
{ "status": "alive" }
```

### GET /readyz

Kubernetes readiness probe. Returns `200` when the server is ready to serve traffic.

```json
{ "status": "ready" }
```

### GET /metrics

Prometheus-compatible text metrics.

```
# HELP metis_customers_total Total customers
# TYPE metis_customers_total gauge
metis_customers_total 42
# HELP metis_jobs_total Total jobs
2.3.0.
```

---

## Job Status Values

| Value | Meaning |
|---|---|
| Scheduled | Job is booked, not yet started |
| In Progress | Technician on-site |
| Completed | Service delivered |
| Cancelled | Job cancelled |

## Invoice Status Values

| Value | Meaning |
|---|---|
| Pending | Invoice sent, payment not received |
| Paid | Payment received |
| Overdue | Past due date, unpaid |
