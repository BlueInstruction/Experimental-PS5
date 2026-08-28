#ifndef PX5_CRASH_HANDLER_H
#define PX5_CRASH_HANDLER_H

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
// ---------------------------------------------------------------------------
class CrashHandler {
public:
    // Installs handlers. Call once from JNI_OnLoad. Re-calling is a no-op.
    static void Install(const std::string& logsDir);

    // Directory where px5_crash.log is written (must be app-writable).
    static const std::string& LogsDir();
};

} // namespace PX5

#endif // PX5_CRASH_HANDLER_H
