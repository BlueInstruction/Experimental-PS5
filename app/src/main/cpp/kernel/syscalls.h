#ifndef PX5_SYSCALLS_H
#define PX5_SYSCALLS_H

#include <cstdint>
#include <string>

namespace PX5 {

// ---------------------------------------------------------------------------
// GuestSyscalls — the REAL Linux x86-64 syscall bridge used by FEXCore.
//
// Honesty contract (replaces two earlier fake layers):
//   * The old KernelSyscalls table mixed FreeBSD/PS5 numbers with Linux
//     host calls and returned 0 for everything.
//   * FEXCore previously ran with a NullSyscallHandler returning -1:
//     ANY guest syscall killed execution. Foundation guests could never
//     do real work (no write, no exit_group).
//   * This bridge implements a small but genuinely functional set of
//     Linux x86-64 syscalls, converts guest pointers through the memory
//     window, captures guest stdout, records exit codes, and logs every
//     UNIMPLEMENTED number loudly instead of lying about success.
// ---------------------------------------------------------------------------
struct GuestSyscallStats {
    uint64_t totalCalls      = 0;
    uint64_t handledCalls    = 0;
    uint64_t unhandledCalls  = 0;
    uint64_t bytesWritten    = 0;
};

class GuestSyscalls {
public:
    // Dispatch one syscall from guest context.
    // Args follow x86-64 Linux convention: rdi, rsi, rdx, r10, r8, r9.
    static uint64_t Dispatch(uint32_t nr,
                             uint64_t a0 = 0, uint64_t a1 = 0,
                             uint64_t a2 = 0, uint64_t a3 = 0,
                             uint64_t a4 = 0, uint64_t a5 = 0);

    // Captured guest stdout (fd 1) / stderr (fd 2) — surfaced to UI evidence.
    static std::string TakeOutput();
    static void        AppendOutput(const std::string& s);

    // Exit state recorded by exit/exit_group before the guest halts.
    static bool        HasExitCode();
    static uint64_t    ExitCode();
    static void        ResetRun();          // clear output + exit state

    static const GuestSyscallStats& Stats();
};

// NOTE (signal routing): host signal handlers are owned EXCLUSIVELY by
// utils/crash_handler.cpp (installed once at native init). Guest-visible
// synchronous traps are routed through FexCoreIntegration's fault
// intercept (see fexcore_integration.cpp FaultInterceptRouter). Do NOT
// register additional sigaction handlers anywhere else: a handler that
// returns without re-raising silently swallows real faults and breaks the
// SMC/trap pipeline (the old kernel/signals.cpp did exactly that).

} // namespace PX5

#endif // PX5_SYSCALLS_H
