#ifndef PX5_EMULATOR_H
#define PX5_EMULATOR_H

#include <string>
#include <mutex>
#include "../memory/memory.h"
#include "../kernel/syscalls.h"
#include "../loader/elf_loader.h"
#include "../filesystem/vfs.h"
#include "../gpu/vulkan_device.h"
#include "../audio/audio.h"

namespace PX5 {

class Emulator {
public:
    static Emulator& GetInstance();

    bool Initialize(const std::string& baseDir);
    void Shutdown();

    bool LoadExecutable(const std::string& path, bool isSelf);

    // Memory Inspection & Manipulation
    bool ReadMemory(uint64_t addr, void* buffer, size_t size);
    bool WriteMemory(uint64_t addr, const void* buffer, size_t size);
    uint64_t MapMemory(uint64_t addr, size_t size, uint32_t flags);
    bool UnmapMemory(uint64_t addr, size_t size);

    bool IsRunning() const { return m_running; }
    std::string GetStatusString() const;

private:
    Emulator() = default;
    ~Emulator() = default;

    std::mutex m_mutex;
    bool m_initialized = false;
    bool m_running = false;
    uint64_t m_entryPoint = 0;
    std::string m_loadedBinaryPath;
};

} // namespace PX5

#endif // PX5_EMULATOR_H
