'use strict';

const PEST_TYPES    = ['Ants','Bed Bugs','Bees/Wasps','Cockroaches','Fleas','Flies','Mice/Rats','Mosquitoes','Moths','Silverfish','Spiders','Termites','Ticks','Wildlife','Other'];
const SERVICE_TYPES = ['Initial Inspection','General Treatment','Follow-up Treatment','Preventive Service','Emergency Service','Exclusion Service'];
const TECHNICIANS   = ['Mike Johnson','Sara Lee','Tom Rivera','Unassigned'];

function fmt_currency(n) { return '$' + Number(n||0).toFixed(2).replace(/\B(?=(\d{3})+(?!\d))/g,','); }
function fmt_date(s) { if (!s) return ''; const d = new Date(s); return isNaN(d.getTime()) ? s : d.toLocaleDateString('en-US',{month:'short',day:'numeric',year:'numeric'}); }
function today_str() { return new Date().toISOString().slice(0,10); }
function due_str()   { const d = new Date(); d.setDate(d.getDate()+30); return d.toISOString().slice(0,10); }
function sel(id)     { return document.getElementById(id); }

function toast(msg, err) {
  const el = sel('toast');
  el.textContent = msg;
  el.className = 'toast' + (err ? ' error' : '');
  clearTimeout(el._t);
  el._t = setTimeout(() => { el.className = 'toast hidden'; }, 3500);
}

function badge(status) {
  const map = {
    'Scheduled':'badge-scheduled','In Progress':'badge-in-progress',
    'Completed':'badge-completed','Cancelled':'badge-cancelled',
    'Pending':'badge-pending','Paid':'badge-paid','Overdue':'badge-overdue'
  };
  return `<span class="badge ${map[status]||''}">${status}</span>`;
}

let _customers_cache = [];
async function refreshCustomerCache() { _customers_cache = await API.customers(); }
function customerName(id) {
  const c = _customers_cache.find(x => x.id === id);
  return c ? c.name : ('Customer #' + id);
}

const Modal = {
  _onSave: null,
  show(title, bodyHtml, onSave) {
    sel('modal-title').textContent = title;
    sel('modal-body').innerHTML = bodyHtml;
    sel('modal-footer').style.display = onSave ? '' : 'none';
    sel('modal-overlay').classList.remove('hidden');
    this._onSave = onSave;
  },
  hide() { sel('modal-overlay').classList.add('hidden'); this._onSave = null; },
  async save() { if (this._onSave) await this._onSave(); }
};

sel('modal-close').onclick  = () => Modal.hide();
sel('modal-cancel').onclick = () => Modal.hide();
sel('modal-overlay').onclick = (e) => { if (e.target === sel('modal-overlay')) Modal.hide(); };
sel('modal-save').onclick   = () => Modal.save();

document.querySelectorAll('.nav-btn').forEach(btn => {
  btn.onclick = () => {
    document.querySelectorAll('.nav-btn').forEach(b => b.classList.remove('active'));
    btn.classList.add('active');
    document.querySelectorAll('.view').forEach(v => v.classList.remove('active'));
    sel('view-' + btn.dataset.view).classList.add('active');
    loadView(btn.dataset.view);
  };
});

async function loadView(view) {
  if      (view === 'dashboard') await loadDashboard();
  else if (view === 'customers') await loadCustomers();
  else if (view === 'jobs')      await loadJobs();
  else if (view === 'invoices')  await loadInvoices();
  else if (view === 'admin')     await loadAdmin();
  else if (view === 'syslog')    await loadSyslog();
  else if (view === 'auditlog')  await loadAuditlog();
}

// ===== DASHBOARD =====
async function loadDashboard() {
  try {
    const s = await API.stats();
    sel('stat-customers').textContent  = s.customers;
    sel('stat-scheduled').textContent  = s.jobs_scheduled;
    sel('stat-completed').textContent  = s.jobs_completed;
    sel('stat-revenue').textContent    = fmt_currency(s.revenue);
    sel('stat-outstanding').textContent = fmt_currency(s.outstanding);
  } catch(e) { showError('stat-customers', e); }
  try {
    await refreshCustomerCache();
    const jobs = await API.jobs({status:'Scheduled'});
    const upcoming = sel('upcoming-jobs');
    if (!jobs.length) {
      upcoming.innerHTML = '<div class="empty-state"><strong>No scheduled jobs</strong><p>Add a job to see it here.</p></div>';
      return;
    }
    jobs.sort((a,b) => a.scheduled_date.localeCompare(b.scheduled_date));
    upcoming.innerHTML = `<table class="data-table">
      <thead><tr><th>Date</th><th>Customer</th><th>Service</th><th>Pest</th><th>Technician</th></tr></thead>
      <tbody>${jobs.slice(0,10).map(j => `<tr>
        <td>${fmt_date(j.scheduled_date)}</td>
        <td>${customerName(j.customer_id)}</td>
        <td>${j.service_type}</td><td>${j.pest_type}</td>
        <td>${j.technician||'—'}</td>
      </tr>`).join('')}</tbody>
    </table>`;
  } catch(e) { sel('upcoming-jobs').innerHTML = serverError(e); }
}

