// M8 T2 host gate: the vendored AnyPS5 RDNA recompiler handles the graphics
// stages, not just compute. Two hand-assembled GFX10.3 programs (encodings
// documented inline, same trusted style as the T1 recorded fixture) drive the
// vertex and fragment paths end to end:
//
//   vertex   — reads the entry-point-preloaded VertexIndex/InstanceIndex
//              VGPRs, converts them to float, exports POS0 (BuiltIn Position)
//              and PARAM0 (Location 0 output) through the real exp path.
//   fragment — reads interpolated parameter 0 via v_interp_p2_f32 (the
//              non-barycentric GetAttribute path a PS5 driver takes whenever
//              the host lacks VK_EXT_fragment_shader_barycentric), combines
//              it with inline constants, exports MRT0 (Location 0 output).
//
// Evidence contract (host-locked, like T1): this proves the recompiler's
// graphics-stage plumbing — entry preloads, interpolation inputs, export
// targets, interface decorations. It does not prove pixels on a phone; device
// evidence stays with the milestone gates.
#include "Recompiler.hpp"
#include "BdaAbi.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using ShaderRecompiler::Recompile;
using ShaderRecompiler::RecompileRequest;
using ShaderRecompiler::ShaderStage;

// Adreno-class host target: identical to the M8 T1 target (Vulkan 1.3,
// SPIR-V 1.3, 64-wide subgroups, BDA ABI v1). barycentric stays disabled —
// the fragment fixture must take the GetAttribute interpolation path.
ShaderRecompiler::SpirvTarget HostTarget() {
    static const std::array<std::uint32_t, 12> capabilities{
        1,   // Shader
        2,   // Tessellation (declared available; unused here)
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

// ---------------------------------------------------------------------------
// GFX10.3 encodings (verified against the vendored RdnaDecoder bit fields):
//
// VOP1  (prefix bits[30:25]=0b111111): word = 0x7E000000 | (op<<9) | src0 |
//        (vdst<<17). src0: 128..192 int inline {0..64}, 240..247 float inline
//        {0.5,-0.5,1.0,-1.0,2.0,-2.0,4.0,-4.0}, 255 = following literal word,
//        256..511 = v0..v255.
// VOP2  (bit31=0, other prefix): word = (op<<25) | (vdst<<17) | (vsrc1<<9) |
//        src0, src0 as above.
// VINTRP (bits[31:30]=0b11, bits[31:26]=0b110010): word = 0xC8000000 |
//        (vdst<<18) | (op<<16) | (attr<<10) | (chan<<8) | vsrc; op 1 =
//        v_interp_p2_f32.
// EXP   (2 words, bits[31:26]=0b111110): word0 = 0xF8000000 | (target<<4) |
//        en | (compr<<10) | (done<<11) | (vm<<12); word1 = v0 | v1<<8 |
//        v2<<16 | v3<<24. Targets: 0x00..0x07 = MRT0..7, 0x0C..0x0F =
//        POS0..3, 0x20..0x3F = PARAM0..31.
// s_end_prg = 0xBF810000.
// ---------------------------------------------------------------------------

// Vertex program:
//   v_cvt_f32_u32 v0, v5     ; x from VertexIndex (entry-preloaded VGPR)
//   v_mov_b32     v1, -1.0   ; y
//   v_mov_b32     v2, 0      ; z (integer-zero bit pattern)
//   v_mov_b32     v3, 1.0    ; w
//   v_cvt_f32_u32 v4, v8     ; param x from InstanceIndex (entry-preloaded)
//   exp pos0   v0,v1,v2,v3 done,vm
//   exp param0 v4,v1,v2,v3
//   s_end_prg
constexpr std::array<std::uint32_t, 11> VertexCode{
    0x7E000D05u,  // VOP1 op 6 (v_cvt_f32_u32), vdst=0, src0=v5 (261)
    0x7E0202FFu,  // VOP1 op 1 (v_mov_b32), vdst=1, src0=literal
    0xBF800000u,  // literal: -1.0f
    0x7E040280u,  // v_mov_b32 v2, int-inline 0
    0x7E0602F2u,  // v_mov_b32 v3, float-inline 1.0
    0x7E080D08u,  // v_cvt_f32_u32 v4, src0=v8 (264)
    0xF80018CFu,  // exp: target 0x0C (POS0), en=0xF, vm=1, done=1
    0x03020100u,  // exp sources v0,v1,v2,v3
    0xF8001A0Fu,  // exp: target 0x20 (PARAM0), en=0xF, vm=1, done=1
    0x03020104u,  // exp sources v4,v1,v2,v3
    0xBF810000u,  // s_end_prg
};

// Fragment program:
//   v_interp_p2_f32 v0, v0, attr 0, chan 0  ; read parameter 0 (op 1)
//   v_mov_b32 v1, 0.5                        ; float-inline
//   v_add_f32 v0, v0, v1
//   v_mov_b32 v2, 1.0                        ; float-inline
//   v_mul_f32 v0, v0, v2
//   exp mrt0 v0,v0,v0,v0 done,vm
//   s_end_prg
constexpr std::array<std::uint32_t, 8> FragmentCode{
    0xC8010000u,  // VINTRP op 1 (v_interp_p2_f32), vdst=0, attr=0, chan=0
    0x7E0202F0u,  // v_mov_b32 v1, float-inline 0.5
    0x06000300u,  // VOP2 op 3 (v_add_f32), vdst=0, vsrc1=1, src0=v0 (256)
    0x7E0402F2u,  // v_mov_b32 v2, float-inline 1.0
    0x0A000500u,  // VOP2 op 5 (v_mul_f32), vdst=0, vsrc1=2, src0=v0 (256)
    0xF800180Fu,  // exp: target 0x00 (MRT0), en=0xF, vm=1, done=1
    0x00000000u,  // exp sources v0,v0,v0,v0
    0xBF810000u,  // s_end_prg
};

RecompileRequest BuildRequest(ShaderStage stage, std::span<const std::uint32_t> code) {
    alignas(256) static const std::array<std::uint32_t, 4> anchor{};
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{
        {reinterpret_cast<std::uintptr_t>(anchor.data()), std::as_bytes(std::span{anchor})},
    }};
    RecompileRequest request{};
    request.shader = ShaderRecompiler::ShaderBinary{
        stage, reinterpret_cast<std::uintptr_t>(code.data()), code, 0u, {},
    };
    if (stage == ShaderStage::Vertex) {
        // No vertex-buffer resources: the fixture reads only entry-preloaded
        // builtins, so the embedded-fetch path must stay dormant.
        ShaderRecompiler::ShaderVertexStageInfo vertex{};
        vertex.resourcesNum = 0u;
        request.context.vertex = vertex;
    } else {
        // One smooth (non-flat) interpolator; no position/front-face bits in
        // SPI_PS_INPUT_ADDR — the shader only reads parameter 0.
        ShaderRecompiler::ShaderPixelStageInfo pixel{};
        pixel.interpolatorCount = 1u;
        pixel.interpolatorSettings = {};
        pixel.inputAddr = 0u;
        pixel.conservativeZExport = ShaderRecompiler::ConservativeZExport::AnyZ;
        request.context.pixel = pixel;
    }
    request.context.waveSize = 64u;
    request.context.userDataBaseRegister = 0u;
    request.context.memory = memory;
    request.target = HostTarget();
    request.layout = ShaderRecompiler::BindingLayout{0u, 0u, 0u, 128u};
    request.useCache = false;
    return request;
}

// ---------------------------------------------------------------------------
// SPIR-V inspection helpers (offset 5 skips the 5-word header).
// ---------------------------------------------------------------------------

constexpr std::uint32_t OpEntryPoint = 15u;
constexpr std::uint32_t OpExecutionMode = 16u;
constexpr std::uint32_t OpDecorate = 71u;
constexpr std::uint32_t OpMemberDecorate = 72u;
constexpr std::uint32_t OpVariable = 59u;
constexpr std::uint32_t DecorationLocation = 30u;
constexpr std::uint32_t DecorationBuiltIn = 11u;
constexpr std::uint32_t StorageClassInput = 1u;
constexpr std::uint32_t StorageClassOutput = 3u;

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error("M8 T2: " + message);
}

