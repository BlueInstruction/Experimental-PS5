// M8 T4 host gate: real PM4 SET_SH packets decode into GnmState and the
// shader capture layer turns that register state into SPIR-V through the
// M8 T3 service — compute AND vertex AND fragment, with the guest's own
// declarations (RSRC2.USER_SGPR, NUM_THREAD, DISPATCH initiator wave bit,
// SPI_PS_IN_CONTROL_0 interpolators) driving the recompile inputs.
//
// The stream is built with the real Type-3 packet encoders (SET_SH_REG /
// SET_CONTEXT_REG / DISPATCH_DIRECT / DRAW_INDEX_AUTO) and decoded by the
// REAL Pm4Decoder — the same TU the APK runs. The shader code words are
// the proven M8 T1/T2 fixtures. Register shapes were cross-verified from
// KytyPS5 + AnyPS5 (PS5) and shadPS4 (PS4/PGM packing + packet shape); see
// gpu/gnm/sh_registers.h for the full source list.
//
// Evidence contract (host-locked like every milestone T-gate): this proves
// the PM4->GnmState->capture->SPIR-V path works, not that pixels appear
// on a phone.
#include "gpu/gnm/gnm_state.h"
#include "gpu/gnm/pm4_decoder.h"
#include "gpu/gnm/pm4_packet.h"
#include "gpu/gnm/sh_registers.h"
#include "gpu/shader/shader_capture.h"
#include "gpu/shader/shader_recompiler_service.h"

#include <cstdint>
#include <cstdio>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using PX5::Gnm::GnmState;
using PX5::Gnm::Pm4Decoder;
using PX5::Gnm::Type3Header;
using PX5::Gpu::GuestShaderRecompiler;
using PX5::Gpu::GuestShaderStage;
using PX5::Gpu::ShaderCapture;
using PX5::Gpu::ShaderCaptureResult;

// ---- M8 T1/T2 fixture programs (recorded / hand-assembled GFX10.3) ------
constexpr std::array<uint32_t, 20> ComputeCode{
    0x34020082, 0xe0302000, 0x80000401, 0xbf8c3f70, 0xbf068008, 0xbf840008,
    0x4a080881, 0xe0702000, 0x80010401, 0xbf810000, 0x4a080882, 0xe0702000,
    0x80010401, 0xbf810000, 0xbf068108, 0xbf85fffa, 0x4a080883, 0xe0702000,
    0x80010401, 0xbf810000,
};
constexpr std::array<uint32_t, 11> VertexCode{
    0x7E000D05u, 0x7E0202FFu, 0xBF800000u, 0x7E040280u, 0x7E0602F2u,
    0x7E080D08u, 0xF80018CFu, 0x03020100u, 0xF8001A0Fu, 0x03020104u,
    0xBF810000u,
};
constexpr std::array<uint32_t, 8> FragmentCode{
    0xC8010000u, 0x7E0202F0u, 0x06000300u, 0x7E0402F2u,
    0x0A000500u, 0xF800180Fu, 0x00000000u, 0xBF810000u,
};

constexpr uint32_t OpEntryPoint = 15u;
constexpr uint32_t ExecutionModelGLCompute = 5u;
constexpr uint32_t ExecutionModelVertex = 0u;
constexpr uint32_t ExecutionModelFragment = 4u;

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error("M8 T4: " + message);
}

std::uint32_t EntryPointModel(const std::vector<uint32_t>& spirv) {
    std::size_t offset = 5u;
    std::uint32_t model = 0xffffffffu;
    std::size_t count = 0u;
    while (offset < spirv.size()) {
        const uint32_t word = spirv[offset];
        const uint32_t wordCount = word >> 16u;
        if (wordCount == 0u) throw std::runtime_error("zero word count");
        if ((word & 0xffffu) == OpEntryPoint) {
            model = spirv[offset + 1u];
            ++count;
        }
        offset += wordCount;
    }
    Require(count == 1u, "expected exactly one OpEntryPoint");
    return model;
}

void CheckModule(const std::vector<uint32_t>& spirv, uint32_t expectedModel,
                 const char* label) {
    Require(!spirv.empty(), std::string(label) + ": empty module");
    Require(spirv[0] == 0x07230203u, std::string(label) + ": bad magic");
    Require(spirv[1] == 0x00010300u, std::string(label) + ": bad version");
    Require(EntryPointModel(spirv) == expectedModel,
            std::string(label) + ": wrong entry point model");
}

// ---- PM4 stream builders (real Type-3 packet shapes) --------------------
std::vector<uint32_t> SetReg(uint32_t opcode, uint32_t spaceOffset,
                             const std::vector<uint32_t>& values) {
    std::vector<uint32_t> body;
    body.push_back(spaceOffset);
    body.insert(body.end(), values.begin(), values.end());
    std::vector<uint32_t> packet;
    packet.push_back(Type3Header::Encode(opcode, body.size()));
    packet.insert(packet.end(), body.begin(), body.end());
    return packet;
}

