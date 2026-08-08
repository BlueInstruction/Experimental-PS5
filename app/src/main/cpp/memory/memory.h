#ifndef PX5_MEMORY_H
#define PX5_MEMORY_H

#include <cstdint>
#include <cstddef>
#include <vector>
#include <mutex>
#include <unordered_map>

namespace PX5 {

namespace MemoryFlags {
    constexpr uint32_t PAGE_NONE = 0x0;
    constexpr uint32_t PAGE_READ = 0x1;
    constexpr uint32_t PAGE_WRITE = 0x2;
    constexpr uint32_t PAGE_EXEC = 0x4;
    constexpr uint32_t PAGE_GUARD = 0x8;
}

struct MemoryBlock {
    uint64_t virtualAddress;
    size_t size;
    uint32_t flags;
    bool isMapped;
    std::string tag;
};

class MemoryManager {
public:
    static MemoryManager& GetInstance();

    bool Initialize(size_t totalMemoryMB = 8192);
    void Shutdown();

    uint64_t MapMemory(uint64_t vaddr, size_t size, uint32_t flags, const std::string& tag = "");
    int CreateSharedMemory(const char* name, size_t size);
    bool UnmapMemory(uint64_t vaddr, size_t size);
    bool ProtectMemory(uint64_t vaddr, size_t size, uint32_t flags);

    bool ReadGuestMemory(uint64_t vaddr, void* outBuffer, size_t size);
    bool WriteGuestMemory(uint64_t vaddr, const void* inBuffer, size_t size);

    void* GetHostPointer(uint64_t vaddr);
    bool IsValidAddress(uint64_t vaddr, size_t size) const;

    size_t GetTotalAllocatedMB() const;

private:
    MemoryManager() = default;
    ~MemoryManager() = default;

    std::mutex m_mutex;
    bool m_initialized = false;
    void* m_virtualBase = nullptr;
    size_t m_totalSize = 0;
    size_t m_allocatedBytes = 0;
    std::unordered_map<uint64_t, MemoryBlock> m_allocations;
};

} // namespace PX5

#endif // PX5_MEMORY_H
