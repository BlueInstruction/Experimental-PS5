#ifndef PX5_FEX_WRAPPER_H
#define PX5_FEX_WRAPPER_H

#include <cstdint>
#include <string>
#include <mutex>

namespace PX5 {

struct CpuRegisters {
    uint64_t rip;
    uint64_t rsp;
    uint64_t rbp;
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsi, rdi;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t rflags;
};

struct FexConfig {
    bool multiblock = true;
    bool x87ReducedPrecision = true;
    bool paranoidTSO = false;
    uint32_t vectorSize = 256;
    bool tsoEnabled = true;
    bool smcTriggers = true;
    bool inlineSyscalls = true;
    std::string rootFS = "/sdcard/PX5/rootfs";
    std::string thunkConfig = "/sdcard/PX5/thunks.json";
};

class FexCpuEngine {
public:
    static FexCpuEngine& GetInstance();

    bool Initialize();
    void Shutdown();

    bool LoadConfig(const std::string& configJson);
    bool LoadThunks(const std::string& thunksJson);
    void SetConfig(const FexConfig& config);
    FexConfig GetConfig() const;

    bool ExecuteStep();
    bool Run(uint64_t entryPoint);
    bool RunConformanceTest();
    void Pause();
    void Resume();

    CpuRegisters GetRegisters() const;
    void SetRegisters(const CpuRegisters& regs);

    bool IsRunning() const { return m_running; }
    std::string GetArchitectureSummary() const;

private:
    FexCpuEngine() = default;
    ~FexCpuEngine() = default;

    mutable std::mutex m_mutex;
    bool m_initialized = false;
    bool m_running = false;
    bool m_paused = false;
    CpuRegisters m_regs{};
    FexConfig m_config{};
    uint64_t m_instructionCount = 0;
};

} // namespace PX5

#endif // PX5_FEX_WRAPPER_H
