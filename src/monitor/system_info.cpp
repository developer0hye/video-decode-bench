#include "monitor/system_info.hpp"
#include <thread>
#include <fstream>
#include <sstream>
#include <cstring>
#include <array>
#include <cstdio>

#if defined(__APPLE__)
#include <sys/sysctl.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#endif

#if defined(__linux__)
namespace {

std::string trimLeading(const std::string& s) {
    size_t start = s.find_first_not_of(" \t");
    return (start != std::string::npos) ? s.substr(start) : "";
}

std::string parseCpuinfoField(const std::string& field) {
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    while (std::getline(cpuinfo, line)) {
        if (line.find(field) == 0) {
            size_t pos = line.find(':');
            if (pos != std::string::npos) {
                return trimLeading(line.substr(pos + 1));
            }
        }
    }
    return "";
}

std::string tryDeviceTreeModel() {
    std::ifstream file("/sys/firmware/devicetree/base/model");
    if (!file.is_open()) return "";
    std::string model;
    std::getline(file, model);
    // Device tree strings may contain a trailing null byte
    if (!model.empty() && model.back() == '\0') {
        model.pop_back();
    }
    return trimLeading(model);
}

std::string tryLscpu() {
    std::unique_ptr<FILE, decltype(&pclose)> pipe(
        popen("lscpu 2>/dev/null", "r"), pclose);
    if (!pipe) return "";

    std::string vendor_id;
    std::string model_name;
    std::array<char, 256> buffer{};
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get())) {
        std::string line(buffer.data());
        if (line.find("Vendor ID:") == 0) {
            size_t pos = line.find(':');
            if (pos != std::string::npos) {
                vendor_id = trimLeading(line.substr(pos + 1));
                if (!vendor_id.empty() && vendor_id.back() == '\n')
                    vendor_id.pop_back();
            }
        } else if (line.find("Model name:") == 0) {
            size_t pos = line.find(':');
            if (pos != std::string::npos) {
                model_name = trimLeading(line.substr(pos + 1));
                if (!model_name.empty() && model_name.back() == '\n')
                    model_name.pop_back();
            }
        }
    }

    // Return model name if it's a real value (not just "-")
    if (!model_name.empty() && model_name != "-") return model_name;
    // Fall back to vendor ID alone
    if (!vendor_id.empty() && vendor_id != "-") return vendor_id;
    return "";
}

struct ArmPartEntry {
    uint16_t part_id;
    const char* name;
};

struct ArmVendorEntry {
    uint8_t implementer_id;
    const char* vendor_name;
    const ArmPartEntry* parts;
    size_t part_count;
};

constexpr ArmPartEntry arm_parts[] = {
    {0xD03, "Cortex-A53"}, {0xD04, "Cortex-A35"}, {0xD05, "Cortex-A55"},
    {0xD06, "Cortex-A65"}, {0xD07, "Cortex-A57"}, {0xD08, "Cortex-A72"},
    {0xD09, "Cortex-A73"}, {0xD0A, "Cortex-A75"}, {0xD0B, "Cortex-A76"},
    {0xD0C, "Neoverse-N1"}, {0xD0D, "Cortex-A77"}, {0xD40, "Neoverse-V1"},
    {0xD41, "Cortex-A78"}, {0xD44, "Cortex-X1"}, {0xD46, "Cortex-A510"},
    {0xD47, "Cortex-A710"}, {0xD48, "Cortex-X2"}, {0xD4D, "Cortex-A715"},
    {0xD4E, "Cortex-X3"},
};

constexpr ArmPartEntry qualcomm_parts[] = {
    {0x800, "Kryo 260"}, {0x801, "Kryo 280"}, {0x802, "Kryo 385 Gold"},
    {0x803, "Kryo 385 Silver"}, {0xC00, "Falkor"}, {0xC01, "Saphira"},
};

constexpr ArmPartEntry apple_parts[] = {
    {0x022, "M1 Icestorm"},  {0x023, "M1 Firestorm"},
    {0x028, "M1 Pro/Max Icestorm"}, {0x029, "M1 Pro/Max Firestorm"},
    {0x032, "M2 Blizzard"},  {0x033, "M2 Avalanche"},
    {0x038, "M2 Pro/Max Blizzard"}, {0x039, "M2 Pro/Max Avalanche"},
};

constexpr ArmPartEntry nvidia_parts[] = {
    {0x000, "Denver"}, {0x003, "Denver 2"}, {0x004, "Carmel"},
};

constexpr ArmPartEntry samsung_parts[] = {
    {0x001, "Exynos M1"}, {0x002, "Exynos M3"},
};

constexpr ArmVendorEntry arm_vendors[] = {
    {0x41, "ARM",       arm_parts,      std::size(arm_parts)},
    {0x51, "Qualcomm",  qualcomm_parts, std::size(qualcomm_parts)},
    {0x61, "Apple",     apple_parts,    std::size(apple_parts)},
    {0x4E, "NVIDIA",    nvidia_parts,   std::size(nvidia_parts)},
    {0x53, "Samsung",   samsung_parts,  std::size(samsung_parts)},
};

std::string tryArmImplementerPart() {
    std::string impl_str = parseCpuinfoField("CPU implementer");
    std::string part_str = parseCpuinfoField("CPU part");
    if (impl_str.empty() || part_str.empty()) return "";

    unsigned long impl_val = std::strtoul(impl_str.c_str(), nullptr, 0);
    unsigned long part_val = std::strtoul(part_str.c_str(), nullptr, 0);

    std::string vendor_name = "Unknown";
    std::string part_name;

    for (const auto& vendor : arm_vendors) {
        if (vendor.implementer_id == static_cast<uint8_t>(impl_val)) {
            vendor_name = vendor.vendor_name;
            for (size_t i = 0; i < vendor.part_count; ++i) {
                if (vendor.parts[i].part_id == static_cast<uint16_t>(part_val)) {
                    part_name = vendor.parts[i].name;
                    break;
                }
            }
            break;
        }
    }

    if (!part_name.empty()) {
        return vendor_name + " " + part_name;
    }
    return vendor_name + " CPU (part " + part_str + ")";
}

} // anonymous namespace
#endif // __linux__

namespace video_bench {

std::string SystemInfo::getCpuName() {
#if defined(__linux__)
    // 1. Try "model name" (x86 standard)
    std::string name = parseCpuinfoField("model name");
    if (!name.empty()) return name;

    // 2. Try "Hardware" field (some ARM boards)
    name = parseCpuinfoField("Hardware");
    if (!name.empty()) return name;

    // 3. Try device tree model file (ARM SoC description)
    name = tryDeviceTreeModel();
    if (!name.empty()) return name;

    // 4. Try lscpu output
    name = tryLscpu();
    if (!name.empty()) return name;

    // 5. Construct from CPU implementer + CPU part
    name = tryArmImplementerPart();
    if (!name.empty()) return name;

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
