// PX5 v1.62 — the guest-space C++ runtime ABI module.
//
// WHY THIS EXISTS
//   The 2026-10-11 device session proved the wall: Dreaming Sarah's
//   image carries ~372 UNDEF *data* relocations (typeinfo vtables,
//   exception RTTI, locale/stdio statics) and every one of them was
//   redirected at a 16-byte import-call stub — CODE. The guest read a
//   vtable pointer through such a slot, then dereferenced stub bytes as
//   data; the session's exit field carried a guest VA (0x140000070-
//   shaped), the classic unrelocated/garbage-data signature. A data slot
//   can never be served by a call stub: it needs a real object in guest
//   memory whose layout the guest's own C++ runtime agrees with.
//
// WHAT THIS BUILDS
//   One RWX-then-split-protected region inside the guest window holding
//   the subset of the PS5 libc/libc++ ABI that this game's static
//   initializers touch:
//     * the three __cxxabiv1 type_info vtables (11-slot layout, per the
//       AnyPS5 TypeInfoVtable contract: offset, type, destroy, delete,
//       isPointer, isFunction, do_catch, do_upcast, upcast_result,
//       dyncast, find_public — GPL-2.0 reference, device-verified);
//     * the std::exception family: typeinfo objects (class/si instances
//       with real mangled name strings and real base chains) + 5-slot
//       vtables (offset, type, destroy, delete, what);
//     * the function addresses those vtables contain, as small guest
//       stubs (syscall + ret) that route into the ABI-stub ledger so a
//       session shows which ABI virtual actually ran;
//     * the misc data statics the game imports verbatim
//       (__stack_chk_guard, _Stdout/_Stderr, Need_sceLibc, locale ids).
//
// HONEST BOUNDARIES
//   * Exception THROWING is not implemented: __cxa_* calls stay on the
//     import-call ledger. The objects here make static initialization
//     and RTTI *reads* structurally sound; a thrown exception's unwind
//     path is a later tranche and stays named when it happens.
//   * what() returns a pointer to an empty string, not a message: the
//     emitted stubs carry no message storage. Catch-and-print paths see
//     "" instead of garbage — honest, and named in the ledger when hit.
//   * Data symbols that exist here but are not in this game's import
//     set are simply unreferenced bytes; nothing pretends to be used.
#ifndef PX5_LOADER_GUEST_ABI_H
#define PX5_LOADER_GUEST_ABI_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace PX5::GuestAbi {

struct BuildResult {
    bool        ok = false;        ///< false = nothing was built; symbols empty
    std::string error;             ///< Named reason when !ok
    uint64_t    regionVa = 0;      ///< Guest VA of the region base
    size_t      regionBytes = 0;   ///< Mapped size
    size_t      symbolCount = 0;   ///< Named symbols registered
    size_t      stubCount = 0;     ///< Code stubs emitted
    std::string summary;           ///< One evidence line
};

/**
 * Builds the guest ABI region inside the memory window and fills the
 * name -> guest-VA registry. Re-runs replace the registry (the loader's
 * replace-on-map model); the previous region's pages stay mapped until
 * MemoryManager::Shutdown — the same lifecycle the import-stub region
 * already has.
 * @param label Memory mapping label for evidence
 * @param avoidBase Inclusive start of a guest range the region must not
 *        overlap (the import-stub region); 0 = nothing to avoid
 * @param avoidEnd Exclusive end of the avoided range
 * @return BuildResult; ok=false names the failure and leaves the
 *         registry EMPTY (callers must not consult Resolve)
 */
BuildResult Build(const char* label, uint64_t avoidBase = 0,
                  uint64_t avoidEnd = 0);

/**
 * Resolves an ABI symbol name (exact mangled form, e.g. "_ZTVSt9exception")
 * to its guest VA. False when not built or the name is not provided.
 */
bool Resolve(const std::string& name, uint64_t* vaOut);

/**
 * True when Build() succeeded and the registry is live.
 */
bool IsBuilt();

/**
 * Clears the registry (per-run teardown). Region pages are owned by the
 * MemoryManager and are released with its Shutdown, like every other
 * guest mapping.
 */
void Reset();

/**
 * Ledger for one ABI stub entry from the guest (kPx5AbiStubBase + id).
 * First hit per id is logged by name; repeats are counted only.
 * @param stubId Stub id (must be < kPx5AbiStubSlots)
 * @return Guest-visible RAX (0 for every emitted stub)
 */
uint64_t DispatchStub(uint64_t stubId);

/**
 * Names the ABI stub ledger's state for evidence summaries.
 */
std::string StubLedgerSummary();

} // namespace PX5::GuestAbi

#endif // PX5_LOADER_GUEST_ABI_H
