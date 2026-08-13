// SPDX-License-Identifier: MIT
// PX5 — Native Crash Handler
//
// Installs sigaction handlers for SIGSEGV, SIGABRT, SIGBUS, SIGILL, SIGFPE,
// SIGPIPE, SIGTRAP, SIGSYS. When a fatal signal fires, the handler:
//
//   1. Writes a structured crash report to
//      <log_dir>/px5_crash_<YYYYMMDD_HHMMSS>.log
//      (one file per crash, so we never lose history to overwrites)
//   2. Dumps: timestamp, signal info (signo/si_code/si_addr), faulting thread
//      id + name, all ARM64 general-purpose registers from ucontext, and a
//      best-effort stack backtrace using _Unwind_Backtrace.
//   3. Re-raises the original signal so the system's normal tombstone flow
//      still runs (so we keep the logcat "signal XX" line + process death).
//
// All syscalls used here are async-signal-safe (open, write, close, fsync,
// getpid, gettid, prctl, localtime_r, sigaction). _Unwind_Backtrace is
// technically not async-signal-safe but is the only practical option on
// Android/Bionic; in practice it works for most crashes.

#ifndef PX5_CRASH_HANDLER_H
#define PX5_CRASH_HANDLER_H

#include <string_view>

namespace PX5 {

class CrashHandler {
public:
    // Install signal handlers. `log_dir` must be the same directory passed
    // to Logger::Initialize. Returns true on success.
    // Safe to call multiple times; subsequent calls are no-ops.
    static bool Install(std::string_view log_dir) noexcept;

    // Uninstall (restores default disposition). Mainly for tests.
    static void Uninstall() noexcept;

    // Returns true if Install() has been called and the handler is active.
    static bool IsInstalled() noexcept;
};

} // namespace PX5

#endif // PX5_CRASH_HANDLER_H
