'use strict';

const API = (() => {
  const SERVER_HTTPS = 'https://127.0.0.1:9100';
  const SERVER_HTTP  = 'http://127.0.0.1:9100';
  let useServer = false, serverChecked = false, activeBase = SERVER_HTTPS;
  let _token = null;  // set after login via API.setToken()

  async function probe() {
    if (serverChecked) return useServer;
    serverChecked = true;
    try { const r = await fetch(SERVER_HTTPS+'/api/stats',{signal:AbortSignal.timeout(1500)}); if(r.ok){useServer=true;activeBase=SERVER_HTTPS;return true;} } catch {}
    try { const r = await fetch(SERVER_HTTP+'/api/stats',{signal:AbortSignal.timeout(1200)}); if(r.ok){useServer=true;activeBase=SERVER_HTTP;return true;} } catch {}
    useServer=false; return false;
  }

  // Every request automatically sends the Bearer token if one is set
  async function req(method, path, body) {
    if (!useServer) throw new Error('SERVER_OFFLINE');
    const headers = {'Content-Type': 'application/json'};
    if (_token) headers['Authorization'] = 'Bearer ' + _token;
    const opts = { method, headers };
    if (body != null) opts.body = JSON.stringify(body);
    const r = await fetch(activeBase + path, opts);
    if (r.status === 204) return null;
    return r.json();
  }

  return {
    // Token management
    setToken(tok)                { _token = tok; },
    clearToken()                 { _token = null; },

    // Core
    async isOnline()             { return probe(); },
    async version()              { try { return await req('GET','/api/version'); } catch { return {}; } },
    async stats()                { return req('GET','/api/stats'); },

    // Auth (login does NOT need token; logout/me/shutdown do)
    async login(u, p)            { return req('POST','/api/auth/login',{username:u,password:p}); },
    async logout(tok)            { return req('POST','/api/auth/logout',{token:tok}); },
    async me(tok)                { return req('GET','/api/auth/me'); },
    async shutdown()             { return req('POST','/api/shutdown',{}); },
    async changePassword(d)      { return req('PUT','/api/auth/password',d); },
    async generateApiKey()       { return req('POST','/api/auth/apikey',{}); },

    // Users (admin)
    async users()                { return req('GET','/api/users'); },
    async createUser(d)          { return req('POST','/api/users',d); },
    async deactivateUser(id)     { return req('DELETE','/api/users/'+id); },
    async setPassword(id,pw)     { return req('PUT','/api/users/'+id+'/password',{new_password:pw}); },

    // Customers
    async customers(q)           { return req('GET','/api/customers'+(q?'?q='+encodeURIComponent(q):'')); },
    async customer(id)           { return req('GET','/api/customers/'+id); },
    async addCustomer(d)         { return req('POST','/api/customers',d); },
    async updateCustomer(id,d)   { return req('PUT','/api/customers/'+id,d); },
    async deleteCustomer(id)     { return req('DELETE','/api/customers/'+id); },

    // Jobs
    async jobs(p)                { const qs=new URLSearchParams(p||{}).toString(); return req('GET','/api/jobs'+(qs?'?'+qs:'')); },
    async job(id)                { return req('GET','/api/jobs/'+id); },
    async addJob(d)              { return req('POST','/api/jobs',d); },
    async updateJob(id,d)        { return req('PUT','/api/jobs/'+id,d); },
    async deleteJob(id)          { return req('DELETE','/api/jobs/'+id); },
    async updateJobTime(id,d)    { return req('PUT','/api/jobs/'+id+'/time',d); },
    async spawnRecurring(id)     { return req('POST','/api/jobs/'+id+'/spawn-recurring',{}); },

    // Chemicals
    async chemicalsForJob(id)    { return req('GET','/api/jobs/'+id+'/chemicals'); },
    async addChemical(id,d)      { return req('POST','/api/jobs/'+id+'/chemicals',d); },
    async deleteChemical(jid,cid){ return req('DELETE','/api/jobs/'+jid+'/chemicals/'+cid); },

    // Invoices
    async invoices(p)            { const qs=new URLSearchParams(p||{}).toString(); return req('GET','/api/invoices'+(qs?'?'+qs:'')); },
    async invoice(id)            { return req('GET','/api/invoices/'+id); },
    async addInvoice(d)          { return req('POST','/api/invoices',d); },
    async updateInvoice(id,d)    { return req('PUT','/api/invoices/'+id,d); },
    async deleteInvoice(id)      { return req('DELETE','/api/invoices/'+id); },
    async checkOverdue()         { return req('POST','/api/invoices/check-overdue',{}); },
    async sendInvoiceEmail(id)   { return req('POST','/api/invoices/'+id+'/send-email',{}); },

    // Route optimization
    async optimizeRoute(date,tech){ return req('GET','/api/route/optimize?date='+encodeURIComponent(date||'')+'&technician='+encodeURIComponent(tech||'')); },

    // Search
    async search(q)              { return req('GET','/api/search?q='+encodeURIComponent(q)); },

    // CSV export
    exportInvoicesCsv()          { window.open(activeBase+'/api/export/invoices.csv?token='+(_token||'')); },
    exportCustomersCsv()         { window.open(activeBase+'/api/export/customers.csv?token='+(_token||'')); },

    // Logs & audit
    async logs(lines)            { return req('GET','/api/logs?lines='+(lines||100)); },
    async audit()                { return req('GET','/api/audit'); },
    async auditEvents(limit)     { return req('GET','/api/audit/events?limit='+(limit||100)); },

    // Admin
    async rotateLog()            { return req('POST','/api/admin/rotate-log',{}); },
  };
})();
