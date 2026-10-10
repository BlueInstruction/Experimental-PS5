// SPDX-License-Identifier: MIT
// PX5 M8 T4 — PS5 SH-space register map for shader capture.
//
// WHAT THIS IS
//   The packet-relative register offsets (SET_SH_REG body[0] values) at
//   which a PS5 guest programs its shader stages, plus the few bit fields
//   the capture layer needs. These are the addresses games reach through
//   sceGnmSetPsShader / SetComputeShader / driver-generated SET_SH packets.
//
// SOURCES (honesty rule: no invented constants — every value below was
//   read from at least one real emulator repo, and the load-bearing ones
//   from two independent PS5 emulators):
//   * KytyPS5 src/graphics/guest_gpu/pm4.h (PS5): SPI_SHADER_PGM_LO_PS=0x8,
//     SPI_SHADER_PGM_HI_PS=0x9, PGM_RSRC1_PS=0xA, RSRC2_PS=0xB,
//     SPI_SHADER_USER_DATA_PS_0=0xC..PS_31=0x2B (32 entries), GS block
//     PGM_LO=0x88/RSRC1=0x8A, ES PGM_LO=0xC8, HS PGM_LO=0x108/RSRC1=0x10A,
//     LS PGM_LO=0x148, COMPUTE_NUM_THREAD_X/Y/Z=0x207..0x209,
//     COMPUTE_PGM_LO=0x20C, HI=0x20D, RSRC1=0x212, RSRC2=0x213,
//     COMPUTE_USER_DATA_0..15=0x240..0x24F (16 entries),
//     RSRC2.USER_SGPR[5:1] + USER_SGPR_MSB[27],
//     RSRC2.TGID_X/Y/Z_EN[9:7] + TG_SIZE_EN[10] + TIDIG_COMP_CNT[12:11],
//     DISPATCH initiator CS_W32_EN[15] (ComputeWaveSize helper).
//   * AnyPS5 core/libs/prx/libSceAgc/Shader/include/ShaderConstants.hpp
//     (PS5, independent emulator, identical values): PGM_LO_PS=0x008,
//     GS=0x088, ES=0x0C8, HS=0x108, LS=0x148, COMPUTE_PGM_LO=0x20C,
//     context anchors SPI_PS_INPUT_CNTL_0=0x191, SPI_VS_OUT_CONFIG=0x1B1.
//   * shadPS4 (PS4, same GCN register lineage): PM4CmdDispatchDirect
//     {dim_x,dim_y,dim_z,dispatch_initiator} — the real DISPATCH packet
//     body is FOUR dwords; gnmdriver writes SPI_SHADER_PGM_LO_VS at SH
//     offset 0x48 and RSRC1_VS at 0x4A (matches the 0x40-per-stage stride
//     Kyty's PS blocks establish); ShaderProgram::Address() = 40-bit
//     (LO|HI[7:0]<<32) << 8 — the PGM pair encodes a 256B-aligned address
//     with HI[31:8] junk; context block ps_inputs[32]@0x191 + VS_OUT_CONFIG
//     + pads puts SPI_PS_IN_CONTROL_0 (num_interp[5:0]) at offset 0x1B6.
//
// WHAT THIS IS NOT
//   Not a full GPU register database: only the registers the shader
//   capture path consumes today. Everything else stays raw-tracked in the
//   GnmState banks.
//
// Platform-independent C++ (host-testable and APK-compilable).

#ifndef PX5_GPU_GNM_SH_REGISTERS_H
#define PX5_GPU_GNM_SH_REGISTERS_H

#include <cstdint>

#include "gpu/gnm/pm4_packet.h"

