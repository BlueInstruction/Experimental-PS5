// PX5 v1.65 — the pthread tranche (device-import-driven).
//
// WHAT THIS IS
//   The 27 scePthread* imports Dreaming Sarah's decoded import list pulls
//   from PS5 libkernel (475/484 NIDs resolved via aerolib: 27 pthread,
//   21 kernel, 189 sceAgc*, 162 libc/libm, 12 __cxa, 6 pad, 5 audio,
//   4 videoOut, 1 _Unwind). Implemented host-side over the guest window
//   with real POSIX semantics: std::mutex-backed errorcheck/recursive
//   mutexes, std::condition_variable conds with the console's
//   microseconds timeout contract, and guest threads that execute REAL
//   guest code through the FEXCore launcher seam.
//
// DESIGN (KytyPS5 reference, GPL-2.0 — our implementation is original)
//   * Handles: ScePthread / ScePthreadMutex / ScePthreadCond are opaque
//     pointers libkernel hands out. The guest never dereferences them, so
//     our handle is the host pointer of the private object (Kyty does the
//     same). Init functions write the handle into the guest's slot;
//     by-value calls read it back.
//   * Threads: scePthreadCreate spawns a host std::thread whose body is
//     the GuestLauncher seam. The device launcher (fexcore_integration)
//     builds a real FEXCore InternalThreadState — per-thread callret
//     stack, GDT, TCB — and dispatches the guest entry. Host tests
//     override the seam with a plain function launcher, so every
//     lifecycle/semantics path runs without the JIT.
//   * Exit: scePthreadExit records the exit value and marks the thread;
//     the crt thread wrapper then reaches its hlt and ExecuteThread
//     returns. Join waits on the recorded truth, never on a guess.
//
// HONEST BOUNDARIES
//   * Error codes: the real console returns ORBIS 0x8002xxxx codes; this
//     floor standardizes on signed errno (-EINVAL, -EDEADLK, ...) like
//     the rest of sce_kernel_hle. Named here — a game that compares
//     against ORBIS codes will tell us on the ledger.
//   * scePthreadExit cannot unwind the guest frame; it records the value
//     and lets the crt wrapper run its natural hlt end. A wrapper that
//     keeps running after Exit stays bounded by the session run budget.
//   * schedparam/priority/inheritsched are stored, not scheduled — one
//     guest thread priority class exists until the scheduler tranche.
//   * Per-thread TLS: the launcher builds the FreeBSD-variant-II TCB
//     (same recipe as the main thread, v1.40) from the published image
//     PT_TLS. DTV is the static single-block form — dlopen'd modules
//     with TLS are not supported yet.
#ifndef PX5_KERNEL_PTHREAD_HLE_H
#define PX5_KERNEL_PTHREAD_HLE_H

#include <cstdint>
#include <string>
#include <vector>

#include "../loader/runtime_linker.h"   // ImportCall

namespace PX5::SceKernelHle {

/**
 * PS5 libkernel pthread constants (console ABI, see KytyPS5 reference).
 */
namespace ScePthread {
    // mutex type (scePthreadMutexattrSettype)
    constexpr int kMutexDefault    = 0;
    constexpr int kMutexErrorcheck = 1;
    constexpr int kMutexRecursive  = 2;
    constexpr int kMutexNormal     = 3;
    constexpr int kMutexAdaptive   = 4;

    // detach state (scePthreadAttrSetdetachstate)
    constexpr int kCreateJoinable = 0;
    constexpr int kCreateDetached = 1;

    constexpr uint64_t kDefaultStackBytes = 1ull << 20;  // 1 MiB
    constexpr uint64_t kMinStackBytes     = 4096;
    constexpr size_t   kTlsRegionBytes    = 0x10000;     // per-thread TLS block
    constexpr size_t   kMaxTlsImageBytes  = 0xF000;

    // sched policy family (stored, not scheduled)
    constexpr int kSchedOther   = 0;
    constexpr int kSchedFifo    = 1;
    constexpr int kSchedRr      = 2;
    constexpr int kSchedIdle    = 3;
} // namespace ScePthread

struct SymbolEntry;

/**
 * Guest thread launcher seam.
 *
 * @param entryGuestVa guest RIP of the thread entry (void* (*)(void*))
 * @param argGuestVa   guest argument passed in RDI
 * @param stackTopVa   guest SP for the new thread (stack grows down)
 * @param fsBaseVa     guest FSBASE (the per-thread TCB address)
 * @param exitRaxOut   receives RAX at dispatch end (the entry's return)
 * @return true when the thread ran and returned normally
 */
using GuestThreadLauncher = bool (*)(uint64_t entryGuestVa, uint64_t argGuestVa,
                                     uint64_t stackTopVa, uint64_t fsBaseVa,
                                     uint64_t* exitRaxOut);

/**
 * The 27-import pthread surface, host-backed over the guest window.
 */
class PthreadHle {
public:
    static PthreadHle& Instance();

    /**
     * Appends this surface's symbol names to the kernel HLE table
     * (invocation counters shared).
     */
    void CollectSymbols(std::vector<SymbolEntry>* tableOut);

    bool HasSymbol(const std::string& symbol) const;

    /**
     * Dispatches one import call. Returns the guest-visible RAX.
     * xmmRetOut is the bridge's double-return channel (v1.66); the
     * pthread surface never returns FP and ignores it.
     */
    uint64_t Invoke(const std::string& symbol, const ImportCall& call,
                    uint64_t* xmmRetOut = nullptr);

    /**
     * Installs the guest-thread launcher (device: FEXCore dispatcher;
     * host tests: direct function launcher). Call before the first
     * scePthreadCreate. nullptr resets to the default FEXCore launcher.
     */
    void SetGuestLauncher(GuestThreadLauncher launcher);

    /**
     * Publishes the loaded image's PT_TLS geometry so the launcher can
     * build per-thread TCBs (called by the load path; no-op defaults
     * mean threads run with a bare TCB and no TLS image).
     */
    static void PublishImageTls(uint64_t tlsVa, size_t filesz, size_t memsz,
                                uint64_t align);

    /**
     * Session teardown: drops every object and joiner (the guest window
     * dies with the session; handles must not outlive it).
     */
    void ResetForSession();

    struct Stats {
        uint64_t threadsCreated = 0;
        uint64_t threadsJoined  = 0;
        uint64_t threadsExited  = 0;
        uint64_t mutexLocks     = 0;
        uint64_t condSignals    = 0;
        uint64_t liveThreads    = 0;   // snapshot at stats time
    };
    Stats GetStats() const;

private:
    PthreadHle() = default;
    struct Impl;
    Impl* m = nullptr;   // pimpl: keeps pthread.h/condition_variable out of
                         // every TU that includes this header
};

} // namespace PX5::SceKernelHle

#endif // PX5_KERNEL_PTHREAD_HLE_H
