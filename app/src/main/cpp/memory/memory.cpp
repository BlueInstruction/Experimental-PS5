#include "memory.h"
#include "page_size.h"
#include "../utils/logger.h"

#include <sys/mman.h>
#include <sys/syscall.h>
#include <android/sharedmem.h>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <unistd.h>

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

namespace PX5 {

namespace {
size_t RoundUp(size_t v, size_t a) { return (v + a - 1) & ~(a - 1); }
uint64_t RoundUp64(uint64_t v, uint64_t a) { return (v + a - 1) & ~(a - 1); }
uint64_t RoundDown64(uint64_t v, uint64_t a) { return v & ~(a - 1); }
} // namespace

MemoryManager& MemoryManager::GetInstance() {
    static MemoryManager instance;
    return instance;
}

std::vector<MemoryManager::WindowCandidate> MemoryManager::WindowCandidates() {
    // Candidate ANCHORS only. The size comes from Initialize(totalMemoryMB)
    // and is the same for every candidate -- these entries used to carry a
    // hardcoded 0x10000000 (256 MiB) that no caller ever read, so
    // Initialize(4096) looked like it asked for 4 GiB while the constant
    // here said 256 MiB. Removing the field removes the contradiction.
    //
    // CANONICAL anchor chosen so that TEST_GUEST_LOAD_VADDR (0x140000000)
    // lives INSIDE the window. Larger windows / dynamic layout (FEXCore
    // BaseAddress path) is a documented Phase-C upgrade, not silently
    // assumed here.
    constexpr uint64_t kPrimaryAnchor = 0x140000000ULL;
    constexpr uint64_t kFallbackShift = 0x00800000ULL * 32; // -256 MiB step
    return {
        { kPrimaryAnchor },
        { kPrimaryAnchor - kFallbackShift },
    };
}

bool MemoryManager::Initialize(size_t totalMemoryMB) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) return true;

    const size_t windowSize = RoundUp(totalMemoryMB * 1024ull * 1024ull,
                                      HostPageSize());

    void* hostWindow = nullptr;
    uint64_t chosenBase = 0;
    std::string chosenNote = "none";

    for (const auto& cand : WindowCandidates()) {
        // Where possible we want the guest numeric base and the host mapping
        // to start at the SAME address (identity-friendly), but correctness
        // never depends on it: the bridge math works either way.
        //
        // MAP_FIXED_NOREPLACE, never bare MAP_FIXED. Plain MAP_FIXED does not
        // fail when the range is taken -- it silently UNMAPS whatever lives
        // there. At 0x140000000 on Android that can be an ART heap region, a
        // loaded .so, or the JIT cache, and the corruption surfaces later as
        // an unrelated crash. NOREPLACE makes a busy range return EEXIST so
        // the fallback candidate below is actually reachable instead of being
        // dead code.
        void* p = mmap(reinterpret_cast<void*>(cand.base),
                       windowSize,
                       PROT_NONE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE |
                           MAP_FIXED_NOREPLACE,
                       -1, 0);
        if (p != MAP_FAILED && reinterpret_cast<uint64_t>(p) == cand.base) {
            hostWindow = p;
            chosenBase = cand.base;
            chosenNote = "MAP_FIXED_NOREPLACE at preferred anchor";
            break;
        }
        if (p != MAP_FAILED) {
            // Kernel too old to honour NOREPLACE: it fell back to a
            // "hint" mapping elsewhere. Release it and keep looking rather
            // than accepting a base we did not ask for.
            munmap(p, windowSize);
            PX5_LOGD(LogCategory::MEMORY,
                     "Window candidate 0x%llx: kernel relocated the mapping "
                     "(no MAP_FIXED_NOREPLACE support) - rejected",
                     (unsigned long long)cand.base);
            continue;
        }
        PX5_LOGD(LogCategory::MEMORY,
                 "Window candidate 0x%llx unavailable (%s)",
                 (unsigned long long)cand.base, strerror(errno));
    }

    if (!hostWindow) {
        PX5_LOGE(LogCategory::MEMORY,
                 "MemoryManager: all guest window anchors failed");
        return false;
    }

    m_guestBase  = chosenBase;
    m_windowSize = windowSize;
    m_hostWindow = hostWindow;
    m_programBreak = 0;      // set by the loader after segments are placed
    m_programBreakLimit = m_guestBase + m_windowSize;
    m_allocatedBytes = 0;
    m_initialized = true;

    PX5_LOGI(LogCategory::MEMORY,
             "Guest window reserved: VA=[0x%llx..0x%llx] host=%p via %s",
             (unsigned long long)m_guestBase,
             (unsigned long long)(m_guestBase + m_windowSize),
             m_hostWindow, chosenNote.c_str());
    return true;
}

