#ifndef SYSTEM_INFO_HPP
#define SYSTEM_INFO_HPP

#include <string>

namespace video_bench {

class SystemInfo {
public:
    // Get CPU model name
    static std::string getCpuName();

    // Get number of hardware threads
    static unsigned int getThreadCount();
};

} // namespace video_bench

#endif // SYSTEM_INFO_HPP
