#include "memory.h"
#include "../utils/logger.h"
#include <sys/mman.h>
#include <sys/syscall.h>
#include <android/sharedmem.h>
#include <cstring>
#include <unistd.h>

namespace PX5 {

MemoryManager& MemoryManager::GetInstance() {
    static MemoryManager instance;
    return instance;
}

bool MemoryManager::Initialize(size_t totalMemoryMB) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) return true;

    m_totalSize = totalMemoryMB * 1024 * 1024;
    // Attempt mmap reservation for Guest Address Space
    m_virtualBase = mmap(nullptr, m_totalSize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (m_virtualBase == MAP_FAILED) {
        PX5_LOGE(LogCategory::MEMORY, "Failed to allocate 64-bit virtual memory space of %zu MB", totalMemoryMB);
        m_virtualBase = nullptr;
        return false;
    }

    m_initialized = true;
    m_allocatedBytes = 0;
    PX5_LOGI(LogCategory::MEMORY, "Virtual Memory Manager initialized: Base %p, Total Reserved %zu MB", m_virtualBase, totalMemoryMB);
    return true;
}

void MemoryManager::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) return;

    if (m_virtualBase) {
        munmap(m_virtualBase, m_totalSize);
        m_virtualBase = nullptr;
    }
    m_allocations.clear();
    m_initialized = false;
    PX5_LOGI(LogCategory::MEMORY, "Virtual Memory Manager shut down successfully");
}

uint64_t MemoryManager::MapMemory(uint64_t vaddr, size_t size, uint32_t flags, const std::string& tag) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) Initialize();

    int prot = 0;
    if (flags & MemoryFlags::PAGE_READ) prot |= PROT_READ;
    if (flags & MemoryFlags::PAGE_WRITE) prot |= PROT_WRITE;
    if (flags & MemoryFlags::PAGE_EXEC) prot |= PROT_EXEC;

    void* target = reinterpret_cast<void*>(vaddr);
    void* mapped = mmap(target, size, prot, MAP_PRIVATE | MAP_ANONYMOUS | (vaddr ? MAP_FIXED : 0), -1, 0);

    if (mapped == MAP_FAILED) {
        PX5_LOGE(LogCategory::MEMORY, "MapMemory failed for address 0x%llx (Size: %zu)", (unsigned long long)vaddr, size);
        return 0;
    }

    uint64_t assignedAddr = reinterpret_cast<uint64_t>(mapped);
    m_allocations[assignedAddr] = { assignedAddr, size, flags, true, tag };
    m_allocatedBytes += size;

    PX5_LOGI(LogCategory::MEMORY, "Mapped VAddr 0x%llx - Size: %zu B - Tag: '%s'", (unsigned long long)assignedAddr, size, tag.c_str());
    return assignedAddr;
}

int MemoryManager::CreateSharedMemory(const char* name, size_t size) {
    int fd = -1;
#if defined(__NR_memfd_create)
    // Primary path: Use memfd_create (Linux kernel anonymous memory FD, bypasses ashmem & pinning deprecations)
    fd = syscall(__NR_memfd_create, name ? name : "px5_shm", 1 /* MFD_CLOEXEC */);
    if (fd >= 0) {
        if (ftruncate(fd, size) < 0) {
            close(fd);
            fd = -1;
        }
    }
#endif
    if (fd < 0) {
        // Fallback: ASharedMemory_create (Android NDK API)
        fd = ASharedMemory_create(name ? name : "px5_shm", size);
    }
    if (fd >= 0) {
        PX5_LOGI(LogCategory::MEMORY, "Created shared memory region '%s' via memfd/ASharedMemory (FD: %d, Size: %zu)", name ? name : "px5_shm", fd, size);
    } else {
        PX5_LOGE(LogCategory::MEMORY, "Failed to create shared memory region '%s'", name ? name : "px5_shm");
    }
    return fd;
}

bool MemoryManager::UnmapMemory(uint64_t vaddr, size_t size) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) return false;

    if (munmap(reinterpret_cast<void*>(vaddr), size) == 0) {
        m_allocations.erase(vaddr);
        if (m_allocatedBytes >= size) m_allocatedBytes -= size;
        PX5_LOGI(LogCategory::MEMORY, "Unmapped VAddr 0x%llx (Size: %zu B)", (unsigned long long)vaddr, size);
        return true;
    }
    return false;
}

bool MemoryManager::ProtectMemory(uint64_t vaddr, size_t size, uint32_t flags) {
    int prot = 0;
    if (flags & MemoryFlags::PAGE_READ) prot |= PROT_READ;
    if (flags & MemoryFlags::PAGE_WRITE) prot |= PROT_WRITE;
    if (flags & MemoryFlags::PAGE_EXEC) prot |= PROT_EXEC;

    return mprotect(reinterpret_cast<void*>(vaddr), size, prot) == 0;
}

bool MemoryManager::ReadGuestMemory(uint64_t vaddr, void* outBuffer, size_t size) {
    if (!outBuffer) return false;
    std::memcpy(outBuffer, reinterpret_cast<const void*>(vaddr), size);
    return true;
}

bool MemoryManager::WriteGuestMemory(uint64_t vaddr, const void* inBuffer, size_t size) {
    if (!inBuffer) return false;
    std::memcpy(reinterpret_cast<void*>(vaddr), inBuffer, size);
    return true;
}

void* MemoryManager::GetHostPointer(uint64_t vaddr) {
    return reinterpret_cast<void*>(vaddr);
}

bool MemoryManager::IsValidAddress(uint64_t vaddr, size_t size) const {
    return vaddr != 0;
}

size_t MemoryManager::GetTotalAllocatedMB() const {
    return m_allocatedBytes / (1024 * 1024);
}

} // namespace PX5
