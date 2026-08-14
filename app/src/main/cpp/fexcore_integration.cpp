#include "fexcore_integration.h"

#include <FEXCore/Core/Context.h>
#include <FEXCore/Core/CodeCache.h>
#include <FEXCore/Core/HostFeatures.h>
#include <FEXCore/Core/SignalDelegator.h>
#include <FEXCore/Debug/InternalThreadState.h>
#include <FEXCore/HLE/SyscallHandler.h>

#include <cstring>
#include <memory>
#include <mutex>
#include <optional>
#include <sys/auxv.h>
#include <sys/mman.h>
#include <unistd.h>

#include "utils/logger.h"

namespace PX5::FexCoreIntegration {
namespace {

std::mutex g_mutex;
std::unique_ptr<FEXCore::Context::Context> g_context;

class NullSyscallHandler final : public FEXCore::HLE::SyscallHandler {
public:
    uint64_t HandleSyscall(FEXCore::Core::CpuStateFrame* frame,
                           FEXCore::HLE::SyscallArguments* args) override {
        (void)frame;
        (void)args;
        return static_cast<uint64_t>(-1);
    }

    std::optional<FEXCore::ExecutableFileSectionInfo> LookupExecutableFileSection(
            FEXCore::Core::InternalThreadState*, uint64_t) override {
        return std::nullopt;
    }

    FEXCore::HLE::ExecutableRangeInfo QueryGuestExecutableRange(
            FEXCore::Core::InternalThreadState*, uint64_t) override {
        return {0, UINT64_MAX, true};
    }
};

NullSyscallHandler g_syscallHandler;
FEXCore::SignalDelegator g_signalDelegator;

// Detect actual CPU features from hardware instead of hardcoding.
// The previous version hardcoded SupportsAES=true, SupportsCRC=true,
// SupportsAtomics=true — if the device's CPU doesn't actually support
// these, FEXCore's JIT would generate ARM64 instructions that trigger
// SIGILL/SIGSEGV at runtime.
FEXCore::HostFeatures CreateHostFeatures() {
    FEXCore::HostFeatures features{};

    // Detect cache line size from hardware
    long cachelinesize = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
    features.DCacheLineSize = (cachelinesize > 0) ? static_cast<uint32_t>(cachelinesize) : 64;
    features.ICacheLineSize = features.DCacheLineSize;

    // Read hardware capabilities
    unsigned long hwcap = getauxval(AT_HWCAP);

    // ARM64 feature bits from <asm/hwcap.h>
    // HWCAP_AES      = 1 << 3
    // HWCAP_CRC32    = 1 << 7
    // HWCAP_ATOMICS  = 1 << 8
    // HWCAP_FP       = 1 << 0
    // HWCAP_ASIMD    = 1 << 1

    features.SupportsAES    = (hwcap & (1 << 3)) != 0;  // HWCAP_AES
    features.SupportsCRC    = (hwcap & (1 << 7)) != 0;  // HWCAP_CRC32
    features.SupportsAtomics = (hwcap & (1 << 8)) != 0;  // HWCAP_ATOMICS

    // SVE detection
    // HWCAP_SVE = 1 << 22
    bool sve_supported = (hwcap & (1 << 22)) != 0;
    features.SupportsSVE128 = false;  // Don't use SVE even if available — too experimental
    features.SupportsSVE256 = false;

    features.HostType = FEXCore::HostFeatures::HostTypeEnum::Linux;

    PX5_LOGI(LogCategory::FEX,
             "HostFeatures: DCache=%u ICache=%u AES=%d CRC=%d Atomics=%d SVE=%d hwcap=0x%lx",
             features.DCacheLineSize, features.ICacheLineSize,
             features.SupportsAES ? 1 : 0, features.SupportsCRC ? 1 : 0,
             features.SupportsAtomics ? 1 : 0, sve_supported ? 1 : 0, hwcap);

    return features;
}

}

bool Initialize() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_context) {
        return true;
    }

    // Step 1: Detect host features
    PX5_LOGI(LogCategory::FEX, "Initialize: step 1 — detecting host features");
    const auto features = CreateHostFeatures();

    // Step 2: Create FEXCore context
    PX5_LOGI(LogCategory::FEX, "Initialize: step 2 — CreateNewContext");
    g_context = FEXCore::Context::Context::CreateNewContext(features);
    if (!g_context) {
        PX5_LOGE(LogCategory::FEX, "Initialize: CreateNewContext returned null");
        return false;
    }
    PX5_LOGI(LogCategory::FEX, "Initialize: CreateNewContext succeeded");

    // Step 3: Set syscall handler
    PX5_LOGI(LogCategory::FEX, "Initialize: step 3 — SetSyscallHandler");
    g_context->SetSyscallHandler(&g_syscallHandler);

    // Step 4: Set signal delegator
    // NOTE: FEXCore uses signal handlers for JIT page-fault handling.
    // On Android, this can conflict with debuggerd/tombstone.
    // If this step causes a crash, we can try skipping it.
    PX5_LOGI(LogCategory::FEX, "Initialize: step 4 — SetSignalDelegator");
    g_context->SetSignalDelegator(&g_signalDelegator);

    // Step 5: Enable HLT exit
    PX5_LOGI(LogCategory::FEX, "Initialize: step 5 — EnableExitOnHLT");
    g_context->EnableExitOnHLT();

    // Step 6: InitCore — this initializes the JIT compiler
    // This is the most likely step to cause a SIGSEGV because:
    //   - It allocates executable memory (mmap PROT_EXEC)
    //   - It sets up signal handlers for page faults
    //   - It might use ARM64 instructions based on the host features
    PX5_LOGI(LogCategory::FEX, "Initialize: step 6 — InitCore");
    if (!g_context->InitCore()) {
        PX5_LOGE(LogCategory::FEX, "Initialize: InitCore returned false");
        g_context.reset();
        return false;
    }
    PX5_LOGI(LogCategory::FEX, "Initialize: InitCore succeeded");

    PX5_LOGI(LogCategory::FEX,
             "FEXCore Context initialized: DCache=%u ICache=%u AES=%d CRC=%d Atomics=%d",
             features.DCacheLineSize, features.ICacheLineSize,
             features.SupportsAES ? 1 : 0, features.SupportsCRC ? 1 : 0,
             features.SupportsAtomics ? 1 : 0);
    return true;
}

