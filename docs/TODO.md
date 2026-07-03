# TODO - Metis Exterminator Plus

**Version 3.1.0**

---

## Security (all done in v3.1.0)
- [x] Change-password REST endpoint
- [x] Per-user role enforcement
- [x] Account lockout
- [x] Rate limiting on login
- [x] CORS restriction
- [x] Audit log to SQLite
- [x] API key authentication

## Data Layer
- [x] Migrate CustomerStore, JobStore, InvoiceStore fully to SQLite (retire flat .dat files)
- [x] Soft-delete UI for customers and jobs (schema ready)
- [x] PDF invoice generation

## Business Features
- [x] Recurring job scheduling UI (schema ready)
- [x] Chemical usage tracking UI (schema and API ready)
- [x] Technician time tracking UI (schema ready)
- [x] Route optimization
- [x] Email notifications (SMTP config in PSON)

## UI / Client
- [x] Pagination for large lists
- [x] Print-optimized invoice layout

## Infrastructure
- [ ] GPU acceleration (Windows CUDA/OpenCL, Linux ROCm, macOS Metal)
- [ ] Kubernetes deployment manifests
- [ ] Container image (containerd/podman)
- [ ] Structured JSON log file output
