// SPDX-License-Identifier: MIT
// PX5 heartbeat — last-known-stage evidence for UNCATCHABLE process deaths.
//
// WHY: a SIGKILL-class kill (low-memory killer, ANR watchdog, lmkd) cannot
// run ANY in-process handler, so no px5_crash_*.log appears — the 2026-08-30
// session reported exactly that: "app dies on game boot with zero logs".
// A signal handler is structurally blind to that class; the fix is to keep
// evidence on disk CONTINUOUSLY while alive, not to write it while dying.
//
// HOW: one daemon thread rewrites <logsDir>/px5_heartbeat.log every second:
//   HB <unix_ts> <uptime_ms> <last breadcrumb>
// After ANY death, the file's last line names the stage that was alive —
// converting a silent kill into stage-attributed evidence. Rewritten in
// place (O_TRUNC, no fsync needed: page-cache contents survive SIGKILL).
//
// The thread is started once from nativeInitRuntimeContext (real logs dir)
// and dies with the process. Never throws, never blocks callers.

#ifndef PX5_UTILS_HEARTBEAT_H
#define PX5_UTILS_HEARTBEAT_H

#include <string>

namespace PX5::Heartbeat {

// Starts the 1 Hz heartbeat thread for `logsDir`. Idempotent: later calls
// with any dir are no-ops (first caller wins, same contract as the crash
// handler's arming). Empty dir -> no thread.
void Start(const std::string& logsDir);

} // namespace PX5::Heartbeat

#endif // PX5_UTILS_HEARTBEAT_H
