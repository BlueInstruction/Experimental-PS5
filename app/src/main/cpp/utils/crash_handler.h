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
    // Arms the signal handlers (ONCE, from JNI_OnLoad with an empty dir —
    // no Android context exists yet) and sets the report directory (EVERY
    // call with a non-empty dir wins; MainActivity supplies the real app
    // logs dir via nativeInitRuntimeContext afterwards). The old
    // first-call-wins rule froze the dir empty, sending every report to
    // /data/local/tmp — unwritable for app processes — so real crashes
    // left NO file and the UI claimed otherwise (fixed 2026-08-30).
    static void Install(const std::string& logsDir);

    // Directory where px5_crash_latest.log is written (must be app-writable).
    static const std::string& LogsDir();

    // Return true from the intercept when the fault was consumed (SMC write
    // invalidated, unaligned access repaired, context adjusted for retry).
    // Runs in signal context: async-signal-safe work only.
    using FaultIntercept = bool (*)(int sig, void* siginfo, void* ucontext);
    static void SetFaultIntercept(FaultIntercept fn);

    // v1.21 — give the CALLING thread its own 256KB alternate signal stack.
    // sigaltstack is per-thread; Install() only covers the main thread, so
    // engine probes running on Kotlin/DefaultDispatch workers fault with no
    // reserve — and when the fault fires while the thread is on the GUEST
    // stack, SA_ONSTACK has nothing healthy to switch to and the report
    // dies mid-write. Arm at the entry of every engine/probe path; cheap
    // (one pthread_getspecific check) after the first call per thread.
    static void ArmThreadAltStack();
};

} // namespace PX5

#endif // PX5_CRASH_HANDLER_H
