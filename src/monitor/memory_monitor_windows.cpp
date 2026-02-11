#include "monitor/memory_monitor.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>

namespace video_bench {

class WindowsMemoryMonitor : public MemoryMonitor {
public:
    WindowsMemoryMonitor() = default;

    size_t getProcessMemoryMB() override {
        PROCESS_MEMORY_COUNTERS pmc{};
        pmc.cb = sizeof(pmc);

        if (!GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
            return 0;
        }

        return pmc.WorkingSetSize / (1024 * 1024);
    }

    size_t getTotalSystemMemoryMB() override {
        MEMORYSTATUSEX mem_status{};
        mem_status.dwLength = sizeof(mem_status);

        if (!GlobalMemoryStatusEx(&mem_status)) {
            return 0;
        }

        return static_cast<size_t>(mem_status.ullTotalPhys / (1024 * 1024));
    }
};

std::unique_ptr<MemoryMonitor> MemoryMonitor::create() {
    return std::make_unique<WindowsMemoryMonitor>();
}

} // namespace video_bench
