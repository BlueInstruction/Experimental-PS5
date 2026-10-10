// M8 T3 host gate: the APP-side shader service (gpu/shader/
// shader_recompiler_service.cpp — the same TU the arm64 APK compiles) turns
// real RDNA programs into structurally valid SPIR-V, serves identical
// requests from its bounded cache, and reports recompiler rejections as
// counted failures — never crashes, never guessed modules.
//
// Fixtures are the proven M8 T1 (recorded RDNA compute program) and M8 T2
// (hand-assembled GFX10.3 vertex/fragment programs) code words. Evidence
// contract is host-locked like every milestone T1: it proves the service
// works, not that pixels appear on a phone.
#include "gpu/shader/shader_recompiler_service.h"

#include <array>
#include <cstdio>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using PX5::Gpu::GuestShaderMeta;
using PX5::Gpu::GuestShaderRecompiler;
using PX5::Gpu::GuestShaderStage;

// Recorded RDNA compute program (M8 T1 fixture): flat load/add/store through
// user-data buffer descriptors with control flow past s_end_prg.
constexpr std::array<uint32_t, 20> ComputeCode{
    0x34020082, 0xe0302000, 0x80000401, 0xbf8c3f70, 0xbf068008, 0xbf840008, 0x4a080881, 0xe0702000, 0x80010401, 0xbf810000,
    0x4a080882, 0xe0702000, 0x80010401, 0xbf810000, 0xbf068108, 0xbf85fffa, 0x4a080883, 0xe0702000, 0x80010401, 0xbf810000,
};

// Hand-assembled GFX10.3 vertex program (M8 T2 fixture): converts the
// entry-preloaded VertexIndex/InstanceIndex VGPRs, exports POS0 + PARAM0.
constexpr std::array<uint32_t, 11> VertexCode{
    0x7E000D05u, 0x7E0202FFu, 0xBF800000u, 0x7E040280u, 0x7E0602F2u,
    0x7E080D08u, 0xF80018CFu, 0x03020100u, 0xF8001A0Fu, 0x03020104u, 0xBF810000u,
};

// Hand-assembled GFX10.3 fragment program (M8 T2 fixture): reads parameter 0
// through v_interp_p2_f32, combines, exports MRT0.
constexpr std::array<uint32_t, 8> FragmentCode{
    0xC8010000u, 0x7E0202F0u, 0x06000300u, 0x7E0402F2u,
    0x0A000500u, 0xF800180Fu, 0x00000000u, 0xBF810000u,
};

constexpr uint32_t OpEntryPoint = 15u;
constexpr uint32_t ExecutionModelGLCompute = 5u;
constexpr uint32_t ExecutionModelVertex = 0u;
constexpr uint32_t ExecutionModelFragment = 4u;

std::uint32_t EntryPointModel(const std::vector<uint32_t>& spirv) {
    std::size_t offset = 5u;
    std::uint32_t model = 0xffffffffu;
    std::size_t count = 0u;
    while (offset < spirv.size()) {
        const uint32_t word = spirv[offset];
        const uint32_t wordCount = word >> 16u;
        if (wordCount == 0u) throw std::runtime_error("zero word count in module");
        if ((word & 0xffffu) == OpEntryPoint) {
            model = spirv[offset + 1u];
            ++count;
        }
        offset += wordCount;
    }
    if (count != 1u) throw std::runtime_error("expected exactly one OpEntryPoint");
    return model;
}

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error("M8 T3: " + message);
}

void CheckModule(const std::vector<uint32_t>& spirv, uint32_t expectedModel,
                 const char* label) {
    Require(!spirv.empty(), std::string(label) + ": empty module");
    Require(spirv[0] == 0x07230203u, std::string(label) + ": bad magic");
    Require(spirv[1] == 0x00010300u, std::string(label) + ": bad version word");
    Require(EntryPointModel(spirv) == expectedModel,
            std::string(label) + ": wrong entry point model");
}

} // namespace

