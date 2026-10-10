// PX5 port layer — implements the ValidateAndOptimizeSpirv contract declared
// by core/shader/recompiler/SpirvBackend/include/SpirvBackend/SpirvOptimizer.hpp
// WITHOUT a SPIRV-Tools dependency.
//
// Upstream compiles SpirvOptimizer.cpp only when ANYPS5_ENABLE_SPIRV_TOOLS=1
// and links SPIRV-Tools-opt/static for full grammar validation + optimization
// passes. PX5's Android/host builds do not vendor SPIRV-Tools yet, so this
// translation unit provides the same symbol with a structural-validation
// implementation: header/schema checks plus an instruction-stream walk, then
// the module is returned unchanged (no optimization passes).
//
// Upgrading to full SPIRV-Tools validation later means replacing this file's
// body with a spirv-tools-backed one and setting ANYPS5_ENABLE_SPIRV_TOOLS=1
// — the vendor tree itself stays untouched either way.
#include "SpirvBackend/SpirvOptimizer.hpp"

#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace ShaderRecompiler {

namespace {

constexpr std::uint32_t MagicNumber = 0x07230203u;

std::vector<std::uint32_t> ValidateStructure(std::span<const std::uint32_t> spirv, std::uint32_t spirvVersion) {
    if (spirv.size() < 5) throw std::runtime_error("SPIR-V validation: module shorter than the 5-word header");
    if (spirv[0] != MagicNumber) throw std::runtime_error("SPIR-V validation: bad magic number");
    const std::uint32_t versionWord = spirv[1];
    if ((versionWord >> 16u) != 0u) throw std::runtime_error("SPIR-V validation: reserved version word bits set");
    if (versionWord > spirvVersion) throw std::runtime_error("SPIR-V validation: module version exceeds target SPIR-V version");
    if (spirv[3] == 0u) throw std::runtime_error("SPIR-V validation: zero bound");

    std::size_t offset = 5u;
    while (offset < spirv.size()) {
        const std::uint32_t word = spirv[offset];
        const std::uint32_t wordCount = word >> 16u;
        const std::uint32_t opcode = word & 0xffffu;
        if (wordCount == 0u) throw std::runtime_error("SPIR-V validation: instruction with zero word count at " + std::to_string(offset));
        if (offset + wordCount > spirv.size()) throw std::runtime_error("SPIR-V validation: instruction overruns the module at " + std::to_string(offset));
        if (wordCount == 1u && opcode != 0u) throw std::runtime_error("SPIR-V validation: OpNop is the only single-word instruction besides padding at " + std::to_string(offset));
        offset += wordCount;
    }
    if (offset != spirv.size()) throw std::runtime_error("SPIR-V validation: instruction stream does not end on the module size");
    return {spirv.begin(), spirv.end()};
}

}  // namespace

std::vector<std::uint32_t> ValidateAndOptimizeSpirv(std::span<const std::uint32_t> spirv, std::uint32_t /*vulkanVersion*/, std::uint32_t spirvVersion, bool /*allowOffsetTextureOperand*/, bool /*optimize*/, bool /*specialize*/) {
    return ValidateStructure(spirv, spirvVersion);
}

}  // namespace ShaderRecompiler
