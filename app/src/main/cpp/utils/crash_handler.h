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

/**
 * Signal-level crash handler with full register dumps and symbolized backtraces.
 */
class CrashHandler {
public:
    /**
     * Installs signal handlers and sets crash report directory.
     * First call from JNI_OnLoad arms handlers; subsequent calls with non-empty dir
     * update the logs directory (MainActivity supplies real app logs path later).
     * @param logsDir Directory where px5_crash_latest.log will be written
     */
    static void Install(const std::string& logsDir);

    /**
     * Returns directory where crash logs are written (must be app-writable).
     * @return Logs directory path
     */
    static const std::string& LogsDir();

    /**
     * Fault intercept callback type for FEXCore SMC/unaligned-atomic handling.
     * Must be async-signal-safe. Return true if fault consumed, false to report crash.
     */
    using FaultIntercept = bool (*)(int sig, void* siginfo, void* ucontext);

    /**
     * Registers fault intercept for engine-level fault routing.
     * Called before crash reporting; true return consumes fault, false reports crash.
     * @param fn Intercept function (async-signal-safe)
     */
    static void SetFaultIntercept(FaultIntercept fn);

    /**
     * Arms calling thread's 256KB alternate signal stack.
     * Per-thread operation; must be called at entry of every engine/probe path.
     * Cheap after first call per thread (pthread_getspecific check).
     */
    static void ArmThreadAltStack();
};

} // namespace PX5

#endif // PX5_CRASH_HANDLER_H
