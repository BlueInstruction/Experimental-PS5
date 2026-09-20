// SPDX-License-Identifier: MIT
// M6 gate stream fixture — the SINGLE source of truth shared by the host
// tests that exercise the M6 lowering output.
//
// gpu_ir_test.cpp (M6) locks the decode+lower of this stream op by op;
// vulkan_backend_test.cpp (M7-T1) plans the SAME lowered op list into
// Vulkan commands. One fixture, two gates: when the fixture changes, both
// gates change with it — a copied-by-hand variant would let one gate drift
// stale while the other passes (review finding on PR #6).
//
// Included by both test TUs via the quote-include rule (same directory);
// no build-system change needed. Platform-independent, no Vulkan.

#ifndef PX5_HOSTTESTS_M6_GATE_FIXTURE_H
#define PX5_HOSTTESTS_M6_GATE_FIXTURE_H

#include <cstdint>
#include <initializer_list>
#include <vector>

#include "gpu/gnm/pm4_decoder.h"
#include "gpu/gnm/pm4_packet.h"
#include "gpu/ir/gpu_ir.h"

namespace px5test {

// ---- fixture constants (single source of truth) ---------------------------
constexpr uint32_t kScTL = 0x00050002u;   // TL: x=2, y=5
constexpr uint32_t kScBR = 0x01400100u;   // BR: x=256, y=320
constexpr uint32_t kConfigA = 0x11110001u;
constexpr uint32_t kShA = 0x33330001u, kShB = 0x33330002u, kShC = 0x33330003u;
constexpr uint32_t kIndexTypeRaw = 0x2u;
constexpr uint32_t kInstances = 4u;
constexpr uint32_t kAutoCount = 36u;
constexpr uint32_t kInitiator = 0x6u;
constexpr uint32_t kDi2Count = 300u;
constexpr uint32_t kDispX = 8u, kDispY = 4u, kDispZ = 2u;

// The M6 gate stream: set state, then draw twice, then dispatch. Positions
// are asserted by both consumers, so keep this the single source of truth
// and derive from it — never re-copy it into a test body.
inline std::vector<uint32_t> BuildM6GateStream() {
    using PX5::Gnm::kItDrawIndexAuto;
    using PX5::Gnm::kItDrawIndex2;
    using PX5::Gnm::kItDispatchDirect;
    using PX5::Gnm::kItIndexType;
    using PX5::Gnm::kItNop;
    using PX5::Gnm::kItNumInstances;
    using PX5::Gnm::kItSetConfigReg;
    using PX5::Gnm::kItSetContextReg;
    using PX5::Gnm::kItSetShReg;
    using PX5::Gnm::kCtxOffPaScScreenScissorTL;
    using PX5::Gnm::kCtxOffPaScScreenScissorBR;
    using PX5::Gnm::Type3Header;

    std::vector<uint32_t> s;
    auto push = [&](uint32_t op, uint32_t bodyCount,
                    std::initializer_list<uint32_t> body,
                    uint32_t shaderType = 0) {
        s.push_back(Type3Header::Encode(op, bodyCount, shaderType));
        for (uint32_t d : body) s.push_back(d);
    };
    // 0: NOP                                  -> no op
    push(kItNop, 1, {0x0});
    // 1: SET_CONTEXT_REG scissor TL (ctx off 0xC)  -> pending box, no op
    push(kItSetContextReg, 2, {kCtxOffPaScScreenScissorTL, kScTL});
    // 2: SET_CONTEXT_REG scissor BR (ctx off 0xD)  -> SetScissor op
    push(kItSetContextReg, 2, {kCtxOffPaScScreenScissorBR, kScBR});
    // 3: SET_CONFIG_REG offset 0x10, one value     -> unmapped write
    push(kItSetConfigReg, 2, {0x10, kConfigA});
    // 4: SET_SH_REG offset 0x0C, three values      -> 3 unmapped writes
    push(kItSetShReg, 4, {0x0C, kShA, kShB, kShC});
    // 5: INDEX_TYPE                                -> carried write
    push(kItIndexType, 1, {kIndexTypeRaw});
    // 6: NUM_INSTANCES                             -> state only, no reg write
    push(kItNumInstances, 1, {kInstances});
    // 7: DRAW_INDEX_AUTO                           -> Draw op
    push(kItDrawIndexAuto, 2, {kAutoCount, kInitiator});
    // 8: DRAW_INDEX_2 (public 5-dword body)        -> DrawIndexed op
    push(kItDrawIndex2, 5, {0x1FF, 0x0, 0x1000, kDi2Count, kInitiator});
    // 9: DISPATCH_DIRECT, compute                  -> Dispatch op
    push(kItDispatchDirect, 3, {kDispX, kDispY, kDispZ}, /*shaderType=*/1);
    return s;
}

} // namespace px5test

#endif // PX5_HOSTTESTS_M6_GATE_FIXTURE_H
