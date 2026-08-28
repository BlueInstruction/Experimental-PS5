#include "driver_manager.h"
#include "../utils/logger.h"

#include <dlfcn.h>
#include <libgen.h>
#include <cstring>
#include <algorithm>

#ifdef PX5_HAVE_ADRENOTOOLS
#include <adrenotools/driver.h>
#endif

namespace PX5 {

// Every custom Turnip-style Android driver package ships this soname
// (mesa/freedreno Android build convention, used by Winlator & Eden too).
static constexpr const char* kCustomDriverSoname = "libvulkan_adreno.so";

GpuDriverManager& GpuDriverManager::GetInstance() {
    static GpuDriverManager inst;
    return inst;
}

uint32_t GpuDriverManager::RegisterSlot(const std::string& label,
                                        const std::string& soPath) {
    if (label.empty() || soPath.empty()) return 0;
    std::string dir = soPath;
    const auto slash = dir.find_last_of('/');
    dir = (slash == std::string::npos) ? "." : dir.substr(0, slash);
    m_slots.push_back({label, soPath, dir});
    const uint32_t id = static_cast<uint32_t>(m_slots.size());
    PX5_LOGI(LogCategory::GPU, "Driver slot %u registered: %s (%s)",
             id, label.c_str(), soPath.c_str());
    return id;
}

void GpuDriverManager::SetActiveMode(uint32_t mode) {
    if (mode > m_slots.size()) mode = static_cast<uint32_t>(m_slots.size());
    m_active = mode;
    PX5_LOGI(LogCategory::GPU, "Driver mode set: %u (%s)", mode,
             mode == 0 ? "system ICD" : m_slots[mode - 1].label.c_str());
}

void GpuDriverManager::SetRuntimeDirs(const std::string& hookLibDir,
                                      const std::string& tmpLibDir,
                                      const std::string& driverRootDir) {
    m_hookLibDir = hookLibDir;
    m_tmpLibDir  = tmpLibDir;
    m_driverRoot = driverRootDir;
    PX5_LOGI(LogCategory::GPU,
             "Driver runtime dirs: hook=%s tmp=%s root=%s",
             hookLibDir.c_str(), tmpLibDir.c_str(), driverRootDir.c_str());
}

void* GpuDriverManager::OpenHostVulkanLibrary(int dlopenMode) {
    if (m_active == 0) {
        void* sys = dlopen("libvulkan.so", dlopenMode);
        if (!sys) {
            PX5_LOGE(LogCategory::GPU,
                     "system libvulkan dlopen failed: %s", dlerror());
        }
        return sys;
    }

#ifdef PX5_HAVE_ADRENOTOOLS
    const auto& slot = m_slots[m_active - 1];
    PX5_LOGI(LogCategory::GPU,
             "Loading custom driver '%s' via adrenotools from %s",
             slot.label.c_str(), slot.dir.c_str());
    void* handle = adrenotools_open_libvulkan(
            dlopenMode,
            ADRENOTOOLS_DRIVER_CUSTOM,
            m_tmpLibDir.empty() ? nullptr : m_tmpLibDir.c_str(),
            m_hookLibDir.c_str(),
            slot.dir.c_str(),
            kCustomDriverSoname,
            nullptr,               // fileRedirectDir unused for now
            &m_mappingHandle);
    if (handle) {
        PX5_LOGI(LogCategory::GPU,
                 "Custom driver loaded through linker-namespace hook");
        return handle;
    }
    PX5_LOGE(LogCategory::GPU,
             "adrenotools_open_libvulkan returned null for '%s' "
             "(dir=%s soname=%s) — falling back to system ICD",
             slot.label.c_str(), slot.dir.c_str(), kCustomDriverSoname);
    SetActiveMode(0);   // honest fallback, reflected in summaries
    return dlopen("libvulkan.so", dlopenMode);
#else
    PX5_LOGW(LogCategory::GPU,
             "Custom driver requested but this build has no adrenotools "
             "support compiled in — using system ICD");
    return dlopen("libvulkan.so", dlopenMode);
#endif
}

std::string GpuDriverManager::SummaryString() const {
    std::string s = "mode=" + std::to_string(m_active) +
                    " slots=" + std::to_string(m_slots.size());
    for (size_t i = 0; i < m_slots.size(); ++i) {
        s += " | slot" + std::to_string(i + 1) + "=" + m_slots[i].label;
    }
#ifdef PX5_HAVE_ADRENOTOOLS
    s += " | adrenotools=yes";
#else
    s += " | adrenotools=no";
#endif
    return s;
}

} // namespace PX5