void MemoryManager::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) return;
    if (m_hostWindow) {
        munmap(m_hostWindow, m_windowSize);
        m_hostWindow = nullptr;
    }
    m_allocations.clear();
    m_guestBase = 0;
    m_windowSize = 0;
    m_allocatedBytes = 0;
    m_initialized = false;
    PX5_LOGI(LogCategory::MEMORY, "Memory window shut down successfully");
}

void MemoryManager::CollectExecOverlaps_Unlocked(
        uint64_t vaLo, uint64_t vaHi,
        std::vector<std::pair<uint64_t, size_t>>& out) const {
    // Caller holds m_mutex. Snapshots every mapped, currently-executable
    // block overlapping [vaLo, vaHi) so the invalidation notify can fire
    // AFTER m_mutex is released (FEXCore's CodeInvalidationMutex must never
    // nest inside ours).
    for (const auto& [base, blk] : m_allocations) {
        if (!(blk.flags & MemoryFlags::PAGE_EXEC)) continue;
        const uint64_t blkHi = blk.va + blk.size;
        if (blk.va >= vaHi || blkHi <= vaLo) continue;
        const uint64_t lo = std::max<uint64_t>(blk.va, vaLo);
        const uint64_t hi = std::min<uint64_t>(blkHi, vaHi);
        out.push_back({lo, hi - lo});
    }
}

void MemoryManager::SetCodeInvalidationNotify(CodeInvalidationNotify fn) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_codeInvalidationNotify = std::move(fn);
}

bool MemoryManager::MapMemoryImpl_Unlocked(uint64_t vaddr, size_t size,
                                           uint32_t flags,
                                           const std::string& tag,
                                           std::vector<std::pair<uint64_t, size_t>>* invalidatedOut) {
    // NOTE: caller holds m_mutex. Overlapping exec ranges are snapshotted to
    // *invalidatedOut and fired by the CALLER after m_mutex is released.
    if (!m_initialized) return false;
    if (vaddr == 0 || size == 0) return false;

    const uint64_t vaLo = RoundDown64(vaddr, HostPageSize());
    const uint64_t vaHi = RoundUp64(vaddr + size, HostPageSize());

    if (vaLo < m_guestBase || vaHi > m_guestBase + m_windowSize) {
        PX5_LOGE(LogCategory::MEMORY,
                 "MapMemory REJECTED: [0x%llx..0x%llx] outside window "
                 "[0x%llx..0x%llx]",
                 (unsigned long long)vaLo, (unsigned long long)vaHi,
                 (unsigned long long)m_guestBase,
                 (unsigned long long)(m_guestBase + m_windowSize));
        return false;
    }

    // Snapshot overlapping EXEC ranges BEFORE mutating anything: a map that
    // lands on compiled guest memory replaces the bytes and faults nothing,
    // so this notify is the only invalidation signal it produces (the
    // sharpdroid c423471 bug class, found by their public git history).
    if (invalidatedOut) {
        CollectExecOverlaps_Unlocked(vaLo, vaHi, *invalidatedOut);
    }

    // v1.38: executable pages are never mapped below-read
    // (MemoryFlags::HostReadableExec) — the JIT must be able to fetch
    // guest bytes. W/X bits pass through untouched.
    const uint32_t hostFlags = MemoryFlags::HostReadableExec(flags);
    int prot = PROT_NONE;
    if (hostFlags & MemoryFlags::PAGE_READ)  prot |= PROT_READ;
    if (hostFlags & MemoryFlags::PAGE_WRITE) prot |= PROT_WRITE;
    if (hostFlags & MemoryFlags::PAGE_EXEC)  prot |= PROT_EXEC;

    char* hostLo = static_cast<char*>(m_hostWindow) + (vaLo - m_guestBase);
    if (mprotect(hostLo, vaHi - vaLo, prot) != 0) {
        PX5_LOGE(LogCategory::MEMORY,
                 "mprotect failed for VA 0x%llx: %s",
                 (unsigned long long)vaLo, strerror(errno));
        return false;
    }

    m_allocations[vaLo] = { vaLo, vaHi - vaLo, hostFlags, tag };
    m_allocatedBytes += vaHi - vaLo;
    PX5_LOGI(LogCategory::MEMORY,
             "Mapped guest VA 0x%llx-0x%llx (%zu B, prot=%d, tag=%s)",
             (unsigned long long)vaLo, (unsigned long long)vaHi,
             vaHi - vaLo, prot, tag.c_str());
    return true;
}

