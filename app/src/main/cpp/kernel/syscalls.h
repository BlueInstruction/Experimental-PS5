#ifndef PX5_SYSCALLS_H
#define PX5_SYSCALLS_H

#include <cstdint>
#include <string>
#include <unordered_map>

namespace PX5 {

using SyscallHandler = uint64_t(*)(uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6);

class KernelSyscalls {
public:
    static KernelSyscalls& GetInstance();

    void Initialize();
    uint64_t DispatchSyscall(uint32_t syscallNum, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6);

private:
    KernelSyscalls() = default;
    ~KernelSyscalls() = default;

    std::unordered_map<uint32_t, SyscallHandler> m_syscallTable;
};

void RegisterSignalHandlers();

} // namespace PX5

#endif // PX5_SYSCALLS_H