struct InstructionView {
    std::uint32_t opcode;
    std::size_t offset;  // index of the opcode word
    std::uint32_t wordCount;
};

std::vector<InstructionView> WalkInstructions(const std::vector<std::uint32_t>& spirv) {
    std::vector<InstructionView> result;
    std::size_t offset = 5u;
    while (offset < spirv.size()) {
        const std::uint32_t word = spirv[offset];
        const std::uint32_t wordCount = word >> 16u;
        if (wordCount == 0u) throw std::runtime_error("zero word count in emitted module");
        result.push_back({word & 0xffffu, offset, wordCount});
        offset += wordCount;
    }
    return result;
}

std::uint32_t SingleEntryPointModel(const std::vector<std::uint32_t>& spirv) {
    std::uint32_t model = 0xffffffffu;
    std::size_t count = 0u;
    for (const auto& inst : WalkInstructions(spirv)) {
        if (inst.opcode == OpEntryPoint) {
            Require(inst.wordCount >= 2u, "OpEntryPoint too short");
            model = spirv[inst.offset + 1u];
            ++count;
        }
    }
    Require(count == 1u, "expected exactly one OpEntryPoint");
    return model;
}

std::set<std::uint32_t> DecoratedBuiltins(const std::vector<std::uint32_t>& spirv) {
    // BuiltIn decorations arrive two ways in practice: OpDecorate on a plain
    // interface variable (VertexIndex, InstanceIndex, ...) and
    // OpMemberDecorate on the per-vertex output block's Position member
    // (the gl_PerVertex convention). Collect both.
    std::set<std::uint32_t> builtins;
    for (const auto& inst : WalkInstructions(spirv)) {
        // OpDecorate: <id> target, <decoration>, [operands...]
        if (inst.opcode == OpDecorate && inst.wordCount >= 4u && spirv[inst.offset + 2u] == DecorationBuiltIn) {
            builtins.insert(spirv[inst.offset + 3u]);
        }
        // OpMemberDecorate: <id> target, <literal> member, <decoration>, [operands...]
        if (inst.opcode == OpMemberDecorate && inst.wordCount >= 5u && spirv[inst.offset + 3u] == DecorationBuiltIn) {
            builtins.insert(spirv[inst.offset + 4u]);
        }
    }
    return builtins;
}

