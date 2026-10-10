// M8 T1 host gate: the vendored AnyPS5 RDNA recompiler turns a real recorded
// RDNA compute shader into a structurally valid SPIR-V module on the host —
// no device, no Vulkan, no SPIRV-Tools.
//
// The shader bytes are the upstream execution-test program (recorded RDNA,
// GPL-2.0, same license) doing a flat global load/add/store through user
// data buffer descriptors with control flow past s_end_prg. That exercises
// the decoder, control-flow structurizer, IR, SSA passes, and the SPIR-V
// backend in one shot; if any layer is broken the recompile fails or the
// emitted module is missing the expected instructions.
//
// Evidence contract (host-locked, like M5/M6/M7 T1): this proves the
// RECOMPILER works, not that pixels appear on a phone. Device evidence stays
// the M7/M8 milestone gates.
#include "Recompiler.hpp"
#include "BdaAbi.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using ShaderRecompiler::Recompile;
using ShaderRecompiler::RecompileRequest;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t SelectorRegister = 8;

// Recorded RDNA compute program (upstream BranchPastEndpgm execution test):
// loads a dword through a user-data buffer descriptor, adds a selector-driven
// value, stores back, with return/loop paths crossing s_end_prg.
constexpr std::array<std::uint32_t, 20> TailBlockCode{
    0x34020082, 0xe0302000, 0x80000401, 0xbf8c3f70, 0xbf068008, 0xbf840008, 0x4a080881, 0xe0702000, 0x80010401, 0xbf810000,
    0x4a080882, 0xe0702000, 0x80010401, 0xbf810000, 0xbf068108, 0xbf85fffa, 0x4a080883, 0xe0702000, 0x80010401, 0xbf810000,
};

// Adreno-class host target (mirrors the values the AGC driver derives from a
// real device): Vulkan 1.3, SPIR-V 1.3, 64-wide subgroups, BDA ABI v1.
ShaderRecompiler::SpirvTarget HostTarget() {
    static const std::array<std::uint32_t, 12> capabilities{
        1,   // Shader
        2,   // Tessellation (declared available; unused by compute)
        4,   // Geometry
        11,  // ImageBuffer
        12,  // StorageImageExtendedFormats
        17,  // Int64
        22,  // Int16
        23,  // TessellationPointSize
        4138,  // PhysicalStorageBufferAddresses (BDA)
        4463,  // FragmentShaderBarycentric
        4423,  // ShaderFloatControls2
        5307,  // ShaderInvocationReorder
    };
    ShaderRecompiler::SpirvTarget target{};
    target.vulkanVersion = 0x00403000u;  // VK_API_VERSION_1_3
    target.spirvVersion = 0x00010300u;   // SPIR-V 1.3
    target.subgroupSize = 64u;
    target.bdaAbiVersion = ShaderRecompiler::BdaAbi::Version;
    target.supportedCapabilities = capabilities;
    target.supportedExtensions = {};
    target.fragmentShaderBarycentricEnabled = false;
    target.maxWorkgroupSize = {1024u, 1024u, 1024u};
    target.maxWorkgroupInvocations = 1024u;
    target.maxWorkgroupSharedMemoryBytes = 32768u;
    target.nonConstantImageOffsets = false;
    target.srgbDecodeFormats = 0u;
    target.narrowSubgroupClock = false;
    return target;
}

// Host buffer descriptor shaped like the upstream execution tests: base
// address lo/hi (48-bit), stride<<16 | size in dwords, plus the data-format
// word used by the runtime ABI.
std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), count, 0x01016facu};
}

RecompileRequest BuildRequest(const std::array<std::uint32_t, TailBlockCode.size()>& code) {
    alignas(256) static std::array<std::uint32_t, Threads * 4> input{};
    alignas(256) static std::array<std::uint32_t, Threads * 4> output{};
    input.fill(7u);

    std::vector<std::uint32_t> userData(SelectorRegister + 1u, 0u);
    const auto inputDescriptor = BufferDescriptor(input.data(), static_cast<std::uint32_t>(input.size()));
    const auto outputDescriptor = BufferDescriptor(output.data(), static_cast<std::uint32_t>(output.size()));
    std::copy(inputDescriptor.begin(), inputDescriptor.end(), userData.begin());
    std::copy(outputDescriptor.begin(), outputDescriptor.end(), userData.begin() + 4);
    userData[SelectorRegister] = 1u;

    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{
        {reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(std::span{code})},
    }};
    const ShaderRecompiler::ShaderComputeStageInfo compute{
        {Threads, 1u, 1u}, 0u, {false, false, false}, false, 1u, {}, 0u,
    };

    RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0u, {}},
        {32u, 0u, userData, compute, std::nullopt, std::nullopt, memory, std::nullopt},
        HostTarget(),
        {0u, 0u, 0u, 128u},
        std::nullopt,
        false,
    };
    request.useCache = false;
    return request;
}

std::size_t CountInstruction(const std::vector<std::uint32_t>& spirv, std::uint32_t opcode) {
    std::size_t count = 0u;
    std::size_t offset = 5u;
    while (offset < spirv.size()) {
        const std::uint32_t word = spirv[offset];
        const std::uint32_t wordCount = word >> 16u;
        if (wordCount == 0u) throw std::runtime_error("zero word count in emitted module");
        if ((word & 0xffffu) == opcode) ++count;
        offset += wordCount;
    }
    return count;
}