std::vector<uint32_t> SetShReg(uint32_t spaceOffset,
                               const std::vector<uint32_t>& values) {
    return SetReg(PX5::Gnm::kItSetShReg, spaceOffset, values);
}

std::vector<uint32_t> DispatchDirect(uint32_t x, uint32_t y, uint32_t z,
                                     uint32_t initiator,
                                     uint32_t shaderType) {
    std::vector<uint32_t> packet;
    packet.push_back(Type3Header::Encode(PX5::Gnm::kItDispatchDirect, 4,
                                         shaderType));
    packet.push_back(x);
    packet.push_back(y);
    packet.push_back(z);
    packet.push_back(initiator);
    return packet;
}

// Map-backed guest memory + capture MemoryReader.
using GuestMemory = std::map<uint64_t, std::vector<uint32_t>>;

ShaderCapture::MemoryReader MapReader(GuestMemory& mem) {
    return [&mem](uint64_t vaddr, uint32_t maxDwords,
                  std::vector<uint32_t>& out) {
        const auto it = mem.find(vaddr);
        if (it == mem.end()) return false;
        out = it->second;
        if (out.size() > maxDwords) out.resize(maxDwords);
        return true;
    };
}

void Append(std::vector<uint32_t>& stream, const std::vector<uint32_t>& tail) {
    stream.insert(stream.end(), tail.begin(), tail.end());
}

// Decodes a stream and asserts clean decode (no stream errors).
void DecodeClean(const std::vector<uint32_t>& stream, GnmState& state) {
    Pm4Decoder decoder;
    PX5::Gnm::DecodeStats stats;
    std::vector<PX5::Gnm::StreamError> errors;
    decoder.Decode(stream.data(), stream.size(), state, stats, &errors);
    Require(stats.streamErrors == 0u, "clean fixture stream reported errors");
}

// The T4 compute stream: NUM_THREAD, PGM pair (HI carries junk upper bits),
// RSRC1+RSRC2 (declared user SGPRs), user-data window, DISPATCH with the
// wave32 initiator bit set. Shapes mirror what sceGnmSetComputeShader +
// sceGnmDispatchDirect produce on a real guest.
std::vector<uint32_t> BuildComputeStream(uint32_t declaredUserSgprs,
                                         uint64_t codeVaddr) {
    std::vector<uint32_t> stream;
    // COMPUTE_NUM_THREAD_X/Y/Z (SH offsets 0x207..0x209).
    Append(stream, SetShReg(PX5::Gnm::Sh::kComputeNumThreadX, {64u, 1u, 1u}));
    // COMPUTE_PGM_LO/HI (SH offsets 0x20C/0x20D): 40-bit field << 8;
    // HI[31:8] junk must be masked by the capture.
    const uint32_t pgmLo = static_cast<uint32_t>((codeVaddr >> 8) & 0xFFFFFFFFu);
    const uint32_t pgmHiJunk = 0xABCD0000u |
        static_cast<uint32_t>((codeVaddr >> 40) & 0xFFu);
    Append(stream, SetShReg(PX5::Gnm::Sh::kComputePgmLo, {pgmLo, pgmHiJunk}));
    // COMPUTE_PGM_RSRC1/RSRC2 (SH offsets 0x212/0x213): USER_SGPR[5:1].
    Append(stream, SetShReg(PX5::Gnm::Sh::kComputeRsrc1,
                            {0u, declaredUserSgprs << 1}));
    // COMPUTE_USER_DATA_0.. (SH offset 0x240): the T1 fixture ABI — input
    // descriptor, output descriptor, selector (values are compile-time
    // descriptor words; the recompiler parses them, never dereferences).
    Append(stream, SetShReg(PX5::Gnm::Sh::kComputeUserData0,
                            {0x11223344u, 0x00040010u, 64u, 0x01016facu,
                             0x55667788u, 0x00040010u, 64u, 0x01016facu,
                             1u}));
    // DISPATCH_DIRECT: real four-dword body; initiator bit 15 = wave32.
    Append(stream, DispatchDirect(64u, 1u, 1u, 0x8000u, 1u));
    return stream;
}

} // namespace