bool HasExecutionMode(const std::vector<std::uint32_t>& spirv, std::uint32_t mode) {
    for (const auto& inst : WalkInstructions(spirv)) {
        if (inst.opcode == OpExecutionMode && inst.wordCount >= 3u && spirv[inst.offset + 2u] == mode) return true;
    }
    return false;
}

// Location-decorated interface variables by storage class (Input/Output).
std::map<std::uint32_t, std::set<std::uint32_t>> InterfaceLocations(const std::vector<std::uint32_t>& spirv) {
    // First pass: OpVariable id -> storage class.
    std::map<std::uint32_t, std::uint32_t> storageById;
    for (const auto& inst : WalkInstructions(spirv)) {
        if (inst.opcode == OpVariable && inst.wordCount >= 4u) {
            storageById.emplace(spirv[inst.offset + 2u], spirv[inst.offset + 3u]);
        }
    }
    // Second pass: Location decorations on those ids.
    std::map<std::uint32_t, std::set<std::uint32_t>> result;
    for (const auto& inst : WalkInstructions(spirv)) {
        if (inst.opcode != OpDecorate || inst.wordCount < 4u || spirv[inst.offset + 2u] != DecorationLocation) continue;
        const auto var = storageById.find(spirv[inst.offset + 1u]);
        if (var == storageById.end()) continue;
        result[var->second].insert(spirv[inst.offset + 3u]);
    }
    return result;
}

std::size_t CountInstruction(const std::vector<std::uint32_t>& spirv, std::uint32_t opcode) {
    std::size_t count = 0u;
    for (const auto& inst : WalkInstructions(spirv)) {
        if (inst.opcode == opcode) ++count;
    }
    return count;
}