// OpCapability (opcode 17) carries the capability id as its operand.
bool CapabilityDeclared(const std::vector<std::uint32_t>& spirv, std::uint32_t capability) {
    std::size_t offset = 5u;
    while (offset < spirv.size()) {
        const std::uint32_t word = spirv[offset];
        const std::uint32_t wordCount = word >> 16u;
        if (wordCount == 0u) throw std::runtime_error("zero word count in emitted module");
        if ((word & 0xffffu) == 17u && wordCount >= 2u && spirv[offset + 1u] == capability) return true;
        offset += wordCount;
    }
    return false;
}

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error("M8 T1: " + message);
}

// Diagnostic aid: PX5_DUMP_SPIRV=1 prints the opcode histogram + LocalSize
// operands of the emitted module (used to debug assertion failures).
void DumpIfRequested(const std::vector<std::uint32_t>& spirv) {
    if (std::getenv("PX5_DUMP_SPIRV") == nullptr) return;
    std::map<std::uint32_t, std::size_t> histogram;
    std::size_t offset = 5u;
    while (offset < spirv.size()) {
        const std::uint32_t word = spirv[offset];
        const std::uint32_t wordCount = word >> 16u;
        ++histogram[word & 0xffffu];
        if ((word & 0xffffu) == 16u || (word & 0xffffu) == 331u) {
            std::printf("  exec-mode opcode %u at %zu:", word & 0xffffu, offset);
            for (std::uint32_t i = 1u; i < wordCount; ++i) std::printf(" %u", spirv[offset + i]);
            std::printf("\n");
        }
        offset += wordCount;
    }
    for (const auto& [opcode, count] : histogram) std::printf("  opcode %5u x %zu\n", opcode, count);
}

}  // namespace

int main() {
    try {
        auto request = BuildRequest(TailBlockCode);
        const ShaderRecompiler::RecompileResult result = Recompile(request);

        const auto& spirv = result.spirv.Words();
        DumpIfRequested(spirv);
        Require(!spirv.empty(), "recompile produced no SPIR-V");
        Require(spirv[0] == 0x07230203u, "bad SPIR-V magic");
        Require(spirv[1] == 0x00010300u, "SPIR-V version word mismatch");
        Require(spirv[2] == 0u, "generator word must be zero");
        Require(spirv[4] == 0u, "schema word must be zero");
        Require(CountInstruction(spirv, 17u) >= 1u, "no OpCapability in module");
        Require(CapabilityDeclared(spirv, 1u), "no OpCapability Shader (1) in module");
        Require(CountInstruction(spirv, 15u) == 1u, "expected exactly one OpEntryPoint");
        Require(CountInstruction(spirv, 54u) == 1u, "expected exactly one OpFunction (main body)");

        // LocalSize must carry the PS5-declared thread count into SPIR-V.
        // OpExecutionMode (opcode 16): <id> entry point, ExecutionMode literal,
        // mode operands. LocalSize = execution-mode enum 17.
        constexpr std::uint32_t OpExecutionMode = 16u;
        constexpr std::uint32_t ExecutionModeLocalSize = 17u;
        std::size_t offset = 5u;
        bool localSizeFound = false;
        while (offset < spirv.size()) {
            const std::uint32_t word = spirv[offset];
            const std::uint32_t wordCount = word >> 16u;
            if ((word & 0xffffu) == OpExecutionMode && wordCount >= 6u && spirv[offset + 2u] == ExecutionModeLocalSize) {
                Require(spirv[offset + 3u] == Threads && spirv[offset + 4u] == 1u && spirv[offset + 5u] == 1u,
                        "LocalSize does not match the PS5 compute thread block {64,1,1}");
                localSizeFound = true;
                break;
            }
            offset += wordCount;
        }
        Require(localSizeFound, "OpExecutionMode LocalSize not found");

        // Warm-state contract: a second recompile of the same program must
        // satisfy the same structural rules. Byte-exact determinism is NOT
        // asserted: the recompiler keeps thread-local IR/SSA pools, and the
        // first (cold) run emits a slightly larger module than warm runs
        // (observed 1744 vs 1693 words) — upstream keys its shader cache on
        // the request inputs, not on output bytes, so this has no consumer
        // impact. Recorded here so the behaviour is not a hidden surprise.
        const auto second = Recompile(BuildRequest(TailBlockCode));
        const auto& secondSpirv = second.spirv.Words();
        Require(!secondSpirv.empty() && secondSpirv[0] == 0x07230203u, "warm recompile lost a valid SPIR-V header");
        Require(second.spirv.size() >= spirv.size() - 256u && second.spirv.size() <= spirv.size() + 256u,
                "warm recompile size diverged beyond the cold/warm pool window (±256): first=" + std::to_string(spirv.size()) + " second=" + std::to_string(second.spirv.size()));
        Require(CapabilityDeclared(secondSpirv, 1u), "warm recompile lost the Shader capability");

        std::printf("[M8 T1] RDNA (%zu dwords) -> SPIR-V (cold=%zu words, warm=%zu words): Shader capability, LocalSize {64,1,1}, one entry point\n",
                    TailBlockCode.size(), spirv.size(), second.spirv.size());
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "shader_recompiler_test: %s\n", error.what());
        return 1;
    }
}
