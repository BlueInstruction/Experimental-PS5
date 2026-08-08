#ifndef PX5_KERNEL_HLE_H
#define PX5_KERNEL_HLE_H

#include <string>
#include <vector>
#include <cstdint>

struct PS5ModuleInfo {
    std::string moduleName;
    uint64_t baseAddress;
    uint64_t entryPoint;
    uint32_t exportedSymbolsCount;
    bool isLoaded;
};

class PS5KernelHLE {
public:
    static PS5KernelHLE& getInstance();

    bool initializeKernel();
    bool loadSelfPackage(const std::string& path);
    std::string getKernelStateSummary() const;
    
    // Syscall translation
    int32_t handleSyscall(uint32_t syscallNum, uint64_t arg1, uint64_t arg2, uint64_t arg3);

private:
    PS5KernelHLE() = default;
    bool m_initialized = false;
    uint64_t m_allocatedMemoryBytes = 0;
    std::vector<PS5ModuleInfo> m_loadedModules;
};

#endif // PX5_KERNEL_HLE_H
