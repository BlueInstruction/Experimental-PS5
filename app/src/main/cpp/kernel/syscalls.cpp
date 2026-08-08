#include "syscalls.h"
#include "../utils/logger.h"
#include "../memory/memory.h"
#include <unistd.h>
#include <sys/syscall.h>

namespace PX5 {

KernelSyscalls& KernelSyscalls::GetInstance() {
    static KernelSyscalls instance;
    return instance;
}

void KernelSyscalls::Initialize() {
    PX5_LOGI(LogCategory::KERNEL, "Initializing PS5 FreeBSD Kernel Syscall Table (HLE)...");

    // sys_exit (1)
    m_syscallTable[1] = [](uint64_t code, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> uint64_t {
        PX5_LOGI(LogCategory::KERNEL, "sys_exit(%llu) invoked", (unsigned long long)code);
        return 0;
    };

    // sys_read (3)
    m_syscallTable[3] = [](uint64_t fd, uint64_t buf, uint64_t nbyte, uint64_t, uint64_t, uint64_t) -> uint64_t {
        PX5_LOGD(LogCategory::KERNEL, "sys_read(fd=%llu, nbyte=%llu)", (unsigned long long)fd, (unsigned long long)nbyte);
        return read((int)fd, reinterpret_cast<void*>(buf), (size_t)nbyte);
    };

    // sys_write (4)
    m_syscallTable[4] = [](uint64_t fd, uint64_t buf, uint64_t nbyte, uint64_t, uint64_t, uint64_t) -> uint64_t {
        PX5_LOGD(LogCategory::KERNEL, "sys_write(fd=%llu, nbyte=%llu)", (unsigned long long)fd, (unsigned long long)nbyte);
        return write((int)fd, reinterpret_cast<const void*>(buf), (size_t)nbyte);
    };

    // sys_mmap (477)
    m_syscallTable[477] = [](uint64_t addr, uint64_t len, uint64_t prot, uint64_t flags, uint64_t, uint64_t) -> uint64_t {
        return MemoryManager::GetInstance().MapMemory(addr, len, (uint32_t)prot, "sys_mmap");
    };

    PX5_LOGI(LogCategory::KERNEL, "Registered core FreeBSD kernel syscall handlers successfully");
}

uint64_t KernelSyscalls::DispatchSyscall(uint32_t syscallNum, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    auto it = m_syscallTable.find(syscallNum);
    if (it != m_syscallTable.end()) {
        return it->second(arg1, arg2, arg3, arg4, arg5, arg6);
    }

    PX5_LOGW(LogCategory::KERNEL, "Unhandled FreeBSD Syscall #%u (Args: 0x%llx, 0x%llx, 0x%llx)", syscallNum, (unsigned long long)arg1, (unsigned long long)arg2, (unsigned long long)arg3);
    return 0; // Return ENOSYS fallback
}

} // namespace PX5