function showError(id, e) { const el = sel(id); if (el) el.textContent = '—'; }
function serverError(e) { return `<div class="empty-state"><strong>Cannot connect to server</strong><p>${e.message||'Ensure Metis_Exterminator_Plus.exe is running.'}</p></div>`; }

// ===== CUSTOMERS =====
async function loadCustomers(q) {
  try {
    await refreshCustomerCache();
    const list = q ? _customers_cache.filter(c => {
      const lq = q.toLowerCase();
      return c.name.toLowerCase().includes(lq) || (c.phone||'').includes(lq) || (c.email||'').toLowerCase().includes(lq);
    }) : _customers_cache;
    const el = sel('customers-list');
    if (!list.length) { el.innerHTML = '<div class="empty-state"><strong>No customers yet</strong><p>Add your first customer.</p></div>'; return; }
    el.innerHTML = `<table class="data-table">
      <thead><tr><th>Name</th><th>Phone</th><th>Email</th><th>City</th><th>Actions</th></tr></thead>
      <tbody>${list.map(c => `<tr>
        <td><strong>${c.name}</strong></td><td>${c.phone||'—'}</td>
        <td>${c.email||'—'}</td>
        <td>${c.city ? c.city+(c.state?', '+c.state:'') : '—'}</td>
        <td><div class="actions">
          <button class="btn-secondary btn-sm" onclick="editCustomer(${c.id})">Edit</button>
          <button class="btn-secondary btn-sm" onclick="viewCustomerJobs(${c.id},'${c.name.replace(/'/g,'\\x27')}')">Jobs</button>
          <button class="btn-danger btn-sm" onclick="deleteCustomer(${c.id})">Delete</button>
        </div></td>
      </tr>`).join('')}</tbody>
    </table>`;
  } catch(e) { sel('customers-list').innerHTML = serverError(e); }
}

sel('customer-search').oninput = (e) => loadCustomers(e.target.value.trim());

function customerFormHtml(c) {
  c = c||{};
  return `
    <div class="form-row"><label>Name *</label><input id="f-name" type="text" value="${c.name||''}"></div>
    <div class="form-row-2">
      <div class="form-row"><label>Phone</label><input id="f-phone" type="tel" value="${c.phone||''}"></div>
      <div class="form-row"><label>Email</label><input id="f-email" type="email" value="${c.email||''}"></div>
    </div>
    <div class="form-row"><label>Address</label><input id="f-address" type="text" value="${c.address||''}"></div>
    <div class="form-row-3">
      <div class="form-row"><label>City</label><input id="f-city" type="text" value="${c.city||''}"></div>
      <div class="form-row"><label>State</label><input id="f-state" type="text" value="${c.state||''}" maxlength="2"></div>
      <div class="form-row"><label>ZIP</label><input id="f-zip" type="text" value="${c.zip||''}"></div>
    </div>
    <div class="form-row"><label>Notes</label><textarea id="f-notes">${c.notes||''}</textarea></div>`;
}

sel('btn-new-customer').onclick = () => {
  Modal.show('New customer', customerFormHtml(), async () => {
    const name = sel('f-name').value.trim();
    if (!name) { toast('Name is required', true); return; }
    try {
      await API.addCustomer({name, phone:sel('f-phone').value.trim(), email:sel('f-email').value.trim(), address:sel('f-address').value.trim(), city:sel('f-city').value.trim(), state:sel('f-state').value.trim(), zip:sel('f-zip').value.trim(), notes:sel('f-notes').value.trim()});
      Modal.hide(); toast('Customer added'); await loadCustomers();
    } catch(e) { toast('Save failed: ' + e.message, true); }
  });
};

window.editCustomer = async (id) => {
  const c = await API.customer(id);
  if (!c) return;
  Modal.show('Edit customer', customerFormHtml(c), async () => {
    const name = sel('f-name').value.trim();
    if (!name) { toast('Name is required', true); return; }
    try {
      await API.updateCustomer(id, {name, phone:sel('f-phone').value.trim(), email:sel('f-email').value.trim(), address:sel('f-address').value.trim(), city:sel('f-city').value.trim(), state:sel('f-state').value.trim(), zip:sel('f-zip').value.trim(), notes:sel('f-notes').value.trim()});
      Modal.hide(); toast('Customer updated'); await loadCustomers();
    } catch(e) { toast('Save failed', true); }
  });
};

