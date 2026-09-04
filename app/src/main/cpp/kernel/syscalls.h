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

// v1.43 — guest RIP checkpoints. Every guest syscall records the guest RIP
// the CPU state carried when the bridge was entered. These are real
// execution checkpoints written by the dispatch path itself — the "how far
// did the game actually get" answer the vc42 session lacked (its crash
// report showed only the stale initial rip). Fixed-size BSS ring, dumped
// on crash and on every exec epilogue.
class GuestPcRing {
public:
    static constexpr size_t kSize = 64;
    static void Note(uint64_t rip);                    // one per syscall
    static uint64_t Last();                            // 0 when empty
    static uint64_t Seq();                             // total pushes ever
    // "seq=%llu last=0x… recent=[0x…,0x…,…]" — last 8, into a caller
    // buffer. Async-signal-safe shape: snprintf only, no allocation.
    static void Format(char* out, size_t outCap);
};

class GuestSyscalls {
public:
    // Dispatch one syscall from guest context.
    // Args follow x86-64 Linux convention: rdi, rsi, rdx, r10, r8, r9.
    static uint64_t Dispatch(uint32_t nr,
                             uint64_t a0 = 0, uint64_t a1 = 0,
                             uint64_t a2 = 0, uint64_t a3 = 0,
                             uint64_t a4 = 0, uint64_t a5 = 0);

    // v1.43 — forwarder used by the FEXCore syscall handler so the bridge
    // translation unit keeps owning the checkpoint plumbing.
    static void NoteGuestRip(uint64_t rip) { GuestPcRing::Note(rip); }

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
