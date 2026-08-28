#ifndef PX5_DRIVER_MANAGER_H
#define PX5_DRIVER_MANAGER_H

#include <cstdint>
#include <string>
#include <vector>

namespace PX5 {

// ---------------------------------------------------------------------------
// GpuDriverManager — real Vulkan ICD selection through libadrenotools.
//
// Contract (mirrors Winlator/Eden behavior):
//   * Mode 0 = SYSTEM loader (dlopen "libvulkan.so").
//   * Slots 1..N = user-imported Turnip/other drivers, extracted by the
//     Kotlin layer into app-private storage and registered here with the
//     real directory holding libvulkan_adreno.so.
//   * When a non-system slot is active, OpenHostVulkanLibrary() loads the
//     driver through adrenotools_open_libvulkan() (linker-namespace hook),
//     exactly the path used by Winlator and the Eden emulator.
//   * The hook can silently fall back to the system driver while still
//     returning a usable handle, so VerifyActiveDriverMapped() proves the
//     chosen driver against /proc/self/maps after the ICD is bound.
//   * No fabricated "driver installed" state ever leaves this class.
// ---------------------------------------------------------------------------
class GpuDriverManager {
public:
    static GpuDriverManager& GetInstance();

    // Returns the assigned slot id (>=1), or 0 on rejection.
    // soname = the exact file name inside dir the loader must open —
    // normally meta.json's "libraryName" (vulkan.ad0863.so,
    // libvulkan_freedreno.so, ...), or the normalized libvulkan_adreno.so
    // when a package carries no meta.json. The loader never guesses.
    uint32_t RegisterSlot(const std::string& label,
                          const std::string& soAbsolutePath,
                          const std::string& soname);

    // Drops every registered slot and falls back to the system ICD.
    // Used by the UI when the user removes an imported driver: slots are
    // then re-registered from the persisted store so ids stay consistent.
    void   ClearSlots();

    void   SetActiveMode(uint32_t mode);        // 0 = system
    uint32_t ActiveMode() const { return m_active; }

    // Runtime directories supplied once from JNI (Kotlin context paths):
    //   hookLibDir  = ApplicationInfo.nativeLibraryDir (required by the hook)
    //   tmpLibDir   = context.cacheDir for patched libs (api<29 path)
    //   driverRoot  = app files dir where driver slots live
    void   SetRuntimeDirs(const std::string& hookLibDir,
                          const std::string& tmpLibDir,
                          const std::string& driverRootDir);

    // Central loader used by vulkan_device.cpp instead of a raw dlopen.
    // Returns a dlopen-style handle, or nullptr with the failure logged.
    void*  OpenHostVulkanLibrary(int dlopenMode);

    // Post-binding proof that the chosen custom driver is really in use.
    // A non-null handle from adrenotools is NOT that proof: the hook can
    // silently fall back to the system driver and still return a perfectly
    // good loader handle. Call once AFTER the first instance-level Vulkan
    // call (vkCreateInstance) — the Android loader binds an ICD on its first
    // entry point, not at dlopen, so checking earlier would condemn every
    // driver ever loaded.
    // Answers: true when the driver is confirmed mapped in /proc/self/maps,
    // false on definite absence. An unreadable map or an unresolvable path
    // answers "unknown" (true): the expensive mistake is condemning a
    // driver that did load. Result is computed once and cached.
    bool VerifyActiveDriverMapped();

    std::string SummaryString() const;

private:
    GpuDriverManager() = default;

    // 0 = not yet run, 1 = verified, 2 = definitely absent, 3 = unknown
    int         m_verifyState = 0;
    std::string m_verifyDetail;

    struct Slot {
        std::string label;
        std::string soPath;
        std::string dir;
        std::string soname;   // exact file name the loader opens
    };

    std::vector<Slot> m_slots;
    uint32_t m_active = 0;
    std::string m_hookLibDir, m_tmpLibDir, m_driverRoot;
    void* m_mappingHandle = nullptr;
};

} // namespace PX5

#endif // PX5_DRIVER_MANAGER_H