window.deleteCustomer = async (id) => {
  if (!confirm('Delete this customer?')) return;
  try { await API.deleteCustomer(id); toast('Customer deleted'); await loadCustomers(); }
  catch(e) { toast('Delete failed', true); }
};

window.viewCustomerJobs = async (id, name) => {
  const jobs = await API.jobs({customer_id:id});
  const html = jobs.length ? `<table class="data-table">
    <thead><tr><th>Date</th><th>Service</th><th>Status</th><th>Price</th></tr></thead>
    <tbody>${jobs.map(j=>`<tr><td>${fmt_date(j.scheduled_date)}</td><td>${j.service_type}</td><td>${badge(j.status)}</td><td>${fmt_currency(j.price)}</td></tr>`).join('')}</tbody>
  </table>` : '<div class="empty-state"><strong>No jobs for this customer</strong></div>';
  Modal.show(`Jobs for ${name}`, html, null);
};

// ===== JOBS =====
async function loadJobs() {
  try {
    await refreshCustomerCache();
    const status = sel('job-status-filter').value;
    const jobs = await API.jobs(status ? {status} : {});
    const el = sel('jobs-list');
    if (!jobs.length) { el.innerHTML = '<div class="empty-state"><strong>No jobs found</strong><p>Create a job to get started.</p></div>'; return; }
    jobs.sort((a,b) => b.scheduled_date.localeCompare(a.scheduled_date));
    el.innerHTML = `<table class="data-table">
      <thead><tr><th>Date</th><th>Customer</th><th>Service</th><th>Pest</th><th>Tech</th><th>Status</th><th>Price</th><th>Actions</th></tr></thead>
      <tbody>${jobs.map(j=>`<tr>
        <td>${fmt_date(j.scheduled_date)}</td>
        <td>${customerName(j.customer_id)}</td>
        <td>${j.service_type}</td><td>${j.pest_type}</td>
        <td>${j.technician||'—'}</td><td>${badge(j.status)}</td>
        <td>${fmt_currency(j.price)}</td>
        <td><div class="actions">
          <button class="btn-secondary btn-sm" onclick="editJob(${j.id})">Edit</button>
          <button class="btn-danger btn-sm" onclick="deleteJob(${j.id})">Delete</button>
        </div></td>
      </tr>`).join('')}</tbody>
    </table>`;
  } catch(e) { sel('jobs-list').innerHTML = serverError(e); }
}

sel('job-status-filter').onchange = () => loadJobs();

function jobFormHtml(j, customers) {
  j = j||{};
  const co = customers.map(c=>`<option value="${c.id}" ${j.customer_id===c.id?'selected':''}>${c.name}</option>`).join('');
  const so = SERVICE_TYPES.map(s=>`<option ${j.service_type===s?'selected':''}>${s}</option>`).join('');
  const po = PEST_TYPES.map(p=>`<option ${j.pest_type===p?'selected':''}>${p}</option>`).join('');
  const to = TECHNICIANS.map(t=>`<option ${j.technician===t?'selected':''}>${t}</option>`).join('');
  const sto = ['Scheduled','In Progress','Completed','Cancelled'].map(s=>`<option ${j.status===s?'selected':''}>${s}</option>`).join('');
  return `
    <div class="form-row"><label>Customer *</label><select id="f-cust"><option value="">Select customer</option>${co}</select></div>
    <div class="form-row-2">
      <div class="form-row"><label>Service type *</label><select id="f-svc">${so}</select></div>
      <div class="form-row"><label>Pest type</label><select id="f-pest">${po}</select></div>
    </div>
    <div class="form-row-2">
      <div class="form-row"><label>Scheduled date</label><input id="f-date" type="date" value="${j.scheduled_date||today_str()}"></div>
      <div class="form-row"><label>Time</label><input id="f-time" type="time" value="${j.scheduled_time||'08:00'}"></div>
    </div>
    <div class="form-row-2">
      <div class="form-row"><label>Technician</label><select id="f-tech">${to}</select></div>
      <div class="form-row"><label>Price ($)</label><input id="f-price" type="number" step="0.01" min="0" value="${j.price||''}"></div>
    </div>
    <div class="form-row"><label>Service address</label><input id="f-addr" type="text" value="${j.address||''}"></div>
    <div class="form-row"><label>Status</label><select id="f-status">${sto}</select></div>
    <div class="form-row"><label>Notes</label><textarea id="f-notes">${j.notes||''}</textarea></div>`;
}