// Public overload keeps the old signature/behavior contract.
uint64_t MemoryManager::MapMemory(uint64_t vaddr, size_t size, uint32_t flags,
                                  const std::string& tag) {
    std::vector<std::pair<uint64_t, size_t>> invalidated;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!MapMemoryImpl_Unlocked(vaddr, size, flags, tag, &invalidated)) {
            return 0;
        }
    }
    // Fire the notify AFTER m_mutex is released (contract: the consumer's
    // unprotect path re-enters MemoryManager, which would deadlock under our
    // non-recursive mutex).
    if (!invalidated.empty() && m_codeInvalidationNotify) {
        for (const auto& [base, len] : invalidated) {
            PX5_LOGI(LogCategory::MEMORY,
                     "Code invalidation notify (map-overwrite): 0x%llx +0x%zx",
                     (unsigned long long)base, len);
            m_codeInvalidationNotify(base, len);
        }
    }
    return vaddr;
}

int MemoryManager::CreateSharedMemory(const char* name, size_t size) {
    int fd = -1;
#if defined(__NR_memfd_create)
    fd = syscall(__NR_memfd_create, name ? name : "px5_shm", 1 /* MFD_CLOEXEC */);
    if (fd >= 0 && ftruncate(fd, static_cast<off_t>(size)) < 0) {
        close(fd);
        fd = -1;
    }
#endif
    if (fd < 0) {
        fd = ASharedMemory_create(name ? name : "px5_shm", size);
    }
    if (fd >= 0) {
        PX5_LOGI(LogCategory::MEMORY,
                 "Shared memory '%s' created (fd=%d, %zu B)",
                 name ? name : "px5_shm", fd, size);
    } else {
        PX5_LOGE(LogCategory::MEMORY, "Shared memory '%s' FAILED",
                 name ? name : "px5_shm");
    }
    return fd;
}

