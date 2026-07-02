#pragma once
// Metis Exterminator Plus - Container Infrastructure Stub
// Placeholder for future container runtime introspection
// Enabled via config: infra.containers_enabled = true
// No Docker dependency - uses environment variable detection and cgroup inspection
// Works on Windows (containerd/WSL2), Linux (containerd/podman), macOS (containerd)
#include <string>
#include <cstdlib>

namespace metis::infra::containers {

enum class Runtime { None, Containerd, Podman, Unknown };

inline bool running_in_container() {
    // Check standard container environment signals
    if (std::getenv("container") != nullptr) return true;     // systemd-nspawn
    if (std::getenv("KUBERNETES_SERVICE_HOST") != nullptr) return true;
#if defined(__linux__)
    // cgroup v2 check
    FILE* f = std::fopen("/proc/1/cgroup", "r");
    if (f) {
        char buf[256];
        while (std::fgets(buf, sizeof(buf), f)) {
            if (std::strstr(buf, "docker") || std::strstr(buf, "containerd")) {
                std::fclose(f);
                return true;
            }
        }
        std::fclose(f);
    }
#endif
    return false;
}

inline Runtime detect_runtime() {
    // TODO: query runtime sockets
    // Linux:   /run/containerd/containerd.sock or /run/podman/podman.sock
    // Windows: \\.\pipe\containerd-containerd or npipe
    // macOS:   ~/Library/Containers/... or /var/run/containerd.sock
    return Runtime::None;
}

inline std::string runtime_name(Runtime r) {
    switch (r) {
        case Runtime::Containerd: return "containerd";
        case Runtime::Podman:     return "podman";
        case Runtime::Unknown:    return "unknown";
        default:                  return "none";
    }
}

} // namespace metis::infra::containers