sel('btn-new-job').onclick = async () => {
  await refreshCustomerCache();
  Modal.show('New job', jobFormHtml(null, _customers_cache), async () => {
    const cid = parseInt(sel('f-cust').value);
    if (!cid) { toast('Select a customer', true); return; }
    try {
      await API.addJob({customer_id:cid, service_type:sel('f-svc').value, pest_type:sel('f-pest').value, scheduled_date:sel('f-date').value, scheduled_time:sel('f-time').value, technician:sel('f-tech').value, price:parseFloat(sel('f-price').value)||0, address:sel('f-addr').value.trim(), status:sel('f-status').value, notes:sel('f-notes').value.trim()});
      Modal.hide(); toast('Job added'); await loadJobs();
    } catch(e) { toast('Save failed', true); }
  });
};

window.editJob = async (id) => {
  await refreshCustomerCache();
  const j = await API.job(id);
  if (!j) return;
  Modal.show('Edit job', jobFormHtml(j, _customers_cache), async () => {
    try {
      await API.updateJob(id, {service_type:sel('f-svc').value, pest_type:sel('f-pest').value, scheduled_date:sel('f-date').value, scheduled_time:sel('f-time').value, technician:sel('f-tech').value, price:parseFloat(sel('f-price').value)||0, address:sel('f-addr').value.trim(), status:sel('f-status').value, notes:sel('f-notes').value.trim()});
      Modal.hide(); toast('Job updated'); await loadJobs();
    } catch(e) { toast('Save failed', true); }
  });
};

window.deleteJob = async (id) => {
  if (!confirm('Delete this job?')) return;
  try { await API.deleteJob(id); toast('Job deleted'); await loadJobs(); }
  catch(e) { toast('Delete failed', true); }
};

// ===== INVOICES =====
async function loadInvoices() {
  try {
    await refreshCustomerCache();
    const invs = await API.invoices();
    const el = sel('invoices-list');
    if (!invs.length) { el.innerHTML = '<div class="empty-state"><strong>No invoices yet</strong><p>Create an invoice to bill customers.</p></div>'; return; }
    invs.sort((a,b) => b.created_at.localeCompare(a.created_at));
    el.innerHTML = `<table class="data-table">
      <thead><tr><th>Invoice #</th><th>Customer</th><th>Date</th><th>Due</th><th>Total</th><th>Status</th><th>Actions</th></tr></thead>
      <tbody>${invs.map(inv=>`<tr>
        <td><strong>${inv.invoice_number}</strong></td>
        <td>${customerName(inv.customer_id)}</td>
        <td>${fmt_date(inv.issued_date)}</td><td>${fmt_date(inv.due_date)}</td>
        <td>${fmt_currency(inv.total)}</td><td>${badge(inv.status)}</td>
        <td><div class="actions">
          <button class="btn-secondary btn-sm" onclick="viewInvoice(${inv.id})">View</button>
          ${inv.status==='Pending'?`<button class="btn-primary btn-sm" onclick="markPaid(${inv.id})">Mark paid</button>`:''}
          <button class="btn-danger btn-sm" onclick="deleteInvoice(${inv.id})">Delete</button>
        </div></td>
      </tr>`).join('')}</tbody>
    </table>`;
  } catch(e) { sel('invoices-list').innerHTML = serverError(e); }
}

function buildItemsHtml(items) {
  items = items&&items.length ? items : [{description:'',unit_price:'',quantity:1}];
  return items.map((it,i)=>`
    <div class="invoice-item-row" id="item-row-${i}">
      <input type="text" placeholder="Description" value="${it.description||''}" class="item-desc" data-i="${i}">
      <input type="number" placeholder="Price" step="0.01" min="0" value="${it.unit_price||''}" class="item-price" data-i="${i}">
      <input type="number" placeholder="Qty" min="1" value="${it.quantity||1}" class="item-qty" data-i="${i}">
      <button class="btn-icon" onclick="removeItemRow(${i})" title="Remove">&times;</button>
    </div>`).join('');
}

function invoiceFormHtml(inv, customers) {
  inv = inv||{};
  const co = customers.map(c=>`<option value="${c.id}" ${inv.customer_id===c.id?'selected':''}>${c.name}</option>`).join('');
  return `
    <div class="form-row"><label>Customer *</label><select id="f-cust"><option value="">Select customer</option>${co}</select></div>
    <div class="form-row-2">
      <div class="form-row"><label>Issue date</label><input id="f-issued" type="date" value="${inv.issued_date||today_str()}"></div>
      <div class="form-row"><label>Due date</label><input id="f-due" type="date" value="${inv.due_date||due_str()}"></div>
    </div>
    <div class="form-row"><label>Line items</label>
      <div class="invoice-items" id="invoice-items">${buildItemsHtml(inv.items)}</div>
      <button class="btn-add-item" onclick="addItemRow()">+ Add line item</button>
    </div>
    <div class="invoice-totals" id="inv-totals">
      <div class="total-row"><span>Subtotal</span><span id="inv-subtotal">$0.00</span></div>
      <div class="total-row"><span>Tax</span><span id="inv-tax">$0.00</span></div>
      <div class="total-row total-final"><span>Total</span><span id="inv-total">$0.00</span></div>
    </div>
    <div class="form-row"><label>Notes</label><textarea id="f-notes">${inv.notes||''}</textarea></div>`;
}

