#ifndef PX5_DRIVER_MANAGER_H
#define PX5_DRIVER_MANAGER_H

#include <cstdint>
#include <string>
#include <vector>

namespace PX5 {

// ---------------------------------------------------------------------------
// GpuDriverManager — Vulkan ICD selection state.
//
// Honest contract:
//   * Mode 0 always exists: the SYSTEM loader (libvulkan.so).
//   * Slots 1..N are registered by the Kotlin layer after it has extracted
//     a user-imported driver archive (e.g., Turnip) into app storage and
//     verified it contains an aarch64 Vulkan library. The metadata here is
//     REAL.
//   * Selecting a non-system slot today configures intent only: actual
//     out-of-tree ICD injection requires libadrenotools (linker-namespace
//     bypass) which lands with Phase C. Until then, initialization logs
//     the requested mode loudly and falls back to the system loader —
//     we never claim Turnip is active when the system ICD is in use.
// ---------------------------------------------------------------------------
class GpuDriverManager {
public:
    static GpuDriverManager& GetInstance();

    // Returns the assigned slot id (>=1), or 0 on rejection.
    uint32_t RegisterSlot(const std::string& label,
                          const std::string& soAbsolutePath);

    void   SetActiveMode(uint32_t mode);        // 0 = system
    uint32_t ActiveMode() const { return m_active; }

    std::string SummaryString() const;

private:
    GpuDriverManager() = default;

    struct Slot { std::string label; std::string soPath; };

    std::vector<Slot> m_slots;
    uint32_t m_active = 0;
};

} // namespace PX5

#endif // PX5_DRIVER_MANAGER_H
