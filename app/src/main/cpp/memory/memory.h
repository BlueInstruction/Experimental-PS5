#ifndef PX5_MEMORY_H
#define PX5_MEMORY_H

#include <cstdint>
#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>

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
    /**
     * Returns the singleton memory manager instance.
     * @return Reference to the singleton MemoryManager
     */
    static MemoryManager& GetInstance();

    /**
     * Reserves the guest memory window at a suitable guest VA anchor.
     * @param totalMemoryMB Total memory window size in megabytes (default 4096)
     * @return true if window reservation succeeded, false otherwise
     */
    bool Initialize(size_t totalMemoryMB = 4096);

    /**
     * Releases the memory window and clears all allocations.
     */
    void Shutdown();

    /**
     * Maps memory at the specified guest virtual address with given protections.
     * @param vaddr Guest virtual address
     * @param size Region size in bytes
     * @param flags Protection flags (PAGE_READ | PAGE_WRITE | PAGE_EXEC)
     * @param tag Optional debug tag for logging
     * @return Guest VA on success, 0 on failure
     */
    uint64_t MapMemory(uint64_t vaddr, size_t size, uint32_t flags,
                       const std::string& tag = "");

    /**
     * Creates a shared memory object (memfd or ASharedMemory).
     * @param name Optional name for the shared memory
     * @param size Size in bytes
     * @return File descriptor on success, -1 on failure
     */
    int  CreateSharedMemory(const char* name, size_t size);

    /**
     * Unmaps guest memory, resealing it as PROT_NONE.
     * @param vaddr Guest virtual address
     * @param size Region size in bytes
     * @return true if unmap succeeded, false otherwise
     */
    bool UnmapMemory(uint64_t vaddr, size_t size);

    /**
     * Changes protection flags for a mapped guest memory region.
     * @param vaddr Guest virtual address
     * @param size Region size in bytes
     * @param flags New protection flags
     * @return true if protection change succeeded, false otherwise
     */
    bool ProtectMemory(uint64_t vaddr, size_t size, uint32_t flags);

    /**
     * Reads guest memory into a host buffer.
     * @param vaddr Guest virtual address
     * @param outBuffer Host buffer to receive data
     * @param size Bytes to read
     * @return true if read succeeded, false otherwise
     */
    bool ReadGuestMemory(uint64_t vaddr, void* outBuffer, size_t size);

    /**
     * Writes host buffer contents into guest memory.
     * @param vaddr Guest virtual address
     * @param inBuffer Host buffer containing data
     * @param size Bytes to write
     * @return true if write succeeded, false otherwise
     */
    bool WriteGuestMemory(uint64_t vaddr, const void* inBuffer, size_t size);

    /**
     * Converts guest virtual address to host pointer (NULL if unmapped).
     * @param vaddr Guest virtual address
     * @return Host pointer, or nullptr if vaddr is outside window or unmapped
     */
    void* GetHostPointer(uint64_t vaddr);

    /**
     * Checks whether a guest address range is validly mapped.
     * @param vaddr Guest virtual address
     * @param size Region size in bytes
     * @return true if the entire range is mapped, false otherwise
     */
    bool  IsValidAddress(uint64_t vaddr, size_t size) const;

    /**
     * Sets the initial program break base address (called by loader).
     * @param base Initial program break address
     */
    void     SetProgramBreak(uint64_t base);

    /**
     * Implements brk(2) syscall: adjusts program break, backing new memory with RW pages.
     * requested==0 queries current break without changing it.
     * @param requested New break address (0 to query)
     * @param outBreak Output parameter receiving the resulting break address
     * @return true if request succeeded, false if it cannot be honoured
     */
    bool SetBrk(uint64_t requested, uint64_t& outBreak);

    /**
     * Returns total allocated memory in megabytes.
     * @return Allocated bytes divided by 1024*1024
     */
    size_t GetTotalAllocatedMB() const;

    /**
     * Executable mapping information structure for FEXCore decoder queries.
     */
    struct ExecMapInfo {
        uint64_t base;      ///< Base address of the executable region
        size_t   size;      ///< Size of the region in bytes
        bool     exec;      ///< Whether the region is executable
        bool     writable;  ///< Whether the region is writable
    };

    /**
     * Finds the executable mapping containing the given address.
     * Returns false (exec=false) if address is not in an executable region.
     * Signal-safe: reads an atomically published snapshot instead of taking
     * m_mutex, because the SMC fault handler reaches this call from signal
     * context (see fexcore_integration.cpp FaultSafeLock notes).
     * @param vaddr Guest virtual address to query
     * @param out Output parameter receiving mapping info
     * @return true if vaddr is in a mapped executable region, false otherwise
     */
    bool FindExecutableMapping(uint64_t vaddr, ExecMapInfo& out) const;

    /**
     * Code invalidation notification callback type.
     */
    using CodeInvalidationNotify = std::function<void(uint64_t base, size_t size)>;

    /**
     * Registers a callback for code invalidation notifications.
     * Null callback = no consumer yet (e.g., before FEXCore init).
     * @param fn Callback invoked when code memory is modified
     */
    void SetCodeInvalidationNotify(CodeInvalidationNotify fn);

    /**
     * Returns human-readable window information string for diagnostics.
     * @return Formatted string with guest VA range, size, mapped bytes, block count
     */
    std::string GetWindowInfoString() const;

    /**
     * Returns the guest window base address (0 before Initialize succeeds).
     * @return Guest virtual address anchor
     */
    uint64_t GetGuestBase() const;

    /**
     * Returns the exclusive end of the guest window.
     * @return Guest virtual address marking the end (exclusive)
     */
    uint64_t GetGuestEnd() const;

    /**
     * Translates low fixed-address mappings into the guest window (PS5 ABI policy).
     * Addresses below window anchor are relocated +4 GiB into the window.
     * @param va Input/output parameter: guest address to translate
     * @return true if translation applied and result is in window, false otherwise
     */
    bool TranslateLowFixedVa(uint64_t& va) const;