int main() {
    try {
        auto& service = GuestShaderRecompiler::Instance();
        service.ClearCache();

        // ---- compute: real recorded program through the service ----
        // User data mirrors the T1 fixture ABI: two buffer descriptors
        // (input, output) + a selector word at SGPR 8. Compute always needs
        // user data — the workgroup SGPRs land after it.
        alignas(256) static std::array<uint32_t, 64> buffers{};
        const uintptr_t bufferAddr = reinterpret_cast<uintptr_t>(buffers.data());
        const std::array<uint32_t, 9> userData{
            static_cast<uint32_t>(bufferAddr),
            static_cast<uint32_t>((bufferAddr >> 32u) & 0xffffu) | (4u << 16u),
            64u, 0x01016facu,
            static_cast<uint32_t>(bufferAddr),
            static_cast<uint32_t>((bufferAddr >> 32u) & 0xffffu) | (4u << 16u),
            64u, 0x01016facu,
            1u,
        };
        std::vector<uint32_t> spirv;
        std::string error;
        GuestShaderMeta meta{};
        meta.stage = GuestShaderStage::Compute;
        meta.code = ComputeCode.data();
        meta.codeDwords = static_cast<uint32_t>(ComputeCode.size());
        meta.userData = userData;
        meta.threadsX = 64u;
        Require(service.Recompile(meta, spirv, error),
                "compute recompile failed: " + error);
        CheckModule(spirv, ExecutionModelGLCompute, "compute");

        // ---- compute without user data: clear, counted rejection ----
        const uint64_t failures0 = service.Stats().failures;
        meta.userData = {};
        Require(!service.Recompile(meta, spirv, error),
                "compute without user data must be rejected");
        Require(error.find("user data") != std::string::npos,
                "rejection must name the user-data ABI, got: " + error);
        Require(service.Stats().failures == failures0 + 1u,
                "empty-user-data rejection was not counted");
        meta.userData = userData;

        // ---- vertex + fragment through the service ----
        meta.stage = GuestShaderStage::Vertex;
        meta.code = VertexCode.data();
        meta.codeDwords = static_cast<uint32_t>(VertexCode.size());
        meta.wave32 = false;
        Require(service.Recompile(meta, spirv, error),
                "vertex recompile failed: " + error);
        CheckModule(spirv, ExecutionModelVertex, "vertex");

        meta.stage = GuestShaderStage::Fragment;
        meta.code = FragmentCode.data();
        meta.codeDwords = static_cast<uint32_t>(FragmentCode.size());
        // The fixture reads parameter 0 — the guest must declare it.
        meta.fragmentInterpolators = 1u;
        Require(service.Recompile(meta, spirv, error),
                "fragment recompile failed: " + error);
        CheckModule(spirv, ExecutionModelFragment, "fragment");

        // ---- fragment reading beyond the declared interpolators: honest
        //      recompiler rejection, counted as a failure ----
        const uint64_t failures1 = service.Stats().failures;
        meta.fragmentInterpolators = 0u;
        Require(!service.Recompile(meta, spirv, error),
                "fragment read beyond interpolators must fail");
        Require(spirv.empty(), "failed fragment recompile must not return a module");
        Require(service.Stats().failures == failures1 + 1u,
                "interpolator overrun was not counted");

        // ---- cache: identical request served without a recompile ----
        const uint64_t recompilesBefore = service.Stats().recompiles;
        const uint64_t hitsBefore = service.Stats().cacheHits;
        meta.stage = GuestShaderStage::Vertex;
        meta.code = VertexCode.data();
        meta.codeDwords = static_cast<uint32_t>(VertexCode.size());
        Require(service.Recompile(meta, spirv, error),
                "cached vertex recompile failed: " + error);
        CheckModule(spirv, ExecutionModelVertex, "cached vertex");
        Require(service.Stats().recompiles == recompilesBefore,
                "cached request triggered a recompile");
        Require(service.Stats().cacheHits == hitsBefore + 1u,
                "cached request did not count a hit");

        // ---- failure honesty: garbage code is a counted rejection ----
        const std::array<uint32_t, 4> garbage{0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu};
        meta.stage = GuestShaderStage::Vertex;
        meta.code = garbage.data();
        meta.codeDwords = static_cast<uint32_t>(garbage.size());
        const uint64_t failuresBefore = service.Stats().failures;
        Require(!service.Recompile(meta, spirv, error),
                "garbage shader must not compile");
        Require(spirv.empty(), "failed recompile must not return a module");
        Require(!error.empty(), "failed recompile must carry an error string");
        Require(service.Stats().failures == failuresBefore + 1u,
                "failure was not counted");
        Require(service.Stats().SummaryString().find("failures=") != std::string::npos,
                "stats summary must carry failure accounting");

        std::printf("M8 T3: guest shader service gates passed\n");
        return 0;
    } catch (const std::exception& e) {
        std::printf("M8 T3 FAILED: %s\n", e.what());
        return 1;
    }
}