function recalcTotals() {
  let sub = 0;
  document.querySelectorAll('.invoice-item-row').forEach(row => {
    sub += (parseFloat(row.querySelector('.item-price').value)||0) * (parseInt(row.querySelector('.item-qty').value)||1);
  });
  const tax = sub * (_tax_rate || 0.0875);
  sel('inv-subtotal').textContent = fmt_currency(sub);
  sel('inv-tax').textContent      = fmt_currency(tax);
  sel('inv-total').textContent    = fmt_currency(sub + tax);
}

window.addItemRow = () => {
  const container = sel('invoice-items');
  const i = container.querySelectorAll('.invoice-item-row').length;
  const div = document.createElement('div');
  div.className = 'invoice-item-row'; div.id = 'item-row-' + i;
  div.innerHTML = `<input type="text" placeholder="Description" class="item-desc" data-i="${i}">
    <input type="number" placeholder="Price" step="0.01" min="0" class="item-price" data-i="${i}">
    <input type="number" placeholder="Qty" min="1" value="1" class="item-qty" data-i="${i}">
    <button class="btn-icon" onclick="removeItemRow(${i})" title="Remove">&times;</button>`;
  container.appendChild(div);
  div.querySelectorAll('input').forEach(el => el.oninput = recalcTotals);
};

window.removeItemRow = (i) => { const r = sel('item-row-'+i); if(r){r.remove();recalcTotals();} };

function gatherItems() {
  const rows = document.querySelectorAll('.invoice-item-row');
  return Array.from(rows).map(row => ({
    description: row.querySelector('.item-desc').value.trim(),
    unit_price:  parseFloat(row.querySelector('.item-price').value)||0,
    quantity:    parseInt(row.querySelector('.item-qty').value)||1
  })).filter(it => it.description);
}

sel('btn-new-invoice').onclick = async () => {
  await refreshCustomerCache();
  Modal.show('New invoice', invoiceFormHtml(null, _customers_cache), async () => {
    const cid = parseInt(sel('f-cust').value);
    if (!cid) { toast('Select a customer', true); return; }
    const items = gatherItems();
    if (!items.length) { toast('Add at least one line item', true); return; }
    try {
      await API.addInvoice({customer_id:cid, issued_date:sel('f-issued').value, due_date:sel('f-due').value, notes:sel('f-notes').value.trim(), items});
      Modal.hide(); toast('Invoice created'); await loadInvoices();
    } catch(e) { toast('Save failed', true); }
  });
  setTimeout(() => {
    document.querySelectorAll('.item-price,.item-qty').forEach(el => el.oninput = recalcTotals);
  }, 50);
};

window.viewInvoice = async (id) => {
  await refreshCustomerCache();
  const inv = await API.invoice(id);
  if (!inv) return;
  const html = `
    <div style="display:flex;justify-content:space-between;margin-bottom:1rem">
      <div><strong>${inv.invoice_number}</strong><br><span style="font-size:13px;color:var(--text-muted)">Issued ${fmt_date(inv.issued_date)} &bull; Due ${fmt_date(inv.due_date)}</span></div>
      <div>${badge(inv.status)}</div>
    </div>
    <div style="margin-bottom:1rem"><strong>Bill to:</strong> ${customerName(inv.customer_id)}</div>
    <table class="data-table" style="margin-bottom:1rem">
      <thead><tr><th>Description</th><th style="text-align:right">Price</th><th style="text-align:center">Qty</th><th style="text-align:right">Amount</th></tr></thead>
      <tbody>${(inv.items||[]).map(it=>`<tr>
        <td>${it.description}</td>
        <td style="text-align:right">${fmt_currency(it.unit_price)}</td>
        <td style="text-align:center">${it.quantity}</td>
        <td style="text-align:right">${fmt_currency(it.unit_price*it.quantity)}</td>
      </tr>`).join('')}</tbody>
    </table>
    <div class="invoice-totals">
      <div class="total-row"><span>Subtotal</span><span>${fmt_currency(inv.subtotal)}</span></div>
      <div class="total-row"><span>Tax (${((inv.tax_rate||0.0875)*100).toFixed(2)}%)</span><span>${fmt_currency(inv.tax_amount)}</span></div>
      <div class="total-row total-final"><span>Total</span><span>${fmt_currency(inv.total)}</span></div>
    </div>
    ${inv.notes?`<div style="margin-top:1rem;font-size:13px;color:var(--text-muted)">Notes: ${inv.notes}</div>`:''}
    ${inv.paid_date?`<div style="margin-top:.5rem;font-size:13px;color:var(--text-secondary)">Paid ${fmt_date(inv.paid_date)}</div>`:''}`;
  Modal.show('Invoice ' + inv.invoice_number, html, null);
};

