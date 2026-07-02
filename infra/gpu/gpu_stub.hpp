#pragma once
// Metis Exterminator Plus - GPU Infrastructure Stub
// Placeholder for future GPU acceleration (CUDA/OpenCL/Metal/Vulkan Compute)
// Enabled via config: infra.gpu_enabled = true
// Windows: CUDA via nvcc or OpenCL; Linux: CUDA/OpenCL/ROCm; macOS: Metal
#include <string>

namespace metis::infra::gpu {

struct GpuInfo {
    bool available = false;
    std::string vendor;
    std::string device_name;
    size_t vram_bytes = 0;
};

inline GpuInfo detect() {
    // TODO: implement platform detection
    // Windows:  query DXGI adapters or CUDA runtime
    // Linux:    parse /sys/class/drm or OpenCL platform list
    // macOS:    Metal MTLCopyAllDevices()
    GpuInfo info;
    info.available = false;
    info.vendor = "none";
    return info;
}

inline bool is_enabled(bool cfg_flag) {
    if (!cfg_flag) return false;
    return detect().available;
}

} // namespace metis::infra::gpu
