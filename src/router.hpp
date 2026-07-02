#pragma once
#include "server.hpp"
#include "customer_store.hpp"
#include "job_store.hpp"
#include "invoice_store.hpp"
#include "database.hpp"
#include "auth.hpp"
#include "pson.hpp"

void register_routes(HttpServer& srv,
                     CustomerStore& customers,
                     JobStore& jobs,
                     InvoiceStore& invoices,
                     const Pson& cfg,
                     Database& db,
                     AuthManager& auth);
