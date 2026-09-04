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

/**
 * Guest syscall execution statistics.
 */
struct GuestSyscallStats {
    uint64_t totalCalls      = 0;  ///< Total syscalls dispatched
    uint64_t handledCalls    = 0;  ///< Syscalls successfully handled
    uint64_t unhandledCalls  = 0;  ///< Unimplemented syscalls
    uint64_t bytesWritten    = 0;  ///< Guest output bytes captured
};

/**
 * Guest RIP checkpoint ring buffer.
 * Records guest RIP at every syscall entry for execution forensics.
 * Fixed-size BSS ring, async-signal-safe formatting.
 */
class GuestPcRing {
public:
    static constexpr size_t kSize = 64;

    /**
     * Records one guest RIP checkpoint (called at every syscall entry).
     * @param rip Guest instruction pointer
     */
    static void Note(uint64_t rip);

    /**
     * Returns the most recent checkpoint (0 when empty).
     * @return Last recorded guest RIP, or 0 if ring is empty
     */
    static uint64_t Last();

    /**
     * Returns total number of checkpoints ever recorded.
     * @return Monotonic sequence counter
     */
    static uint64_t Seq();

    /**
     * Formats checkpoint ring into caller buffer (async-signal-safe).
     * Output: "seq=%llu last=0x… recent=[0x…,0x…,…]" (last 8 entries).
     * @param out Output buffer
     * @param outCap Buffer capacity in bytes
     */
    static void Format(char* out, size_t outCap);
};

/**
 * Guest syscall bridge dispatching Linux x86-64 syscalls to host.
 */
class GuestSyscalls {
public:
    /**
     * Dispatches one syscall from guest context.
     * Args follow x86-64 Linux convention: rdi, rsi, rdx, r10, r8, r9.
     * @param nr Syscall number
     * @param a0 First argument (rdi)
     * @param a1 Second argument (rsi)
     * @param a2 Third argument (rdx)
     * @param a3 Fourth argument (r10)
     * @param a4 Fifth argument (r8)
     * @param a5 Sixth argument (r9)
     * @return Syscall return value (guest RAX)
     */
    static uint64_t Dispatch(uint32_t nr,
                             uint64_t a0 = 0, uint64_t a1 = 0,
                             uint64_t a2 = 0, uint64_t a3 = 0,
                             uint64_t a4 = 0, uint64_t a5 = 0);

    /**
     * Records guest RIP checkpoint (forwarder for FEXCore handler).
     * @param rip Guest instruction pointer
     */
    static void NoteGuestRip(uint64_t rip) { GuestPcRing::Note(rip); }

    /**
     * Takes captured guest stdout/stderr output, clearing internal buffer.
     * @return All guest output since last ResetRun() or TakeOutput()
     */
    static std::string TakeOutput();

    /**
     * Appends text to captured guest output buffer.
     * @param s Text to append
     */
    static void        AppendOutput(const std::string& s);

    /**
     * Returns whether an exit code was recorded (exit/exit_group called).
     * @return true if guest called exit, false otherwise
     */
    static bool        HasExitCode();

    /**
     * Returns recorded exit code (valid only if HasExitCode() == true).
     * @return Guest exit code
     */
    static uint64_t    ExitCode();

    /**
     * Resets per-run state (clears output buffer and exit code).
     */
    static void        ResetRun();

    /**
     * Returns current syscall statistics.
     * @return Reference to GuestSyscallStats structure
     */
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
