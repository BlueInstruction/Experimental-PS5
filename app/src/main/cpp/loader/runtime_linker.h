// SPDX-License-Identifier: MIT
// PX5 v1.31 — Runtime linker + NID gate (the bionic-native HLE bridge).
//
// WHAT THIS IS
//   The registry that lets guest x86-64 code reach PS5-style imports.
//   On a real PS5, every libSce* import is an NID resolved by the runtime
//   linker to an exported function. PSX5 has no Sony modules; the exports
//   we can provide are HLE host functions compiled natively against
//   bionic (this file) or guest addresses inside already-loaded modules.
//
// THE GATE
//   Guest side, the call enters through a reserved syscall number
//   (kPx5NidGateSyscall): rdi = NID, rsi/rdx/r10/r8/r9 = first five HLE
//   arguments. FEXCore already routes every guest `syscall` through
//   kernel/syscalls.cpp GuestSyscalls::Dispatch — the gate is a case
//   there, so the bridge rides the seam that is runtime-proven since
//   v1.23 instead of inventing a second entry mechanism.
//   The number is deliberately synthetic: above the x86-64 Linux range
//   (0..447) and outside the x32 range (0x40000000..0x400003FF), so no
//   real guest syscall can ever collide with it.
//
// THE IMPORT TRAP (v1.45)
//   The vc45 session proved the null-import wall: the game's _start calls
//   a PLT entry whose GOT slot the loader correctly refuses to invent
//   (slot stays 0) -> jmp [0] -> SIGSEGV. The trap flips that wall into
//   the ledger: every UNDEF STRONG import slot gets a 16-byte guest stub
//   (mov eax,kPx5ImportTrapSyscall; mov edi,<import index>; syscall; ret)
//   written by the loader into a dedicated RWX-then-RX guest region.
//   A call now lands in GuestSyscalls::Dispatch, names the missing import
//   (dynstr symbol, verbatim — no invented NID mapping), and returns 0 so
//   the guest keeps running and the next session collects MORE misses
//   instead of one mystery crash. UNDEF WEAK slots stay 0 (ELF semantics:
//   weak undefined resolves to null; crt code tests those for NULL).
//
// HONEST BOUNDARIES
//   * The gate executes HLE host functions ONLY. A NID registered as a
//     guest export is NOT callable through the gate (the host bridge
//     would have to synthesize a guest call frame — not built yet);
//     such calls fail by name. Guest-to-guest calls are direct branches
//     inside guest code and never involve this gate.
//   * No NID database is invented: only explicitly registered exports
//     resolve. Unknown NIDs fail loudly (stat + logcat), never zero.
//   * Dynamic-segment parsing (ParseDynamicFromElfImage) reads REAL
//     ELF64 images — the rebuilt SELF inner ELF or any raw ELF — and
//     reports what it finds. SCE-specific dynamic tags (0x61000000-
//     0x610000FF range, e.g. 0x61000013 export-lib — Kyty elf.h DT_OS_*
//     constants) are enumerated and named, not semantically guessed.

