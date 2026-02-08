#include "monitor/cpu_monitor.hpp"
#include <mach/mach.h>
#include <mach/processor_info.h>
#include <mach/mach_host.h>

namespace video_bench {

struct MacCpuTicks {
    uint64_t user;
    uint64_t system;
    uint64_t idle;
    uint64_t nice;

    uint64_t totalActive() const {
        return user + system + nice;
    }

    uint64_t total() const {
        return user + system + idle + nice;
    }
};

class MacOSCpuMonitor : public CpuMonitor {
public:
    MacOSCpuMonitor() = default;

    void startMeasurement() override {
        start_ticks_ = readCpuTicks();
    }

    double getCpuUsage() override {
        MacCpuTicks current = readCpuTicks();

        uint64_t total_diff = current.total() - start_ticks_.total();
        uint64_t active_diff = current.totalActive() - start_ticks_.totalActive();

        if (total_diff == 0) {
            return 0.0;
        }

        double usage = 100.0 * static_cast<double>(active_diff) /
                               static_cast<double>(total_diff);
        return usage;
    }

private:
    MacCpuTicks readCpuTicks() {
        MacCpuTicks ticks{};

        host_cpu_load_info_data_t cpu_info;
        mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;

        kern_return_t kr = host_statistics(mach_host_self(),
                                           HOST_CPU_LOAD_INFO,
                                           reinterpret_cast<host_info_t>(&cpu_info),
                                           &count);

        if (kr == KERN_SUCCESS) {
            ticks.user = cpu_info.cpu_ticks[CPU_STATE_USER];
            ticks.system = cpu_info.cpu_ticks[CPU_STATE_SYSTEM];
            ticks.idle = cpu_info.cpu_ticks[CPU_STATE_IDLE];
            ticks.nice = cpu_info.cpu_ticks[CPU_STATE_NICE];
        }

        return ticks;
    }

    MacCpuTicks start_ticks_{};
};

std::unique_ptr<CpuMonitor> CpuMonitor::create() {
    return std::make_unique<MacOSCpuMonitor>();
}

} // namespace video_bench
