# How-To Guide - Metis Exterminator Plus

**Version 2.3.0**

---

## Add Your First Customer

1. Open `http://2.3.0.1:9100` in Chrome
2. Click **Customers** in the navbar
3. Click **+ New customer**
4. Fill in name (required), phone, email, address
5. Click **Save**

The customer appears in the customer list immediately.

---

## Schedule a Job

1. Click **Jobs** in the navbar
2. Click **+ New job**
3. Select the customer from the dropdown
4. Choose service type and pest type
5. Set the scheduled date and time
6. Assign a technician
7. Enter the job price
8. Click **Save**

The job appears on the dashboard under Upcoming Jobs and in the Jobs list.

---

## Mark a Job Complete

1. Click **Jobs**
2. Find the job, click **Edit**
3. Change Status to **Completed**
4. Click **Save**

---

## Create an Invoice

1. Click **Invoices**
2. Click **+ New invoice**
3. Select the customer
4. Set issue and due dates
5. Add line items (description, unit price, quantity)
6. The subtotal, tax, and total update automatically
7. Click **Save**

The invoice number is assigned automatically (e.g. INV-01000).

---

## Record a Payment

1. Click **Invoices**
2. Find the invoice
3. Click **Mark paid**

The invoice status changes to Paid and the payment date is recorded.

---

## Back Up Your Data

**Standalone HTML (
1. Click the **Export** button in the top-right navbar
2. A JSON file downloads to your Downloads folder
3. Optionally move it to Google Drive for cloud backup

**Server mode:**
- Copy the file `data/metis_exterminator.db` to a safe location
- This single file contains all customers, jobs, invoices, users, and sessions

---

## Restore from Backup

**Standalone HTML:**
1. Click the **Import** button in the navbar
2. Select the `.json` backup file
3. Confirm the import in the dialog
4. All data is restored with correct ID sequences

**Server mode:**
1. Stop the server
2. Replace `data/metis_exterminator.db` with your backup copy
3. Restart the server

---

## Change the Tax Rate

Edit `config/config.pson`:
```
tax {
    default_rate = 0.07
    name = "State Tax"
}
```
Restart the server. New invoices will use the new rate. Existing invoices retain the rate at which they were created.

---

## Change the Admin Password

Currently: delete the `users` table row for admin and restart the server (the bootstrap will recreate it and prompt for a new password at startup). A change-password endpoint is on the TODO list.

---

## Add a New Technician

Edit `web/js/app.js`, find the `TECHNICIANS` array near the top, and add the name:
```javascript
const TECHNICIANS = ['Mike Johnson','Sara Lee','Tom Rivera','New Name','Unassigned'];
```
Rebuild the .

---

## Run on a Non-Default Port

Edit `config/config.pson`:
```
server {
    port = 9200
}
```
Restart the server and open `http://2.3.0.1:9200`.

---

## Check System Health

- Liveness: `http://2.3.0.1:9100/healthz`
- Readiness: `http://2.3.0.1:9100/readyz`
- Metrics: `http://2.3.0.1:9100/metrics`
