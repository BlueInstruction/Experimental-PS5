#ifndef PX5_CRASH_HANDLER_H
#define PX5_CRASH_HANDLER_H

#include <cstdarg>
#include <cstdint>
#include <string>

namespace PX5 {

// ---------------------------------------------------------------------------
// CrashHandler — real, signal-level diagnostics.
//
// Installs sigaction handlers for SIGSEGV/SIGBUS/SIGILL/SIGFPE/SIGABRT/
// SIGTRAP. On fault it writes <logs>/px5_crash.log containing:
//   * signal name, si_addr, sender pid/uid
//   * faulting thread id
//   * full ARM64 register dump (x0..x30, sp, pc, pstate) from ucontext
//   * best-effort symbolized backtrace
//   * the tail of the main log file for context
// Kotlin side complements it with an uncaught-exception writer so Java
// crashes land in the same file.
//
// FAULT ROUTING: a hosting emulator has faults that are NOT crashes —
// FEXCore's mtrack SMC write faults and unaligned-atomic repairs both
// arrive as ordinary SIGSEGV/SIGBUS and must be HANDLED, not reported.
// The engine registers an intercept (SetSegvIntercept); every routed
// signal is asked there FIRST, in the sharpdroid/FEX-frontend question
// order. A false return falls through to the full crash report, so a
// fault nobody claims is still a crash with forensics — never silence.
// ---------------------------------------------------------------------------
class CrashHandler {
public:
    // Installs handlers. Call once from JNI_OnLoad. Re-calling is a no-op.
    static void Install(const std::string& logsDir);

    // Directory where px5_crash.log is written (must be app-writable).
    static const std::string& LogsDir();

    // Return true from the intercept when the fault was consumed (SMC write
    // invalidated, unaligned access repaired, context adjusted for retry).
    // Runs in signal context: async-signal-safe work only.
    using FaultIntercept = bool (*)(int sig, void* siginfo, void* ucontext);
    static void SetFaultIntercept(FaultIntercept fn);
};

} // namespace PX5

#endif // PX5_CRASH_HANDLER_H
