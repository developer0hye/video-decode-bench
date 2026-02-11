#include "monitor/memory_monitor.hpp"
#include <mach/mach.h>
#include <sys/sysctl.h>

namespace video_bench {

class MacOSMemoryMonitor : public MemoryMonitor {
public:
    MacOSMemoryMonitor() = default;

    size_t getProcessMemoryMB() override {
        mach_task_basic_info_data_t info{};
        mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;

        kern_return_t kr = task_info(mach_task_self(),
                                     MACH_TASK_BASIC_INFO,
                                     reinterpret_cast<task_info_t>(&info),
                                     &count);

        if (kr != KERN_SUCCESS) {
            return 0;
        }

        return info.resident_size / (1024 * 1024);
    }

    size_t getTotalSystemMemoryMB() override {
        int mib[2] = {CTL_HW, HW_MEMSIZE};
        uint64_t mem_size = 0;
        size_t len = sizeof(mem_size);

        if (sysctl(mib, 2, &mem_size, &len, nullptr, 0) != 0) {
            return 0;
        }

        return static_cast<size_t>(mem_size / (1024 * 1024));
    }
};

std::unique_ptr<MemoryMonitor> MemoryMonitor::create() {
    return std::make_unique<MacOSMemoryMonitor>();
}

} // namespace video_bench
