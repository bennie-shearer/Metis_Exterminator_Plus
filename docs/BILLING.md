# Billing - Metis Exterminator Plus

**Version 2.3.0**

---

## Invoice Workflow

```
Job Completed
    --> Create Invoice (select customer, add line items)
        --> Invoice assigned number (INV-NNNNN)
            --> Invoice status: Pending
                --> Email or print invoice to customer
                    --> Payment received
                        --> Mark Paid
                            --> Invoice status: Paid
```

---

## Invoice Numbering

Invoice numbers are generated sequentially using the format `PREFIX-NNNNN` where:
- `PREFIX` is configured in `config.pson` under `app.invoice_prefix` (default: `INV`)
- `NNNNN` is a zero-padded 5-digit number starting from `app.next_invoice_number` (default: 1000)

Example: `INV-01000`, `INV-01001`, `INV-01002`

To use a company-specific prefix (e.g. `APC` for Acme Pest Control):
```
app {
    invoice_prefix = "APC"
    next_invoice_number = 1
}
```
This produces `APC-00001`, `APC-00002`, etc.

---

## Tax Calculation

Tax is calculated automatically when an invoice is created:

```
subtotal  = sum(unit_price * quantity) for all line items
tax_rate  = app.tax_rate from config.pson (default 0.0875 = 8.75%)
tax_amount = subtotal * tax_rate
total     = subtotal + tax_amount
```

The tax rate at the time of invoice creation is stored with the invoice record. Changing the tax rate in config.pson only affects new invoices.

---

## Invoice Statuses

| Status | Meaning | Transitions |
|---|---|---|
| Pending | Invoice created, payment not received | --> Paid |
| Paid | Payment received and recorded | Terminal |
| Overdue | Past due date, payment not received | --> Paid |

Overdue status is currently set manually (mark via PUT /api/invoices/:id with `{"status":"Overdue"}`). Automatic overdue detection is on the TODO list.

---

## Revenue Reporting

The Dashboard stat cards show:
- **Revenue collected**: sum of all Paid invoice totals
- **Outstanding**: sum of all Pending invoice totals

These figures update in real time when the dashboard is loaded.

---

## Line Items

Each invoice supports unlimited line items. Common line items for pest control:

| Description | Unit Price | Quantity |
|---|---|---|
| Initial inspection | 75.00 | 1 |
| General ant treatment | 125.00 | 1 |
| Bait station installation | 12.50 | 4 |
| Follow-up visit | 65.00 | 1 |
| Termite treatment (per linear ft) | 8.00 | 45 |

---

## Printing Invoices

The invoice detail modal (click View on any invoice) is designed for screen-readable display. For a printed invoice, use Chrome's print function (Ctrl+P) with the modal open, or select "Print" and adjust margins to fit.

A dedicated print stylesheet and PDF export are on the TODO list.

---

## Data Backup for Accounting

For integration with external accounting software:
1. Use the **Export** button to download a JSON file
2. The JSON contains all invoices with full line item detail, subtotals, tax, and totals
3. Import or map the JSON to your accounting system manually

A CSV export for invoices is on the TODO list.
