// SPDX-License-Identifier: MIT
// PX5 M8 T4 — guest shader capture from PM4 SET_SH register state
// (implementation). See shader_capture.h for the contract, sources, and
// honesty rules.

#include "gpu/shader/shader_capture.h"

#include "gpu/gnm/gnm_state.h"
#include "gpu/gnm/pm4_packet.h"
#include "gpu/gnm/sh_registers.h"

namespace PX5::Gpu {

namespace {

using PX5::Gnm::kContextRegBase;
using PX5::Gnm::kShRegBase;

// Reads one SH register (packet offset -> absolute 0x2C00+off), reporting
// whether the stream ever wrote it.
uint32_t ReadSh(const GnmState& state, uint32_t packetOffset, bool* wasWritten) {
    return state.ReadRegister(kShRegBase + packetOffset, wasWritten);
}

// Fills the user-data window for a graphics stage (32 SGPRs) or compute
// (16 SGPRs), trimmed to the guest's declared root count (RSRC2.USER_SGPR
// + MSB). The declaration is authoritative: USER_SGPR=0 means the program
// reads no user-data SGPRs, so the honest capture is an EMPTY window
// (registers the program never reads must not shape the recompile or the
// cache key); a declaration past the hardware window is a malformed
// stream and clamps to the window.
void ReadUserData(const GnmState& state, uint32_t firstOffset,
                  uint32_t count, uint32_t declared,
                  std::vector<uint32_t>& out) {
    out.clear();
    uint32_t keep = (declared < count) ? declared : count;
    out.reserve(keep);
    for (uint32_t i = 0; i < keep; ++i) {
        out.push_back(ReadSh(state, firstOffset + i, nullptr));
    }
}

// Trailing-zero trim: the code window may extend past the program (the
// reader returns what is mapped up to kMaxCodeDwords). Valid RDNA programs
// terminate at s_n and zero dwords never decode, so trimming them keeps
// the service's cache key tied to the program, not to the mapping shape.
void TrimTrailingZeros(std::vector<uint32_t>& code) {
    while (!code.empty() && code.back() == 0u) {
        code.pop_back();
    }
}

} // namespace

ShaderCaptureResult ShaderCapture::ExtractShader(const GnmState& state,
                                                 GuestShaderStage stage) {
    ShaderCaptureResult out;
    out.stage = stage;

    bool loWritten = false;
    bool hiWritten = false;
    uint32_t pgmLo = 0;
    uint32_t pgmHi = 0;

    if (stage == GuestShaderStage::Compute) {
        // Threads are read as the stream wrote them (zero when the stream
        // never set them — a named-shape input, not a guessed default).
        out.threadsX = ReadSh(state, PX5::Gnm::Sh::kComputeNumThreadX, nullptr);
        out.threadsY = ReadSh(state, PX5::Gnm::Sh::kComputeNumThreadY, nullptr);
        out.threadsZ = ReadSh(state, PX5::Gnm::Sh::kComputeNumThreadZ, nullptr);
        pgmLo = ReadSh(state, PX5::Gnm::Sh::kComputePgmLo, &loWritten);
        pgmHi = ReadSh(state, PX5::Gnm::Sh::kComputePgmHi, &hiWritten);
        const uint32_t rsrc2 = ReadSh(state, PX5::Gnm::Sh::kComputeRsrc2, nullptr);
        out.declaredUserSgprs = PX5::Gnm::Sh::Rsrc2UserSgprs(rsrc2);
        ReadUserData(state, PX5::Gnm::Sh::kComputeUserData0,
                     PX5::Gnm::Sh::kComputeUserDataCount,
                     out.declaredUserSgprs, out.userData);
        // Kyty ComputeWaveSize: DISPATCH packet initiator bit 15.
        out.wave32 = PX5::Gnm::Sh::ComputeWave32(state.LastDispatchInitiator());
    } else {
        const uint32_t base = (stage == GuestShaderStage::Vertex)
                                  ? PX5::Gnm::Sh::kVsBase
                                  : PX5::Gnm::Sh::kPsBase;
        pgmLo = ReadSh(state, base + PX5::Gnm::Sh::kOffPgmLo, &loWritten);
        pgmHi = ReadSh(state, base + PX5::Gnm::Sh::kOffPgmHi, &hiWritten);
        const uint32_t rsrc2 = ReadSh(state, base + PX5::Gnm::Sh::kOffRsrc2,
                                      nullptr);
        out.declaredUserSgprs = PX5::Gnm::Sh::Rsrc2UserSgprs(rsrc2);
        ReadUserData(state, base + PX5::Gnm::Sh::kOffUserData0,
                     PX5::Gnm::Sh::kGfxUserDataCount,
                     out.declaredUserSgprs, out.userData);
        // PS5 graphics default: wave64. No verified per-stage W32 bit yet
        // (see header honesty rules).
        out.wave32 = false;
        if (stage == GuestShaderStage::Fragment) {
            const uint32_t ctrl = state.ReadRegister(
                PX5::Gnm::Sh::kRegSpiPsInControl0, nullptr);
            out.fragmentInterpolators = PX5::Gnm::Sh::NumInterpolators(ctrl);
        }
    }

    out.hasProgram = (loWritten || hiWritten) && (pgmLo != 0u || pgmHi != 0u);
    if (out.hasProgram) {
        out.codeVaddr = PX5::Gnm::Sh::PgmAddress(pgmLo, pgmHi);
    }
    return out;
}

bool ShaderCapture::CaptureAndRecompile(const GnmState& state,
                                        GuestShaderStage stage,
                                        const MemoryReader& read,
                                        std::vector<uint32_t>& spirv,
                                        std::string& error) {
    spirv.clear();
    error.clear();

    if (!read) {
        error = "shader capture: no memory reader installed";
        return false;
    }

    const ShaderCaptureResult captured = ExtractShader(state, stage);
    if (!captured.hasProgram) {
        error = "shader capture: no program loaded (PGM pair never written)";
        return false;
    }

    std::vector<uint32_t> code;
    if (!read(captured.codeVaddr, kMaxCodeDwords, code) || code.empty()) {
        error = "shader capture: code unreadable @0x" +
                [](uint64_t v) {
                    char b[32];
                    std::snprintf(b, sizeof(b), "%llX",
                                  static_cast<unsigned long long>(v));
                    return std::string(b);
                }(captured.codeVaddr);
        return false;
    }
    TrimTrailingZeros(code);

    GuestShaderMeta meta;
    meta.stage = stage;
    meta.code = code.data();
    meta.codeDwords = static_cast<uint32_t>(code.size());
    meta.userData = captured.userData;
    meta.threadsX = captured.threadsX;
    meta.threadsY = captured.threadsY;
    meta.threadsZ = captured.threadsZ;
    meta.wave32 = captured.wave32;
    meta.fragmentInterpolators = captured.fragmentInterpolators;

    return GuestShaderRecompiler::Instance().Recompile(meta, spirv, error);
}

} // namespace PX5::Gpu
