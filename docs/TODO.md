# TODO - Metis Exterminator Plus

**Version 2.3.0**

---

## Security

- [ ] Change-password REST endpoint (`PUT /api/auth/password`)
- [ ] Per-user role enforcement across all REST endpoints
- [ ] Account lockout after `max_login_attempts` failures
- [ ] Rate limiting on `/api/auth/login`
- [ ] CORS restriction to configured origins
- [ ] Audit log entries written to SQLite (not just log file)
- [ ] API key authentication for machine-to-machine access

## Data Layer

- [ ] Migrate CustomerStore, JobStore, InvoiceStore fully to SQLite
- [ ] Retire flat-file `.dat` stores
- [ ] Full-text search on customers/jobs (SQLite FTS5 already enabled)
- [ ] Soft-delete for customers and jobs
- [ ] CSV export for invoices
- [ ] PDF invoice generation

## Business Features

- [ ] Recurring service scheduling
- [ ] Automatic overdue invoice detection
- [ ] Chemical usage tracking per job
- [ ] Technician time tracking
- [ ] Route optimization
- [ ] Email notifications

## UI / Client

- [ ] Pagination for large lists
- [ ] Calendar view for scheduled jobs
- [ ] Print-optimized invoice layout
- [ ] Dark/light mode respects `client.theme` PSON setting on first load

## Infrastructure

- [ ] GPU acceleration implementation (Windows CUDA/OpenCL, Linux ROCm, macOS Metal)
- [ ] Kubernetes deployment manifest (`k8s/deployment.yaml`)
- [ ] Container image definition (containerd/podman)
- [ ] Log rotation implementation
- [ ] Structured JSON logging to file
- [ ] macOS and Linux OpenSSL prebuilts

## API

- [ ] `POST /api/users` — create user
- [ ] `PUT /api/auth/password` — change password
- [ ] `GET /api/users` — list users (admin only)
- [ ] `DELETE /api/users/:id` — deactivate user

## Documentation

- [ ] OpenAPI specification for all REST endpoints
- [ ] Deployment guide for Linux server (systemd)
- [ ] Deployment guide for Windows Server (NSSM)
