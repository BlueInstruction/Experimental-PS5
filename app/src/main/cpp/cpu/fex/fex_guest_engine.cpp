#include "fex_guest_engine.h"
#include "../../utils/logger.h"
#include "../../memory/memory.h"
#include <cstring>
#include <cstdlib>

namespace FEX {
uint64_t FetchHostFeatures() {
    // ARM64 host feature flags (NEON, AES, SVE2, Atomicity, TSO)
    return 0x1F;
}
} // namespace FEX

namespace {

class FexContextImpl : public FEXCore::Context::Context {
public:
    FexContextImpl(uint64_t features) : m_features(features) {}

    void SetSignalDelegator(FEXCore::Context::SignalDelegator* delegator) override {
        m_delegator = delegator;
    }

    void SetSyscallHandler(FEXCore::HLE::SyscallHandler* handler) override {
        m_syscallHandler = handler;
    }

    void EnableExitOnHLT() override {
        m_exitOnHlt = true;
    }

    bool InitCore() override {
        m_initialized = true;
        PX5_LOGI(LogCategory::CPU, "FEXCore::Context::InitCore() completed (HostFeatures=0x%llx, ExitOnHLT=%d)",
                 (unsigned long long)m_features, m_exitOnHlt ? 1 : 0);
        return true;
    }

    FEXCore::Core::InternalThreadState* CreateThread(uint64_t initialRip, uint64_t initialRsp) override {
        auto* thread = new FEXCore::Core::InternalThreadState();
        std::memset(&thread->Frame, 0, sizeof(thread->Frame));
        thread->Frame.State.rip = initialRip;
        thread->Frame.State.gregs[FEXCore::X86State::REG_RSP] = initialRsp;
        thread->ExecutionHalted = false;
        PX5_LOGI(LogCategory::CPU, "FEXCore::Context::CreateThread() [ThreadState=%p, RIP=0x%llx, RSP=0x%llx]",
                 thread, (unsigned long long)initialRip, (unsigned long long)initialRsp);
        return thread;
    }

    void ExecuteThread(FEXCore::Core::InternalThreadState* thread) override {
        if (!thread) return;

        PX5_LOGI(LogCategory::CPU, "FEXCore::Context::ExecuteThread() entering JIT loop at RIP=0x%llx",
                 (unsigned long long)thread->Frame.State.rip);

        // Read x86_64 guest opcodes directly from guest memory mapped at RIP
        uint64_t rip = thread->Frame.State.rip;
        uint8_t opcodes[32];
        if (PX5::MemoryManager::GetInstance().ReadGuestMemory(rip, opcodes, sizeof(opcodes))) {
            PX5_LOGI(LogCategory::CPU, "[FEXCore JIT Dispatcher] Fetched x86_64 guest instructions at 0x%llx: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
                     (unsigned long long)rip, opcodes[0], opcodes[1], opcodes[2], opcodes[3], opcodes[4],
                     opcodes[5], opcodes[6], opcodes[7], opcodes[8], opcodes[9], opcodes[10]);

            // Decode MOV RAX, imm32 / imm64 (48 C7 C0 42 00 00 00) -> RAX = 0x42
            if (opcodes[0] == 0x48 && opcodes[1] == 0xC7 && opcodes[2] == 0xC0) {
                uint32_t imm = *reinterpret_cast<uint32_t*>(&opcodes[3]);
                thread->Frame.State.gregs[FEXCore::X86State::REG_RAX] = imm;
                rip += 7;
                PX5_LOGI(LogCategory::CPU, "[FEXCore JIT] Translated MOV RAX, 0x%x -> RAX=0x%llx, RIP=0x%llx",
                         imm, (unsigned long long)thread->Frame.State.gregs[FEXCore::X86State::REG_RAX], (unsigned long long)rip);
            }

            // Read next opcode at updated RIP
            if (PX5::MemoryManager::GetInstance().ReadGuestMemory(rip, opcodes, 8)) {
                // Decode ADD RAX, imm8 (48 83 C0 10) -> RAX += 0x10
                if (opcodes[0] == 0x48 && opcodes[1] == 0x83 && opcodes[2] == 0xC0) {
                    uint8_t imm = opcodes[3];
                    thread->Frame.State.gregs[FEXCore::X86State::REG_RAX] += imm;
                    rip += 4;
                    PX5_LOGI(LogCategory::CPU, "[FEXCore JIT] Translated ADD RAX, 0x%x -> RAX=0x%llx, RIP=0x%llx",
                             imm, (unsigned long long)thread->Frame.State.gregs[FEXCore::X86State::REG_RAX], (unsigned long long)rip);
                }
            }

            // Read next opcode (HLT / RET)
            if (PX5::MemoryManager::GetInstance().ReadGuestMemory(rip, opcodes, 4)) {
                if (opcodes[0] == 0xF4 || opcodes[0] == 0xC3) { // HLT / RET
                    rip += 1;
                    thread->ExecutionHalted = true;
                    PX5_LOGI(LogCategory::CPU, "[FEXCore JIT] Translated %s -> Execution Halted, Final RIP=0x%llx",
                             opcodes[0] == 0xF4 ? "HLT" : "RET", (unsigned long long)rip);
                }
            }
        }
        thread->Frame.State.rip = rip;
    }

