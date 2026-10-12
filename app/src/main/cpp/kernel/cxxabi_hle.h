// PX5 v1.67 — the C++ ABI runtime tranche (device-import-driven).
//
// WHAT THIS IS
//   The __cxa surface Dreaming Sarah's decoded import list pulls from
//   PS5 libc++/libc (12 __cxa + 1 _Unwind). v1.62 built the ABI OBJECTS
//   in guest memory (typeinfo vtables, RTTI, statics); this tranche makes
//   the RUNTIME paths real where the boot path needs them:
//
//   * __cxa_guard_acquire / release / abort — static-local initialization
//     guards with REAL Itanium semantics: double-init returns 0, a second
//     thread blocks on an in-use guard, abort reopens the slot. The guest
//     guard bytes (byte0 = done, byte1 = in-use) are written through the
//     window so guest-side fast paths observe the truth.
//   * __cxa_allocate_exception / __cxa_free_exception — exception objects
//     allocated from the guest heap like a real libc++.
//   * __cxa_throw / begin_catch / end_catch / rethrow / get_exception_ptr
//     / _Unwind_Resume — a per-thread pending-exception registry that
//     records the thrown object + typeinfo and hands it to the catch
//     path. The unwind propagation machinery (LSDA scan, phase 1/2) is
//     NOT implemented: a throw records and the ledger names it; the
//     catch surfaces return the recorded object. A game that throws on
//     the boot path will say so on the ledger — that is the point.
//
// HONEST BOUNDARIES
//   * No two-phase unwinding: the guest does not jump to catch handlers
//     through our HLE. The registry + ledger give the next tranche its
//     ground truth; nothing here fakes a successful catch.
//   * _Unwind_Resume is the fatal path (no handler caught): logged loudly,
//     recorded in the ledger, returns to the caller (which will fault if
//     it genuinely expected unwinding — loud beats silent).
#ifndef PX5_KERNEL_CXXABI_HLE_H
#define PX5_KERNEL_CXXABI_HLE_H

#include <cstdint>
#include <string>
#include <vector>

#include "../loader/runtime_linker.h"   // ImportCall

namespace PX5::SceKernelHle {

struct SymbolEntry;

/**
 * The 13-symbol C++ ABI runtime surface.
 */
class CxxAbiHle {
public:
    static CxxAbiHle& Instance();

    void CollectSymbols(std::vector<SymbolEntry>* tableOut);
    bool HasSymbol(const std::string& symbol) const;

    /**
     * Dispatches one import call. xmmRetOut is the bridge's FP-return
     * channel; the C++ ABI surface never returns FP and ignores it.
     */
    uint64_t Invoke(const std::string& symbol, const ImportCall& call,
                    uint64_t* xmmRetOut = nullptr);

    /**
     * Session teardown: drops the guard registry and pending exceptions.
     */
    void ResetForSession();

    struct Stats {
        uint64_t guardAcquires  = 0;
        uint64_t guardContended = 0;
        uint64_t exceptionsThrown = 0;
        uint64_t exceptionsCaught = 0;
    };
    Stats GetStats() const;

private:
    CxxAbiHle() = default;
    struct Impl;
    Impl* m = nullptr;
};

} // namespace PX5::SceKernelHle

#endif // PX5_KERNEL_CXXABI_HLE_H