int main() {
    try {
        auto& service = GuestShaderRecompiler::Instance();
        service.ClearCache();

        // ---- compute: full PM4 stream -> capture -> SPIR-V --------------
        constexpr uint64_t kComputeVaddr = 0x112345600ull;  // 256B aligned
        GuestMemory mem;
        mem[kComputeVaddr] =
            std::vector<uint32_t>(ComputeCode.begin(), ComputeCode.end());
        const ShaderCapture::MemoryReader reader = MapReader(mem);

        {
            GnmState state;
            const std::vector<uint32_t> stream =
                BuildComputeStream(9u, kComputeVaddr);
            DecodeClean(stream, state);

            // Dispatch accounting: the initiator rides the packet.
            Require(state.Dispatches() == 1u, "expected one dispatch record");
            Require(state.DispatchLog().back().initiator == 0x8000u,
                    "dispatch record lost the packet initiator");
            Require(state.LastDispatchInitiator() == 0x8000u,
                    "last-dispatch initiator not persisted");

            const ShaderCaptureResult captured =
                ShaderCapture::ExtractShader(state, GuestShaderStage::Compute);
            Require(captured.hasProgram, "compute: PGM pair not detected");
            Require(captured.codeVaddr == kComputeVaddr,
                    "compute: PGM HI junk bits not masked / wrong vaddr");
            Require(captured.threadsX == 64u && captured.threadsY == 1u &&
                        captured.threadsZ == 1u,
                    "compute: NUM_THREAD regs not captured");
            Require(captured.wave32, "compute: wave32 initiator bit lost");
            Require(captured.declaredUserSgprs == 9u,
                    "compute: RSRC2.USER_SGPR not decoded");
            Require(captured.userData.size() == 9u,
                    "compute: user-data window not trimmed to declaration");

            std::vector<uint32_t> spirv;
            std::string error;
            Require(ShaderCapture::CaptureAndRecompile(
                        state, GuestShaderStage::Compute, reader, spirv,
                        error),
                    "compute capture recompile failed: " + error);
            CheckModule(spirv, ExecutionModelGLCompute, "compute T4");
        }

        // ---- compute wave64: no initiator dword / bit unset -------------
        {
            GnmState state;
            const std::vector<uint32_t> stream =
                BuildComputeStream(9u, kComputeVaddr);
            // Rewrite the dispatch initiator without bit 15 (wave64) by
            // rebuilding the stream from scratch: simplest honest path.
            std::vector<uint32_t> stream64;
            stream64.insert(stream64.end(), stream.begin(), stream.end() - 5);
            stream64.push_back(Type3Header::Encode(
                PX5::Gnm::kItDispatchDirect, 4, 1u));
            stream64.push_back(64u);
            stream64.push_back(1u);
            stream64.push_back(1u);
            stream64.push_back(0u);  // no CS_W32_EN -> wave64
            DecodeClean(stream64, state);
            const ShaderCaptureResult captured =
                ShaderCapture::ExtractShader(state, GuestShaderStage::Compute);
            Require(!captured.wave32,
                    "compute: initiator bit 15 must default to wave64");
        }

        // ---- vertex: VS block capture through a draw stream -------------
        {
            GnmState state;
            std::vector<uint32_t> stream;
            constexpr uint64_t kVsVaddr = 0x22000000ull;  // 256B aligned
            mem[kVsVaddr] =
                std::vector<uint32_t>(VertexCode.begin(), VertexCode.end());
            const uint32_t pgmLo =
                static_cast<uint32_t>((kVsVaddr >> 8) & 0xFFFFFFFFu);
            // SPI_SHADER_PGM_LO/HI_VS at SH offsets 0x48/0x49 (Kyty stride,
            // shadPS4 PS4 cross-check), then RSRC1/RSRC2 at 0x4A/0x4B with
            // USER_SGPR=2 and the matching 2-dword user-data window at 0x4C.
            Append(stream, SetShReg(PX5::Gnm::Sh::kVsBase +
                                        PX5::Gnm::Sh::kOffPgmLo,
                                    {pgmLo, 0u}));
            Append(stream, SetShReg(PX5::Gnm::Sh::kVsBase +
                                        PX5::Gnm::Sh::kOffRsrc1,
                                    {0u, 2u << 1}));
            Append(stream, SetShReg(PX5::Gnm::Sh::kVsBase +
                                        PX5::Gnm::Sh::kOffUserData0,
                                    {0x0BADF00Du, 0x00040010u}));
            // A draw after the program: the realistic graphics stream.
            // DRAW_INDEX_AUTO body = {count, initiator}.
            stream.push_back(Type3Header::Encode(PX5::Gnm::kItDrawIndexAuto, 2));
            stream.push_back(6u);
            stream.push_back(2u);
            DecodeClean(stream, state);
            Require(state.DrawCalls() == 1u, "vertex stream lost its draw");

            const ShaderCaptureResult captured =
                ShaderCapture::ExtractShader(state, GuestShaderStage::Vertex);
            Require(captured.hasProgram, "vertex: PGM pair not detected");
            Require(captured.codeVaddr == kVsVaddr, "vertex: wrong vaddr");
            Require(captured.declaredUserSgprs == 2u,
                    "vertex: RSRC2.USER_SGPR not decoded");
            Require(captured.userData.size() == 2u,
                    "vertex: user-data window wrong");
            Require(!captured.wave32,
                    "vertex: PS5 graphics default must be wave64");

            std::vector<uint32_t> spirv;
            std::string error;
            Require(ShaderCapture::CaptureAndRecompile(
                        state, GuestShaderStage::Vertex, reader, spirv, error),
                    "vertex capture recompile failed: " + error);
            CheckModule(spirv, ExecutionModelVertex, "vertex T4");
        }

        // ---- fragment: PS block + context interpolator count ------------
        {
            GnmState state;
            std::vector<uint32_t> stream;
            constexpr uint64_t kPsVaddr = 0x33000000ull;  // 256B aligned
            mem[kPsVaddr] =
                std::vector<uint32_t>(FragmentCode.begin(), FragmentCode.end());
            const uint32_t pgmLo =
                static_cast<uint32_t>((kPsVaddr >> 8) & 0xFFFFFFFFu);
            Append(stream, SetShReg(PX5::Gnm::Sh::kPsBase +
                                        PX5::Gnm::Sh::kOffPgmLo,
                                    {pgmLo, 0u}));
            // SPI_PS_IN_CONTROL_0 (context offset 0x1B6): NUM_INTERP=1 —
            // the fixture reads parameter 0 only.
            Append(stream, SetReg(PX5::Gnm::kItSetContextReg,
                                  PX5::Gnm::Sh::kCtxOffSpiPsInControl0, {1u}));
            DecodeClean(stream, state);

            const ShaderCaptureResult captured = ShaderCapture::ExtractShader(
                state, GuestShaderStage::Fragment);
            Require(captured.hasProgram, "fragment: PGM pair not detected");
            Require(captured.codeVaddr == kPsVaddr, "fragment: wrong vaddr");
            Require(captured.fragmentInterpolators == 1u,
                    "fragment: SPI_PS_IN_CONTROL_0.NUM_INTERP not captured");
            Require(captured.userData.empty(),
                    "fragment: undeclared user data must stay empty");

            std::vector<uint32_t> spirv;
            std::string error;
            Require(ShaderCapture::CaptureAndRecompile(
                        state, GuestShaderStage::Fragment, reader, spirv,
                        error),
                    "fragment capture recompile failed: " + error);
            CheckModule(spirv, ExecutionModelFragment, "fragment T4");
        }

        // ---- failure honesty: no program / unmapped code / empty roots ---
        {
            GnmState state;  // nothing decoded
            const ShaderCaptureResult captured = ShaderCapture::ExtractShader(
                state, GuestShaderStage::Compute);
            Require(!captured.hasProgram,
                    "empty state must not report a program");

            std::vector<uint32_t> spirv;
            std::string error;
            Require(!ShaderCapture::CaptureAndRecompile(
                        state, GuestShaderStage::Compute, reader, spirv,
                        error),
                    "capture without a program must fail");
            Require(error.find("no program loaded") != std::string::npos,
                    "no-program failure must be named, got: " + error);
            Require(spirv.empty(), "failed capture must not return a module");
        }
        {
            GnmState state;
            // PGM pair pointing outside the memory map: named failure.
            const std::vector<uint32_t> stream =
                BuildComputeStream(9u, 0x999999000ull);
            DecodeClean(stream, state);
            std::vector<uint32_t> spirv;
            std::string error;
            Require(!ShaderCapture::CaptureAndRecompile(
                        state, GuestShaderStage::Compute, reader, spirv,
                        error),
                    "capture with unmapped code must fail");
            Require(error.find("code unreadable") != std::string::npos,
                    "unreadable-code failure must be named, got: " + error);
        }
        {
            GnmState state;
            // Declared zero roots: the service's clear workgroup-ABI
            // rejection must carry through the capture path (counted).
            const std::vector<uint32_t> stream =
                BuildComputeStream(0u, kComputeVaddr);
            DecodeClean(stream, state);
            std::vector<uint32_t> spirv;
            std::string error;
            const uint64_t failuresBefore = service.Stats().failures;
            Require(!ShaderCapture::CaptureAndRecompile(
                        state, GuestShaderStage::Compute, reader, spirv,
                        error),
                    "compute with zero declared roots must fail");
            Require(error.find("user data") != std::string::npos,
                    "zero-roots failure must name the ABI, got: " + error);
            Require(service.Stats().failures == failuresBefore + 1u,
                    "capture-path rejection was not counted");
        }

        std::printf("M8 T4: guest shader capture gates passed\n");
        return 0;
    } catch (const std::exception& e) {
        std::printf("M8 T4 FAILED: %s\n", e.what());
        return 1;
    }
}