window.markPaid = async (id) => {
  try { await API.updateInvoice(id, {status:'Paid', paid_date:today_str()}); toast('Invoice marked paid'); await loadInvoices(); }
  catch(e) { toast('Update failed', true); }
};

window.deleteInvoice = async (id) => {
  if (!confirm('Delete this invoice?')) return;
  try { await API.deleteInvoice(id); toast('Invoice deleted'); await loadInvoices(); }
  catch(e) { toast('Delete failed', true); }
};

// ===== SYSTEM LOG & AUDIT LOG =====
function colorize_log_line(line) {
  const esc = line.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
  if (esc.includes('[ERROR]'))   return `<span class="log-error">${esc}</span>`;
  if (esc.includes('[WARN'))     return `<span class="log-warn">${esc}</span>`;
  if (esc.includes('[login]') || esc.includes('SUCCESS') || esc.includes('FAIL')) return `<span class="log-login">${esc}</span>`;
  if (esc.includes('[auth]') || esc.includes('reset_admin')) return `<span class="log-auth">${esc}</span>`;
  return `<span class="log-info">${esc}</span>`;
}

window.loadSyslog = async function() {
  const el = sel('syslog-content');
  const lines = sel('log-lines') ? sel('log-lines').value : 100;
  el.innerHTML = '<div class="log-empty">Loading...</div>';
  try {
    const data = await API.logs(lines);
    if (!data||!data.length) { el.innerHTML = '<div class="log-empty">No log entries yet.</div>'; return; }
    el.innerHTML = data.map(colorize_log_line).join('\n');
    el.scrollTop = el.scrollHeight;
  } catch(e) { el.innerHTML = '<div class="log-empty">Cannot load log. ' + e.message + '</div>'; }
};

window.loadAuditlog = async function() {
  const el = sel('auditlog-content');
  el.innerHTML = '<div class="log-empty">Loading...</div>';
  try {
    const data = await API.audit();
    if (!data||!data.length) { el.innerHTML = '<div class="log-empty">No audit events yet.</div>'; return; }
    el.innerHTML = data.map(colorize_log_line).join('\n');
    el.scrollTop = el.scrollHeight;
  } catch(e) { el.innerHTML = '<div class="log-empty">Cannot load audit log. ' + e.message + '</div>'; }
};

// ===== ADMIN =====
async function loadAdmin() {
  const online = await API.isOnline();
  const list = sel('admin-user-list');
  const header = `<div class="user-list-row user-list-header">
    <div></div><div>Username</div><div>Role</div><div>Status</div><div>Actions</div>
  </div>`;
  if (!online) {
    list.innerHTML = header + `<div class="user-list-row" style="grid-column:1/-1;padding:1rem;color:var(--text-muted)">Server not connected.</div>`;
    return;
  }
  const user = _current_user;
  list.innerHTML = header + (user ? `<div class="user-list-row">
    <div><div class="user-avatar">${user.username.slice(0,2).toUpperCase()}</div></div>
    <div><strong>${user.username}</strong></div>
    <div><span class="user-role-pill ${user.role==='admin'?'role-admin':'role-technician'}">${user.role}</span></div>
    <div><span class="badge badge-completed">Active</span></div>
    <div><button class="btn-secondary btn-sm" onclick="sel('dd-change-pw').click()">Change password</button></div>
  </div>` : '');
}

sel('btn-new-user') && (sel('btn-new-user').onclick = () => {
  Modal.show('New user', `
    <div class="form-row"><label>Username *</label><input id="f-uname" type="text" autocomplete="off"></div>
    <div class="form-row"><label>Password *</label><input id="f-upw" type="password" autocomplete="new-password"></div>
    <div class="pw-strength" id="pw-strength" style="width:0;background:#e5e7eb"></div>
    <div class="form-row"><label>Role</label><select id="f-urole"><option value="technician">Technician</option><option value="admin">Admin</option></select></div>`,
  async () => { toast('User creation requires server API — POST /api/users coming in v2.3.0', true); });
  setTimeout(() => {
    const inp = sel('f-upw');
    if (inp) inp.oninput = () => {
      const v = inp.value; let score = 0;
      if (v.length>=8) score++; if (/[A-Z]/.test(v)) score++;
      if (/[0-9]/.test(v)) score++; if (/[^A-Za-z0-9]/.test(v)) score++;
      const colors=['#e5e7eb','#ef4444','#f59e0b','#22c55e','#16a34a'];
      const bar = sel('pw-strength');
      if (bar) { bar.style.width=(score*25)+'%'; bar.style.background=colors[score]; }
    };
  }, 50);
});