bool MemoryManager::UnmapMemory(uint64_t vaddr, size_t size) {
    std::vector<std::pair<uint64_t, size_t>> invalidated;
    uint64_t vaLo = 0;
    size_t   len   = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_initialized || size == 0) return false;

        // Range lookup: the request may start in the middle of a block, and
        // it may cover only part of it. The old code required an exact base
        // match and then erased the WHOLE block record while only resealing
        // `len` bytes -- leaving the remainder mapped but invisible to the
        // manager, and m_allocatedBytes permanently wrong.
        const MemoryBlock* hit = FindBlock_Unlocked(vaddr);
        if (!hit) return false;

        vaLo = PageAlignDown(vaddr);
        const uint64_t reqHi = PageAlignUp(vaddr + size);
        const uint64_t blkHi = hit->va + hit->size;
        const uint64_t vaHi = std::min(reqHi, blkHi);
        if (vaHi <= vaLo) return false;
        len = static_cast<size_t>(vaHi - vaLo);

        const MemoryBlock original = *hit;
        CollectExecOverlaps_Unlocked(vaLo, vaHi, invalidated);

        char* hostLo = static_cast<char*>(m_hostWindow) + (vaLo - m_guestBase);
        // Re-seal as PROT_NONE instead of munmap so the window stays coherent.
        if (mprotect(hostLo, len, PROT_NONE) != 0) return false;

        // Split the record so the surviving head/tail stay accounted for.
        m_allocations.erase(original.va);
        if (vaLo > original.va) {
            m_allocations[original.va] = { original.va,
                                           static_cast<size_t>(vaLo - original.va),
                                           original.flags, original.tag };
        }
        if (vaHi < blkHi) {
            m_allocations[vaHi] = { vaHi, static_cast<size_t>(blkHi - vaHi),
                                    original.flags, original.tag };
        }

        m_allocatedBytes -= len;
        PX5_LOGI(LogCategory::MEMORY,
                 "Unmapped guest VA 0x%llx (%zu B resealed, block "
                 "[0x%llx..0x%llx) split head=%llu tail=%llu)",
                 (unsigned long long)vaLo, len,
                 (unsigned long long)original.va, (unsigned long long)blkHi,
                 (unsigned long long)(vaLo > original.va ? vaLo - original.va : 0),
                 (unsigned long long)(vaHi < blkHi ? blkHi - vaHi : 0));
    }
    if (!invalidated.empty() && m_codeInvalidationNotify) {
        for (const auto& [base, blen] : invalidated) {
            PX5_LOGI(LogCategory::MEMORY,
                     "Code invalidation notify (unmap): 0x%llx +0x%zx",
                     (unsigned long long)base, blen);
            m_codeInvalidationNotify(base, blen);
        }
    }
    return true;
}

bool MemoryManager::ProtectMemory(uint64_t vaddr, size_t size, uint32_t flags) {
    std::vector<std::pair<uint64_t, size_t>> invalidated;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_initialized) return false;
        if (vaddr < m_guestBase || vaddr + size > m_guestBase + m_windowSize)
            return false;

        const uint64_t vaLo = RoundDown64(vaddr, HostPageSize());
        const uint64_t vaHi = RoundUp64(vaddr + size, HostPageSize());

        // v1.38: same never-below-read rule as the map path — a guest
        // mprotect(PROT_EXEC) must stay fetchable for the JIT.
        const uint32_t hostFlags = MemoryFlags::HostReadableExec(flags);
        int prot = 0;
        if (hostFlags & MemoryFlags::PAGE_READ)  prot |= PROT_READ;
        if (hostFlags & MemoryFlags::PAGE_WRITE) prot |= PROT_WRITE;
        if (hostFlags & MemoryFlags::PAGE_EXEC)  prot |= PROT_EXEC;

        // Invalidate only when the W bit is being taken OFF an exec range:
        // adding W does not stale the JIT (bytes unchanged); dropping W can
        // only follow a byte change made possible by a prior W, which the
        // map/write paths above already reported. Removing W here (e.g. the
        // guest sealing text) still ends the range's compiled lifetime.
        const bool writeChanging = (hostFlags & MemoryFlags::PAGE_WRITE) == 0;
        if (writeChanging) {
            CollectExecOverlaps_Unlocked(vaLo, vaHi, invalidated);
        }

        char* h = static_cast<char*>(m_hostWindow) + (vaLo - m_guestBase);
        if (mprotect(h, vaHi - vaLo, prot) != 0) {
            PX5_LOGE(LogCategory::MEMORY,
                     "ProtectMemory mprotect failed VA 0x%llx: %s",
                     (unsigned long long)vaLo, strerror(errno));
            return false;
        }
        // Update the recorded flags: the SMC registry and the executable-
        // range query both read what the guest asked for from here.
        for (auto& [base, blk] : m_allocations) {
            if (blk.va + blk.size <= vaLo || blk.va >= vaHi) continue;
            blk.flags = flags;
        }
    }
    if (!invalidated.empty() && m_codeInvalidationNotify) {
        for (const auto& [base, blen] : invalidated) {
            PX5_LOGI(LogCategory::MEMORY,
                     "Code invalidation notify (protect): 0x%llx +0x%zx",
                     (unsigned long long)base, blen);
            m_codeInvalidationNotify(base, blen);
        }
    }
    return true;
}

