#ifndef PX5_FEX_GUEST_ENGINE_H
#define PX5_FEX_GUEST_ENGINE_H

#include <cstdint>
#include <cstddef>
#include <vector>
#include <memory>
#include <array>
#include <mutex>
#include <string>

namespace FEXCore {

namespace X86State {
    constexpr size_t REG_RAX = 0;
    constexpr size_t REG_RCX = 1;
    constexpr size_t REG_RDX = 2;
    constexpr size_t REG_RBX = 3;
    constexpr size_t REG_RSP = 4;
    constexpr size_t REG_RBP = 5;
    constexpr size_t REG_RSI = 6;
    constexpr size_t REG_RDI = 7;
    constexpr size_t REG_R8  = 8;
    constexpr size_t REG_R9  = 9;
    constexpr size_t REG_R10 = 10;
    constexpr size_t REG_R11 = 11;
    constexpr size_t REG_R12 = 12;
    constexpr size_t REG_R13 = 13;
    constexpr size_t REG_R14 = 14;
    constexpr size_t REG_R15 = 15;
}

namespace Core {

struct XMMReg {
    uint64_t data[2];
};

struct CpuStateFrame {
    struct {
        uint64_t gregs[16];
        uint64_t rip;
        uint64_t rflags;
        struct {
            struct {
                uint64_t data[16][2];
            } sse;
        } xmm;
    } State;
};

struct InternalThreadState {
    CpuStateFrame Frame;
    bool ExecutionHalted = false;
};

} // namespace Core

namespace HLE {
struct SyscallArguments {
    uint64_t args[6];
};

class SyscallHandler {
public:
    virtual ~SyscallHandler() = default;
    virtual uint64_t HandleSyscall(Core::CpuStateFrame* frame, SyscallArguments* args) = 0;
};
} // namespace HLE

namespace Context {

class SignalDelegator {
public:
    virtual ~SignalDelegator() = default;
};

class Context {
public:
    static Context* CreateNewContext(uint64_t hostFeatures);
    virtual ~Context() = default;

    virtual void SetSignalDelegator(SignalDelegator* delegator) = 0;
    virtual void SetSyscallHandler(HLE::SyscallHandler* handler) = 0;
    virtual void EnableExitOnHLT() = 0;
    virtual bool InitCore() = 0;

    virtual Core::InternalThreadState* CreateThread(uint64_t initialRip, uint64_t initialRsp) = 0;
    virtual void ExecuteThread(Core::InternalThreadState* thread) = 0;
    virtual void DestroyThread(Core::InternalThreadState* thread) = 0;

    virtual void InvalidateCodeBuffersCodeRange(uint64_t start, uint64_t length) = 0;
    virtual void InvalidateThreadCachedCodeRange(Core::InternalThreadState* thread, uint64_t start, uint64_t length) = 0;
};

} // namespace Context

} // namespace FEXCore

namespace FEX {
uint64_t FetchHostFeatures();
}

namespace PX5 {

class BridgeSyscallHandler : public FEXCore::HLE::SyscallHandler {
public:
    uint64_t HandleSyscall(FEXCore::Core::CpuStateFrame* frame, FEXCore::HLE::SyscallArguments* args) override;
};

class FexGuestEngine {
public:
    static std::unique_ptr<FexGuestEngine> Create();
    ~FexGuestEngine();

    bool Init();
    FEXCore::Context::Context* GetContext() const { return m_context.get(); }
    FEXCore::Core::InternalThreadState* CreateGuestThread(uint64_t rip, uint64_t rsp);
    void ExecuteGuestThread(FEXCore::Core::InternalThreadState* thread);
    void DestroyGuestThread(FEXCore::Core::InternalThreadState* thread);

private:
    FexGuestEngine() = default;
    std::unique_ptr<FEXCore::Context::Context> m_context;
    std::unique_ptr<BridgeSyscallHandler> m_syscallHandler;
    uint64_t m_hostFeatures = 0;
    bool m_initialized = false;
};

} // namespace PX5

#endif // PX5_FEX_GUEST_ENGINE_H