// Diagnostic aid: PX5_DUMP_SPIRV=1 prints the opcode histogram plus entry
// point / decoration details of the emitted module.
void DumpIfRequested(const std::vector<std::uint32_t>& spirv) {
    if (std::getenv("PX5_DUMP_SPIRV") == nullptr) return;
    std::map<std::uint32_t, std::size_t> histogram;
    for (const auto& inst : WalkInstructions(spirv)) {
        ++histogram[inst.opcode];
        if (inst.opcode == OpEntryPoint || inst.opcode == OpDecorate || inst.opcode == OpExecutionMode || inst.opcode == 72u || inst.opcode == OpVariable) {
            std::printf("  opcode %5u:", inst.opcode);
            for (std::uint32_t i = 1u; i < inst.wordCount; ++i) std::printf(" %u", spirv[inst.offset + i]);
            std::printf("\n");
        }
    }
    for (const auto& [opcode, count] : histogram) std::printf("  opcode %5u x %zu\n", opcode, count);
}

void CheckModuleHeader(const std::vector<std::uint32_t>& spirv, const char* label) {
    Require(!spirv.empty(), std::string(label) + ": recompile produced no SPIR-V");
    Require(spirv[0] == 0x07230203u, std::string(label) + ": bad SPIR-V magic");
    Require(spirv[1] == 0x00010300u, std::string(label) + ": SPIR-V version word mismatch");
    Require(spirv[2] == 0u, std::string(label) + ": generator word must be zero");
    Require(spirv[4] == 0u, std::string(label) + ": schema word must be zero");
}

}  // namespace

int main() {
    try {
        // ---------------- vertex stage ----------------
        {
            auto request = BuildRequest(ShaderStage::Vertex, VertexCode);
            const ShaderRecompiler::RecompileResult result = Recompile(request);
            const auto& spirv = result.spirv.Words();
            DumpIfRequested(spirv);
            CheckModuleHeader(spirv, "vertex");

            constexpr std::uint32_t ExecutionModelVertex = 0u;
            Require(SingleEntryPointModel(spirv) == ExecutionModelVertex,
                    "vertex module must have exactly one OpEntryPoint with Vertex model");

            const auto builtins = DecoratedBuiltins(spirv);
            Require(builtins.count(0u) == 1u, "vertex module missing BuiltIn Position (0)");
            Require(builtins.count(42u) == 1u, "vertex module missing BuiltIn VertexIndex (42)");
            Require(builtins.count(43u) == 1u, "vertex module missing BuiltIn InstanceIndex (43)");

            const auto interfaces = InterfaceLocations(spirv);
            const auto outputs = interfaces.find(StorageClassOutput);
            Require(outputs != interfaces.end() && outputs->second.count(0u) == 1u,
                    "vertex module missing a Location 0 output (PARAM0 export)");

            Require(CountInstruction(spirv, 54u) == 1u, "vertex module must contain exactly one OpFunction");
        }

        // ---------------- fragment stage ----------------
        {
            auto request = BuildRequest(ShaderStage::Fragment, FragmentCode);
            const ShaderRecompiler::RecompileResult result = Recompile(request);
            const auto& spirv = result.spirv.Words();
            DumpIfRequested(spirv);
            CheckModuleHeader(spirv, "fragment");

            constexpr std::uint32_t ExecutionModelFragment = 4u;
            Require(SingleEntryPointModel(spirv) == ExecutionModelFragment,
                    "fragment module must have exactly one OpEntryPoint with Fragment model");
            constexpr std::uint32_t ExecutionModeOriginUpperLeft = 7u;
            Require(HasExecutionMode(spirv, ExecutionModeOriginUpperLeft),
                    "fragment module missing OpExecutionMode OriginUpperLeft");

            const auto interfaces = InterfaceLocations(spirv);
            const auto inputs = interfaces.find(StorageClassInput);
            Require(inputs != interfaces.end() && inputs->second.count(0u) == 1u,
                    "fragment module missing a Location 0 input (interpolated parameter 0)");
            const auto outputs = interfaces.find(StorageClassOutput);
            Require(outputs != interfaces.end() && outputs->second.count(0u) == 1u,
                    "fragment module missing a Location 0 output (MRT0 export)");

            Require(CountInstruction(spirv, 54u) == 1u, "fragment module must contain exactly one OpFunction");
        }

        std::printf("M8 T2: vertex + fragment recompile gates passed\n");
        return 0;
    } catch (const std::exception& error) {
        std::printf("M8 T2 FAILED: %s\n", error.what());
        return 1;
    }
}
