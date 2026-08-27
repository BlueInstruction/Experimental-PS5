#ifndef PX5_MEMORY_H
#define PX5_MEMORY_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>

namespace PX5 {

namespace MemoryFlags {
    constexpr uint32_t PAGE_NONE  = 0x0;
    constexpr uint32_t PAGE_READ  = 0x1;
    constexpr uint32_t PAGE_WRITE = 0x2;
    constexpr uint32_t PAGE_EXEC  = 0x4;
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
    void     SetProgramBreak(uint64_t base);
    uint64_t GrowProgramBreak(intptr_t increment);   // returns new break, 0=fail

    size_t GetTotalAllocatedMB() const;

    // Human-readable window info for evidence UI (hex numbers pre-formatted).
    std::string GetWindowInfoString() const;

private:
    MemoryManager() = default;
    ~MemoryManager() = default;

    struct WindowCandidate { uint64_t base; size_t size; };
    static std::vector<WindowCandidate> WindowCandidates();
    bool MapMemoryImpl_Unlocked(uint64_t vaddr, size_t size, uint32_t flags,
                                const std::string& tag);

    struct MemoryBlock { uint64_t va; size_t size; uint32_t flags; std::string tag; };
    std::unordered_map<uint64_t, MemoryBlock> m_allocations;

    std::mutex m_mutex;
    bool     m_initialized = false;
    uint64_t m_guestBase   = 0;      // numeric guest anchor
    uint64_t m_windowSize  = 0;
    void*    m_hostWindow  = nullptr; // host pointer of m_guestBase

    uint64_t m_programBreak = 0;
    uint64_t m_programBreakLimit = 0;

    size_t m_allocatedBytes = 0;
};

} // namespace PX5

#endif // PX5_MEMORY_H
