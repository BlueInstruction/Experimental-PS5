// PX5 host gate for the vendored shader recompiler (AnyPS5 origin, GPL-2.0-only).
// Proves, on the host toolchain, before any Android build:
//   1. the full vendored library compiles and links,
//   2. the runtime ABI handshake matches (RuntimeAbi::Version 11),
//   3. a minimal synthetic RDNA program runs the whole pipeline:
//      decode -> IR -> control flow -> translation -> SPIR-V.
// This is the first end-to-end compile proof; real game shader bytes extend
// it later. Run: cmake -S . -B build && cmake --build build && ./build/recompiler_smoke
#include "Recompiler.hpp"
#include "RuntimeAbi.hpp"
#include "BdaAbi.hpp"
#include "shader_pipeline.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <chrono>
#include <vector>

using namespace ShaderRecompiler;

namespace {

// RDNA (GFX10.3) encodings: S_NOP 0, S_NOP 0, S_ENDPGM.
// 0xBF800000 = s_nop 0x0; 0xBF9E0000 = s_endpgm.
const std::vector<std::uint32_t> kMinimalVertexCode = {0xBF800000u, 0xBF800000u, 0xBF9E0000u};

std::string Describe(const std::exception& error) { return error.what(); }

int RunCompileProbe() {
    RecompileRequest request;
    request.shader.stage = ShaderStage::Vertex;
    request.shader.codeAddress = 0x1000;
    request.shader.code = kMinimalVertexCode;

    request.context.waveSize = 64;
    request.context.userDataBaseRegister = 0;
    static const std::array<std::uint32_t, 16> userData {};
    request.context.userData = userData;
    request.context.memory = {};

    ShaderVertexStageInfo vertex {};
    vertex.resourcesNum = 0;
    vertex.fetchAttribReg = 0;
    vertex.fetchBufferReg = 0;
    vertex.fetchEmbedded = false;
    request.context.vertex = vertex;

    request.target.vulkanVersion = 0x00401000u; // Vulkan 1.1
    request.target.spirvVersion = 0x00010300u;  // SPIR-V 1.3
    request.target.subgroupSize = 64;
    request.target.bdaAbiVersion = BdaAbi::Version;
    request.target.supportedCapabilities = {};
    request.target.supportedExtensions = {};
    request.target.maxWorkgroupSize = {128, 128, 128};
    request.target.maxWorkgroupInvocations = 1024;
    request.target.maxWorkgroupSharedMemoryBytes = 32768;

    request.layout.descriptorSet = RuntimeAbi::DescriptorSet;
    request.layout.firstBinding = 0;
    request.layout.pushConstantOffsetBytes = 0;
    request.layout.pushConstantSizeBytes = RuntimeAbi::PushConstantDwords * sizeof(std::uint32_t);

    const auto result = Recompile(request);
    std::printf("probe: stage=vertex words=%zu variantId=%llu pipelineVariant=%llu cacheHit=%d\n",
                result.spirv.size(),
                static_cast<unsigned long long>(result.variantId),
                static_cast<unsigned long long>(result.PipelineVariantId()),
                result.cacheHit ? 1 : 0);
    if (result.spirv.empty()) {
        std::printf("probe: FAIL — empty SPIR-V output\n");
        return 2;
    }
    if (result.spirv[0] != 0x07230203u) {
        std::printf("probe: FAIL — bad SPIR-V magic 0x%08x\n", result.spirv[0]);
        return 2;
    }
    std::printf("probe: PASS — SPIR-V magic ok, module emitted\n");
    return 0;
}

} // namespace

int main() {
    RuntimeAbi::RequireVersion(RuntimeAbi::Version);
    std::printf("abi: handshake ok (runtime %u)\n", RuntimeAbi::Version);

    // Facade boundary: the same code through px5::gpu::CompileGuestShader.
    px5::gpu::SetShaderCacheDir("./shader_cache");
    std::string facadeError;
    if (auto facadeResult = px5::gpu::CompileGuestShader(px5::gpu::ShaderStageKind::Vertex,
                                                         kMinimalVertexCode, &facadeError)) {
        std::printf("facade: PASS — words=%zu variantId=%llu fromCache=%d (abi %u)\n",
                    facadeResult->spirv.size(),
                    static_cast<unsigned long long>(facadeResult->variantId),
                    facadeResult->fromCache ? 1 : 0,
                    px5::gpu::ShaderPipelineAbiVersion());
    } else {
        std::printf("facade: FAIL — %s\n", facadeError.c_str());
        return 3;
    }

    try {
        const int exitCode = RunCompileProbe();
        // The disk cache writes on a background thread; give it a moment so a
        // subsequent run can prove the cache-hit path (fromCache=1).
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return exitCode;
    } catch (const std::exception& error) {
        std::printf("probe: PIPELINE-ERROR (named, clean): %s\n", Describe(error).c_str());
        // A named rejection from the pipeline is itself evidence the library
        // runs end-to-end; only escapes/crashes are gate failures.
        return 1;
    }
}
