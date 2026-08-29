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
//   * vramUsageMode  -> image memory-type preference in VulkanGpuDevice
// Values are honest engine inputs: changing them changes real behaviour.
// ---------------------------------------------------------------------------
namespace PX5::EngineSettings {

inline std::atomic<int>     resScalePct{100};      // 50 .. 200
inline std::atomic<bool>    vsyncEnabled{true};
inline std::atomic<bool>    verboseLogging{false};
inline std::atomic<uint32_t> driverMode{0};        // 0 = system ICD

// Image memory-type preference, applied wherever VulkanGpuDevice picks a
// memory type for a VkImage/VkBuffer allocation:
//   0 conservative — prefer host-visible (DEVICE_LOCAL not required; keeps
//                    VRAM pressure low; costs bandwidth on upload/download)
//   1 balanced     — prefer DEVICE_LOCAL, accept other types when the heap
//                    is small (current default)
//   2 aggressive   — require DEVICE_LOCAL when a compliant type exists;
//                    allocation fails loudly instead of silently degrading
inline std::atomic<int>     vramUsageMode{1};

// Explicit log level selector (Kotlin Diagnostics). -1 = not chosen yet, so
// the legacy verbose boolean still decides DEBUG vs INFO. 0..5 maps to
// Logger min-level none/error/warn/info/debug/trace.
inline std::atomic<int>     logLevel{-1};

// Swapchain present mode selector, validated at swapchain creation against
// vkGetPhysicalDeviceSurfacePresentModesKHR before it is applied:
//   0 auto (vsync ? FIFO : MAILBOX/IMMEDIATE)  1 FIFO
//   2 FIFO_RELAXED   3 MAILBOX   4 IMMEDIATE
//   5 FIFO_LATEST_READY (only when the device reports it)
inline std::atomic<int>     presentMode{0};

} // namespace PX5::EngineSettings

#endif // PX5_ENGINE_SETTINGS_H
