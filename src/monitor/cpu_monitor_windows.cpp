#include "monitor/cpu_monitor.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace video_bench {

class WindowsCpuMonitor : public CpuMonitor {
public:
    WindowsCpuMonitor() = default;

    void startMeasurement() override {
        FILETIME idle, kernel, user;
        if (GetSystemTimes(&idle, &kernel, &user)) {
            start_idle_ = fileTimeToUInt64(idle);
            start_kernel_ = fileTimeToUInt64(kernel);
            start_user_ = fileTimeToUInt64(user);
        }
    }

    double getCpuUsage() override {
        FILETIME idle, kernel, user;
        if (!GetSystemTimes(&idle, &kernel, &user)) {
            return 0.0;
        }

        uint64_t current_idle = fileTimeToUInt64(idle);
        uint64_t current_kernel = fileTimeToUInt64(kernel);
        uint64_t current_user = fileTimeToUInt64(user);

        uint64_t idle_diff = current_idle - start_idle_;
        uint64_t kernel_diff = current_kernel - start_kernel_;
        uint64_t user_diff = current_user - start_user_;

        // Kernel time includes idle time
        uint64_t total = kernel_diff + user_diff;
        uint64_t active = total - idle_diff;

        if (total == 0) {
            return 0.0;
        }

        double usage = 100.0 * static_cast<double>(active) /
                               static_cast<double>(total);
        return usage;
    }

private:
    static uint64_t fileTimeToUInt64(const FILETIME& ft) {
        return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) |
               static_cast<uint64_t>(ft.dwLowDateTime);
    }

    uint64_t start_idle_ = 0;
    uint64_t start_kernel_ = 0;
    uint64_t start_user_ = 0;
};

std::unique_ptr<CpuMonitor> CpuMonitor::create() {
    return std::make_unique<WindowsCpuMonitor>();
}

} // namespace video_bench
