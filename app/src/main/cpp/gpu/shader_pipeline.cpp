// PX5 facade implementation over the vendored shader recompiler.
// See shader_pipeline.h for the contract and PX5_PORT.md for provenance.
#include "shader_pipeline.h"

#include "Recompiler.hpp"
#include "RuntimeAbi.hpp"
#include "BdaAbi.hpp"

#include <array>
#include <cstdlib>
#include <exception>
#include <utility>

namespace px5::gpu {

namespace {

ShaderRecompiler::ShaderStage MapStage(ShaderStageKind stage) {
    switch (stage) {
    case ShaderStageKind::Vertex: return ShaderRecompiler::ShaderStage::Vertex;
    case ShaderStageKind::Fragment: return ShaderRecompiler::ShaderStage::Fragment;
    case ShaderStageKind::Compute: return ShaderRecompiler::ShaderStage::Compute;
    }
    return ShaderRecompiler::ShaderStage::Vertex;
}

// SPIR-V 1.3 / Vulkan 1.1: the baseline every Adreno driver PX5 supports can
// consume. The M7 backend raises this once it queries the physical device.
constexpr std::uint32_t kTargetVulkanVersion = 0x00401000u;
constexpr std::uint32_t kTargetSpirvVersion = 0x00010300u;
constexpr std::uint32_t kWaveSize = 64u;

} // namespace

void SetShaderCacheDir(const std::string& path) {
    if (path.empty()) {
        setenv("ANYPS5_NO_SHADER_CACHE", "1", 1);
        return;
    }
    setenv("ANYPS5_SHADER_CACHE_DIR", path.c_str(), 1);
}

std::uint32_t ShaderPipelineAbiVersion() { return ShaderRecompiler::RuntimeAbi::Version; }

std::optional<CompiledShader> CompileGuestShader(ShaderStageKind stage,
                                                 std::span<const std::uint32_t> code,
                                                 std::string* error) {
    if (error != nullptr) error->clear();
    if (code.empty()) {
        if (error != nullptr) *error = "px5 shader pipeline: empty guest shader code";
        return std::nullopt;
    }

    try {
        ShaderRecompiler::RecompileRequest request;
        request.shader.stage = MapStage(stage);
        request.shader.codeAddress = 0; // guest address is advisory; the code span is authoritative
        request.shader.code = code;

        request.context.waveSize = kWaveSize;
        request.context.userDataBaseRegister = 0;
        static const std::array<std::uint32_t, 16> kZeroUserData {};
        request.context.userData = kZeroUserData;
        request.context.memory = {};

        // Stage context starts zeroed (see header): the M7 planner replaces
        // these with values lowered from GnmState per draw.
        ShaderRecompiler::ShaderVertexStageInfo vertex {};
        vertex.resourcesNum = 0;
        request.context.vertex = vertex;
        ShaderRecompiler::ShaderPixelStageInfo pixel {};
        request.context.pixel = pixel;

        request.target.vulkanVersion = kTargetVulkanVersion;
        request.target.spirvVersion = kTargetSpirvVersion;
        request.target.subgroupSize = kWaveSize;
        request.target.bdaAbiVersion = ShaderRecompiler::BdaAbi::Version;
        request.target.supportedCapabilities = {};
        request.target.supportedExtensions = {};
        request.target.maxWorkgroupSize = {128, 128, 128};
        request.target.maxWorkgroupInvocations = 1024;
        request.target.maxWorkgroupSharedMemoryBytes = 32768;

        request.layout.descriptorSet = ShaderRecompiler::RuntimeAbi::DescriptorSet;
        request.layout.firstBinding = 0;
        request.layout.pushConstantOffsetBytes = 0;
        request.layout.pushConstantSizeBytes =
            ShaderRecompiler::RuntimeAbi::PushConstantDwords * sizeof(std::uint32_t);

        const auto result = ShaderRecompiler::Recompile(request);
        CompiledShader compiled;
        compiled.spirv = std::move(result.spirv);
        compiled.variantId = result.variantId;
        compiled.fromCache = result.cacheHit;
        if (compiled.spirv.empty()) {
            if (error != nullptr) *error = "px5 shader pipeline: recompiler emitted no SPIR-V";
            return std::nullopt;
        }
        return compiled;
    } catch (const std::exception& e) {
        if (error != nullptr) *error = e.what();
        return std::nullopt;
    } catch (...) {
        if (error != nullptr) *error = "px5 shader pipeline: unknown recompiler failure";
        return std::nullopt;
    }
}

} // namespace px5::gpu
