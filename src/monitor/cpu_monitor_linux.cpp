#include "monitor/cpu_monitor.hpp"
#include <fstream>
#include <sstream>
#include <string>

namespace video_bench {

struct CpuStats {
    uint64_t user;
    uint64_t nice;
    uint64_t system;
    uint64_t idle;
    uint64_t iowait;
    uint64_t irq;
    uint64_t softirq;
    uint64_t steal;

    uint64_t totalActive() const {
        return user + nice + system + irq + softirq + steal;
    }

    uint64_t totalIdle() const {
        return idle + iowait;
    }

    uint64_t total() const {
        return totalActive() + totalIdle();
    }
};

class LinuxCpuMonitor : public CpuMonitor {
public:
    LinuxCpuMonitor() = default;

    void startMeasurement() override {
        start_stats_ = readCpuStats();
    }

    double getCpuUsage() override {
        CpuStats current = readCpuStats();

        uint64_t total_diff = current.total() - start_stats_.total();
        uint64_t idle_diff = current.totalIdle() - start_stats_.totalIdle();

        if (total_diff == 0) {
            return 0.0;
        }

        double usage = 100.0 * (1.0 - static_cast<double>(idle_diff) /
                                       static_cast<double>(total_diff));
        return usage;
    }

private:
    CpuStats readCpuStats() {
        CpuStats stats{};
        std::ifstream proc_stat("/proc/stat");

        if (!proc_stat.is_open()) {
            return stats;
        }

        std::string line;
        if (std::getline(proc_stat, line)) {
            std::istringstream iss(line);
            std::string cpu_label;
            iss >> cpu_label;  // "cpu"

            iss >> stats.user >> stats.nice >> stats.system >> stats.idle
                >> stats.iowait >> stats.irq >> stats.softirq >> stats.steal;
        }

        return stats;
    }

    CpuStats start_stats_{};
};

std::unique_ptr<CpuMonitor> CpuMonitor::create() {
    return std::make_unique<LinuxCpuMonitor>();
}

} // namespace video_bench