bool MemoryManager::ReadGuestMemory(uint64_t vaddr, void* outBuffer, size_t size) {
    // Deliberately lock-free: HLE reads guest memory at high frequency and —
    // decisively — a read fault on an SMC-protected page must not arrive with
    // m_mutex held, because the fault intercept has to re-enter this class
    // (FindExecutableMapping) from signal context. GetHostPointer is pure
    // address math; m_allocations is not consulted here.
    void* h = GetHostPointer(vaddr);
    if (!h || !outBuffer ||
        vaddr + size > m_guestBase + m_windowSize) return false;
    memcpy(outBuffer, h, size);
    return true;
}

bool MemoryManager::WriteGuestMemory(uint64_t vaddr, const void* inBuffer, size_t size) {
    // Lock-free for the same reason as ReadGuestMemory: this is exactly the
    // call that may write into an SMC-protected code page and fault. The
    // fault handler re-enters this class from signal context, so no mutex of
    // ours may be held across the memcpy.
    void* h = GetHostPointer(vaddr);
    if (!h || !inBuffer ||
        vaddr + size > m_guestBase + m_windowSize) return false;
    memcpy(h, inBuffer, size);
    return true;
}

void* MemoryManager::GetHostPointer(uint64_t vaddr) {
    // Pure address math, deliberately lock-free: used on the syscall hot
    // path AND from the fault intercept's unprotect step, which must be able
    // to run from signal context without taking m_mutex.
    if (!m_initialized) return nullptr;
    if (vaddr < m_guestBase || vaddr >= m_guestBase + m_windowSize) return nullptr;
    return static_cast<char*>(m_hostWindow) + (vaddr - m_guestBase);
}

const MemoryManager::MemoryBlock*
MemoryManager::FindBlock_Unlocked(uint64_t vaddr) const {
    // Range lookup, NOT key lookup. m_allocations is keyed by the base VA of
    // each block, and a block spans many pages: a 2.9 MiB PT_LOAD produces
    // exactly ONE entry. Looking up RoundDown(vaddr, pageSize) therefore only
    // ever matched the block's first page and reported every later address as
    // unmapped -- which made IsValidAddress() and FindExecutableMapping()
    // answer "no" for essentially the whole image.
    if (m_allocations.empty()) return nullptr;
    auto it = m_allocations.upper_bound(vaddr);   // first base > vaddr
    if (it == m_allocations.begin()) return nullptr;
    --it;                                        // greatest base <= vaddr
    const MemoryBlock& blk = it->second;
    if (vaddr < blk.va || vaddr >= blk.va + blk.size) return nullptr;
    return &blk;
}

bool MemoryManager::IsValidAddress(uint64_t vaddr, size_t size) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) return false;
    if (vaddr < m_guestBase || vaddr + size > m_guestBase + m_windowSize)
        return false;
    // The whole requested span must be covered, not just its first byte.
    const uint64_t end = vaddr + (size ? size : 1);
    uint64_t cursor = vaddr;
    while (cursor < end) {
        const MemoryBlock* blk = FindBlock_Unlocked(cursor);
        if (!blk) return false;
        cursor = blk->va + blk->size;
    }
    return true;
}

