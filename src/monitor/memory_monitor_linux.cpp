#include "monitor/memory_monitor.hpp"
#include <fstream>
#include <sstream>
#include <string>

namespace video_bench {

class LinuxMemoryMonitor : public MemoryMonitor {
public:
    LinuxMemoryMonitor() = default;

    size_t getProcessMemoryMB() override {
        std::ifstream status("/proc/self/status");
        if (!status.is_open()) {
            return 0;
        }

        std::string line;
        while (std::getline(status, line)) {
            if (line.compare(0, 6, "VmRSS:") == 0) {
                std::istringstream iss(line.substr(6));
                size_t kb = 0;
                iss >> kb;
                return kb / 1024;
            }
        }

        return 0;
    }

    size_t getTotalSystemMemoryMB() override {
        std::ifstream meminfo("/proc/meminfo");
        if (!meminfo.is_open()) {
            return 0;
        }

        std::string line;
        while (std::getline(meminfo, line)) {
            if (line.compare(0, 9, "MemTotal:") == 0) {
                std::istringstream iss(line.substr(9));
                size_t kb = 0;
                iss >> kb;
                return kb / 1024;
            }
        }

        return 0;
    }
};

std::unique_ptr<MemoryMonitor> MemoryMonitor::create() {
    return std::make_unique<LinuxMemoryMonitor>();
}

} // namespace video_bench
