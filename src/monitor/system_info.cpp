#include "monitor/system_info.hpp"
#include <thread>
#include <fstream>
#include <sstream>
#include <cstring>

#if defined(__APPLE__)
#include <sys/sysctl.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#endif

namespace video_bench {

std::string SystemInfo::getCpuName() {
#if defined(__linux__)
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    while (std::getline(cpuinfo, line)) {
        if (line.find("model name") != std::string::npos) {
            size_t pos = line.find(':');
            if (pos != std::string::npos) {
                std::string name = line.substr(pos + 1);
                // Trim leading whitespace
                size_t start = name.find_first_not_of(" \t");
                if (start != std::string::npos) {
                    return name.substr(start);
                }
            }
        }
    }
    return "Unknown CPU";

#elif defined(__APPLE__)
    char buffer[256];
    size_t size = sizeof(buffer);
    if (sysctlbyname("machdep.cpu.brand_string", buffer, &size, nullptr, 0) == 0) {
        return std::string(buffer);
    }
    return "Unknown CPU";

#elif defined(_WIN32)
    int cpu_info[4] = {0};
    char brand[49] = {0};

    // Get brand string parts
    __cpuid(cpu_info, 0x80000002);
    std::memcpy(brand, cpu_info, sizeof(cpu_info));

    __cpuid(cpu_info, 0x80000003);
    std::memcpy(brand + 16, cpu_info, sizeof(cpu_info));

    __cpuid(cpu_info, 0x80000004);
    std::memcpy(brand + 32, cpu_info, sizeof(cpu_info));

    brand[48] = '\0';

    // Trim leading whitespace
    char* p = brand;
    while (*p == ' ') p++;

    return std::string(p);

#else
    return "Unknown CPU";
#endif
}

unsigned int SystemInfo::getThreadCount() {
    unsigned int count = std::thread::hardware_concurrency();
    return count > 0 ? count : 1;
}

} // namespace video_bench
