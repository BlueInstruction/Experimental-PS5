#include "driver_manager.h"
#include "../utils/logger.h"

#include <dlfcn.h>
#include <libgen.h>
#include <cstring>
#include <cstdlib>
#include <algorithm>

#ifdef PX5_HAVE_ADRENOTOOLS
#include <adrenotools/driver.h>
#endif

namespace PX5 {

// Every custom Turnip-style Android driver package ships this soname
// (mesa/freedreno Android build convention, used by Winlator & Eden too).
static constexpr const char* kCustomDriverSoname = "libvulkan_adreno.so";

namespace {

// /proc/self/maps reports app-private paths under /data/data/<pkg>, while
// the framework hands out /data/user/0/<pkg> for the same directory. Both
// spellings are the whole of the path check; a realpath that fails answers
// "unknown" rather than "no" (empty return).
std::string PathForMapsCheck(const std::string& path) {
    if (path.empty()) return {};
    char* r = realpath(path.c_str(), nullptr);
    if (!r) return {};
    std::string out(r);
    free(r);
    return out;
}

} // namespace

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

void GpuDriverManager::ClearSlots() {
    m_slots.clear();
    m_active = 0;
    // Any previous verification answered for an old slot set; make it
    // recomputed against whatever gets registered next.
    m_verifyState = 0;
    m_verifyDetail.clear();
    PX5_LOGI(LogCategory::GPU,
             "Driver slots cleared — back to system ICD until re-registered");
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

bool GpuDriverManager::VerifyActiveDriverMapped() {
    if (m_verifyState != 0) return m_verifyState != 2;   // computed once

    if (m_active == 0) {
        m_verifyState = 1;
        m_verifyDetail = "system ICD — nothing to verify";
        return true;
    }
#ifndef PX5_HAVE_ADRENOTOOLS
    m_verifyState = 3;
    m_verifyDetail = "no adrenotools in build";
    return true;
#else
    const auto& slot = m_slots[m_active - 1];

    // Candidate paths the loader may legitimately map the driver from:
    //   1. the exact file registered (both /data spellings),
    //   2. the copy adrenotools itself patches into tmpLibDir on the
    //      API < 29 path, where the hook loads its patched output, not ours.
    std::vector<std::string> candidates;
    std::string real = PathForMapsCheck(slot.soPath);
    if (!real.empty()) candidates.push_back(real);

    static constexpr const char* kUserPrefix = "/data/user/0/";
    if (slot.soPath.rfind(kUserPrefix, 0) == 0) {
        std::string realAlt = PathForMapsCheck(
            std::string("/data/data/") + slot.soPath.substr(strlen(kUserPrefix)));
        if (!realAlt.empty()) candidates.push_back(realAlt);
    }

    if (!m_tmpLibDir.empty()) {
        std::string realPatched = PathForMapsCheck(
            m_tmpLibDir + "/" + kCustomDriverSoname);
        if (!realPatched.empty()) candidates.push_back(realPatched);
    }

    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) {
        // Unknown, not absent: an unreadable map is not evidence of absence.
        m_verifyState = 3;
        m_verifyDetail = "cannot open /proc/self/maps";
        PX5_LOGW(LogCategory::GPU,
                 "Driver verification unknown: cannot open /proc/self/maps");
        return true;
    }

    bool found = false;
    char line[1024];
    while (!found && fgets(line, sizeof line, f)) {
        // The pathname is the last whitespace-separated field; lines for
        // anonymous mappings have none.
        const char* end = line + strlen(line);
        while (end > line && (end[-1] == '\n' || end[-1] == ' ')) --end;
        const char* p = end;
        while (p > line && p[-1] != ' ') --p;
        if (p == end) continue;
        const size_t n = static_cast<size_t>(end - p);

        for (const auto& c : candidates) {
            if (c.size() == n && memcmp(p, c.c_str(), n) == 0) {
                found = true;
                break;
            }
        }
        if (!found && n > strlen(kCustomDriverSoname) &&
            memcmp(end - strlen(kCustomDriverSoname), kCustomDriverSoname,
                   strlen(kCustomDriverSoname)) == 0) {
            // A mapping of our soname from the slot dir or the tmp dir —
            // covers layout surprises the exact-path list cannot name.
            const std::string path(p, n);
            if ((!m_tmpLibDir.empty() &&
                 path.rfind(m_tmpLibDir, 0) == 0) ||
                path.rfind(slot.dir, 0) == 0) {
                found = true;
            }
        }
    }
    fclose(f);

    m_verifyState = found ? 1 : 2;
    m_verifyDetail = found
        ? ("verified mapped: " + (real.empty() ? slot.soPath : real))
        : ("chosen driver NOT in /proc/self/maps — "
           "the hook fell back to the system driver");

    if (found) {
        PX5_LOGI(LogCategory::GPU,
                 "Custom driver '%s' verified in /proc/self/maps",
                 slot.label.c_str());
    } else {
        // Loud, because a run that quietly renders through the system driver
        // looks exactly like a successful injection that did not help.
        PX5_LOGE(LogCategory::GPU,
                 "VERIFICATION FAILED: driver '%s' (%s) is NOT mapped — "
                 "adrenotools returned a handle but the system driver is in "
                 "use; every driver measurement in this run is invalid",
                 slot.label.c_str(), slot.soPath.c_str());
    }
    return found;
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
    switch (m_verifyState) {
        case 1:  s += " | driverVerified=yes"; break;
        case 2:  s += " | driverVerified=NO";  break;
        case 3:  s += " | driverVerified=unknown"; break;
        default: s += " | driverVerified=not-run"; break;
    }
    if (!m_verifyDetail.empty()) {
        s += " (" + m_verifyDetail + ")";
    }
    return s;
}

} // namespace PX5