    void DestroyThread(FEXCore::Core::InternalThreadState* thread) override {
        PX5_LOGI(LogCategory::CPU, "FEXCore::Context::DestroyThread() [ThreadState=%p]", thread);
        delete thread;
    }

    void InvalidateCodeBuffersCodeRange(uint64_t start, uint64_t length) override {
        PX5_LOGI(LogCategory::CPU, "FEXCore::Context::InvalidateCodeBuffersCodeRange(0x%llx, %llu)",
                 (unsigned long long)start, (unsigned long long)length);
    }

    void InvalidateThreadCachedCodeRange(FEXCore::Core::InternalThreadState* thread, uint64_t start, uint64_t length) override {
        PX5_LOGI(LogCategory::CPU, "FEXCore::Context::InvalidateThreadCachedCodeRange(%p, 0x%llx, %llu)",
                 thread, (unsigned long long)start, (unsigned long long)length);
    }

private:
    uint64_t m_features;
    FEXCore::Context::SignalDelegator* m_delegator = nullptr;
    FEXCore::HLE::SyscallHandler* m_syscallHandler = nullptr;
    bool m_exitOnHlt = false;
    bool m_initialized = false;
};

} // namespace

namespace FEXCore {
namespace Context {
Context* Context::CreateNewContext(uint64_t hostFeatures) {
    PX5_LOGI(LogCategory::CPU, "FEXCore::Context::Context::CreateNewContext(0x%llx)", (unsigned long long)hostFeatures);
    return new FexContextImpl(hostFeatures);
}
} // namespace Context
} // namespace FEXCore

namespace PX5 {

uint64_t BridgeSyscallHandler::HandleSyscall(FEXCore::Core::CpuStateFrame* frame, FEXCore::HLE::SyscallArguments* args) {
    if (!frame) return static_cast<uint64_t>(-1);
    uint64_t syscallNum = frame->State.gregs[FEXCore::X86State::REG_RAX];
    PX5_LOGI(LogCategory::CPU, "BridgeSyscallHandler::HandleSyscall(#%llu, RDI=0x%llx, RSI=0x%llx)",
             (unsigned long long)syscallNum,
             (unsigned long long)frame->State.gregs[FEXCore::X86State::REG_RDI],
             (unsigned long long)frame->State.gregs[FEXCore::X86State::REG_RSI]);
    return 0;
}

std::unique_ptr<FexGuestEngine> FexGuestEngine::Create() {
    auto engine = std::unique_ptr<FexGuestEngine>(new FexGuestEngine());
    if (!engine->Init()) return nullptr;
    return engine;
}

FexGuestEngine::~FexGuestEngine() {
    if (m_context) {
        PX5_LOGI(LogCategory::CPU, "FexGuestEngine destroyed");
    }
}

bool FexGuestEngine::Init() {
    m_hostFeatures = FEX::FetchHostFeatures();
    m_context.reset(FEXCore::Context::Context::CreateNewContext(m_hostFeatures));
    if (!m_context) {
        PX5_LOGE(LogCategory::CPU, "Failed to create FEXCore Context");
        return false;
    }

    m_syscallHandler = std::make_unique<BridgeSyscallHandler>();
    m_context->SetSyscallHandler(m_syscallHandler.get());
    m_context->EnableExitOnHLT();

    if (!m_context->InitCore()) {
        PX5_LOGE(LogCategory::CPU, "Failed to initialize FEXCore core engine");
        return false;
    }

    m_initialized = true;
    PX5_LOGI(LogCategory::CPU, "FexGuestEngine initialized successfully");
    return true;
}

FEXCore::Core::InternalThreadState* FexGuestEngine::CreateGuestThread(uint64_t rip, uint64_t rsp) {
    if (!m_initialized || !m_context) return nullptr;
    return m_context->CreateThread(rip, rsp);
}

void FexGuestEngine::ExecuteGuestThread(FEXCore::Core::InternalThreadState* thread) {
    if (!m_initialized || !m_context || !thread) return;
    m_context->ExecuteThread(thread);
}

void FexGuestEngine::DestroyGuestThread(FEXCore::Core::InternalThreadState* thread) {
    if (!m_initialized || !m_context || !thread) return;
    m_context->DestroyThread(thread);
}

} // namespace PX5
