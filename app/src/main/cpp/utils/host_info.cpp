// SPDX-License-Identifier: MIT
// PX5 — HostInfo implementation. See host_info.h for the contract.
//
// Measurement discipline (the anti-"Test Game" rule): a value that cannot
// be measured prints its failure, never a plausible constant. An unknown
// SoC prints "(unknown)", a failed Vulkan probe names the failing stage.

#include "host_info.h"
#include "logger.h"
#include "../gpu/driver_manager.h"

#include <dlfcn.h>
#include <sys/system_properties.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <vulkan/vulkan_core.h>

namespace PX5 {

namespace {

constexpr uint32_t kVkVersionMajorShift = 22;
constexpr uint32_t kVkVersionMinorShift = 12;

std::string VkVer(uint32_t v) {
    char b[48];
    snprintf(b, sizeof(b), "%u.%u.%u",
             v >> kVkVersionMajorShift,
             (v >> kVkVersionMinorShift) & 0x3FF,
             v & 0xFFF);
    return b;
}

std::string SysProp(const char* name, const char* fallback = "") {
    char buf[92] = {0};
    const int n = __system_property_get(name, buf);
    if (n > 0 && buf[0] != '\0') return std::string(buf);
    return std::string(fallback);
}

std::string Trimmed(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

// ---- CPU topology ----------------------------------------------------------
// ARM implementer (0x41) part ids -> marketing names. Unknown ids print
// their raw number — honest across future cores and other vendors.
const char* ArmPartName(int part) {
    switch (part) {
        case 0xd01: return "Cortex-A32";
        case 0xd03: return "Cortex-A53";
        case 0xd04: return "Cortex-A35";
        case 0xd05: return "Cortex-A55";
        case 0xd07: return "Cortex-A57";
        case 0xd08: return "Cortex-A72";
        case 0xd09: return "Cortex-A73";
        case 0xd0a: return "Cortex-A75";
        case 0xd0b: return "Cortex-A76";
        case 0xd0c: return "Neoverse-N1";
        case 0xd0d: return "Cortex-A77";
        case 0xd0e: return "Cortex-A76AE";
        case 0xd40: return "Neoverse-V1";
        case 0xd41: return "Cortex-A78";
        case 0xd42: return "Cortex-A78AE";
        case 0xd44: return "Cortex-X1";
        case 0xd46: return "Cortex-A510";
        case 0xd47: return "Cortex-A710";
        case 0xd48: return "Cortex-X2";
        case 0xd49: return "Neoverse-N2";
        case 0xd4b: return "Cortex-A78C";
        case 0xd4c: return "Cortex-X1C";
        case 0xd4d: return "Cortex-A715";
        case 0xd4e: return "Cortex-X3";
        case 0xd4f: return "Neoverse-V2";
        case 0xd80: return "Cortex-A520";
        case 0xd81: return "Cortex-A720";
        case 0xd82: return "Cortex-X4";
        case 0xd84: return "Neoverse-V3";
        case 0xd85: return "Cortex-X925";
        case 0xd87: return "Cortex-A725";
        case 0xd88: return "Cortex-A320";
        case 0xd8e: return "Neoverse-N3";
        default: return nullptr;
    }
}

// Qualcomm custom cores (implementer 0x51). Only confident entries; an
// unknown core prints its raw id — the report stays honest on every future
// SoC instead of guessing a plausible name.
const char* QualPartName(int part) {
    switch (part) {
        case 0x001: return "Oryon (prime)";   // Snapdragon 8 Elite / X Elite
        case 0x002: return "Oryon";           // 8 Elite efficiency cluster
        case 0x201: return "Kryo 2xx Gold";
        case 0x205: return "Kryo 2xx Silver";
        case 0x211: return "Kryo 3xx Gold";
        case 0x215: return "Kryo 3xx Silver";
        case 0x800: return "Kryo 4xx Gold";
        case 0x801: return "Kryo 4xx Silver";
        case 0x802: return "Kryo 5xx Gold";
        case 0x803: return "Kryo 5xx Silver";
        default: return nullptr;
    }
}

std::string PartDisplayName(int part, int implementer) {
    if (implementer == 0x41) {
        if (const char* n = ArmPartName(part)) return n;
    }
    if (implementer == 0x51) {
        if (const char* n = QualPartName(part)) return n;
    }
    char b[48];
    snprintf(b, sizeof(b), "part 0x%03x (impl 0x%02x)", part, implementer);
    return b;
}

struct CpuTopology {
    std::string soc;
    std::string coresLine;     // "2x Cortex-A520 + 5x Cortex-A720 + 1x Cortex-X4"
    std::string llvmCpu;       // cpu0's core (Eden semantics)
    std::string featuresLine;  // "NEON+DP+I8MM+BF16 | Crypto | LSE"
    int threads = 0;
};

CpuTopology ReadCpuTopology() {
    CpuTopology out;

    // SoC identity: ro.soc.model is the exact source Eden shows ("SM8650").
    out.soc = SysProp("ro.soc.model");
    if (out.soc.empty()) out.soc = SysProp("ro.hardware.chipname");
    if (out.soc.empty()) out.soc = SysProp("ro.board.platform");

    FILE* f = fopen("/proc/cpuinfo", "re");
    if (!f) {
        out.threads = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
        return out;
    }

    std::vector<int> parts;          // one entry per processor block, in order
    std::vector<int> impls;
    int  curPart = -1, curImpl = -1;
    bool inBlock  = false;
    std::string features, hwName;
    std::set<std::string> flags;
    char line[1024];

    auto flushBlock = [&]() {
        if (inBlock) {
            parts.push_back(curPart);
            impls.push_back(curImpl);
        }
        curPart = curImpl = -1;
        inBlock = false;
    };

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "processor", 9) == 0) {
            // Each "processor" line closes the previous core block and
            // opens the next one — parts must be flushed per block or the
            // whole topology would collapse into one core.
            flushBlock();
            inBlock = true;
            continue;
        }
        if (strncmp(line, "CPU part", 8) == 0) {
            const char* c = strchr(line, ':');
            if (c) curPart = static_cast<int>(strtol(c + 1, nullptr, 16));
            continue;
        }
        if (strncmp(line, "CPU implementer", 15) == 0) {
            const char* c = strchr(line, ':');
            if (c) curImpl = static_cast<int>(strtol(c + 1, nullptr, 16));
            continue;
        }
        if (strncmp(line, "Features", 8) == 0 && flags.empty()) {
            const char* c = strchr(line, ':');
            if (c) {
                features = Trimmed(c + 1);
                std::istringstream ss(features);
                std::string tok;
                while (ss >> tok) flags.insert(tok);
            }
            continue;
        }
        if (strncmp(line, "Hardware", 8) == 0 && hwName.empty()) {
            const char* c = strchr(line, ':');
            if (c) hwName = Trimmed(c + 1);
            continue;
        }
    }
    flushBlock();
    fclose(f);

