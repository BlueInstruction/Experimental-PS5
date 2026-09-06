#include "driver_manager.h"
#include "../utils/logger.h"

#include <dlfcn.h>
#include <libgen.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <android/dlext.h>
#ifdef PX5_HAVE_ADRENOTOOLS
// NOTE: the linker-namespace APIs (android_create_namespace and friends)
// have NO public NDK header — <android/linker.h> does not exist. The
// pinned adrenotools build already compiles liblinkernsbypass, whose
// android_linker_ns.h declares the real contracts (constants included:
// ANDROID_NAMESPACE_TYPE_SHARED = 2) and resolves the __loader_ symbols
// from libdl_android at runtime. We reuse that target instead of guessing.
#include <android_linker_ns.h>
#endif
#include <cstring>
#include <cerrno>
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

// One-line inventory of the slot directory: name(size) pairs, the loaded
// soname starred. A driver package missing a bundled dependency (libc++_
// shared.so, libz.so, ...) is visible here without any logcat.
void LogSlotInventory(const std::string& dir, const std::string& soname) {
    DIR* d = opendir(dir.c_str());
    if (!d) {
        PX5_LOGE(LogCategory::GPU,
                 "Slot dir unreadable: %s (errno=%d)", dir.c_str(), errno);
        return;
    }
    std::string inv;
    int files = 0, shown = 0;
    struct dirent* e;
    while ((e = readdir(d)) != nullptr) {
        if (e->d_name[0] == '.') continue;
        std::string full = dir + (dir.back() == '/' ? "" : "/") + e->d_name;
        struct stat st{};
        if (stat(full.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) continue;
        ++files;
        if (shown++ < 12) {
            char sz[24];
            snprintf(sz, sizeof sz, "%zu", static_cast<size_t>(st.st_size));
            if (!inv.empty()) inv += ", ";
            inv += std::string(e->d_name) + "(" + sz + "B)";
            if (soname == e->d_name) inv += "*";
        }
    }
    closedir(d);
    PX5_LOGI(LogCategory::GPU,
             "Slot inventory (%d files%s): %s", files,
             shown < files ? ", first 12 shown" : "",
             inv.c_str());
}

// v1.36 — the platform library directories the namespace searches AFTER the
// slot dir. The 2026-09-02 vc36 session showed the real blocker: 2026-era
// AdrenoTools-style Turnip packages (e.g. "Turnip v26.3.0-R4") carry
// DT_NEEDED entries for NON-public platform libs (libhardware.so) that no
// app-visible namespace links by default — SHARED shares only the public
// library list. Searching the device's own lib dirs mirrors exactly what
// the system sphal namespace does when IT loads /vendor's vulkan.adreno.so:
// vendor dir first, then the platform set. Bundled deps still win (slot dir
// is first), and the import pipeline additionally COPIES the needed
// platform libs into the slot dir (see MainActivity's dependency bundler),
// so both this preload and the adrenotools hook at vkCreateInstance see
// the same file set.
#ifdef PX5_HAVE_ADRENOTOOLS
std::vector<std::string> PlatformLibDirs() {
    return {
        "/odm/lib64", "/vendor/lib64", "/system/vendor/lib64",
        "/system_ext/lib64", "/system/lib64",
    };
}

std::string SlotFirstLdPath(const std::string& slotDir) {
    std::string ld = slotDir.back() == '/' ? slotDir : slotDir + "/";
    for (const auto& p : PlatformLibDirs()) { ld += ":" + p; }
    return ld;
}
#endif

// Replicates what the adrenotools hook does internally — an isolated,
// system-sharing linker namespace whose search path is the driver dir —
// and reports the REAL linker error for the driver soname. Runs only on
// the failure path, so a future adrenotools null can never again leave us
// guessing between "package problem" and "hook plumbing problem".
// Uses liblinkernsbypass (already in the adrenotools link graph) for the
// namespace APIs — the NDK ships no public header for them.
#ifdef PX5_HAVE_ADRENOTOOLS
void NamespaceDlopenProbe(const std::string& dir, const std::string& soname) {
    if (!linkernsbypass_load_status()) {
        PX5_LOGE(LogCategory::GPU,
                 "DiagNS probe: linkernsbypass failed to initialize — probe "
                 "unavailable on this device");
        return;
    }
    const std::string dirSlash = dir.back() == '/' ? dir : dir + "/";
    const std::string ldPath = SlotFirstLdPath(dir);
    struct android_namespace_t* ns = android_create_namespace(
            "px5-driver-diag",
            ldPath.c_str(),            // slot dir first, then platform dirs
            nullptr,
            ANDROID_NAMESPACE_TYPE_SHARED,   // share the parent's public libs
            dirSlash.c_str(),          // permitted path for app-files dir
            nullptr);                  // parent = caller namespace
    if (!ns) {
        PX5_LOGE(LogCategory::GPU,
                 "DiagNS probe: android_create_namespace failed: %s",
                 dlerror());
        return;
    }
    const std::string path = dirSlash + soname;
    void* h = linkernsbypass_namespace_dlopen(path.c_str(), RTLD_NOW, ns);
    if (h) {
        PX5_LOGI(LogCategory::GPU,
                 "DiagNS probe: '%s' dlopens FINE in its own namespace — "
                 "the package is loadable; adrenotools failed in its own "
                 "hook plumbing. Full story stays in this log.",
                 soname.c_str());
        dlclose(h);
    } else {
        // dlerror() clears its message after the first read — capture once.
        const char* err = dlerror();
        PX5_LOGE(LogCategory::GPU,
                 "DiagNS probe: driver itself fails to load: %s — if a "
                 "DT_NEEDED library is named there, it is missing from the "
                 "driver package (see slot inventory above)",
                 err ? err : "(dlerror empty)");
    }
}

// The LAST thing adrenotools does (pinned fork src/driver.cpp:116) is
// linkernsbypass_namespace_dlopen_unique("/system/lib64/libvulkan.so",
// tmpLibDir, flags, hookNs) — and it is the only failure point in the whole
// call chain that emits NO logcat line at all: the fd creation, the soname
// patch and the final android_dlopen_ext all fail silently there. This
// probe reproduces that exact step with a namespace of our own and reads
// the dlerror() string adrenotools never reads.
// Honest scope note: our namespace is not the hook's (ld path differs), so
// a probe SUCCESS narrows the remaining failure to the hook's own plumbing
// (its namespace link / its hook libs — those DO log to logcat tag
// 'adrenotools'), while a probe FAILURE names the real linker error either
// way. Both outcomes add a fact; neither guesses.
void FinalStepProbe(const std::string& tmpLibDir, int dlopenMode) {
    const bool memfdMode = tmpLibDir.empty();
    if (memfdMode) {
        PX5_LOGI(LogCategory::GPU,
                 "FinalStep probe: no tmpLibDir configured — adrenotools "
                 "runs its last step through memfd (a silent ENOSYS there "
                 "fails the call with zero log output)");
    } else if (access(tmpLibDir.c_str(), W_OK) != 0) {
        PX5_LOGE(LogCategory::GPU,
                 "FinalStep probe: tmpLibDir '%s' is NOT writable (errno=%d)"
                 " — dlopen_unique cannot create its <N>_patched.so copy "
                 "there, the call dies silently at that exact point",
                 tmpLibDir.c_str(), errno);
        return;
    } else {
        // How far earlier attempts actually got: dlopen_unique names its
        // artifacts "<N>_patched.so" (N increments per call).
        DIR* d = opendir(tmpLibDir.c_str());
        int artifacts = 0;
        std::string names;
        if (d) {
            struct dirent* e;
            while ((e = readdir(d)) != nullptr) {
                const std::string name = e->d_name;
                static constexpr const char kSuffix[] = "_patched.so";
                if (name.size() >= sizeof kSuffix - 1 &&
                    name.compare(name.size() - (sizeof kSuffix - 1),
                                 sizeof kSuffix - 1, kSuffix) == 0) {
                    ++artifacts;
                    if (names.size() < 160) names += name + " ";
                }
            }
            closedir(d);
        }
        PX5_LOGI(LogCategory::GPU,
                 "FinalStep probe: tmpLibDir writable, %d previous "
                 "_patched.so artifacts%s%s", artifacts,
                 artifacts ? ": " : "(none yet)", names.c_str());
    }

    struct stat sysv{};
    if (stat("/system/lib64/libvulkan.so", &sysv) != 0) {
        PX5_LOGE(LogCategory::GPU,
                 "FinalStep probe: /system/lib64/libvulkan.so stat failed "
                 "(errno=%d) — the soname patch would fail reading it",
                 errno);
        return;
    }

    struct android_namespace_t* ns = android_create_namespace(
            "px5-final-step-diag",
            nullptr,                                   // ld_library_path
            nullptr,                                   // default_library_path
            ANDROID_NAMESPACE_TYPE_SHARED,             // share system libs
            memfdMode ? nullptr : tmpLibDir.c_str(),   // permitted path
            nullptr);                                  // caller namespace
    if (!ns) {
        const char* err = dlerror();
        PX5_LOGE(LogCategory::GPU,
                 "FinalStep probe: android_create_namespace failed: %s",
                 err ? err : "(dlerror empty)");
        return;
    }

    void* h = linkernsbypass_namespace_dlopen_unique(
            "/system/lib64/libvulkan.so",
            memfdMode ? nullptr : tmpLibDir.c_str(), dlopenMode, ns);
    if (h) {
        PX5_LOGI(LogCategory::GPU,
                 "FinalStep probe: patched system libvulkan loads FINE — "
                 "an adrenotools null is then inside its own hook plumbing "
                 "(hook-namespace link or hook lib load; those paths DO "
                 "log to logcat tag 'adrenotools')");
        dlclose(h);
    } else {
        const char* err = dlerror();
        PX5_LOGE(LogCategory::GPU,
                 "FinalStep probe: EXACT last-step replication FAILED: %s — "
                 "this is the error string adrenotools never surfaces",
                 err ? err : "(dlerror empty — linker failed silently)");
    }
}

#endif  // PX5_HAVE_ADRENOTOOLS

// v1.18 — the load mechanism custom-driver packages actually need.
// A plain dlopen() runs in the app's classloader-namespace, which Android
// builds ISOLATED: only /system/etc/public_libraries.txt entries and app
// libraries are reachable. Mesa Turnip packages carry DT_NEEDED entries
// for NON-public system libs (libcutils.so) — the plain dlopen fails with
// "library libcutils.so not found" and the old verification answered
// "unknown" even when the driver was perfectly loadable (2026-08-30
// device logs). SharpDroid loads the SAME zip on the SAME device because
// its loader dlopens through a SHARED linker namespace: that namespace
// type shares the system namespace's library list, where libcutils.so
// lives. We do the same here — android_create_namespace(SHARED) with the
// driver dir as ld_library_path so bundled deps (libc++_shared.so, ...)
// resolve next to the driver, system deps via the shared link.
// Guarded by PX5_HAVE_ADRENOTOOLS: the namespace APIs arrive through the
// liblinkernsbypass target, which is linked only in that configuration.
#ifdef PX5_HAVE_ADRENOTOOLS
void* DlopenDriverSharedNamespace(const std::string& dir,
                                  const std::string& soname,
                                  std::string& errOut) {
    if (!linkernsbypass_load_status()) {
        errOut = "linkernsbypass failed to initialize";
        return nullptr;
    }
    const std::string dirSlash = dir.back() == '/' ? dir : dir + "/";
    const std::string ldPath = SlotFirstLdPath(dir);
    struct android_namespace_t* ns = android_create_namespace(
            "px5-driver-load",
            ldPath.c_str(),            // slot dir first, then platform dirs
            nullptr,
            ANDROID_NAMESPACE_TYPE_SHARED,   // public system libs reachable
            dirSlash.c_str(),          // permitted path for app-files dir
            nullptr);                  // parent = caller namespace
    if (!ns) {
        const char* e = dlerror();
        errOut = std::string("android_create_namespace failed: ") +
                 (e ? e : "(no detail)");
        return nullptr;
    }
    const std::string path = dirSlash + soname;
    void* h = linkernsbypass_namespace_dlopen(path.c_str(),
                                              RTLD_NOW | RTLD_LOCAL, ns);
    if (!h) {
        const char* e = dlerror();   // read once — dlerror clears it
        errOut = e ? e : "(linker failed silently)";
    }
    return h;
}
#endif

} // namespace

