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

FEXCore::HostFeatures CreateHostFeatures() {
    FEXCore::HostFeatures features{};
    features.DCacheLineSize = 64;
    features.ICacheLineSize = 64;
    features.SupportsAES = true;
    features.SupportsCRC = true;
    features.SupportsAtomics = true;
    features.SupportsSVE128 = false;
    features.SupportsSVE256 = false;
    features.HostType = FEXCore::HostFeatures::HostTypeEnum::Linux;
    return features;
}

}

bool Initialize() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_context) {
        return true;
    }

    const auto features = CreateHostFeatures();
    g_context = FEXCore::Context::Context::CreateNewContext(features);
    if (!g_context) {
        PX5_LOGE(LogCategory::FEX, "FEXCore Context creation failed");
        return false;
    }

    g_context->SetSyscallHandler(&g_syscallHandler);
    g_context->SetSignalDelegator(&g_signalDelegator);
    g_context->EnableExitOnHLT();
    if (!g_context->InitCore()) {
        PX5_LOGE(LogCategory::FEX, "FEXCore Context::InitCore failed");
        g_context.reset();
        return false;
    }

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

}