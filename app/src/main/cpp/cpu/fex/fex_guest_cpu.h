#ifndef PX5_FEX_GUEST_CPU_H
#define PX5_FEX_GUEST_CPU_H

#include "fex_guest_engine.h"
#include <memory>
#include <vector>

namespace PX5 {

struct GuestExecutionRequest {
    uint64_t Rip = 0;
    uint64_t Rsp = 0;
    uint64_t Rflags = 0x2;
    std::array<uint64_t, 16> Gpr{};
    std::array<std::array<uint64_t, 2>, 16> Xmm{};
};

enum class GuestStopReason {
    Unknown,
    Halted,
    Syscall,
    Exception
};

struct GuestExecutionState {
    std::array<uint64_t, 16> Gpr{};
    std::array<std::array<uint64_t, 2>, 16> Xmm{};
    uint64_t Rip = 0;
    uint64_t Rsp = 0;
    uint64_t Rflags = 0;
    GuestStopReason StopReason = GuestStopReason::Unknown;
};

class FexGuestCpuBackend {
public:
    class Thread {
    public:
        Thread(FEXCore::Core::InternalThreadState* threadState) : m_threadState(threadState) {}
        FEXCore::Core::InternalThreadState* GetState() const { return m_threadState; }
    private:
        FEXCore::Core::InternalThreadState* m_threadState = nullptr;
    };

    static std::unique_ptr<FexGuestCpuBackend> Create();
    ~FexGuestCpuBackend();

    std::unique_ptr<Thread> CreateThread(const GuestExecutionRequest& request);
    GuestExecutionState Run(Thread& thread);
    void Invalidate(Thread& thread, uint64_t rip, size_t size);
    void DestroyThread(std::unique_ptr<Thread> thread);

private:
    FexGuestCpuBackend() = default;
    std::unique_ptr<FexGuestEngine> m_engine;
};

} // namespace PX5

#endif // PX5_FEX_GUEST_CPU_H
