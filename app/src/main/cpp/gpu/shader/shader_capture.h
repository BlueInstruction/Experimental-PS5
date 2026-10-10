// SPDX-License-Identifier: MIT
// PX5 M8 T4 — guest shader capture from PM4 SET_SH register state.
//
// WHAT THIS IS
//   The bridge between the PM4 decoder's register banks (GnmState) and the
//   M8 T3 shader recompiler service. After a command buffer decodes, the
//   SH banks hold exactly what the guest programmed: PGM_LO/HI (code
//   address), RSRC2 (declared user-SGPR count), NUM_THREAD (compute), and
//   the user-data window — plus the DISPATCH packet's initiator (wave
//   size) and the context-space SPI_PS_IN_CONTROL_0 interpolator count.
//   ExtractShader reads those, trims the user data to the declared count,
//   and CaptureAndRecompile pulls the code words from guest memory through
//   a caller-provided reader and runs the service.
//
//   Register sources are cross-verified in sh_registers.h (Kyty + AnyPS5
//   for PS5; shadPS4 for the PGM address packing and the DISPATCH packet
//   shape). No invented constants.
//
// LAYER CONTRACT (docs/gpu.md chain)
//   This TU includes ONLY the app-side service interface
//   (shader_recompiler_service.h) — never the vendored recompiler headers.
//   shader_recompiler_service.cpp remains the only TU that does. It never
//   names a Vulkan type; VkShaderModule creation is M8 T5.
//
// HONESTY RULES
//   * No program loaded (PGM pair never written / all zero) -> false with
//     a named reason, never a guessed module.
//   * Code words are read through the caller's MemoryReader; an unmapped
//     or short program -> false with the vaddr in the error.
//   * The guest's declared RSRC2.USER_SGPR count trims the user-data
//     window (root registers the shader never reads are not passed on).
//     A zero declaration passes an empty window — the service then
//     rejects compute with its clear workgroup-ABI error (M8 T3
//     behavior, exactly as probed).
//   * Graphics stages run wave64 (PS5 default). The per-stage graphics
//     wave-size control has no Kyty/AnyPS5-verified bit today; the
//     compute wave size (DISPATCH initiator bit 15) is verified.
//
// Platform-independent C++ (host-testable and APK-compilable).

#ifndef PX5_GPU_SHADER_SHADER_CAPTURE_H
#define PX5_GPU_SHADER_SHADER_CAPTURE_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "gpu/shader/shader_recompiler_service.h"

namespace PX5::Gnm {
class GnmState;
} // namespace PX5::Gnm

namespace PX5::Gpu {

using PX5::Gnm::GnmState;   // forward-declared; defined in gpu/gnm/gnm_state.h

struct ShaderCaptureResult {
    bool hasProgram = false;          // PGM pair written and non-zero
    GuestShaderStage stage = GuestShaderStage::Compute;
    uint64_t codeVaddr = 0;           // PGM_HI:LO decoded, 256B-aligned
    std::vector<uint32_t> userData;   // trimmed to RSRC2.USER_SGPR when set
    uint32_t threadsX = 0, threadsY = 0, threadsZ = 0;  // compute, as written
    bool wave32 = false;              // compute: DISPATCH initiator bit 15
    uint32_t declaredUserSgprs = 0;   // RSRC2.USER_SGPR + MSB
    uint32_t fragmentInterpolators = 0;  // SPI_PS_IN_CONTROL_0.NUM_INTERP
};

class ShaderCapture {
public:
    // Guest-memory provider. Returns true and fills `out` with up to
    // `maxDwords` code words on success. On device this maps through the
    // MemoryManager; host tests inject a map-backed reader.
    using MemoryReader =
        std::function<bool(uint64_t vaddr, uint32_t maxDwords,
                           std::vector<uint32_t>& out)>;

    // Bounded read window. Real programs end at s_n (the recompiler's
    // decoder stops there and rejects code boundaries reached before it),
    // so over-reading a window is safe; 16 KiB covers every observed
    // guest shader comfortably.
    static constexpr uint32_t kMaxCodeDwords = 4096;

    // Reads the stage's register block out of the decoded GnmState. Never
    // throws; !hasProgram carries a named reason in `out.error`-style
    // strings returned by CaptureAndRecompile (extraction itself reports
    // through hasProgram + the caller's inspection of the fields).
    static ShaderCaptureResult ExtractShader(const GnmState& state,
                                             GuestShaderStage stage);

    // Extract + read code + recompile through GuestShaderRecompiler.
    // Returns false with a named `error` on every failure path (no
    // program, unreadable code, recompiler rejection); stats land in the
    // service's counters either way.
    static bool CaptureAndRecompile(const GnmState& state,
                                    GuestShaderStage stage,
                                    const MemoryReader& read,
                                    std::vector<uint32_t>& spirv,
                                    std::string& error);
};

} // namespace PX5::Gpu

#endif // PX5_GPU_SHADER_SHADER_CAPTURE_H