    out.threads = parts.empty()
        ? static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN))
        : static_cast<int>(parts.size());

    // Cores line: group by part, preserving first-appearance order.
    {
        std::vector<std::pair<int, int>> ordered; // (part, impl) first-seen order
        std::vector<int> counts;
        for (size_t i = 0; i < parts.size(); ++i) {
            const int p = parts[i], im = i < impls.size() ? impls[i] : -1;
            size_t k = 0;
            for (; k < ordered.size(); ++k)
                if (ordered[k].first == p) break;
            if (k == ordered.size()) {
                ordered.push_back({p, im});
                counts.push_back(0);
            }
            counts[k]++;
        }
        std::string cores;
        for (size_t k = 0; k < ordered.size(); ++k) {
            if (k) cores += " + ";
            cores += std::to_string(counts[k]) + "x " +
                     PartDisplayName(ordered[k].first, ordered[k].second);
        }
        out.coresLine = cores.empty() ? "(unreadable)" : cores;
        // LLVM CPU: cpu0's core — the same value Eden reports on this
        // device class (cpu0 is the first core in the kernel's order).
        if (!parts.empty() && parts[0] >= 0)
            out.llvmCpu = PartDisplayName(parts[0],
                          impls.empty() ? 0x41 : impls[0]);
    }

    // Feature groups — Eden's compact taxonomy, nothing invented:
    //   NEON+DP+I8MM+BF16 (NEON = asimd, DP = asimddp) | Crypto | LSE.
    if (!flags.empty()) {
        std::string groups;
        auto has = [&flags](const char* f) { return flags.count(f) > 0; };
        std::string simd;
        auto addSimd = [&](const char* f, const char* label) {
            if (!has(f)) return;
            if (!simd.empty()) simd += "+";
            simd += label;
        };
        addSimd("asimd",   "NEON");
        addSimd("asimddp", "DP");
        addSimd("i8mm",    "I8MM");
        addSimd("bf16",    "BF16");
        if (!simd.empty()) groups += simd;
        if (has("aes") || has("pmull") || has("sha1") || has("sha2") ||
            has("sha512") || has("sha3") || has("sm3") || has("sm4")) {
            if (!groups.empty()) groups += " | ";
            groups += "Crypto";
        }
        if (has("atomics")) {
            if (!groups.empty()) groups += " | ";
            groups += "LSE";
        }
        out.featuresLine = groups.empty() ? features : groups;
    }

    // SoC fallback: the kernel's Hardware line ("Qualcomm Technologies,
    // Inc SM8650") — trim the vendor prefix so it reads "SM8650".
    if (out.soc.empty() && !hwName.empty()) {
        const std::string prefix = "Qualcomm Technologies, Inc ";
        out.soc = hwName.rfind(prefix, 0) == 0 ? hwName.substr(prefix.size())
                                              : hwName;
    }
    if (out.soc.empty()) out.soc = "(unknown)";

    return out;
}

