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
//   * No fabricated "driver installed" state ever leaves this class.
// ---------------------------------------------------------------------------
class GpuDriverManager {
public:
    static GpuDriverManager& GetInstance();

    // Returns the assigned slot id (>=1), or 0 on rejection.
    uint32_t RegisterSlot(const std::string& label,
                          const std::string& soAbsolutePath);

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

    std::string SummaryString() const;

private:
    GpuDriverManager() = default;

    struct Slot { std::string label; std::string soPath; std::string dir; };

    std::vector<Slot> m_slots;
    uint32_t m_active = 0;
    std::string m_hookLibDir, m_tmpLibDir, m_driverRoot;
    void* m_mappingHandle = nullptr;
};

} // namespace PX5

#endif // PX5_DRIVER_MANAGER_H