bool MemoryManager::FindExecutableMapping(uint64_t vaddr, ExecMapInfo& out) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    out = {0, 0, false, false};
    if (!m_initialized) return false;
    // Range lookup (see FindBlock_Unlocked). FEXCore calls this through
    // QueryGuestExecutableRange before translating every block; the old
    // key lookup answered {0,0,false} for every address past a segment's
    // first page, so the decoder refused to translate almost all guest code.
    const MemoryBlock* blk = FindBlock_Unlocked(vaddr);
    if (!blk) return false;
    if (!(blk->flags & MemoryFlags::PAGE_EXEC)) return false;
    out.base     = blk->va;
    out.size     = blk->size;
    out.exec     = true;
    out.writable = (blk->flags & MemoryFlags::PAGE_WRITE) != 0;
    return true;
}

void MemoryManager::SetProgramBreak(uint64_t base) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (base >= m_guestBase && base < m_programBreakLimit)
        m_programBreak = base;
}

bool MemoryManager::SetBrk(uint64_t requested, uint64_t& outBreak) {
    // Real brk(2): the guest's crt allocator calls this and then WRITES to
    // the returned range. The previous implementation only moved a counter
    // ("commit pages lazily" followed by a discarded variable), so the
    // syscall bridge handed back an address backed by PROT_NONE window
    // pages and the first malloc() store faulted.
    std::vector<std::pair<uint64_t, size_t>> invalidated;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_initialized || m_programBreak == 0) return false;

        if (requested == 0) {           // query form
            outBreak = m_programBreak;
            return true;
        }
        const uint64_t next = PageAlignUp(requested);
        if (next < m_guestBase || next >= m_programBreakLimit) return false;

        const uint64_t cur = PageAlignUp(m_programBreak);
        if (next > cur) {
            // Grow: actually back the new span with RW pages.
            if (!MapMemoryImpl_Unlocked(cur, static_cast<size_t>(next - cur),
                                        MemoryFlags::PAGE_READ |
                                            MemoryFlags::PAGE_WRITE,
                                        "guest_brk", &invalidated)) {
                return false;
            }
        }
        m_programBreak = requested;
        outBreak = m_programBreak;
    }
    if (!invalidated.empty() && m_codeInvalidationNotify) {
        for (const auto& [base, blen] : invalidated) {
            m_codeInvalidationNotify(base, blen);
        }
    }
    return true;
}

size_t MemoryManager::GetTotalAllocatedMB() const {
    return m_allocatedBytes / (1024 * 1024);
}

std::string MemoryManager::GetWindowInfoString() const {
    if (!m_initialized) return "window=NOT_INITIALIZED";
    char buf[160];
    snprintf(buf, sizeof(buf),
             "guestVA=[0x%llx..0x%llx] sizeMB=%zu mapped=%.2f MiB blocks=%zu",
             (unsigned long long)m_guestBase,
             (unsigned long long)(m_guestBase + m_windowSize),
             m_windowSize / (1024 * 1024),
             m_allocatedBytes / (1024.0 * 1024.0),
             m_allocations.size());
    return buf;
}

uint64_t MemoryManager::GetGuestBase() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_guestBase;
}

uint64_t MemoryManager::GetGuestEnd() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_guestBase + m_windowSize;
}

bool MemoryManager::TranslateLowFixedVa(uint64_t& va) const {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (!m_initialized || va >= m_guestBase) return false;
    // RELOCATION, NOT EMULATION -- and the caller must tell the guest.
    //
    // A guest MAP_FIXED below the window anchor cannot be honoured at the
    // address it asked for: that memory belongs to the Android process.
    // Rather than fail every such request, the request is relocated 4 GiB
    // up (0x49000000 -> 0x149000000), which lands inside the window because
    // the anchor 0x140000000 is itself 4 GiB-shaped.
    //
    // This is a deliberate ABI deviation, not PS5 behaviour: a real
    // MAP_FIXED either gets its address or fails. It is safe ONLY because
    // the caller returns the translated address to the guest, so a guest
    // that checks mmap's return value follows the relocation. A guest that
    // hardcodes the low address and ignores the return value WILL fault --
    // that is a known limitation of this path, not a working translation.
    const uint64_t rebased = va + 0x100000000ull;
    if (rebased < m_guestBase ||
        rebased >= m_guestBase + m_windowSize) return false;
    va = rebased;
    return true;
}

} // namespace PX5
