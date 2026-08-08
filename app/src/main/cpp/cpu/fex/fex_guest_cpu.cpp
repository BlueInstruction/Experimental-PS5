#include "fex_guest_cpu.h"
#include "../../utils/logger.h"

namespace PX5 {

std::unique_ptr<FexGuestCpuBackend> FexGuestCpuBackend::Create() {
    auto backend = std::unique_ptr<FexGuestCpuBackend>(new FexGuestCpuBackend());
    backend->m_engine = FexGuestEngine::Create();
    if (!backend->m_engine) {
        PX5_LOGE(LogCategory::CPU, "FexGuestCpuBackend::Create() failed to initialize engine");
        return nullptr;
    }
    PX5_LOGI(LogCategory::CPU, "FexGuestCpuBackend created successfully");
    return backend;
}

FexGuestCpuBackend::~FexGuestCpuBackend() {
    PX5_LOGI(LogCategory::CPU, "FexGuestCpuBackend destroyed");
}

std::unique_ptr<FexGuestCpuBackend::Thread> FexGuestCpuBackend::CreateThread(const GuestExecutionRequest& request) {
    if (!m_engine) return nullptr;

    auto* threadState = m_engine->CreateGuestThread(request.Rip, request.Rsp);
    if (!threadState) return nullptr;

    for (size_t i = 0; i < 16; ++i) {
        threadState->Frame.State.gregs[i] = request.Gpr[i];
    }
    threadState->Frame.State.rflags = request.Rflags;

    PX5_LOGI(LogCategory::CPU, "FexGuestCpuBackend::CreateThread() [RIP=0x%llx, RSP=0x%llx]",
             (unsigned long long)request.Rip, (unsigned long long)request.Rsp);

    return std::make_unique<Thread>(threadState);
}

GuestExecutionState FexGuestCpuBackend::Run(Thread& thread) {
    GuestExecutionState state{};
    if (!m_engine || !thread.GetState()) return state;

    auto* threadState = thread.GetState();
    m_engine->ExecuteGuestThread(threadState);

    for (size_t i = 0; i < 16; ++i) {
        state.Gpr[i] = threadState->Frame.State.gregs[i];
    }
    state.Rip = threadState->Frame.State.rip;
    state.Rsp = threadState->Frame.State.gregs[FEXCore::X86State::REG_RSP];
    state.Rflags = threadState->Frame.State.rflags;
    state.StopReason = threadState->ExecutionHalted ? GuestStopReason::Halted : GuestStopReason::Unknown;

    PX5_LOGI(LogCategory::CPU, "FexGuestCpuBackend::Run() completed [RAX=0x%llx, RIP=0x%llx, StopReason=%d]",
             (unsigned long long)state.Gpr[FEXCore::X86State::REG_RAX],
             (unsigned long long)state.Rip, static_cast<int>(state.StopReason));

    return state;
}

void FexGuestCpuBackend::Invalidate(Thread& thread, uint64_t rip, size_t size) {
    if (!m_engine || !m_engine->GetContext() || !thread.GetState()) return;
    m_engine->GetContext()->InvalidateCodeBuffersCodeRange(rip, size);
    m_engine->GetContext()->InvalidateThreadCachedCodeRange(thread.GetState(), rip, size);
}

void FexGuestCpuBackend::DestroyThread(std::unique_ptr<Thread> thread) {
    if (!m_engine || !thread || !thread->GetState()) return;
    m_engine->DestroyGuestThread(thread->GetState());
}

} // namespace PX5
