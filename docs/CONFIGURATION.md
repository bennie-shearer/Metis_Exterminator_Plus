# Configuration Reference - Metis Exterminator Plus

**Version 3.1.0** | File: `config/config.pson`

All operational values are read from `config/config.pson`. Nothing is hardcoded in source.

---

## server
| Key | Default | Description |
|---|---|---|
| host | 127.0.0.1 | Bind address |
| port | 9100 | HTTPS listen port |
| static_dir | web | Browser client assets |
| data_dir | data | Flat-file data directory |
| max_connections | 64 | Maximum simultaneous connections |
| thread_pool_size | 4 | Worker threads |
| tls_enabled | true | Enable TLS 1.3 |
| tls_cert | tls/server.crt | Certificate path |
| tls_key | tls/server.key | Private key path |

## app
| Key | Default | Description |
|---|---|---|
| name | Metis Exterminator Plus | Application display name |
| version | 3.0.0 | Mirrors VERSION file |
| company | Your Company Name | Company name in UI |
| currency | USD | Currency code |
| invoice_prefix | INV | Invoice number prefix |
| next_invoice_number | 1000 | Starting invoice number |
| session_timeout_minutes | 60 | Session expiry |
| bcrypt_cost | 12 | bcrypt work factor (4-31) |

## database
| Key | Default | Description |
|---|---|---|
| path | data/metis_exterminator.db | SQLite file path |
| wal_mode | true | WAL journal mode |
| cache_size_kb | 4096 | Page cache |
| busy_timeout_ms | 5000 | Lock wait timeout |

## tax
| Key | Default | Description |
|---|---|---|
| default_rate | 0.0875 | Tax rate (8.75% = 0.0875) |
| name | Sales Tax | Tax label on invoices |

## auth
| Key | Default | Description |
|---|---|---|
| admin_user | admin | Admin username |
| admin_password | Admin#2026 | Admin password — change this |
| admin_role | admin | Admin role |
| require_login | true | Require authentication |
| max_login_attempts | 5 | Failures before lockout |
| lockout_minutes | 15 | Lockout duration |

## security
| Key | Default | Description |
|---|---|---|
| allowed_origins | https://127.0.0.1:9100 | CORS allowed origins |
| api_key_enabled | false | Enable API key auth |
| api_key_header | X-API-Key | API key HTTP header name |
| rate_limit_login_per_minute | 10 | Max login attempts per IP per minute |

## infra
| Key | Default | Description |
|---|---|---|
| gpu_enabled | false | GPU acceleration (stub) |
| gpu_vendor | auto | auto / nvidia / amd / intel / apple |
| kubernetes_enabled | false | Kubernetes (stub) |
| kubernetes_namespace | metis | K8s namespace |
| containers_enabled | false | Container runtime (stub) |
| container_runtime | none | none / containerd / podman |

## logging
| Key | Default | Description |
|---|---|---|
| level | info | debug / info / warn / error |
| file | logs/metis_exterminator.log | Log file path |
| max_size_mb | 10 | Rotate at this size |
| max_files | 5 | Rotated files to keep |
| structured | true | JSON-structured entries |
| audit_to_sqlite | true | Write audit events to SQLite |

## session
| Key | Default | Description |
|---|---|---|
| purge_interval_minutes | 10 | Expired session cleanup interval |

## client
| Key | Default | Description |
|---|---|---|
| theme | dark | Default UI theme: dark / light |
| session_storage_key | mep_session_token | Browser sessionStorage key |
| theme_storage_key | mep_theme | Browser theme storage key |

## email
| Key | Default | Description |
|---|---|---|
| enabled | false | Enable email notifications |
| smtp_host | smtp.example.com | SMTP server host |
| smtp_port | 587 | SMTP port |
| smtp_user | (empty) | SMTP username |
| smtp_password | (empty) | SMTP password |
| from_address | noreply@example.com | From address |

## features
| Key | Default | Description |
|---|---|---|
| recurring_jobs | true | Recurring service scheduling |
| chemical_tracking | true | Chemical usage per job |
| technician_time_tracking | true | Time start/end per job |
| pdf_export | false | PDF invoice generation |
| csv_export | true | CSV data export |
| route_optimization | false | Route optimization |