#ifndef PX5_LOADER_RUNTIME_LINKER_H
#define PX5_LOADER_RUNTIME_LINKER_H

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace PX5 {

// Reserved guest syscall numbers for the NID gate and the import trap
// (see header comment; both deliberately outside every real x86-64 range).
constexpr uint32_t kPx5NidGateSyscall   = 0x5C500001u;
constexpr uint32_t kPx5ImportTrapSyscall = 0x5C500002u;

/**
 * Bionic-native HLE export function type.
 * Receives guest arguments passed through the NID gate (args[0..4] = a1..a5).
 * @param args Array of guest arguments
 * @param argc Argument count (currently <= 5)
 * @return Value to place in guest RAX
 */
using HleHostFn = std::function<int64_t(const uint64_t* args, size_t argc)>;

/**
 * Result of a NID gate dispatch.
 */
struct GateResult {
    bool        ok = false;      ///< Whether dispatch succeeded
    int64_t     value = 0;       ///< Guest-visible return when ok
    std::string error;           ///< Named reason when !ok
};

/**
 * NID gate dispatch statistics.
 */
struct DispatchStats {
    uint64_t gateCalls   = 0;         ///< Gate syscalls seen
    uint64_t resolvedHle = 0;         ///< Dispatched into bionic HLE function
    uint64_t guestRouted = 0;         ///< NID exists but is guest export (not gate-callable)
    uint64_t unresolved  = 0;         ///< NID not registered (repeat hits included)
    uint64_t unresolvedUnique = 0;    ///< DISTINCT missing NIDs (per-game HLE gap size)
};

/**
 * One unresolved strong import redirected into a trap stub (v1.45).
 * Index in RuntimeLinker's trap table == stub index: the guest stub at
 * stubVa encodes its own index, and DispatchImportTrap maps it back here.
 */
struct ImportTrapEntry {
    uint64_t    stubVa = 0;      ///< Guest VA of the 16-byte trap stub
    uint64_t    slotVa = 0;      ///< Guest VA of the GOT/data slot redirected
    std::string name;            ///< dynstr symbol name, verbatim (may be
                                 ///< empty when the string table is missing)
    bool        isPlt = false;   ///< true = DT_JMPREL/JUMP_SLOT, false = RELA
    uint32_t    symIndex = 0;    ///< dynsym index of the UNDEF symbol
    bool        slotWritten = false;  ///< false = slot could not be written
                                     ///< (old content stays, stub never runs)
};

/**
 * Loaded module record for addressing and evidence.
 */
struct ModuleRecord {
    std::string name;                 ///< Module name (e.g., "eboot.bin")
    uint64_t    base    = 0;          ///< Base virtual address
    uint64_t    highVa  = 0;          ///< High virtual address (exclusive)
    uint64_t    entry   = 0;          ///< Entry point address
    bool        isSelf  = false;      ///< Whether module came from SELF container
    size_t      segmentCount = 0;     ///< Number of loaded segments
};

/**
 * Runtime linker and NID gate for PS5-style imports bridging to bionic HLE.
 */
class RuntimeLinker {
public:
    /**
     * Returns the singleton RuntimeLinker instance.
     * @return Reference to singleton
     */
    static RuntimeLinker& GetInstance();

    /**
     * Clears modules/exports/stats (per-run teardown).
     */
    void Reset();

    /**
     * Records a loaded image in the module registry (evidence + addressing).
     * @param name Module name
     * @param base Base virtual address
     * @param highVa High virtual address (exclusive)
     * @param entry Entry point address
     * @param isSelf Whether module came from SELF container
     * @param segmentCount Number of loaded segments
     * @return true if registration succeeded, false otherwise
     */
    bool RegisterModule(const std::string& name, uint64_t base,
                        uint64_t highVa, uint64_t entry, bool isSelf,
                        size_t segmentCount);

    /**
     * Registers a bionic-native HLE export callable through the NID gate.
     * Duplicate NIDs fail by name.
     * @param library Library name (e.g., "libkernel")
     * @param nid NID (Name ID)
     * @param name Symbol name (e.g., "sceKernelOpen")
     * @param fn Host function to invoke
     * @return true if registration succeeded, false if NID duplicate
     */
    bool RegisterHleExport(const std::string& library, uint64_t nid,
                           const std::string& name, HleHostFn fn);

    /**
     * Registers a NID exported by loaded guest code (for guest-to-guest calls).
     * NOT gate-callable (HLE bridge cannot synthesize guest call frame yet).
     * @param nid NID (Name ID)
     * @param guestAddr Guest virtual address of export
     * @param name Symbol name
     * @return true if registration succeeded, false otherwise
     */
    bool RegisterGuestExport(uint64_t nid, uint64_t guestAddr,
                             const std::string& name);

    /**
     * Dispatches NID gate call from guest (used by GuestSyscalls::Dispatch).
     * Does not hold mutex while HLE function runs (re-entry allowed).
     * @param nid NID to dispatch
     * @param args Guest argument array
     * @param argc Argument count
     * @return GateResult with success/value or failure/error
     */
    GateResult DispatchNid(uint64_t nid, const uint64_t* args, size_t argc);

    /**
     * Returns dispatch statistics.
     * @return Reference to DispatchStats
     */
    const DispatchStats& Stats();

    /**
     * Returns human-readable summary for evidence reports.
     * @return Summary string
     */
    std::string GetSummaryString();

    /**
     * Returns distinct missing-NID list with per-NID hit counts.
     * This is the real per-game compatibility gap: what guest asked for that no HLE provides.
     * @return Formatted missing NIDs summary
     */
    std::string GetMissingNidsSummary();

    /**
     * Returns count of distinct missing NIDs.
     * @return Missing NID count
     */
    size_t MissingNidCount();

    /**
     * Installs the import-trap table built by the loader after relocation
     * processing (v1.45). Replaces any previous table (replace-on-map load
     * model: one image owns the registry at a time).
     * @param regionBase Guest VA of the trap-stub region start
     * @param regionEnd Guest VA one past the trap-stub region
     * @param entries The trap entries, index-aligned with the stub layout
     */
    void SetImportTraps(uint64_t regionBase, uint64_t regionEnd,
                        std::vector<ImportTrapEntry> entries);

    /**
     * Handles one import-trap syscall from guest (stub-encoded index in a0).
     * First hit per index is logged and ledgered; repeats are counted only.
     * Always returns 0 — the guest keeps running past the missing import.
     * @param importIndex Stub-encoded import index
     * @return Value for guest RAX (always 0)
     */
    uint64_t DispatchImportTrap(uint64_t importIndex);

    /**
     * Returns the import-trap ledger summary (counts + hottest imports).
     * @return Formatted summary string
     */
    std::string GetImportTrapSummary();

    /**
     * Returns count of installed import traps.
     * @return Trap entry count
     */
    size_t ImportTrapCount();

    /**
     * Returns count of registered modules.
     * @return Module count
     */
    size_t ModuleCount();

    /**
     * Returns count of registered exports (HLE + guest).
     * @return Export count
     */
    size_t ExportCount();

private:
    RuntimeLinker() = default;
    struct NidEntry {
        std::string library;
        std::string name;
        bool        isHle = false;
        HleHostFn   hle;            // valid when isHle
        uint64_t    guestAddr = 0;  // valid when !isHle
    };
    std::mutex m_mutex;
    std::vector<ModuleRecord> m_modules;
    std::unordered_map<uint64_t, NidEntry> m_exports;  // need <unordered_map>
    std::unordered_map<uint64_t, uint32_t> m_missingNids;  // nid -> hits
                                                           // (bounded)
    DispatchStats m_stats;

    // v1.45 — import traps (see header comment). m_trapHits is index-
    // aligned with m_importTraps; the region bounds let crash reports
    // and summaries attribute addresses to stubs.
    std::vector<ImportTrapEntry> m_importTraps;
    std::vector<uint32_t>        m_trapHits;
    uint64_t m_trapRegionBase = 0;
    uint64_t m_trapRegionEnd  = 0;
    uint64_t m_trapTotalHits  = 0;
    uint64_t m_trapDistinct   = 0;   ///< distinct imports hit at least once
    uint64_t m_trapLedgered   = 0;   ///< distinct misses written to the ledger
    uint64_t m_trapOob        = 0;   ///< out-of-range indices (corrupt guest)
};

// ---------------------------------------------------------------------------
// Real ELF64 PT_DYNAMIC reader (bounded; named errors; no guesses).
// ---------------------------------------------------------------------------

/**
 * Parsed ELF64 dynamic section information.
 */
struct DynamicInfo {
    bool        ok = false;                  ///< Whether parse succeeded
    std::string error;                       ///< Named reason when !ok
    std::string soname;                      ///< DT_SONAME string
    std::vector<std::string> needed;         ///< DT_NEEDED strings
    size_t      dynEntries = 0;              ///< Entries before DT_NULL
    std::vector<std::pair<uint64_t, uint64_t>> sceTags;  ///< SCE OS-specific tags (0x61000000 range)
    std::string summary;                     ///< One evidence line
};

/**
 * Parses PT_DYNAMIC from an ELF64 image in memory.
 * @param data Pointer to ELF image data
 * @param size Size of ELF image in bytes
 * @return DynamicInfo with parse results
 */
DynamicInfo ParseDynamicFromElfImage(const uint8_t* data, size_t size);

} // namespace PX5

#endif // PX5_LOADER_RUNTIME_LINKER_H
