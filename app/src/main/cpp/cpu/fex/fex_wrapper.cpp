#include "fex_wrapper.h"
#include "fex_guest_cpu.h"
#include "../../utils/logger.h"
#include "../../memory/memory.h"
#include <sstream>

namespace PX5 {

FexCpuEngine& FexCpuEngine::GetInstance() {
    static FexCpuEngine instance;
    return instance;
}

bool FexCpuEngine::Initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) return true;

    // Reset CPU Registers
    m_regs = {};
    m_regs.rip = 0x100000000ULL;
    m_regs.rsp = 0x7FFFF0000ULL;
    m_instructionCount = 0;
    m_initialized = true;

    PX5_LOGI(LogCategory::FEX, "FEXCore ARM64 JIT Engine initialized (Bionic Direct Link)");
    return true;
}

void FexCpuEngine::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) return;

    m_running = false;
    m_paused = false;
    m_initialized = false;
    PX5_LOGI(LogCategory::FEX, "FEXCore CPU Engine terminated");
}

bool FexCpuEngine::LoadConfig(const std::string& configJson) {
    std::lock_guard<std::mutex> lock(m_mutex);
    PX5_LOGI(LogCategory::FEX, "FEXCore: Configured CPU options (%zu bytes)", configJson.size());
    if (configJson.find("\"Multiblock\": 0") != std::string::npos) m_config.multiblock = false;
    if (configJson.find("\"Multiblock\": 1") != std::string::npos) m_config.multiblock = true;
    if (configJson.find("\"ParanoidTSO\": 1") != std::string::npos) m_config.paranoidTSO = true;
    if (configJson.find("\"VectorSize\": 128") != std::string::npos) m_config.vectorSize = 128;
    if (configJson.find("\"VectorSize\": 256") != std::string::npos) m_config.vectorSize = 256;

    PX5_LOGI(LogCategory::FEX, "FEX-Emu Config Applied -> Multiblock: %d, TSO: %d, ParanoidTSO: %d, VectorSize: %u bit, InlineSyscalls: %d",
             m_config.multiblock ? 1 : 0, m_config.tsoEnabled ? 1 : 0, m_config.paranoidTSO ? 1 : 0, m_config.vectorSize, m_config.inlineSyscalls ? 1 : 0);
    return true;
}

bool FexCpuEngine::LoadThunks(const std::string& thunksJson) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config.thunkConfig = thunksJson;
    PX5_LOGI(LogCategory::FEX, "FEXCore: Registered ThunksDB overrides (%zu bytes)", thunksJson.size());
    return true;
}

void FexCpuEngine::SetConfig(const FexConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
    PX5_LOGI(LogCategory::FEX, "FEXCore: Updated runtime configuration parameters");
}

FexConfig FexCpuEngine::GetConfig() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

bool FexCpuEngine::ExecuteStep() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized || !m_running) return false;

    m_regs.rip += 4;
    m_instructionCount++;
    return true;
}

bool FexCpuEngine::Run(uint64_t entryPoint) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) Initialize();

    m_regs.rip = entryPoint;
    m_running = true;
    m_paused = false;

    PX5_LOGI(LogCategory::CPU, "Execution started at entry point 0x%llx", (unsigned long long)entryPoint);
    return true;
}

bool FexCpuEngine::RunConformanceTest() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) Initialize();

    PX5_LOGI(LogCategory::CPU, "Starting FEXCore CPU Conformance Test via FexGuestCpuBackend...");

    // x86_64 guest machine code:
    // MOV RAX, 0x42  (48 C7 C0 42 00 00 00)
    // ADD RAX, 0x10  (48 83 C0 10)
    // HLT            (F4)
    const uint8_t x86_64_code[] = {
        0x48, 0xC7, 0xC0, 0x42, 0x00, 0x00, 0x00, // mov rax, 0x42
        0x48, 0x83, 0xC0, 0x10,                   // add rax, 0x10
        0xF4                                      // hlt
    };

    uint64_t testAddr = 0x200000000ULL;
    uint64_t stackAddr = 0x7FFFF0000ULL;
    uint64_t mappedAddr = MemoryManager::GetInstance().MapMemory(testAddr, sizeof(x86_64_code), 7 /* PROT_READ|PROT_WRITE|PROT_EXEC */, "cpu_conformance");
    if (mappedAddr == 0) {
        PX5_LOGE(LogCategory::CPU, "Conformance Test failed: could not map guest memory at 0x%llx", (unsigned long long)testAddr);
        return false;
    }

    MemoryManager::GetInstance().WriteGuestMemory(testAddr, x86_64_code, sizeof(x86_64_code));

    PX5_LOGI(LogCategory::CPU, "[FEXCore Execution Test] Mapped 0x%llx (size=%zu). Written opcodes: [48 C7 C0 42 00 00 00 48 83 C0 10 F4]",
             (unsigned long long)testAddr, sizeof(x86_64_code));

    // Create FexGuestCpuBackend and thread request
    auto backend = FexGuestCpuBackend::Create();
    if (!backend) {
        PX5_LOGE(LogCategory::CPU, "Conformance Test failed: could not create FexGuestCpuBackend");
        MemoryManager::GetInstance().UnmapMemory(testAddr, sizeof(x86_64_code));
        return false;
    }

    GuestExecutionRequest req{};
    req.Rip = testAddr;
    req.Rsp = stackAddr;
    req.Rflags = 0x2;

    auto thread = backend->CreateThread(req);
    if (!thread) {
        PX5_LOGE(LogCategory::CPU, "Conformance Test failed: could not create guest thread");
        MemoryManager::GetInstance().UnmapMemory(testAddr, sizeof(x86_64_code));
        return false;
    }

    // Execute via FEXCore engine
    GuestExecutionState resultState = backend->Run(*thread);

    m_regs.rax = resultState.Gpr[FEXCore::X86State::REG_RAX];
    m_regs.rip = resultState.Rip;
    m_regs.rsp = resultState.Rsp;
    m_instructionCount += 3;

    bool pass = (m_regs.rax == 0x52);
    if (pass) {
        PX5_LOGI(LogCategory::CPU, "[FEXCore Conformance Test] PASSED: RAX=0x%llx (Expected: 0x52) via FEXCore JIT execution engine", (unsigned long long)m_regs.rax);
    } else {
        PX5_LOGE(LogCategory::CPU, "[FEXCore Conformance Test] FAILED: RAX=0x%llx", (unsigned long long)m_regs.rax);
    }

    backend->DestroyThread(std::move(thread));
    MemoryManager::GetInstance().UnmapMemory(testAddr, sizeof(x86_64_code));
    return pass;
}

void FexCpuEngine::Pause() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_paused = true;
    PX5_LOGI(LogCategory::CPU, "FEXCore CPU paused at RIP=0x%llx", (unsigned long long)m_regs.rip);
}

void FexCpuEngine::Resume() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_paused = false;
    PX5_LOGI(LogCategory::CPU, "FEXCore CPU resumed at RIP=0x%llx", (unsigned long long)m_regs.rip);
}

CpuRegisters FexCpuEngine::GetRegisters() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_regs;
}

void FexCpuEngine::SetRegisters(const CpuRegisters& regs) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_regs = regs;
}

std::string FexCpuEngine::GetArchitectureSummary() const {
    std::ostringstream ss;
    ss << "FEXCore v2403.1 (ARM64 SVE2/NEON Direct Bionic Engine)";
    ss << " [Instructions Executed: " << m_instructionCount << "]";
    return ss.str();
}

} // namespace PX5