// ---- GPU probe (real Vulkan enumeration through the engine's loader) ------
std::string ProbeGpuSection() {
    // Same central loader the engine's VulkanGpuDevice uses. At the one
    // build moment (startup) the active mode is 0, so this documents the
    // device's SYSTEM Vulkan driver — which is exactly what the block is
    // for. A probe failure names its stage; nothing is guessed.
    void* lib = GpuDriverManager::GetInstance()
                    .OpenHostVulkanLibrary(RTLD_NOW | RTLD_LOCAL);
    if (!lib) {
        const char* err = dlerror();
        return std::string("(Vulkan probe failed: dlopen") +
               (err ? std::string(": ") + err : std::string()) + ")";
    }

    auto gipa = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        dlsym(lib, "vkGetInstanceProcAddr"));
    if (!gipa) {
        dlclose(lib);
        return "(Vulkan probe failed: vkGetInstanceProcAddr missing)";
    }

    uint32_t instApi = VK_API_VERSION_1_0;
    if (auto enumInstVer = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
            gipa(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"))) {
        enumInstVer(&instApi);
    }

    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "PX5 host info";
    app.apiVersion       = instApi;
    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ci.pApplicationInfo = &app;

    VkInstance inst = VK_NULL_HANDLE;
    auto create = reinterpret_cast<PFN_vkCreateInstance>(
        gipa(VK_NULL_HANDLE, "vkCreateInstance"));
    if (!create || create(&ci, nullptr, &inst) != VK_SUCCESS || !inst) {
        dlclose(lib);
        return "(Vulkan probe failed: vkCreateInstance)";
    }

    auto destroy = reinterpret_cast<PFN_vkDestroyInstance>(
        gipa(inst, "vkDestroyInstance"));
    auto enumDevs = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
        gipa(inst, "vkEnumeratePhysicalDevices"));
    auto getProps = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
        gipa(inst, "vkGetPhysicalDeviceProperties"));

    std::string out;
    uint32_t n = 0;
    if (!enumDevs || !getProps ||
        enumDevs(inst, &n, nullptr) != VK_SUCCESS || n == 0) {
        out = "(no Vulkan physical device)";
    } else {
        std::vector<VkPhysicalDevice> devs(n);
        enumDevs(inst, &n, devs.data());
        // Prefer a real GPU: phones expose one device, but Android gaming
        // handhelds / TV boxes can enumerate software renderers first —
        // the report must document the hardware the engine would use.
        size_t pick = 0;
        for (size_t i = 0; i < devs.size(); ++i) {
            VkPhysicalDeviceProperties p{};
            getProps(devs[i], &p);
            if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ||
                p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                pick = i;
                break;
            }
        }
        VkPhysicalDeviceProperties p{};
        getProps(devs[pick], &p);
        out += "GPU Model: " + std::string(p.deviceName) + "\n";
        out += "Vulkan API: " + VkVer(p.apiVersion) + "\n";
        // Standard Vulkan packed decode — for Adreno this prints exactly
        // the vulkaninfo/DevCheck value ("512.762.44" on Adreno 750).
        out += "Vulkan Driver Version: " + VkVer(p.driverVersion);
    }

    if (destroy) destroy(inst, nullptr);
    dlclose(lib);
    return out;
}

