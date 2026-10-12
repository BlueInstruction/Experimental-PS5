// PX5 v1.62 — the libc/libc++ HLE tranche (device-import-driven).
//
// WHAT THIS IS
//   The mem/str/heap/stdio surface Dreaming Sarah's decoded import list
//   (475/484 NIDs resolved via aerolib) pulls from PS5 libc, implemented
//   against the same window-truth plumbing as the kernel HLE: guest VAs
//   translate through the MemoryManager window, unreadable addresses
//   refuse named instead of guessing, and every symbol rides the same
//   table + counters the other HLE surfaces use.
//
// HONEST BOUNDARIES
//   * malloc: a bump allocator over a 256 MiB guest heap region inside
//     the window. free() is a real no-op (memory is reclaimed at session
//     teardown, not per-free) — named here, not hidden. realloc moves
//     and copies through the recorded size table.
//   * printf-family floats (%f/%e/%g) consume the argument slot and emit
//     "<f>": float varargs ride XMM registers, which the import-call
//     bridge (GPR-only, see runtime_linker.h ImportCall) cannot see yet.
//     Integer/string output is complete for the boot path.
//   * vsnprintf/sscanf/qsort/setjmp/longjmp stay OFF this surface: they
//     need guest va_list walking or guest->guest calls through FEX and
//     keep the named-ledger behavior until that bridge exists.
//   * exit()/__stack_chk_fail log and return; the process-exit path is
//     the syscall bridge's exit_group, which a libc call frame cannot
//     invoke directly yet.
#ifndef PX5_KERNEL_LIBC_HLE_H
#define PX5_KERNEL_LIBC_HLE_H

#include <cstdint>
#include <string>
#include <vector>

#include "../loader/runtime_linker.h"   // ImportCall

namespace PX5::SceKernelHle {

/**
 * HLE symbol table entry shape (mirrors sce_kernel_hle.h SymbolEntry).
 */
struct SymbolEntry;

/**
 * PS5 libc/libc++ functions games import at boot, implemented host-side
 * over the guest window.
 */
class LibcHle {
public:
    static LibcHle& Instance();

    /**
     * Appends this surface's symbol names to the kernel HLE table
     * (invocation counters shared).
     */
    void CollectSymbols(std::vector<SymbolEntry>* tableOut);

    /**
     * True when Invoke(symbol, call) routes this symbol.
     */
    bool HasSymbol(const std::string& symbol) const;

    /**
     * Runs the HLE implementation with the caller's preserved arguments.
     * xmmRetOut is the bridge's double-return channel (v1.66): the libm
     * functions the game imports return in XMM0 and publish there.
     * v1.69 — xmmRetValidOut (when non-null) is set to true exactly when
     * the implementation wrote xmmRetOut; validity is never inferred from
     * the returned bits (a legitimate double return can be 0.0).
     * @return Guest-visible RAX
     */
    uint64_t Invoke(const std::string& symbol, const ImportCall& call,
                    uint64_t* xmmRetOut = nullptr,
                    bool* xmmRetValidOut = nullptr);

private:
    LibcHle() = default;
    uint64_t HeapAlloc(size_t size, size_t align);
    uint64_t HeapCalloc(size_t n, size_t size);
    uint64_t HeapRealloc(uint64_t oldVa, size_t size);
    void     HeapFree(uint64_t va);
    size_t   HeapSizeOf(uint64_t va) const;
};

} // namespace PX5::SceKernelHle

#endif // PX5_KERNEL_LIBC_HLE_H
