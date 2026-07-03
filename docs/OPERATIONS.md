# Operations - Metis Exterminator Plus

**Version 3.1.0**

---

## Starting the Server

```
Metis_Exterminator_Plus.exe
```

The server reads `config/config.pson` from the same directory as the executable,
opens the SQLite database, resets admin credentials from PSON, and begins listening.

Console output shows the exact URL to open in your browser.

---

## Windows Deployment (NSSM)

[Non-Sucking Service Manager](https://nssm.cc/) installs the server as a Windows service.

```cmd
nssm install MetisExterminator "C:\Metis\Metis_Exterminator_Plus.exe"
nssm set MetisExterminator AppDirectory "C:\Metis"
nssm set MetisExterminator AppStdout "C:\Metis\logs\service-out.log"
nssm set MetisExterminator AppStderr "C:\Metis\logs\service-err.log"
nssm set MetisExterminator Start SERVICE_AUTO_START
nssm start MetisExterminator
```

To stop or remove:
```cmd
nssm stop MetisExterminator
nssm remove MetisExterminator confirm
```

---

## Linux Deployment (systemd)

Create `/etc/systemd/system/metis-exterminator.service`:

```ini
[Unit]
Description=Metis Exterminator Plus
After=network.target

[Service]
Type=simple
User=metis
WorkingDirectory=/opt/metis-exterminator
ExecStart=/opt/metis-exterminator/Metis_Exterminator_Plus
Restart=on-failure
RestartSec=5
StandardOutput=append:/opt/metis-exterminator/logs/service.log
StandardError=append:/opt/metis-exterminator/logs/service.log

[Install]
WantedBy=multi-user.target
```

```bash
sudo useradd -r -s /bin/false metis
sudo chown -R metis:metis /opt/metis-exterminator
sudo systemctl daemon-reload
sudo systemctl enable metis-exterminator
sudo systemctl start metis-exterminator
sudo systemctl status metis-exterminator
```

---

## Backup

The entire database is a single file: `data/metis_exterminator.db`.
Copy it to back up all data. SQLite WAL mode ensures consistency during copy.

```bash
# Linux
cp data/metis_exterminator.db backups/metis_$(date +%Y%m%d).db

# Windows PowerShell
Copy-Item data\metis_exterminator.db backups\metis_$(Get-Date -Format yyyyMMdd).db
```

---

## Log Rotation

Manual rotation via API (admin only):
```
POST /api/admin/rotate-log
Authorization: Bearer <token>
```

Or call it from the browser: Admin → System Log → Rotate Log button.

Automatic rotation: configure `logging.max_size_mb` and `logging.max_files` in `config.pson`.
Implementation of size-triggered rotation is planned for v3.1.0.

---

## Certificate Management

TLS certificate lives in `tls/server.crt` and `tls/server.key`.
Paths are configurable in `config.pson` under `server.tls_cert` and `server.tls_key`.

To regenerate a self-signed certificate:
```bash
openssl req -x509 -newkey rsa:4096 -keyout tls/server.key -out tls/server.crt \
  -days 365 -nodes -subj "/CN=127.0.0.1" \
  -addext "subjectAltName=IP:127.0.0.1"
```

For production, replace with a certificate from a trusted CA.

---

## Health Checks

- `GET /healthz` — liveness probe (always 200 if server is up)
- `GET /readyz` — readiness probe (200 when data stores are loaded)
- `GET /metrics` — Prometheus-compatible metrics

---

## Changing Admin Password

Edit `config/config.pson`:
```
auth {
    admin_password = "NewSecurePassword#2026"
}
```
Restart the server. The admin account is reset from PSON on every startup.