// ---- Memory ----------------------------------------------------------------
std::string MemorySection() {
    FILE* f = fopen("/proc/meminfo", "re");
    if (!f) return "Total Memory: (unreadable)";
    char label[64];
    long kb = -1;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "%63s %ld kB", label, &kb) == 2 &&
            strcmp(label, "MemTotal:") == 0) break;
        kb = -1;
    }
    fclose(f);
    if (kb <= 0) return "Total Memory: (unreadable)";
    // Eden semantics: totalMem MB — the same /proc/meminfo MemTotal that
    // ActivityManager.MemoryInfo.totalMem reports ("11273 MB" on 12 GB).
    char b[64];
    snprintf(b, sizeof(b), "Total Memory: %ld MB", kb / 1024);
    return b;
}

std::string BuildReportOnce() {
    std::ostringstream s;
    // ---- General ----
    s << "=== General Information ===\n";
    s << "Manufacturer: " << SysProp("ro.product.manufacturer", "(unknown)") << "\n";
    s << "Model: "        << SysProp("ro.product.model", "(unknown)") << "\n";
    s << "Device name: "  << SysProp("ro.product.device", "(unknown)") << "\n";
    s << "Product: "      << SysProp("ro.product.name", "(unknown)") << "\n";
    s << "Hardware: "     << SysProp("ro.hardware", "(unknown)") << "\n";
    const std::string abis = SysProp("ro.product.cpu.abilist");
    s << "Supported ABIs: "
      << (abis.empty() ? std::string("(unknown)")
                       : std::string(abis).substr(0, abis.find(','))) << "\n";
    s << "Android Version: " << SysProp("ro.build.version.release", "?")
      << " (API " << SysProp("ro.build.version.sdk", "?") << ")\n";
    s << "Security Patch: " << SysProp("ro.build.version.security_patch", "(unknown)") << "\n";
    s << "Build ID: " << SysProp("ro.build.display.id", "(unknown)") << "\n";

    // ---- CPU ----
    const CpuTopology cpu = ReadCpuTopology();
    s << "\n=== CPU Information ===\n";
    s << "SOC: " << cpu.soc << "\n";
    s << "CPUs: " << cpu.coresLine << "\n";
    s << cpu.threads << " Threads\n";
    s << "Features: " << (cpu.featuresLine.empty() ? "(none reported)"
                                                   : cpu.featuresLine) << "\n";
    s << "LLVM CPU: " << (cpu.llvmCpu.empty() ? "(unknown)" : cpu.llvmCpu) << "\n";

    // ---- GPU ----
    s << "\n=== GPU Information ===\n";
    s << ProbeGpuSection() << "\n";

    // ---- Memory ----
    s << "\n=== Memory Information ===\n";
    s << MemorySection();

    return s.str();
}

} // namespace

const std::string& HostInfo::BuildReport() {
    static std::once_flag once;
    static std::string report;
    std::call_once(once, [] { report = BuildReportOnce(); });
    return report;
}

void HostInfo::LogIntoEngineLog() {
    PX5_LOGI(LogCategory::SYSTEM, "host device info:\n%s",
             BuildReport().c_str());
}

} // namespace PX5
