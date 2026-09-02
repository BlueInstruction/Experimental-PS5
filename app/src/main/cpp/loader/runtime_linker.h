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

// Reserved guest syscall number for the NID gate (see header comment).
constexpr uint32_t kPx5NidGateSyscall = 0x5C500001u;

// Bionic-native HLE export: receives the guest arguments passed through
// the gate (a1..a5 land at args[0..4]; argc <= 5 for now — the gate ABI
// carries five HLE arguments beside the NID) and returns the guest rax.
using HleHostFn = std::function<int64_t(const uint64_t* args, size_t argc)>;

struct GateResult {
    bool        ok = false;
    int64_t     value = 0;   // guest-visible return when ok
    std::string error;       // named reason when !ok
};

struct DispatchStats {
    uint64_t gateCalls   = 0;  // gate syscalls seen
    uint64_t resolvedHle = 0;  // dispatched into a bionic HLE function
    uint64_t guestRouted = 0;  // NID exists but is a guest export (not gate-callable)
    uint64_t unresolved  = 0;  // NID not registered at all
};

struct ModuleRecord {
    std::string name;
    uint64_t    base    = 0;
    uint64_t    highVa  = 0;
    uint64_t    entry   = 0;
    bool        isSelf  = false;
    size_t      segmentCount = 0;
};

class RuntimeLinker {
public:
    static RuntimeLinker& GetInstance();

    // Clears modules/exports/stats (per-run teardown, like ResetRun).
    void Reset();

    // Records a loaded image in the module registry (evidence + addressing).
    bool RegisterModule(const std::string& name, uint64_t base,
                        uint64_t highVa, uint64_t entry, bool isSelf,
                        size_t segmentCount);

    // The bionic-native side of the bridge. Duplicate NIDs fail by name.
    bool RegisterHleExport(const std::string& library, uint64_t nid,
                           const std::string& name, HleHostFn fn);

    // A NID exported by loaded guest code (direct guest-to-guest calls).
    // Registered for resolution/evidence; NOT gate-callable (see header).
    bool RegisterGuestExport(uint64_t nid, uint64_t guestAddr,
                             const std::string& name);

    // Gate entry used by GuestSyscalls::Dispatch. Does not hold the
    // registry mutex while the HLE function runs (re-entry allowed).
    GateResult DispatchNid(uint64_t nid, const uint64_t* args, size_t argc);

    const DispatchStats& Stats();
    std::string GetSummaryString();      // evidence line for reports
    size_t ModuleCount();
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
    DispatchStats m_stats;
};

// ---------------------------------------------------------------------------
// Real ELF64 PT_DYNAMIC reader (bounded; named errors; no guesses).
// ---------------------------------------------------------------------------
struct DynamicInfo {
    bool        ok = false;
    std::string error;                       // named reason when !ok
    std::string soname;
    std::vector<std::string> needed;         // DT_NEEDED strings
    size_t      dynEntries = 0;              // entries before DT_NULL
    // Every dynamic tag in the SCE OS-specific range, verbatim.
    std::vector<std::pair<uint64_t, uint64_t>> sceTags;
    std::string summary;                     // one evidence line
};

DynamicInfo ParseDynamicFromElfImage(const uint8_t* data, size_t size);

} // namespace PX5

#endif // PX5_LOADER_RUNTIME_LINKER_H
