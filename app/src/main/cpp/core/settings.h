#ifndef PX5_ENGINE_SETTINGS_H
#define PX5_ENGINE_SETTINGS_H

#include <atomic>
#include <cstdint>

// ---------------------------------------------------------------------------
// Live engine settings bridge (Kotlin SharedPreferences -> JNI -> atomics).
//
// The UI writes these through FexCoreWrapper.nativeApplySettings(); native
// subsystems read them at their natural reconfiguration points:
//   * resScalePct    -> swapchain extent scaling (recreated lazily)
//   * vsyncEnabled   -> present-mode selection (MAILBOX vs FIFO)
//   * verboseLogging -> Logger minimum level (DEBUG vs INFO)
//   * driverMode     -> GpuDriverManager selection echo
// Values are honest engine inputs: changing them changes real behaviour.
// ---------------------------------------------------------------------------
namespace PX5::EngineSettings {

inline std::atomic<int>     resScalePct{100};      // 50 .. 200
inline std::atomic<bool>    vsyncEnabled{true};
inline std::atomic<bool>    verboseLogging{false};
inline std::atomic<uint32_t> driverMode{0};        // 0 = system ICD

} // namespace PX5::EngineSettings

#endif // PX5_ENGINE_SETTINGS_H
