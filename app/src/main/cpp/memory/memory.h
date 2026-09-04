#ifndef PX5_MEMORY_H
#define PX5_MEMORY_H

#include <cstdint>
#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include <mutex>

namespace PX5 {

namespace MemoryFlags {
    constexpr uint32_t PAGE_NONE  = 0x0;
    constexpr uint32_t PAGE_READ  = 0x1;
    constexpr uint32_t PAGE_WRITE = 0x2;
    constexpr uint32_t PAGE_EXEC  = 0x4;

    // v1.38 — AArch64 honors execute-only literally: a PROT_EXEC-only page
    // rejects DATA loads (SEGV_ACCERR), and the FEXCore decoder must READ
    // guest instruction bytes to compile them (Frontend.cpp PeekByte:129).
    // PS5 game images ship XOM-hardened text segments (PF_X without PF_R);
    // sealing one literally killed the vc38 session on its first entry-byte
    // fetch (si_addr=0x140000070, si_code=2). W absence is honored — only
    // the READ bit is forced back on executable pages. Every ARM64 FEX host
    // does the same.
    constexpr uint32_t HostReadableExec(uint32_t flags) {
        return (flags & PAGE_EXEC) ? (flags | PAGE_READ) : flags;
    }
}

// ---------------------------------------------------------------------------
// PX5 Memory Model v2 ("windowed mapping", honest revision)
//
// v1 was dishonest/broken: it reserved one contiguous PROT_NONE region and
// then MapMemory() issued MAP_FIXED mmap()s at RAW GUEST ADDRESSES that were
// unrelated to the reservation (heap-stomping risk) — the reserved region
// had zero architectural meaning.
//
// v2 contract:
//   * Initialize() reserves ONE anonymous window at a guest-VA anchor
//     (candidates tried top-down; first success wins).
//   * Every guest allocation must live inside [GuestBase, GuestEnd).
//   * Guest VA  ->  host pointer = windowHost + (guestVA - GuestBase).
//   * FEXCore threads execute with RIP/stack values bridged through
//     GetHostPointer(), so PC-relative guest code runs correctly even
//     though guestVA != hostAddr.
//   * Untouched window pages stay PROT_NONE: stray accesses fault loudly
//     instead of corrupting the Android heap silently.
//
// CODE INVALIDATION CONTRACT (mtrack, FEX-2608):
//   FEXCore compiles guest code and asks THIS layer to make writes to
//   compiled pages fault (Mark path), and to drop stale translations when
//   guest memory changes underneath the JIT. Every mutation of a mapped
//   executable range therefore fires m_codeInvalidationNotify AFTER the
//   mutation is committed and WITHOUT m_mutex held (the consumer takes
//   FEXCore's CodeInvalidationMutex, which must never nest inside ours):
//     * MapMemory over an existing mapped range (the c423471 class: a
//       MAP_FIXED-style overwrite replaces bytes and invalidated nothing),
//     * UnmapMemory (memory stops holding what was compiled),
//     * ProtectMemory whenever an executable range's W bit changes.
//   The notify is best-effort bookkeeping insurance: even with SMC write
//   faults armed, these paths replace bytes with NO fault, so the notify is
//   the ONLY invalidation signal they produce.
//
// Full identity mapping / FEXCore-managed address spaces are a Phase-C
// upgrade path (FEXCore BaseAddress support) and are deliberately NOT
// claimed to exist here.
// ---------------------------------------------------------------------------
class MemoryManager {
public:
    static MemoryManager& GetInstance();

    bool Initialize(size_t totalMemoryMB = 4096);
    void Shutdown();

    // Returns guest VA (== vaddr on success, 0 on failure).
    uint64_t MapMemory(uint64_t vaddr, size_t size, uint32_t flags,
                       const std::string& tag = "");
    int  CreateSharedMemory(const char* name, size_t size);
    bool UnmapMemory(uint64_t vaddr, size_t size);
    bool ProtectMemory(uint64_t vaddr, size_t size, uint32_t flags);

    bool ReadGuestMemory(uint64_t vaddr, void* outBuffer, size_t size);
    bool WriteGuestMemory(uint64_t vaddr, const void* inBuffer, size_t size);

    // NULL when vaddr is outside the managed window or not mapped.
    void* GetHostPointer(uint64_t vaddr);
    bool  IsValidAddress(uint64_t vaddr, size_t size) const;

