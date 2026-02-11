#ifndef MEMORY_MONITOR_HPP
#define MEMORY_MONITOR_HPP

#include <memory>
#include <cstddef>

namespace video_bench {

// Abstract interface for memory usage monitoring
class MemoryMonitor {
public:
    virtual ~MemoryMonitor() = default;

    // Factory method - creates platform-specific implementation
    static std::unique_ptr<MemoryMonitor> create();

    // Get current process RSS (Resident Set Size) in MB
    virtual size_t getProcessMemoryMB() = 0;

    // Get total physical system memory in MB
    virtual size_t getTotalSystemMemoryMB() = 0;

protected:
    MemoryMonitor() = default;
};

} // namespace video_bench

#endif // MEMORY_MONITOR_HPP