// ===== USER MENU & AUTH =====
let _current_user  = null;
let _session_token = sessionStorage.getItem('mep_session_token') || null;
let _ver_str  = '2.3.0';
let _app_name = 'Metis Exterminator Plus';
let _tax_rate = 0.0875;

function update_user_ui(user) {
  _current_user = user;
  const initials = user ? user.username.slice(0,2).toUpperCase() : '?';
  sel('user-avatar').textContent = initials;
  sel('user-label').textContent  = user ? user.username : 'Guest';
  sel('dd-uname').textContent    = user ? user.username : 'Guest';
  sel('dd-role').textContent     = user ? user.role : 'Not signed in';
  ['nav-admin','nav-syslog','nav-auditlog'].forEach(id => {
    const el = sel(id);
    if (el) el.style.display = (user && user.role === 'admin') ? '' : 'none';
  });
}

sel('user-btn').onclick = (e) => { e.stopPropagation(); sel('user-dropdown').classList.toggle('hidden'); };
document.addEventListener('click', () => sel('user-dropdown').classList.add('hidden'));

sel('dd-change-pw').onclick = () => {
  sel('user-dropdown').classList.add('hidden');
  Modal.show('Change password', `
    <div class="form-row"><label>Current password</label><input id="f-pw-old" type="password"></div>
    <div class="form-row"><label>New password</label><input id="f-pw-new" type="password"></div>
    <div class="pw-strength" id="pw-strength" style="width:0;background:#e5e7eb"></div>
    <div class="form-row"><label>Confirm new password</label><input id="f-pw-confirm" type="password"></div>`,
  async () => {
    const newpw = sel('f-pw-new').value;
    const conf  = sel('f-pw-confirm').value;
    if (!newpw) { toast('All fields required', true); return; }
    if (newpw !== conf) { toast('Passwords do not match', true); return; }
    if (newpw.length < 8) { toast('Password must be at least 8 characters', true); return; }
    toast('Change password endpoint coming in v2.3.0', true);
  });
  setTimeout(() => {
    const inp = sel('f-pw-new');
    if (inp) inp.oninput = () => {
      const v = inp.value; let score = 0;
      if (v.length>=8) score++; if (/[A-Z]/.test(v)) score++;
      if (/[0-9]/.test(v)) score++; if (/[^A-Za-z0-9]/.test(v)) score++;
      const colors=['#e5e7eb','#ef4444','#f59e0b','#22c55e','#16a34a'];
      const bar = sel('pw-strength');
      if (bar) { bar.style.width=(score*25)+'%'; bar.style.background=colors[score]; }
    };
  }, 50);
};

// ===== THEME =====
(function() {
  const saved = sessionStorage.getItem('mep_theme') || localStorage.getItem('mep_theme') || 'dark';
  document.documentElement.setAttribute('data-theme', saved);
  function update_icon() {
    const dark = document.documentElement.getAttribute('data-theme') === 'dark';
    const btn = sel('btn-theme');
    if (btn) { btn.textContent = dark ? '\u2600' : '\u263D'; btn.title = dark ? 'Switch to light mode' : 'Switch to dark mode'; }
  }
  update_icon();
  window._toggle_theme = function() {
    const next = document.documentElement.getAttribute('data-theme') === 'dark' ? 'light' : 'dark';
    document.documentElement.setAttribute('data-theme', next);
    sessionStorage.setItem('mep_theme', next);
    update_icon();
  };
})();

// ===== LOGIN GATE =====
function login_error(msg) {
  const err = sel('login-error');
  err.textContent = msg;
  err.classList.remove('hidden');
}

function show_login(msg) {
  sel('login-screen').style.display = '';
  sel('main-content').style.display = 'none';
  if (msg) login_error(msg);
}

async function show_app() {
  sel('login-screen').style.display = 'none';
  sel('main-content').style.display = '';
}

async function attempt_login() {
  const username = sel('login-username').value.trim();
  const password = sel('login-password').value;
  const btn = sel('login-btn');
  if (!username || !password) { login_error('Enter your username and password.'); return; }
  btn.disabled = true; btn.textContent = 'Signing in...';
  sel('login-error').classList.add('hidden');
  try {
    const online = await API.isOnline();
    if (!online) {
      login_error('Cannot connect to server. Ensure Metis_Exterminator_Plus.exe is running and the TLS certificate is accepted in your browser.');
      return;
    }
    let result = null;
    try { result = await API.login(username, password); } catch(e) {
      login_error('Server error: ' + e.message); return;
    }
    if (result && result.token) {
      _session_token = result.token;
      sessionStorage.setItem('mep_session_token', result.token);
      const u = await API.me(result.token);
      if (u) update_user_ui(u);
      await show_app();
      if (_init_promise) await _init_promise;
      await loadDashboard();
    } else {
      login_error('Invalid username or password.');
    }
  } catch(e) {
    login_error('Unexpected error: ' + e.message);
  } finally {
    btn.disabled = false; btn.textContent = 'Sign in';
  }
}

