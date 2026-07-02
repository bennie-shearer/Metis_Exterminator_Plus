'use strict';

const API = (() => {
  const SERVER_HTTPS = 'https://127.0.0.1:9100';
  const SERVER_HTTP  = 'http://127.0.0.1:9100';
  let useServer   = false;
  let serverChecked = false;
  let activeBase  = SERVER_HTTPS;

  async function probe() {
    if (serverChecked) return useServer;
    serverChecked = true;
    try {
      const r = await fetch(SERVER_HTTPS + '/api/stats', {signal: AbortSignal.timeout(1500)});
      if (r.ok) { useServer = true; activeBase = SERVER_HTTPS; return true; }
    } catch {}
    try {
      const r = await fetch(SERVER_HTTP + '/api/stats', {signal: AbortSignal.timeout(1200)});
      if (r.ok) { useServer = true; activeBase = SERVER_HTTP; return true; }
    } catch {}
    useServer = false;
    return false;
  }

  async function req(method, path, body) {
    if (!useServer) throw new Error('SERVER_OFFLINE');
    const opts = { method, headers: {'Content-Type': 'application/json'} };
    if (body != null) opts.body = JSON.stringify(body);
    const r = await fetch(activeBase + path, opts);
    if (r.status === 204) return null;
    return r.json();
  }

  async function authReq(method, path, body, token) {
    if (!useServer) throw new Error('SERVER_OFFLINE');
    const headers = {'Content-Type': 'application/json'};
    if (token) headers['Authorization'] = 'Bearer ' + token;
    const opts = { method, headers };
    if (body != null) opts.body = JSON.stringify(body);
    const r = await fetch(activeBase + path, opts);
    if (r.status === 204) return null;
    return r.json();
  }

  return {
    async isOnline()           { return probe(); },
    async version()            { try { return await req('GET', '/api/version'); } catch { return {}; } },
    async stats()              { return req('GET', '/api/stats'); },
    async logs(lines)          { return req('GET', '/api/logs?lines=' + (lines||100)); },
    async audit()              { return req('GET', '/api/audit'); },
    async login(u, p)          { return req('POST', '/api/auth/login', {username:u, password:p}); },
    async logout(tok)          { return req('POST', '/api/auth/logout', {token:tok}); },
    async me(tok)              { return authReq('GET', '/api/auth/me', null, tok); },
    async shutdown(tok)        { return authReq('POST', '/api/shutdown', {}, tok); },
    async customers(q)         { return req('GET', '/api/customers' + (q ? '?q=' + encodeURIComponent(q) : '')); },
    async customer(id)         { return req('GET', '/api/customers/' + id); },
    async addCustomer(d)       { return req('POST', '/api/customers', d); },
    async updateCustomer(id,d) { return req('PUT', '/api/customers/' + id, d); },
    async deleteCustomer(id)   { return req('DELETE', '/api/customers/' + id); },
    async jobs(p)              { const qs = new URLSearchParams(p||{}).toString(); return req('GET', '/api/jobs' + (qs?'?'+qs:'')); },
    async job(id)              { return req('GET', '/api/jobs/' + id); },
    async addJob(d)            { return req('POST', '/api/jobs', d); },
    async updateJob(id,d)      { return req('PUT', '/api/jobs/' + id, d); },
    async deleteJob(id)        { return req('DELETE', '/api/jobs/' + id); },
    async invoices(p)          { const qs = new URLSearchParams(p||{}).toString(); return req('GET', '/api/invoices' + (qs?'?'+qs:'')); },
    async invoice(id)          { return req('GET', '/api/invoices/' + id); },
    async addInvoice(d)        { return req('POST', '/api/invoices', d); },
    async updateInvoice(id,d)  { return req('PUT', '/api/invoices/' + id, d); },
    async deleteInvoice(id)    { return req('DELETE', '/api/invoices/' + id); },
  };
})();
