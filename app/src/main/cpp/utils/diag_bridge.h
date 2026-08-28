// SPDX-License-Identifier: MIT
// PX5 — Native → Kotlin diagnostic-stream bridge.
//
// PROBLEM THIS SOLVES
//   The app has two log files:
//     px5_main.log        — native Logger output (logcat mirror + rotation)
//     px5_diagnostic.log  — the Kotlin 3-layer EVENT/STATE stream the user
//                           actually pastes into bug reports
//   Critical loader evidence (adrenotools returned null, hook libs MISSING,
//   driver NOT verified in /proc/self/maps) only ever landed in the native
//   file, so a pasted diagnostic stream looked clean while the engine was
//   screaming in the other file.
//
// DESIGN
//   No JNI, no callbacks: the bridge appends the same file the Kotlin side
//   appends (px5_diagnostic.log). Both sides open with O_APPEND, so short
//   lines (<PIPE_BUF) land atomically without a shared lock. One-way only —
//   the bridge never calls back into Kotlin, so no recursion, no deadlocks,
//   and it stays usable from crash-signal context (raw write, no malloc on
//   the hot path beyond the small line buffer).
//
// FILTER (honest, not chatty)
//   - WARNING / ERROR / FATAL: always forwarded (the diagnostic gold).
//   - INFO: forwarded only for GPU / LOADER / VULKAN / FEX / CORE — the
//     driver + engine lifecycle lines. Per-frame or per-syscall noise is
//     not forwarded.
//   - Hard session cap (default 4000 lines): a runaway loop degrades to
//     logcat only instead of exploding the user-visible stream.
#pragma once

#include <string>

#include "logger.h"

namespace PX5 {

class DiagBridge {
public:
    // Point the bridge at the Kotlin diagnostic stream (absolute path of
    // px5_diagnostic.log). Idempotent; empty path disables forwarding.
    static void Enable(const std::string& diagnostic_log_path) noexcept;

    // Called by Logger for every emitted line. Applies the filter above and
    // appends "[ts] NATIVE level=L cat=C msg" to the diagnostic stream.
    static void Forward(LogLevel level, LogCategory category,
                        const char* message) noexcept;
};

} // namespace PX5