let _init_promise = null;

document.addEventListener('DOMContentLoaded', () => {
  const loginBtn = sel('login-btn');
  if (loginBtn) loginBtn.onclick = attempt_login;
  const pwField = sel('login-password');
  if (pwField) pwField.onkeydown = (e) => { if (e.key==='Enter') attempt_login(); };
  const unField = sel('login-username');
  if (unField) unField.onkeydown = (e) => { if (e.key==='Enter') { const p=sel('login-password'); if(p) p.focus(); } };
  const btnTheme = sel('btn-theme');
  if (btnTheme) btnTheme.onclick = () => window._toggle_theme();
  const btnExport = sel('btn-export');
  if (btnExport) btnExport.onclick = exportData;
});

async function exportData() {
  try {
    const [customers, jobs, invoices] = await Promise.all([API.customers(), API.jobs({}), API.invoices()]);
    const payload = { exported_at: new Date().toISOString(), app: _app_name, version: _ver_str, customers, jobs, invoices };
    const blob = new Blob([JSON.stringify(payload, null, 2)], {type:'application/json'});
    const url  = URL.createObjectURL(blob);
    const a    = document.createElement('a');
    a.href = url; a.download = 'MetisExterminator-backup-' + new Date().toISOString().slice(0,10) + '.json';
    a.click(); URL.revokeObjectURL(url);
    toast('Data exported');
  } catch(e) { toast('Export failed: ' + e.message, true); }
}

async function init() {
  const online = await API.isOnline();
  const ver_info = await API.version();
  _ver_str  = ver_info.version || '2.3.0';
  _app_name = ver_info.app    || 'Metis Exterminator Plus';
  _tax_rate = ver_info.tax_rate || 0.0875;

  sel('nav-version').textContent = `v${_ver_str} (server)`;

  const loginTitle = document.querySelector('.login-title');
  if (loginTitle) loginTitle.textContent = _app_name;

  let loginVerEl = document.getElementById('login-version');
  if (!loginVerEl) {
    loginVerEl = document.createElement('p');
    loginVerEl.id = 'login-version';
    loginVerEl.className = 'login-version';
    const sub = document.querySelector('.login-sub');
    if (sub) sub.parentNode.insertBefore(loginVerEl, sub);
  }
  loginVerEl.textContent = `v${_ver_str}`;

  const note = sel('login-standalone-note');
  if (note) {
    if (!online) {
      note.innerHTML = 'Server not detected. Ensure <strong>Metis_Exterminator_Plus.exe</strong> is running.<br>If using TLS, visit <a href="https://127.0.0.1:9100" target="_blank" style="color:var(--green-accent)">https://127.0.0.1:9100</a> to accept the certificate first.';
    } else {
      note.textContent = '';
    }
  }

  sel('dd-logout') && (sel('dd-logout').onclick = async () => {
    if (_session_token) {
      try { await API.logout(_session_token); } catch {}
      _session_token = null;
      sessionStorage.removeItem('mep_session_token');
    }
    update_user_ui(null);
    sel('user-dropdown').classList.add('hidden');
    show_login('You have been signed off.');
  });

  sel('dd-shutdown') && (sel('dd-shutdown').onclick = async () => {
    sel('user-dropdown').classList.add('hidden');
    if (!confirm('Shut down the Metis Exterminator Plus server?\n\nAll data will be saved. The browser will lose connection.')) return;
    try {
      await API.shutdown(_session_token);
      _session_token = null;
      sessionStorage.removeItem('mep_session_token');
      sel('login-screen').style.display = '';
      sel('main-content').style.display = 'none';
      const err = sel('login-error');
      err.textContent = 'Server has shut down. Restart Metis_Exterminator_Plus.exe to reconnect.';
      err.classList.remove('hidden');
      const btn = sel('login-btn');
      if (btn) btn.disabled = true;
    } catch(e) {
      toast('Shutdown failed: ' + e.message, true);
    }
  });

  if (online && _session_token) {
    try {
      const u = await API.me(_session_token);
      if (u && u.id) {
        update_user_ui(u);
        const pill = sel('pill-mode');
        if (pill) { pill.className = 'sec-pill pill-online'; pill.textContent = 'Server'; }
        await show_app();
        await loadDashboard();
        return;
      }
    } catch {}
    _session_token = null;
    sessionStorage.removeItem('mep_session_token');
  }

  if (!online) sel('nav-version').textContent = `v${_ver_str} (offline)`;
  show_login();
  sel('login-username') && sel('login-username').focus();
}

_init_promise = init();
_init_promise.catch(e => console.error('init error:', e));
