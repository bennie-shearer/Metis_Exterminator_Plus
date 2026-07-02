# Operations - Metis Exterminator Plus

**Version 2.3.0**

---

## Starting the Server

```bash
# Windows
Metis_Exterminator_Plus.exe

# Linux/macOS
./Metis_Exterminator_Plus
```

The server prints startup information to stdout including version, database path, and the URL to open in a browser.

---

## Stopping the Server

Press `Ctrl+C` in the terminal. The server handles SIGINT and SIGTERM, flushes all data stores, closes the database cleanly, and exits.

---

## Data Directory

All persistent data lives in `data/` (exe-relative, configurable in config.pson):

```
data/
  metis_exterminator.db   SQLite database (users, sessions, schema)
  customers.dat            Flat-file customer store (flat-file backup)
  jobs.dat                 Flat-file job store
  invoices.dat             Flat-file invoice store
logs/
  metis_exterminator.log   Application log
```

---

## Backup

### SQLite database (recommended)
Copy `data/metis_exterminator.db` while the server is stopped, or use SQLite's online backup API. The WAL file (`metis_exterminator.db-wal`) must also be copied if present.

### Full data backup
Copy the entire `data/` directory.

### Browser-side backup
Use the Export button in the browser navbar to download a JSON snapshot.

---

## Log Rotation

Log rotation is configured in config.pson under the `logging` section:
- `max_size_mb`: rotate when log file reaches this size
- `max_files`: number of rotated files to retain

Log rotation implementation is on the TODO list. Currently logs accumulate without rotation.

---

## Session Cleanup

A background thread purges expired sessions from the SQLite `sessions` table every 10 minutes. No manual intervention required.

---

## Database Maintenance

```bash
# Compact the database (reclaim space from deleted records)
sqlite3 data/metis_exterminator.db "VACUUM;"

# Check integrity
sqlite3 data/metis_exterminator.db "PRAGMA integrity_check;"

# Export to SQL
sqlite3 data/metis_exterminator.db .dump > backup.sql
```

---

## Running as a Windows Service

Use NSSM (Non-Sucking Service Manager) or the Windows Service Control Manager:

```bash
# Install with NSSM
nssm install MetisExterminator "C:\path\to\Metis_Exterminator_Plus.exe"
nssm set MetisExterminator AppDirectory "C:\path\to\"
nssm start MetisExterminator
```

---

## Running as a Linux systemd Service

Create `/etc/systemd/system/metis-exterminator.service`:

```ini
[Unit]
Description=Metis Exterminator Plus
After=network.target

[Service]
Type=simple
WorkingDirectory=/opt/metis-exterminator-plus
ExecStart=/opt/metis-exterminator-plus/Metis_Exterminator_Plus
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl daemon-reload
sudo systemctl enable metis-exterminator
sudo systemctl start metis-exterminator
sudo systemctl status metis-exterminator
```

---

## Kubernetes Deployment

Kubernetes probes are pre-wired:

```yaml
livenessProbe:
  httpGet:
    path: /healthz
    port: 9100
  initialDelaySeconds: 5
  periodSeconds: 30

readinessProbe:
  httpGet:
    path: /readyz
    port: 9100
  initialDelaySeconds: 3
  periodSeconds: 10
```

Enable Kubernetes features in config.pson when ready:
```
infra {
    kubernetes_enabled = true
    kubernetes_namespace = "metis"
}
```

---

## Monitoring

Prometheus metrics are available at `GET /metrics` in text format. Scrape with:

```yaml
# prometheus.yml
scrape_configs:
  - job_name: metis_exterminator
    static_configs:
      - targets: ['2.3.0.1:9100']
```