    // Program break for brk(2) emulation.
    void     SetProgramBreak(uint64_t base);   // loader-side initial anchor

    // Real brk(2) for the syscall bridge. `requested`==0 queries the current
    // break. Growing MAPS the new span RW before returning it -- the guest
    // writes there immediately. Returns false when the request cannot be
    // honoured, so the bridge can answer with the unchanged break instead of
    // handing the guest an address that is not backed by memory.
    bool SetBrk(uint64_t requested, uint64_t& outBreak);

    size_t GetTotalAllocatedMB() const;

    // Real per-mapping answer for FEXCore's QueryGuestExecutableRange.
    // exec=false in the result is how the host layer says "not executable"
    // (the decoder then refuses to translate); a range is reported only when
    // a MAPPED block actually contains the address.
    struct ExecMapInfo {
        uint64_t base;
        size_t   size;
        bool     exec;
        bool     writable;
    };
    bool FindExecutableMapping(uint64_t vaddr, ExecMapInfo& out) const;

    // Registration for the code-invalidation notify (see contract above).
    // Null callback = no consumer yet (foundation tests before FEX init).
    using CodeInvalidationNotify = std::function<void(uint64_t base, size_t size)>;
    void SetCodeInvalidationNotify(CodeInvalidationNotify fn);

    // Human-readable window info for evidence UI (hex numbers pre-formatted).
    std::string GetWindowInfoString() const;

    // Guest window anchor (0 before Initialize succeeds). Read-only fact,
    // safe from any thread after Initialize; used by the loader to base
    // DYN-style images and by the syscall bridge for low-VA mirroring.
    uint64_t GetGuestBase() const;
    uint64_t GetGuestEnd() const;   // exclusive

    // v1.32 guest ABI policy: PS5-style guests issue MAP_FIXED mmaps at
    // low addresses (the vc32 fixture asks 0x49000000 and demands the
    // bridge return 0x149000000 — the window answer). Any fixed address
    // below the window anchor is mirrored 4 GiB up into the window:
    //     translated = va + 0x100000000
    // Returns true and overwrites `va` when the translation applies and
    // the result lands inside the window; false leaves `va` untouched
    // (the caller then sees the manager's honest window rejection).
    bool TranslateLowFixedVa(uint64_t& va) const;

private:
    MemoryManager() = default;
    ~MemoryManager() = default;

    struct WindowCandidate { uint64_t base; };
    static std::vector<WindowCandidate> WindowCandidates();
    bool MapMemoryImpl_Unlocked(uint64_t vaddr, size_t size, uint32_t flags,
                                const std::string& tag,
                                std::vector<std::pair<uint64_t, size_t>>* invalidatedOut);

    struct MemoryBlock { uint64_t va; size_t size; uint32_t flags; std::string tag; };
    // ORDERED by base VA: address queries are RANGE lookups (upper_bound),
    // not key lookups. A block covers many pages, so an unordered_map keyed
    // by page never answered correctly for anything past a block's first
    // page. See FindBlock_Unlocked.
    std::map<uint64_t, MemoryBlock> m_allocations;

    // Greatest block whose [va, va+size) contains vaddr, or nullptr.
    // Caller holds m_mutex.
    const MemoryBlock* FindBlock_Unlocked(uint64_t vaddr) const;

    // mutable: logically-const queries (FindExecutableMapping) still lock.
    mutable std::mutex m_mutex;
    bool     m_initialized = false;
    uint64_t m_guestBase   = 0;      // numeric guest anchor
    uint64_t m_windowSize  = 0;
    void*    m_hostWindow  = nullptr; // host pointer of m_guestBase

    uint64_t m_programBreak = 0;
    uint64_t m_programBreakLimit = 0;

    size_t m_allocatedBytes = 0;
    CodeInvalidationNotify m_codeInvalidationNotify;

    // Snapshot helper: collects every mapped EXEC range overlapping
    // [vaLo, vaHi). Caller holds m_mutex.
    void CollectExecOverlaps_Unlocked(uint64_t vaLo, uint64_t vaHi,
                                      std::vector<std::pair<uint64_t, size_t>>& out) const;
};

} // namespace PX5

#endif // PX5_MEMORY_H
