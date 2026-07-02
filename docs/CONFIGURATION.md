# Configuration Reference - Metis Exterminator Plus

**Version 2.3.0**

All configuration lives in `config/config.pson`. Nothing operational is hardcoded in source.

---

## server

| Key | Default | Description |
|---|---|---|
| host | 127.0.0.1 | Bind address |
| port | 9100 | HTTP/HTTPS listen port |
| static_dir | web | Browser client assets (exe-relative) |
| data_dir | data | Flat-file data directory (exe-relative) |
| log_level | info | debug / info / warn / error |
| max_connections | 64 | Maximum simultaneous connections |
| thread_pool_size | 4 | Worker thread count |
| tls_enabled | true | Enable TLS 1.3 |
| tls_cert | tls/server.crt | TLS certificate path (exe-relative) |
| tls_key | tls/server.key | TLS private key path (exe-relative) |

## app

| Key | Default | Description |
|---|---|---|
| name | Metis Exterminator Plus | Application display name |
| version | 2.3.0 | Application version (mirrors VERSION file) |
| company | Your Company Name | Company name shown in UI |
| currency | USD | Currency code |
| invoice_prefix | INV | Invoice number prefix |
| next_invoice_number | 1000 | Starting invoice number |
| session_timeout_minutes | 60 | Session expiry |
| bcrypt_cost | 12 | bcrypt work factor (4-31) |

## database

| Key | Default | Description |
|---|---|---|
| path | data/metis_exterminator.db | SQLite path (exe-relative) |
| wal_mode | true | WAL journal mode |
| cache_size_kb | 4096 | Page cache size |
| busy_timeout_ms | 5000 | Lock wait time |

## tax

| Key | Default | Description |
|---|---|---|
| default_rate | 0.0875 | Tax rate as decimal (8.75% = 0.0875) |
| name | Sales Tax | Tax label on invoices |

## auth

| Key | Default | Description |
|---|---|---|
| admin_user | admin | Admin username |
| admin_password | Admin#2026 | Admin password (change this) |
| admin_role | admin | Admin role |
| require_login | true | Require authentication |
| max_login_attempts | 5 | Failed attempts before lockout |
| lockout_minutes | 15 | Lockout duration |

## infra

| Key | Default | Description |
|---|---|---|
| gpu_enabled | false | GPU acceleration (stub, future) |
| gpu_vendor | auto | auto / nvidia / amd / intel / apple |
| kubernetes_enabled | false | Kubernetes integration (stub, future) |
| kubernetes_namespace | metis | Kubernetes namespace |
| containers_enabled | false | Container runtime (stub, future) |
| container_runtime | none | none / containerd / podman |

## logging

| Key | Default | Description |
|---|---|---|
| level | info | debug / info / warn / error |
| file | logs/metis_exterminator.log | Log file path (exe-relative) |
| max_size_mb | 10 | Rotate at this size |
| max_files | 5 | Rotated files to keep |
| structured | true | JSON-structured log entries |

## session

| Key | Default | Description |
|---|---|---|
| purge_interval_minutes | 10 | Expired session cleanup interval |

## client

| Key | Default | Description |
|---|---|---|
| theme | dark | Default UI theme: dark / light |
| session_storage_key | mep_session_token | Browser sessionStorage key for token |
| theme_storage_key | mep_theme | Browser storage key for theme preference |
