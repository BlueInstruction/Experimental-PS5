#include "memory.h"
#include "../utils/logger.h"

#include <sys/mman.h>
#include <sys/syscall.h>
#include <android/sharedmem.h>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <unistd.h>

namespace PX5 {

namespace {
constexpr size_t kPageSize = 4096;

size_t RoundUp(size_t v, size_t a) { return (v + a - 1) & ~(a - 1); }
uint64_t RoundUp64(uint64_t v, uint64_t a) { return (v + a - 1) & ~(a - 1); }
uint64_t RoundDown64(uint64_t v, uint64_t a) { return v & ~(a - 1); }
} // namespace

MemoryManager& MemoryManager::GetInstance() {
    static MemoryManager instance;
    return instance;
}

std::vector<MemoryManager::WindowCandidate> MemoryManager::WindowCandidates() {
    // Canonical foundation window. CANONICAL anchor chosen so that
    // TEST_GUEST_LOAD_VADDR (0x140000000) lives INSIDE the first half.
    // Larger windows / dynamic layout (FEXCore BaseAddress path) is a
    // documented Phase-C upgrade, not silently assumed here.
    constexpr uint64_t kPrimaryAnchor = 0x140000000ULL;
    constexpr uint64_t kFallbackShift = 0x00800000ULL * 32; // -256 MiB step
    return {
        { kPrimaryAnchor,                      0x10000000ULL },
        { kPrimaryAnchor - kFallbackShift,     0x10000000ULL },
    };
}

bool MemoryManager::Initialize(size_t totalMemoryMB) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) return true;

    const size_t windowSize = RoundUp(totalMemoryMB * 1024ull * 1024ull,
                                      kPageSize);

    void* hostWindow = nullptr;
    uint64_t chosenBase = 0;
    std::string chosenNote = "none";

    for (const auto& cand : WindowCandidates()) {
        // Where possible we want the guest numeric base and the host mapping
        // to start at the SAME address (identity-friendly), but correctness
        // never depends on it: the bridge math works either way.
        void* p = mmap(reinterpret_cast<void*>(cand.base),
                       windowSize,
                       PROT_NONE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED,
                       -1, 0);
        if (p != MAP_FAILED && reinterpret_cast<uint64_t>(p) == cand.base) {
            hostWindow = p;
            chosenBase = cand.base;
            chosenNote = "MAP_FIXED at preferred anchor";
            break;
        }
        if (p != MAP_FAILED) {
            munmap(p, windowSize);
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

bool MemoryManager::MapMemoryImpl_Unlocked(uint64_t vaddr, size_t size,
                                           uint32_t flags,
                                           const std::string& tag) {
    // NOTE: caller holds m_mutex.
    if (!m_initialized) return false;
    if (vaddr == 0 || size == 0) return false;

    const uint64_t vaLo = RoundDown64(vaddr, kPageSize);
    const uint64_t vaHi = RoundUp64(vaddr + size, kPageSize);

    if (vaLo < m_guestBase || vaHi > m_guestBase + m_windowSize) {
        PX5_LOGE(LogCategory::MEMORY,
                 "MapMemory REJECTED: [0x%llx..0x%llx] outside window "
                 "[0x%llx..0x%llx]",
                 (unsigned long long)vaLo, (unsigned long long)vaHi,
                 (unsigned long long)m_guestBase,
                 (unsigned long long)(m_guestBase + m_windowSize));
        return false;
    }

    int prot = PROT_NONE;
    if (flags & MemoryFlags::PAGE_READ)  prot |= PROT_READ;
    if (flags & MemoryFlags::PAGE_WRITE) prot |= PROT_WRITE;
    if (flags & MemoryFlags::PAGE_EXEC)  prot |= PROT_EXEC;

    char* hostLo = static_cast<char*>(m_hostWindow) + (vaLo - m_guestBase);
    if (mprotect(hostLo, vaHi - vaLo, prot) != 0) {
        PX5_LOGE(LogCategory::MEMORY,
                 "mprotect failed for VA 0x%llx: %s",
                 (unsigned long long)vaLo, strerror(errno));
        return false;
    }

    m_allocations[vaLo] = { vaLo, vaHi - vaLo, flags, tag };
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
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!MapMemoryImpl_Unlocked(vaddr, size, flags, tag)) return 0;
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
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_allocations.find(RoundDown64(vaddr, kPageSize));
    if (it == m_allocations.end()) return false;

    const uint64_t vaLo = it->first;
    const size_t len = std::min(size <= kPageSize ? RoundUp64(vaddr + size - vaLo, kPageSize)
                                                  : size,
                                it->second.size);
    char* hostLo = static_cast<char*>(m_hostWindow) + (vaLo - m_guestBase);
    // Re-seal as PROT_NONE instead of munmap so the window stays coherent.
    if (mprotect(hostLo, len, PROT_NONE) != 0) return false;

    m_allocatedBytes -= len;
    m_allocations.erase(it);
    PX5_LOGI(LogCategory::MEMORY,
             "Unmapped guest VA 0x%llx (%zu B resealed)",
             (unsigned long long)vaLo, len);
    return true;
}

bool MemoryManager::ProtectMemory(uint64_t vaddr, size_t size, uint32_t flags) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) return false;
    if (vaddr < m_guestBase || vaddr + size > m_guestBase + m_windowSize)
        return false;
    int prot = 0;
    if (flags & MemoryFlags::PAGE_READ)  prot |= PROT_READ;
    if (flags & MemoryFlags::PAGE_WRITE) prot |= PROT_WRITE;
    if (flags & MemoryFlags::PAGE_EXEC)  prot |= PROT_EXEC;
    char* h = static_cast<char*>(m_hostWindow) + (vaddr - m_guestBase);
    return mprotect(h, size, prot) == 0;
}

