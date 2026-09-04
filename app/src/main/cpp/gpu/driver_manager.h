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

/**
 * GPU driver manager for Vulkan ICD selection (system vs. imported drivers).
 */
class GpuDriverManager {
public:
    /**
     * Returns the singleton GpuDriverManager instance.
     * @return Reference to singleton
     */
    static GpuDriverManager& GetInstance();

    /**
     * Registers a driver slot for a user-imported Turnip/Mesa driver.
     * @param label Human-readable label (e.g., "Turnip v26.3.0")
     * @param soAbsolutePath Absolute path to driver .so file
     * @param soname Exact file name the loader must open (from meta.json libraryName)
     * @return Assigned slot ID (>=1), or 0 on rejection
     */
    uint32_t RegisterSlot(const std::string& label,
                          const std::string& soAbsolutePath,
                          const std::string& soname);

    /**
     * Drops every registered slot and falls back to system ICD.
     * Used when user removes imported drivers; slots are re-registered from store.
     */
    void   ClearSlots();

    /**
     * Sets the active driver mode (0 = system, 1..N = slot).
     * @param mode Driver mode/slot ID
     */
    void   SetActiveMode(uint32_t mode);

    /**
     * Returns the current active driver mode.
     * @return Active mode (0 = system, 1..N = slot ID)
     */
    uint32_t ActiveMode() const { return m_active; }

    /**
     * Sets runtime directories from JNI (Kotlin context paths).
     * @param hookLibDir ApplicationInfo.nativeLibraryDir (required by adrenotools hook)
     * @param tmpLibDir context.cacheDir for patched libs (API<29)
     * @param driverRootDir App files dir where driver slots live
     */
    void   SetRuntimeDirs(const std::string& hookLibDir,
                          const std::string& tmpLibDir,
                          const std::string& driverRootDir);

    /**
     * Opens Vulkan library (system or custom driver via adrenotools).
     * Central loader used by vulkan_device.cpp instead of raw dlopen.
     * @param dlopenMode dlopen mode flags (RTLD_NOW, etc.)
     * @return dlopen-style handle, or nullptr on failure (logged)
     */
    void*  OpenHostVulkanLibrary(int dlopenMode);

    /**
     * Verifies chosen custom driver is actually mapped (post-binding proof).
     * Must be called AFTER first Vulkan instance-level call (vkCreateInstance).
     * adrenotools can silently fall back to system driver while returning valid handle.
     * @return true if driver confirmed in /proc/self/maps or verification inconclusive,
     *         false if driver definitively absent
     */
    bool VerifyActiveDriverMapped();

    /**
     * Eagerly verifies active driver by directly preloading the .so file.
     * No GPU context needed; makes verification meaningful at import time.
     * @return true if preload succeeded, false otherwise
     */
    bool PreloadActiveDriverForVerification();

    /**
     * Returns human-readable summary of driver state.
     * @return Summary string with active mode, slots, verification status
     */
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
