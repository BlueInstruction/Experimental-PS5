#include "emulator.h"
#include "../fexcore_integration.h"
#include "../utils/logger.h"

namespace PX5 {

Emulator& Emulator::GetInstance() {
    static Emulator instance;
    return instance;
}

bool Emulator::Initialize(const std::string& baseDir) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) return true;

    PX5_LOGI(LogCategory::CORE, "PX5 PS5 ARM64 Emulator Core initializing...");

    // 1. Initialize Memory Subsystem
    MemoryManager::GetInstance().Initialize(8192); // 8GB Virtual Address Space

    // 2. Initialize Kernel Syscall Table & Signal Handler
    KernelSyscalls::GetInstance().Initialize();
    RegisterSignalHandlers();

    // 3. Initialize Virtual File System
    VirtualFileSystem::GetInstance().Initialize(baseDir);

    // 4. Initialize the upstream FEXCore runtime.
    if (!FexCoreIntegration::Initialize()) return false;

    // 5. Initialize Vulkan Device & Audio
    VulkanGpuDevice::GetInstance().Initialize();
    AudioEngine::GetInstance().Initialize();

    m_initialized = true;
    PX5_LOGI(LogCategory::CORE, "PX5 Emulator Core initialized successfully.");
    return true;
}

void Emulator::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) return;

    FexCoreIntegration::Shutdown();
    VulkanGpuDevice::GetInstance().Shutdown();
    MemoryManager::GetInstance().Shutdown();

    m_running = false;
    m_initialized = false;
    PX5_LOGI(LogCategory::CORE, "PX5 Emulator Core shutdown complete.");
}

bool Emulator::LoadExecutable(const std::string& path, bool isSelf) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) Initialize("/sdcard/PX5");

    bool success = false;
    if (isSelf) {
        success = ElfLoader::LoadSelf(path, m_entryPoint);
    } else {
        success = ElfLoader::LoadElf(path, m_entryPoint);
    }

    if (success) {
        m_loadedBinaryPath = path;
        PX5_LOGI(LogCategory::CORE, "Loaded %s binary into memory: Entry point = 0x%llx", isSelf ? "SELF" : "ELF", (unsigned long long)m_entryPoint);
    }
    return success;
}

bool Emulator::ReadMemory(uint64_t addr, void* buffer, size_t size) {
    return MemoryManager::GetInstance().ReadGuestMemory(addr, buffer, size);
}

bool Emulator::WriteMemory(uint64_t addr, const void* buffer, size_t size) {
    return MemoryManager::GetInstance().WriteGuestMemory(addr, buffer, size);
}

uint64_t Emulator::MapMemory(uint64_t addr, size_t size, uint32_t flags) {
    return MemoryManager::GetInstance().MapMemory(addr, size, flags, "JNI_Alloc");
}

bool Emulator::UnmapMemory(uint64_t addr, size_t size) {
    return MemoryManager::GetInstance().UnmapMemory(addr, size);
}

std::string Emulator::GetStatusString() const {
    if (!m_initialized) return "Uninitialized";
    if (m_running) return "Running (FEXCore x86_64 -> ARM64)";
    return "Ready (Bionic Native Core)";
}

} // namespace PX5