bool MemoryManager::ReadGuestMemory(uint64_t vaddr, void* outBuffer, size_t size) {
    std::lock_guard<std::mutex> lock(m_mutex);
    void* h = GetHostPointer(vaddr);
    if (!h || !outBuffer ||
        vaddr + size > m_guestBase + m_windowSize) return false;
    memcpy(outBuffer, h, size);
    return true;
}

bool MemoryManager::WriteGuestMemory(uint64_t vaddr, const void* inBuffer, size_t size) {
    std::lock_guard<std::mutex> lock(m_mutex);
    void* h = GetHostPointer(vaddr);
    if (!h || !inBuffer ||
        vaddr + size > m_guestBase + m_windowSize) return false;
    memcpy(h, inBuffer, size);
    return true;
}

void* MemoryManager::GetHostPointer(uint64_t vaddr) {
    // Caller normally holds m_mutex via the read/write wrappers; standalone
    // use (syscall bridge on the hot path) is address-math only and safe.
    if (!m_initialized) return nullptr;
    if (vaddr < m_guestBase || vaddr >= m_guestBase + m_windowSize) return nullptr;
    return static_cast<char*>(m_hostWindow) + (vaddr - m_guestBase);
}

bool MemoryManager::IsValidAddress(uint64_t vaddr, size_t size) const {
    if (!m_initialized) return false;
    return vaddr >= m_guestBase &&
           vaddr + size <= m_guestBase + m_windowSize &&
           m_allocations.count(RoundDown64(vaddr, kPageSize)) > 0;
}

void MemoryManager::SetProgramBreak(uint64_t base) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (base >= m_guestBase && base < m_programBreakLimit)
        m_programBreak = base;
}

uint64_t MemoryManager::GrowProgramBreak(intptr_t increment) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_programBreak == 0) return 0;
    const uint64_t next =
        increment >= 0 ? RoundUp64(m_programBreak + increment, kPageSize)
                       : (m_programBreak >= static_cast<uint64_t>(-increment)
                            ? RoundDown64(m_programBreak + increment, kPageSize)
                            : 0);
    if (next == 0 || next < m_guestBase || next >= m_programBreakLimit) return 0;
    // Commit pages up to the new break lazily.
    const uint64_t commitFrom =
        RoundUp64(std::min<uint64_t>(next, m_programBreak) - 1, kPageSize);
    (void)commitFrom;
    m_programBreak = next;
    return next;
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

} // namespace PX5
