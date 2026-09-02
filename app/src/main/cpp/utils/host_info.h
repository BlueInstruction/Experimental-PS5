// SPDX-License-Identifier: MIT
// PX5 — HostInfo: Eden-style host documentation block.
//
// Every support paste must answer "what hardware produced this?" on its
// own. Eden documents the device at the top of every log; from v1.23 the
// PX5 engine log does the same, and the Settings > Debug > About device
// panel renders the exact same block:
//
//   === General Information ===
//   Manufacturer: nubia
//   Model: NX779J
//   Device name: PQ83A06
//   Product: PQ83A06-EEA
//   Hardware: qcom
//   Supported ABIs: arm64-v8a
//   Android Version: 16 (API 36)
//   Security Patch: 2026-05-01
//   Build ID: BQ2A.250705.001-BP2A.250605.031.A3
//
//   === CPU Information ===
//   SOC: SM8650
//   CPUs: 2x Cortex-A520 + 5x Cortex-A720 + 1x Cortex-X4
//   8 Threads
//   Features: NEON+DP+I8MM+BF16 | Crypto | LSE
//   LLVM CPU: Cortex-A520
//
//   === GPU Information ===
//   GPU Model: Adreno (TM) 750
//   Vulkan API: 1.3.128
//   Vulkan Driver Version: 512.762.44
//
//   === Memory Information ===
//   Total Memory: 11273 MB
//
// Nothing here is invented: every value is measured at runtime from
//   * General — Android system build properties (ro.product.*, ro.build.*)
//   * CPU     — /proc/cpuinfo core-part topology + feature flags
//   * GPU     — a REAL Vulkan probe through the same loader path the
//               engine itself uses (GpuDriverManager). The driver version
//               uses the standard Vulkan packed decode, which is exactly
//               what vulkaninfo/DevCheck print for Adreno ("512.762.44").
//   * Memory  — /proc/meminfo MemTotal (the same source ActivityManager's
//               totalMem reports, matching Eden's number semantics).
//
// The report is built ONCE per process (device topology does not change
// under a running OS). It documents the DEVICE — which system Vulkan
// driver ships on it — not the currently-selected custom-driver slot;
// active-driver state is reported by GpuDriverManager::SummaryString and
// the eager slot verification instead.

#ifndef PX5_HOST_INFO_H
#define PX5_HOST_INFO_H

#include <string>

namespace PX5 {

class HostInfo {
public:
    // Eden-format multi-section block. Built on first call, cached after.
    static const std::string& BuildReport();

    // Emits the block into the engine log (px5_main.log) as one INFO
    // entry — the Eden "documented at log start" behavior.
    static void LogIntoEngineLog();
};

} // namespace PX5

#endif // PX5_HOST_INFO_H
