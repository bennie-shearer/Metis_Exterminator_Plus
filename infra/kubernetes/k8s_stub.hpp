#pragma once
// Metis Exterminator Plus - Kubernetes Infrastructure Stub
// Placeholder for future Kubernetes integration
// Enabled via config: infra.kubernetes_enabled = true
// Surfaces health/ready endpoints for liveness and readiness probes
// Windows/Linux/macOS: identical HTTP probe API; no Docker required
#include <string>

namespace metis::infra::k8s {

struct K8sConfig {
    bool enabled = false;
    std::string ns = "metis";
    std::string service_name = "metis-exterminator";
};

struct ProbeResult {
    bool healthy = true;
    std::string message = "ok";
};

// GET /healthz  - liveness probe
inline ProbeResult liveness_probe() {
    return {true, "alive"};
}

// GET /readyz   - readiness probe
// TODO: add DB connectivity check, store availability
inline ProbeResult readiness_probe() {
    return {true, "ready"};
}

// GET /metrics  - Prometheus-style text metrics (stub)
inline std::string prometheus_metrics(int customers, int jobs, int invoices) {
    return
        "# HELP metis_customers_total Total customers\n"
        "# TYPE metis_customers_total gauge\n"
        "metis_customers_total " + std::to_string(customers) + "\n"
        "# HELP metis_jobs_total Total jobs\n"
        "# TYPE metis_jobs_total gauge\n"
        "metis_jobs_total " + std::to_string(jobs) + "\n"
        "# HELP metis_invoices_total Total invoices\n"
        "# TYPE metis_invoices_total gauge\n"
        "metis_invoices_total " + std::to_string(invoices) + "\n";
}

} // namespace metis::infra::k8s