GpuDriverManager& GpuDriverManager::GetInstance() {
    static GpuDriverManager inst;
    return inst;
}

uint32_t GpuDriverManager::RegisterSlot(const std::string& label,
                                        const std::string& soPath,
                                        const std::string& soname) {
    if (label.empty() || soPath.empty() || soname.empty()) return 0;
    std::string dir = soPath;
    const auto slash = dir.find_last_of('/');
    dir = (slash == std::string::npos) ? "." : dir.substr(0, slash);
    m_slots.push_back({label, soPath, dir, soname});
    const uint32_t id = static_cast<uint32_t>(m_slots.size());
    PX5_LOGI(LogCategory::GPU, "Driver slot %u registered: %s (%s, soname=%s)",
             id, label.c_str(), soPath.c_str(), soname.c_str());
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
    if (m_active != mode) {
        // A verdict computed for the previous slot says nothing about the
        // new one — recompute on demand (eager verify or real init).
        m_verifyState = 0;
        m_verifyDetail.clear();
    }
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
    // Own the storage: a const& bound to a conditional-expression temporary
    // is valid C++ (lifetime-extended) but reads as dangling to static
    // lifetime analysis, so keep the value in a named local.
    const std::string soname =
        slot.soname.empty() ? std::string(kCustomDriverSoname) : slot.soname;
    // ROOT CAUSE (2026-08-29 device logs): adrenotools concatenates
    // customDriverDir + customDriverName WITHOUT a separator (pinned fork
    // driver.cpp:55 stat check) — the directory must carry a trailing '/'.
    // Without it the stat looked for ".../turnip_<ts>libvulkan_freedreno.so",
    // returned ENOENT, and the call failed with every file actually present.
    // Sharpdroid's own log on the same device confirms the contract:
    //   "adrenotools: libvulkan_freedreno.so injected from .../turnip-.../".
    std::string dirForTools = slot.dir;
    if (!dirForTools.empty() && dirForTools.back() != '/') dirForTools += '/';
    PX5_LOGI(LogCategory::GPU,
             "Loading custom driver '%s' via adrenotools from %s (soname=%s)",
             slot.label.c_str(), dirForTools.c_str(), soname.c_str());
    // ROOT CAUSE #2 (found by source-reading the pinned fork, driver.cpp
    // lines 41-44): adrenotools returns nullptr IMMEDIATELY when a non-null
    // userMappingHandle is passed without ADRENOTOOLS_DRIVER_GPU_MAPPING_
    // IMPORT in featureFlags. v1.4 passed &m_mappingHandle with only
    // ADRENOTOOLS_DRIVER_CUSTOM — every call therefore died at that gate,
    // BEFORE the hook libs were even dlopened, which is exactly why the
    // device log showed null with hookImpl/hookMain/driverSo all present.
    // The corresponding logcat line ("ADRENOTOOLS_DRIVER_GPU_MAPPING_IMPORT
    // present but no user mapping handle found") is misleadingly worded in
    // the fork; the contract is: no flag -> no handle. We do not use GPU
    // mapping import yet, so pass nullptr. m_mappingHandle stays reserved
    // for the Phase-C mapped-memory work, where the flag is set properly.
    void* handle = adrenotools_open_libvulkan(
            dlopenMode,
            ADRENOTOOLS_DRIVER_CUSTOM,
            m_tmpLibDir.empty() ? nullptr : m_tmpLibDir.c_str(),
            m_hookLibDir.c_str(),
            dirForTools.c_str(),
            soname.c_str(),
            nullptr,               // fileRedirectDir unused for now
            nullptr);              // userMappingHandle: null without the flag
    if (handle) {
        // Honest wording: the returned handle is the patched SYSTEM
        // libvulkan. The custom ICD itself is dlopened LATER, inside the
        // hook, at the loader's first android_dlopen_ext for the driver —
        // a failure there logs to logcat and falls back to the system
        // driver silently. VerifyActiveDriverMapped() is the real proof.
        PX5_LOGI(LogCategory::GPU,
                 "Custom driver wired through linker-namespace hook — ICD "
                 "loads at first vkCreateInstance; proof pending maps check");
        return handle;
    }

    // Adrenotools gives us no error string, so narrow the cause ourselves.
    // Its loader dlopens "libhook_impl.so" and "libmain_hook.so" from
    // hookLibDir, then the driver soname from the slot dir — each missing
    // piece has a distinct, honest signature in the log.
    {
        const std::string hookImpl = m_hookLibDir + "/libhook_impl.so";
        const std::string hookMain = m_hookLibDir + "/libmain_hook.so";
        const bool haveImpl  = access(hookImpl.c_str(), F_OK) == 0;
        const bool haveMain  = access(hookMain.c_str(), F_OK) == 0;
        const std::string driverSo = slot.dir + "/" + soname;
        const bool haveDriver = access(driverSo.c_str(), F_OK) == 0;
        PX5_LOGE(LogCategory::GPU,
                 "adrenotools_open_libvulkan returned null for '%s' "
                 "(dir=%s soname=%s) — hookImpl=%s hookMain=%s driverSo=%s",
                 slot.label.c_str(), dirForTools.c_str(), soname.c_str(),
                 haveImpl ? "yes" : "MISSING",
                 haveMain ? "yes" : "MISSING",
                 haveDriver ? "yes" : "MISSING");
        if (!haveImpl || !haveMain) {
            PX5_LOGE(LogCategory::GPU,
                     "Runtime hook libraries are not installed in %s — the APK "
                     "build did not package them; custom drivers cannot load "
                     "until the build ships libhook_impl.so + libmain_hook.so",
                     m_hookLibDir.c_str());
        } else if (haveDriver) {
            // All files exist, so the null came from deeper. Produce the
            // facts that separate a bad package from hook plumbing: what
            // the slot dir actually contains, whether the driver soname
            // loads in a properly-built linker namespace at all, and a
            // replication of adrenotools' silent final step.
            LogSlotInventory(slot.dir, soname);
            NamespaceDlopenProbe(slot.dir, soname);
            FinalStepProbe(m_tmpLibDir, dlopenMode);
        }
    }
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
    const std::string soname =
        slot.soname.empty() ? std::string(kCustomDriverSoname) : slot.soname;

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
            m_tmpLibDir + "/" + soname);
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
        if (!found && n > soname.size() &&
            memcmp(end - soname.size(), soname.c_str(),
                   soname.size()) == 0) {
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

bool GpuDriverManager::PreloadActiveDriverForVerification() {
    // v1.16 — eager verification without a Vulkan instance.
    // The hook wires the ICD at the loader's first vkCreateInstance, but on
    // API >= 29 the driver file itself is dlopen-able from the app's own
    // private storage. Preloading it maps the EXACT library the instance
    // will bind, so the maps check becomes meaningful before any render
    // work — and the driver manager stops reporting "not-run" for slots
    // that were imported but never exercised yet. Loading a driver library
    // without creating an instance starts no GPU context (safe in-process).
    if (m_active == 0) {
        m_verifyState = 1;
        m_verifyDetail = "system ICD — nothing to verify";
        return true;
    }
#ifndef PX5_HAVE_ADRENOTOOLS
    m_verifyState = 3;
    m_verifyDetail = "no adrenotools in build";
    return false;
#else
    if (m_active > m_slots.size()) {
        m_verifyState = 3;
        m_verifyDetail = "active slot out of range";
        return false;
    }
    const auto& slot = m_slots[m_active - 1];
    const std::string soname =
        slot.soname.empty() ? std::string(kCustomDriverSoname) : slot.soname;

    std::vector<std::string> candidates;
    candidates.push_back(slot.dir + "/" + soname);
    if (!m_tmpLibDir.empty()) {
        // API < 29 path: the hook's patched copy is what actually loads.
        candidates.push_back(m_tmpLibDir + "/" + soname);
    }

    // v1.18 — two mechanisms per candidate, both honest:
    //   1. plain dlopen — works for self-contained packages on API >= 29;
    //   2. SHARED-namespace dlopen — the mechanism custom-driver loading
    //      actually requires when the package links non-public system
    //      libs (Turnip's DT_NEEDED libcutils.so). The classloader
    //      namespace cannot resolve those; a shared namespace can (this
    //      is why the same zip loads in SharpDroid on the same device).
    // A success through EITHER mechanism maps the exact library the
    // instance will bind, so the subsequent /proc/self/maps check means
    // what it says.
    std::string plainErr, nsErr;
    for (const auto& cand : candidates) {
        void* h = dlopen(cand.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (h) {
            m_verifyDetail = "preloaded via dlopen: " + cand;
            PX5_LOGI(LogCategory::GPU,
                     "Driver preload for verification OK: %s", cand.c_str());
            return true;
        }
        const char* e = dlerror();
        if (plainErr.empty()) plainErr = e ? e : "unknown dlopen error";

#ifdef PX5_HAVE_ADRENOTOOLS
        nsErr.clear();
        h = DlopenDriverSharedNamespace(slot.dir, soname, nsErr);
        if (h) {
            m_verifyDetail = "preloaded via SHARED linker namespace: " + cand +
                             " (plain dlopen failed: " + plainErr + " — the "
                             "classloader namespace cannot resolve the "
                             "package's non-public system deps; the shared "
                             "namespace can, same mechanism as the loader "
                             "hook)";
            PX5_LOGI(LogCategory::GPU,
                     "Driver preload OK via shared namespace: %s", cand.c_str());
            return true;
        }
        if (nsErr.empty()) nsErr = "unknown namespace-dlopen error";
#endif
    }
    // Unknown, not absent: these preloads never create a Vulkan instance;
    // the hooked load at vkCreateInstance remains the final proof. Say so.
    m_verifyState = 3;
    m_verifyDetail = "preload dlopen failed (" + plainErr + ")" +
#ifdef PX5_HAVE_ADRENOTOOLS
                     std::string(" and shared-namespace load failed (") +
                     nsErr + ")" +
#endif
                     "; final proof at first vkCreateInstance";
    // Conditional arguments live in locals: a preprocessor directive inside a
    // macro argument list is undefined behavior (cppcheck refuses to expand it).
#ifdef PX5_HAVE_ADRENOTOOLS
    const char* sharedNsTag = " | shared-ns: ";
    const char* sharedNsErr = nsErr.c_str();
#else
    const char* sharedNsTag = "";
    const char* sharedNsErr = "";
#endif
    PX5_LOGW(LogCategory::GPU,
             "Driver preload via plain dlopen unavailable for '%s' "
             "(INCONCLUSIVE, not a failure verdict — the shared-namespace "
             "search now covers the slot dir plus the platform lib dirs, and "
             "the importer bundles non-public platform deps into the slot; "
             "the designed load is the adrenotools hook at first "
             "vkCreateInstance, proven by the maps check): plain: %s%s%s",
             slot.label.c_str(), plainErr.c_str(),
             sharedNsTag, sharedNsErr);
    return false;
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