namespace PX5::Gnm::Sh {

// Stage block bases (packet offsets inside the SH space). The graphics
// stages sit on a 0x40-dword stride; compute has its own block at 0x200.
constexpr uint32_t kPsBase = 0x000;
constexpr uint32_t kVsBase = 0x040;
constexpr uint32_t kGsBase = 0x080;
constexpr uint32_t kEsBase = 0x0C0;
constexpr uint32_t kHsBase = 0x100;
constexpr uint32_t kLsBase = 0x140;
constexpr uint32_t kCsBase = 0x200;

// Per-graphics-stage block layout (base + these):
constexpr uint32_t kOffPgmLo   = 0x08;  // SPI_SHADER_PGM_LO_xx
constexpr uint32_t kOffPgmHi   = 0x09;  // SPI_SHADER_PGM_HI_xx
constexpr uint32_t kOffRsrc1   = 0x0A;  // SPI_SHADER_PGM_RSRC1_xx
constexpr uint32_t kOffRsrc2   = 0x0B;  // SPI_SHADER_PGM_RSRC2_xx
constexpr uint32_t kOffUserData0 = 0x0C;  // SPI_SHADER_USER_DATA_xx_0
// PS5 widens the graphics user-data window to 32 SGPRs
// (Kyty: SPI_SHADER_USER_DATA_PS_31 = 0x2B = base+0x1F).
constexpr uint32_t kGfxUserDataCount = 32;

// Compute block (absolute SH packet offsets — NOT base-relative; the
// compute block layout differs from the graphics stride):
constexpr uint32_t kComputeNumThreadX   = 0x207;
constexpr uint32_t kComputeNumThreadY   = 0x208;
constexpr uint32_t kComputeNumThreadZ   = 0x209;
constexpr uint32_t kComputePgmLo        = 0x20C;
constexpr uint32_t kComputePgmHi        = 0x20D;
constexpr uint32_t kComputeRsrc1        = 0x212;
constexpr uint32_t kComputeRsrc2        = 0x213;
constexpr uint32_t kComputeUserData0    = 0x240;
// Kyty: COMPUTE_USER_DATA_15 = 0x24F — the compute window is 16 SGPRs.
constexpr uint32_t kComputeUserDataCount = 16;

// SPI_SHADER_PGM_RSRC2 (all stages) bit fields (Kyty):
inline uint32_t Rsrc2UserSgprs(uint32_t rsrc2) {
    return ((rsrc2 >> 1) & 0x1Fu) | (((rsrc2 >> 27) & 0x1u) << 5);
}

// COMPUTE_PGM_RSRC2 bit fields (Kyty) — exposed for future group-id
// plumbing; the service currently enables group ids unconditionally.
inline bool Rsrc2TgidXEn(uint32_t rsrc2) { return ((rsrc2 >> 7) & 0x1u) != 0u; }
inline bool Rsrc2TgidYEn(uint32_t rsrc2) { return ((rsrc2 >> 8) & 0x1u) != 0u; }
inline bool Rsrc2TgidZEn(uint32_t rsrc2) { return ((rsrc2 >> 9) & 0x1u) != 0u; }
inline bool Rsrc2TgSizeEn(uint32_t rsrc2) { return ((rsrc2 >> 10) & 0x1u) != 0u; }

// DISPATCH packet initiator (body[3]) bit fields (Kyty ComputeWaveSize):
// bit 15 = CS_W32_EN -> 32-wide waves when set, 64-wide otherwise.
inline bool ComputeWave32(uint32_t dispatchInitiator) {
    return ((dispatchInitiator >> 15) & 0x1u) != 0u;
}

// PGM_LO/HI pair -> guest virtual address. shadPS4 ShaderProgram: the
// 40-bit address field packs LO[31:0] | HI[7:0]<<32 and Address() shifts
// left 8 — i.e. a 256B-aligned address whose HI dwords's upper bits are
// junk. LO<<8 discards LO's low 8 bits by construction.
inline uint64_t PgmAddress(uint32_t pgmLo, uint32_t pgmHi) {
    return (static_cast<uint64_t>(pgmHi & 0xFFu) << 40) |
           (static_cast<uint64_t>(pgmLo) << 8);
}

// Context-space register: SPI_PS_IN_CONTROL_0 (Kyty/shadPS4-derived
// offset 0x1B6; see header SOURCES) holds num_interp[5:0] — the fragment
// interpolator count the guest declares.
constexpr uint32_t kCtxOffSpiPsInControl0 = 0x1B6;
constexpr uint32_t kRegSpiPsInControl0 =
    kContextRegBase + kCtxOffSpiPsInControl0;
inline uint32_t NumInterpolators(uint32_t spiPsInControl0) {
    return spiPsInControl0 & 0x3Fu;
}

} // namespace PX5::Gnm::Sh

#endif // PX5_GPU_GNM_SH_REGISTERS_H
