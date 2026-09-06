// SPDX-License-Identifier: MIT
// PX5 Phase C milestone 1 — PM4 packet primitives (real, host-testable).
//
// WHAT THIS IS
//   GNM games never call the driver through an interceptable API: the GNM
//   libraries are statically linked into the game executable and the only
//   thing that reaches the emulated OS is the PM4 command buffer handed to
//   sceGnmSubmitCommandBuffers (fact confirmed by the public GPCS4 design
//   doc the user supplied). Everything GPU-side therefore starts with a
//   decoder for those buffers. This header defines the Type-3 packet
//   header layout and the publicly known GCN-lineage IT opcode table.
//
// SOURCES (honesty rule: no invented constants)
//   * Header bit layout: public reverse-engineering consensus shared by
//     GPCS4 / shadPS4 / Kyty — count[14:0], opcode[25:16] (10 bits),
//     predicate[28:26], shaderType[29], type[31:30]. Sony documents none
//     of this under NDA; the table below is what the public emulators
//     agree on.
//   * IT opcodes: the stable GCN-era table mirrored identically in the
//     Linux amdgpu driver, Mesa (src/amd/common/sid.h), GPCS4 and shadPS4.
//     PS5 (RDNA2) may extend it; anything not in this table is REPORTED
//     as unknown (counted + logged), never guessed.
//
// The whole module is platform-independent C++ (no Android headers) so it
// can be unit-run on the host AND inside both APK ABIs.

#ifndef PX5_GPU_GNM_PM4_PACKET_H
#define PX5_GPU_GNM_PM4_PACKET_H

#include <cstdint>

namespace PX5::Gnm {

// ---------------------------------------------------------------------------
// Type-3 (IT) packet header — the only packet class GNM streams use in
// practice. A Type-3 packet = header dword + (count+1) body dwords.
// ---------------------------------------------------------------------------
struct Type3Header {
    uint32_t raw = 0;

    static constexpr uint32_t kType3 = 3;

    constexpr Type3Header() = default;
    explicit constexpr Type3Header(uint32_t dword) : raw(dword) {}

    // Static encoder used by the self-test to synthesize packets.
    static constexpr uint32_t Encode(uint32_t opcode, uint32_t bodyDwordCount,
                                     uint32_t shaderType = 0,
                                     uint32_t predicate = 0) {
        // bodyDwordCount is the wire count field = (number of body dwords - 1)
        const uint32_t wireCount = (bodyDwordCount == 0) ? 0 : bodyDwordCount - 1;
        return kType3 << 30                       // [31:30] type
               | (shaderType & 0x1u) << 29        // [29]    0=gfx 1=compute
               | (predicate & 0x7u) << 26         // [28:26] predicate
               | (opcode & 0x3FFu) << 16          // [25:16] IT opcode
               | (wireCount & 0x7FFFu);           // [14:0]  count
    }

    constexpr uint32_t Type()       const { return (raw >> 30) & 0x3u; }
    constexpr uint32_t ShaderType() const { return (raw >> 29) & 0x1u; }
    constexpr uint32_t Predicate()  const { return (raw >> 26) & 0x7u; }
    constexpr uint32_t Opcode()     const { return (raw >> 16) & 0x3FFu; }
    // Wire count field: number of body dwords minus one (0 => 1 body dword).
    constexpr uint32_t CountField() const { return raw & 0x7FFFu; }
    constexpr uint32_t BodyDwords() const { return CountField() + 1; }
};

// ---------------------------------------------------------------------------
// Publicly known GCN-lineage IT opcodes (see SOURCES above).
// Values are uint32_t so unknown opcodes can share the same namespace.
// ---------------------------------------------------------------------------
enum ItOpcode : uint32_t {
    kItUnknown = 0,

    kItNop                = 0x10,
    kItClearState         = 0x12,
    kItDispatchDirect     = 0x15,
    kItDispatchIndirect   = 0x16,
    kItIndirectBufferEnd  = 0x17,
    kItContextControl     = 0x28,
    kItIndexType          = 0x2A,
    kItDrawIndexAuto      = 0x2D,
    kItDrawIndex2         = 0x2E,
    kItNumInstances       = 0x2F,
    kItWaitRegMem         = 0x3C,
    kItIndirectBuffer     = 0x3F,
    kItEventWrite         = 0x46,
    kItEventWriteEop      = 0x49,
    kItEventWriteEos      = 0x4A,
    kItDmaData            = 0x50,
    kItAcquireMem         = 0x58,
    kItSetConfigReg       = 0x68,
    kItSetContextReg      = 0x69,
    kItSetUConfigReg      = 0x6D,
    kItSetShReg           = 0x76,
    kItSetShRegOffset     = 0x77,
};

const char* ItOpcodeName(uint32_t opcode);

// ---------------------------------------------------------------------------
// Register spaces (public: Mesa sid.h + shadPS4 Liverpool; identical in
// every PS4 RE project). Packet bodies address REGISTERS AS OFFSETS INSIDE
// these spaces; GnmState stores them at the full MMIO address below.
//   config   : SET_CONFIG_REG   base 0x2000
//   uconfig  : SET_UCONFIG_REG  base 0x3000
//   SH       : SET_SH_REG       base 0x2C00 (graphics+compute banks)
//   context  : SET_CONTEXT_REG  base 0x28000 (packet offsets are the
//                                      0xA000-space values games write)
// ---------------------------------------------------------------------------
constexpr uint32_t kConfigRegBase  = 0x2000;
constexpr uint32_t kUConfigRegBase = 0x3000;
constexpr uint32_t kShRegBase      = 0x2C00;
constexpr uint32_t kContextRegBase = 0x28000;

// A couple of high-confidence named registers the state model decodes
// beyond raw storage (all context-space addresses, public GCN names).
constexpr uint32_t kRegVgtDmaIndexType = 0x28A7C;  // INDEX_TYPE writes here

// Screen scissor pair (context space). SOURCES (same bar as above — public
// RE consensus, no invented constants): KytyPS5 (PS5, this repo's cited M8
// research reference) defines PA_SC_SCREEN_SCISSOR_TL/BR at context packet
// offsets 0xC/0xD with X[15:0] / Y[31:16] per dword; RPCSX (PS4) names the
// same pair at its 0xA00C/0xA00D (0xA000-space + 0xC/0xD). Absolute
// addresses follow this file's convention: kContextRegBase + packet offset.
constexpr uint32_t kCtxOffPaScScreenScissorTL = 0xC;
constexpr uint32_t kCtxOffPaScScreenScissorBR = 0xD;
constexpr uint32_t kRegPaScScreenScissorTL = kContextRegBase + kCtxOffPaScScreenScissorTL;
constexpr uint32_t kRegPaScScreenScissorBR = kContextRegBase + kCtxOffPaScScreenScissorBR;

// Known high-confidence context-space packet offsets (what SET_CONTEXT_REG
// body[0] holds for the register above).
constexpr uint32_t kCtxOffVgtDmaIndexType = kRegVgtDmaIndexType - kContextRegBase;

} // namespace PX5::Gnm

#endif // PX5_GPU_GNM_PM4_PACKET_H