void Shutdown() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_context.reset();
}

bool IsInitialized() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_context != nullptr;
}

std::string GetArchitectureSummary() {
    return IsInitialized() ? "FEXCore upstream fd141ed6d721d03062619e4702bca1a0c93b6dd9 initialized"
                           : "FEXCore upstream fd141ed6d721d03062619e4702bca1a0c93b6dd9 not initialized";
}

bool RunGuestCodeTest() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_context) {
        PX5_LOGE(LogCategory::FEX, "Guest test skipped: FEXCore Context is not initialized");
        return false;
    }

    const uint8_t guestCode[] = {
            0xB8, 0x28, 0x00, 0x00, 0x00, // mov eax, 40
            0x83, 0xC0, 0x02,             // add eax, 2
            0xF4                          // hlt
    };
    const size_t pageSize = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    const size_t codeSize = (sizeof(guestCode) + pageSize - 1) & ~(pageSize - 1);
    void* code = mmap(nullptr, codeSize, PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    void* stack = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (code == MAP_FAILED || stack == MAP_FAILED) {
        PX5_LOGE(LogCategory::FEX, "Guest test failed: mmap could not allocate guest code or stack");
        if (code != MAP_FAILED) munmap(code, codeSize);
        if (stack != MAP_FAILED) munmap(stack, pageSize);
        return false;
    }
    std::memcpy(code, guestCode, sizeof(guestCode));

    auto* thread = g_context->CreateThread(reinterpret_cast<uint64_t>(code),
                                            reinterpret_cast<uint64_t>(stack) + pageSize,
                                            nullptr);
    if (!thread) {
        PX5_LOGE(LogCategory::FEX, "Guest test failed: FEXCore CreateThread returned null");
        munmap(code, codeSize);
        munmap(stack, pageSize);
        return false;
    }
    PX5_LOGI(LogCategory::FEX, "FEXCore guest thread created at RIP=%p", code);
    g_context->ExecuteThread(thread);
    const uint64_t result = thread->CurrentFrame->State.gregs[FEXCore::X86State::REG_RAX];
    PX5_LOGI(LogCategory::FEX, "FEXCore guest execution completed: RAX=%llu",
             static_cast<unsigned long long>(result));
    g_context->DestroyThread(thread);
    munmap(code, codeSize);
    munmap(stack, pageSize);
    return result == 42;
}

} // namespace PX5::FexCoreIntegration