private:
    MemoryManager() = default;
    ~MemoryManager() = default;

    /**
     * Window candidate structure for guest VA anchor selection.
     */
    struct WindowCandidate { uint64_t base; };

    /**
     * Returns list of guest VA anchor candidates (tried in order).
     * @return Vector of window candidates to attempt
     */
    static std::vector<WindowCandidate> WindowCandidates();

    /**
     * Internal map implementation (caller must hold m_mutex).
     * @param vaddr Guest virtual address
     * @param size Region size in bytes
     * @param flags Protection flags
     * @param tag Debug tag
     * @param invalidatedOut Output vector for invalidated exec ranges
     * @return true if map succeeded, false otherwise
     */
    bool MapMemoryImpl_Unlocked(uint64_t vaddr, size_t size, uint32_t flags,
                                const std::string& tag,
                                std::vector<std::pair<uint64_t, size_t>>* invalidatedOut);

    /**
     * Memory block tracking structure.
     */
    struct MemoryBlock { uint64_t va; size_t size; uint32_t flags; std::string tag; };

    /// ORDERED by base VA for range lookups (upper_bound), not key lookups
    std::map<uint64_t, MemoryBlock> m_allocations;

    /**
     * Finds the memory block containing the given address (caller holds m_mutex).
     * @param vaddr Guest virtual address to query
     * @return Pointer to block containing vaddr, or nullptr if unmapped
     */
    const MemoryBlock* FindBlock_Unlocked(uint64_t vaddr) const;

    /// mutable: logically-const queries (IsValidAddress) still lock.
    /// FindExecutableMapping deliberately does NOT: fault handlers reach it,
    /// and a handler taking this mutex could self-deadlock on a thread that
    /// faulted while holding it.
    mutable std::mutex m_mutex;
    bool     m_initialized = false;
    uint64_t m_guestBase   = 0;      ///< numeric guest anchor
    uint64_t m_windowSize  = 0;
    void*    m_hostWindow  = nullptr; ///< host pointer of m_guestBase

    uint64_t m_programBreak = 0;
    uint64_t m_programBreakLimit = 0;

    size_t m_allocatedBytes = 0;
    CodeInvalidationNotify m_codeInvalidationNotify;

    /**
     * Collects mapped EXEC ranges overlapping [vaLo, vaHi) (caller holds m_mutex).
     * @param vaLo Low end of range (inclusive)
     * @param vaHi High end of range (exclusive)
     * @param out Output vector receiving overlapping exec ranges
     */
    void CollectExecOverlaps_Unlocked(uint64_t vaLo, uint64_t vaHi,
                                      std::vector<std::pair<uint64_t, size_t>>& out) const;

    // ---- Signal-safe executable-range snapshot ----------------------------
    //
    // FindExecutableMapping is reachable from the SMC fault handler, where
    // taking m_mutex can self-deadlock (a fault interrupting a thread that
    // holds it). Instead the query reads this snapshot: writers rebuild it
    // under m_mutex after every exec-relevant mutation and publish through a
    // seqlock (odd generation = mid-rebuild); readers retry until the
    // generation is stable. All fields are atomic, so the read has no data
    // race; the seqlock pass only guards generation consistency.
    //
    // The rebuild masks SIGSEGV/SIGBUS (same discipline as FaultSafeLock in
    // fexcore_integration.cpp): a handler on the rebuilding thread can then
    // never spin on a generation its own interrupted thread left odd.
    struct ExecRange {
        std::atomic<uint64_t> base{0};
        std::atomic<uint64_t> end{0};
        std::atomic<uint32_t> writable{0};
    };
    static constexpr size_t kExecSnapshotCap = 128;
    ExecRange              m_execRanges[kExecSnapshotCap];
    std::atomic<uint32_t>  m_execRangeCount{0};
    std::atomic<uint64_t>  m_execSeq{0};          ///< seqlock generation
    std::atomic<bool>      m_execTruncated{false};

    /**
     * Rebuilds and publishes the executable-range snapshot (caller holds m_mutex).
     */
    void RebuildExecSnapshot_Unlocked();
};

} // namespace PX5

#endif // PX5_MEMORY_H
