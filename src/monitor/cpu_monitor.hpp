#ifndef CPU_MONITOR_HPP
#define CPU_MONITOR_HPP

#include <memory>
#include <chrono>

namespace video_bench {

// Abstract interface for CPU usage monitoring
class CpuMonitor {
public:
    virtual ~CpuMonitor() = default;

    // Factory method - creates platform-specific implementation
    static std::unique_ptr<CpuMonitor> create();

    // Start a new measurement period
    virtual void startMeasurement() = 0;

    // Get CPU usage percentage (0.0 - 100.0) since last startMeasurement()
    // Returns average CPU usage across all cores
    virtual double getCpuUsage() = 0;

protected:
    CpuMonitor() = default;
};

} // namespace video_bench

#endif // CPU_MONITOR_HPP
