// SPDX-License-Identifier: MIT
// M6 gate — GnmState + draw records lower to the exact expected IR op list.
//
// docs/milestones.md M6 evidence gate: "GnmState + draw records lower to a
// committed IR op list (SetRenderTarget … Barrier) with a lower-to-IR host
// test." This file IS that gate. It is deterministic: same state, same op
// list, same verdict, every run. Sections:
//   1. the fixed stream — decode + lower, every op (kind, order, payload,
//      provenance seq/packetOffset) asserted against the fixture;
//   2. interleaving — draws, dispatches and a mid-stream scissor change
//      keep the guest's event order in the op list;
//   3. no-Vulkan guard — a compile-time contract lives in gpu_ir.h
//      (#error on Vulkan headers); this section locks the runtime honesty
//      counters instead: named/carried/unmapped register-write accounting;
//   4. bounded list — capacity rejections are counted, never silent;
//   5. Reset — an emptied state lowers to an empty list, no barrier.
//
// Platform-independent: builds with tools/hosttests/run.sh, no device.

#include <cstdio>
#include <string>
#include <vector>

#include "gpu/gnm/pm4_decoder.h"
#include "gpu/gnm/pm4_packet.h"
#include "gpu/ir/gpu_ir.h"

using PX5::Gpu::GpuOp;
using PX5::Gpu::GpuOpList;
using PX5::Gpu::LowerGnmStateToIR;
using PX5::Gpu::LowerStats;
using PX5::Gpu::OpKind;
using PX5::Gpu::OpKindName;
using PX5::Gnm::DecodeStats;
using PX5::Gnm::GnmState;
using PX5::Gnm::kContextRegBase;
using PX5::Gnm::kCtxOffPaScScreenScissorBR;
using PX5::Gnm::kCtxOffPaScScreenScissorTL;
using PX5::Gnm::kItDrawIndexAuto;
using PX5::Gnm::kItDrawIndex2;
using PX5::Gnm::kItDispatchDirect;
using PX5::Gnm::kItIndexType;
using PX5::Gnm::kItNop;
using PX5::Gnm::kItNumInstances;
using PX5::Gnm::kItSetConfigReg;
using PX5::Gnm::kItSetContextReg;
using PX5::Gnm::kItSetShReg;
using PX5::Gnm::Pm4Decoder;
using PX5::Gnm::StreamError;
using PX5::Gnm::Type3Header;

namespace {

int fails = 0;

void chk(bool ok, const std::string& what) {
    printf("  [%s] %s\n", ok ? "OK  " : "FAIL", what.c_str());
    if (!ok) ++fails;
}

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

// Stream section 1: set state, then draw twice, then dispatch. Positions are
// asserted, so keep this the single source of truth and derive from it.
std::vector<uint32_t> BuildGateStream() {
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

} // namespace

int main() {
    // ==== Section 1: the gate stream =======================================
    const std::vector<uint32_t> stream = BuildGateStream();

    GnmState state;
    DecodeStats stats;
    std::vector<StreamError> errors;
    Pm4Decoder dec;
    const size_t consumed =
        dec.Decode(stream.data(), stream.size(), state, stats, &errors);

    printf("M6 gate: fixed PM4 stream (%zu dwords) -> GPU IR\n", stream.size());

    printf("\nDecode sanity:\n");
    chk(consumed == stream.size(), "clean run consumes the whole stream");
    chk(stats.streamErrors == 0 && errors.empty(), "0 stream errors");
    chk(stats.totalPackets == 10, "10 packets decoded");

    printf("\nLowering to IR:\n");
    GpuOpList ops;   // default capacity
    const LowerStats ls = LowerGnmStateToIR(state, ops);

    // Expected exact sequence: SetScissor, Draw, DrawIndexed, Dispatch,
    // Barrier.
    const OpKind expectKinds[] = {
        OpKind::kSetScissor, OpKind::kDraw, OpKind::kDrawIndexed,
        OpKind::kDispatch,   OpKind::kBarrier,
    };
    const size_t nExpect = sizeof(expectKinds) / sizeof(expectKinds[0]);
    chk(ops.Size() == nExpect, "op list holds exactly the 5 expected ops");
    bool kindsExact = ops.Size() == nExpect;
    for (size_t i = 0; kindsExact && i < nExpect; ++i) {
        const GpuOp& op = ops.Ops()[i];
        if (op.kind != expectKinds[i]) {
            printf("  [FAIL] op %zu: got %s, expected %s\n", i,
                   OpKindName(op.kind), OpKindName(expectKinds[i]));
            kindsExact = false;
        }
    }
    chk(kindsExact, "op kinds and order match the fixture exactly");

    if (ops.Size() == nExpect) {
        const GpuOp& sc = ops.Ops()[0];
        chk(sc.xMin == 2 && sc.yMin == 5 && sc.xMax == 256 && sc.yMax == 320,
            "SetScissor box decodes TL/BR X[15:0] Y[31:16]");
        chk(sc.packetOffset == 0,
            "SetScissor is state-derived (packetOffset 0)");
        const GpuOp& draw = ops.Ops()[1];
        chk(draw.kind == OpKind::kDraw && draw.count == kAutoCount &&
                draw.instances == kInstances &&
                draw.indexTypeRaw == kIndexTypeRaw &&
                draw.initiatorRaw == kInitiator,
            "Draw carries count/instances/indexTypeRaw/initiator verbatim");
        const GpuOp& di2 = ops.Ops()[2];
        chk(di2.kind == OpKind::kDrawIndexed && di2.count == kDi2Count &&
                di2.instances == kInstances &&
                di2.indexTypeRaw == kIndexTypeRaw &&
                di2.initiatorRaw == kInitiator,
            "DrawIndexed carries count/instances/indexTypeRaw/initiator");
        const GpuOp& disp = ops.Ops()[3];
        chk(disp.kind == OpKind::kDispatch && disp.gridX == kDispX &&
                disp.gridY == kDispY && disp.gridZ == kDispZ,
            "Dispatch carries the 8x4x2 grid");
        const GpuOp& barrier = ops.Ops()[4];
        chk(barrier.kind == OpKind::kBarrier && barrier.barrierScope == 0,
            "exactly one submit-boundary Barrier, scope 0");
        chk(ops.Ops()[4].seq == ops.Ops()[3].seq,
            "Barrier provenance seq = last EMITTED op (dispatch here)");
        // Provenance chain (packet offsets derived from the fixture stream):
        // NOP@0(2), ctx@2(3), ctx@5(3), cfg@8(3), sh@11(5), idx@16(2),
        // inst@18(2), auto@20(3), di2@23(6), dispatch@29(4).
        chk(ops.Ops()[1].packetOffset == 20,
            "Draw provenance = DRAW_INDEX_AUTO packet offset 20");
        chk(ops.Ops()[2].packetOffset == 23,
            "DrawIndexed provenance = DRAW_INDEX_2 packet offset 23");
        chk(ops.Ops()[3].packetOffset == 29,
            "Dispatch provenance = DISPATCH_DIRECT packet offset 29");
        // seq is strictly increasing across the timeline.
        chk(ops.Ops()[0].seq < ops.Ops()[1].seq &&
                ops.Ops()[1].seq < ops.Ops()[2].seq &&
                ops.Ops()[2].seq < ops.Ops()[3].seq,
            "op seq strictly increases along the guest's event order");
    } else {
        chk(false, "payload assertions (op list incomplete)");
    }

    printf("\nLowering honesty counters:\n");
    chk(ls.opsEmitted == 5, "opsEmitted = 5");
    chk(ls.scissorOps == 1, "scissorOps = 1");
    chk(ls.drawOps == 1 && ls.drawIndexedOps == 1, "draw=1 drawIndexed=1");
    chk(ls.dispatchOps == 1, "dispatchOps = 1");
    chk(ls.barrierOps == 1, "barrierOps = 1");
    // Total register writes: scissor TL+BR (2) + config (1) + SH (3) +
    // index type (1) = 7. Journaled = 3 (TL, BR, index type). Unmapped = 4.
    chk(state.TotalRegisterWrites() == 7, "state saw exactly 7 register writes");
    chk(ls.carriedRegisterWrites == 1,
        "carriedRegisterWrites = 1 (INDEX_TYPE lives in draw payloads)");
    chk(ls.unmappedRegisterWrites == 4,
        "unmappedRegisterWrites = 4 (config + 3 SH writes, no guessed ops)");
    chk(ls.droppedOps == 0, "droppedOps = 0");
    printf("  %s\n", ls.SummaryString().c_str());

    // ==== Section 2: interleaving ==========================================
    printf("\nInterleaving (scissor, draw, scissor change, dispatch, draw):\n");
    state.Reset();
    stats.Reset();
    errors.clear();
    std::vector<uint32_t> s2;
    auto push2 = [&](uint32_t op, uint32_t bodyCount,
                     std::initializer_list<uint32_t> body,
                     uint32_t shaderType = 0) {
        s2.push_back(Type3Header::Encode(op, bodyCount, shaderType));
        for (uint32_t d : body) s2.push_back(d);
    };
    push2(kItSetContextReg, 2, {kCtxOffPaScScreenScissorTL, kScTL});
    push2(kItSetContextReg, 2, {kCtxOffPaScScreenScissorBR, kScBR});
    push2(kItDrawIndexAuto, 2, {10u, kInitiator});            // draw #1
    push2(kItSetContextReg, 2, {kCtxOffPaScScreenScissorTL, 0x00080001u});
    push2(kItSetContextReg, 2, {kCtxOffPaScScreenScissorBR, 0x02000200u});
    push2(kItDispatchDirect, 3, {2u, 1u, 1u}, /*shaderType=*/1);
    push2(kItDrawIndexAuto, 2, {20u, kInitiator});            // draw #2
    const size_t consumed2 =
        dec.Decode(s2.data(), s2.size(), state, stats, &errors);
    chk(consumed2 == s2.size() && stats.streamErrors == 0,
        "interleave stream decodes clean");
    GpuOpList ops2;
    const LowerStats ls2 = LowerGnmStateToIR(state, ops2);
    const OpKind expect2[] = {
        OpKind::kSetScissor, OpKind::kDraw, OpKind::kSetScissor,
        OpKind::kDispatch,   OpKind::kDraw, OpKind::kBarrier,
    };
    const size_t n2 = sizeof(expect2) / sizeof(expect2[0]);
    chk(ops2.Size() == n2, "interleave: 6 ops in guest event order");
    bool interExact = ops2.Size() == n2;
    for (size_t i = 0; interExact && i < n2; ++i) {
        if (ops2.Ops()[i].kind != expect2[i]) {
            printf("  [FAIL] op %zu: got %s, expected %s\n", i,
                   OpKindName(ops2.Ops()[i].kind), OpKindName(expect2[i]));
            interExact = false;
        }
    }
    chk(interExact, "interleave kinds/order exact (state op sits between draws)");
    if (interExact) {
        chk(ops2.Ops()[2].xMin == 1 && ops2.Ops()[2].yMin == 8 &&
                ops2.Ops()[2].xMax == 512 && ops2.Ops()[2].yMax == 512,
            "second SetScissor reflects the mid-stream box change");
        chk(ops2.Ops()[1].seq < ops2.Ops()[2].seq &&
                ops2.Ops()[2].seq < ops2.Ops()[3].seq,
            "scissor change is stamped between draw #1 and the dispatch");
    } else {
        chk(false, "interleave payload assertions");
    }
    chk(ls2.scissorOps == 2, "interleave: two scissor ops emitted");

    // ==== Section 4: bounded capacity =======================================
    printf("\nBounded capacity (cap 2, state with 3 draws):\n");
    GnmState big;
    for (uint32_t i = 0; i < 3; ++i) {
        GnmState::DrawRecord r{};
        r.packetOffset = i;
        r.count = 100u + i;
        big.RecordDraw(r);
    }
    GpuOpList small(2);
    const LowerStats ls3 = LowerGnmStateToIR(big, small);
    chk(small.Size() == 2, "cap-2 list holds exactly 2 ops");
    chk(ls3.droppedOps == 2, "2 ops rejected and counted (3 draws -> 2 push fails incl. barrier)");
    chk(ls3.opsEmitted == 2, "opsEmitted counts only accepted ops");
    if (small.Size() == 2) {
        chk(small.Ops()[0].kind == OpKind::kDraw &&
                small.Ops()[1].kind == OpKind::kDraw,
                "the two accepted ops are the first draws, order kept");
    } else {
        chk(false, "capacity payload assertions (op list incomplete)");
    }

    // ==== Section 5: journal eviction (pairing + accounting) ================
    // The bot-review P1 scenario: >64 named writes between a scissor TL and
    // its BR evict the TL journal entry. The BR record's write-time pairing
    // must still lower the CORRECT box, and the cumulative named/carried
    // counters must keep the honesty split exact.
    printf("\nJournal eviction (TL evicted, BR pairs from write-time bank):\n");
    GnmState ev;
    constexpr uint32_t kEvTL = 0x000A0003u;   // TL: x=3, y=10
    constexpr uint32_t kEvBR = 0x00500200u;   // BR: x=512, y=80
    ev.WriteRegister(PX5::Gnm::kRegPaScScreenScissorTL, kEvTL);
    for (uint32_t i = 0; i < 70; ++i) {
        // 70 named writes (INDEX_TYPE) push the TL entry past the 64 cap.
        ev.WriteRegister(PX5::Gnm::kRegVgtDmaIndexType, i);
    }
    chk(ev.NamedWriteLog().size() == 64,
        "journal is bounded at 64 (eviction happened)");
    bool tlEvicted = true;
    for (const auto& w : ev.NamedWriteLog()) {
        if (w.absoluteAddress == PX5::Gnm::kRegPaScScreenScissorTL) {
            tlEvicted = false;
        }
    }
    chk(tlEvicted, "the TL journal entry was evicted by the 70 writes");
    ev.WriteRegister(PX5::Gnm::kRegPaScScreenScissorBR, kEvBR);
    GpuOpList evOps;
    const LowerStats evStats = LowerGnmStateToIR(ev, evOps);
    chk(evStats.scissorOps == 1, "eviction: exactly one SetScissor emitted");
    if (evStats.scissorOps == 1 && !evOps.Empty()) {
        const GpuOp& evSc = evOps.Ops()[0];
        chk(evSc.xMin == 3 && evSc.yMin == 10 && evSc.xMax == 512 &&
                evSc.yMax == 80,
            "eviction: BR still pairs with the write-time TL (not 0,0)");
        chk(evOps.Ops()[1].kind == OpKind::kBarrier &&
                evOps.Ops()[1].seq == evSc.seq,
            "eviction: barrier seq = last emitted op (the scissor)");
    } else {
        chk(false, "eviction payload assertions (op missing)");
    }
    chk(ev.NamedWritesTotal() == 72,
        "eviction accounting: 72 named writes total (TL + 70 + BR)");
    chk(ev.CarriedWritesTotal() == 70,
        "eviction accounting: 70 carried INDEX_TYPE writes");
    chk(evStats.carriedRegisterWrites == 70,
        "eviction: carried counter immune to journal eviction");
    // Total register writes = 72 named + 0 unmapped; the split stays exact.
    chk(evStats.unmappedRegisterWrites == 0,
        "eviction: no named write misreported as unmapped");
    // BR with no TL ever written: lowers with the register reset value.
    GnmState noTL;
    noTL.WriteRegister(PX5::Gnm::kRegPaScScreenScissorBR, 0x000100C8u);
    GpuOpList noTLOps;
    const LowerStats noTLStats = LowerGnmStateToIR(noTL, noTLOps);
    chk(noTLStats.scissorOps == 1 && !noTLOps.Empty() &&
            noTLOps.Ops()[0].xMin == 0 && noTLOps.Ops()[0].yMin == 0 &&
            noTLOps.Ops()[0].xMax == 200 && noTLOps.Ops()[0].yMax == 1,
        "BR with no TL ever written lowers with reset-value TL (0,0)");

    // ==== Section 6: Reset ==================================================
    printf("\nReset:\n");
    state.Reset();
    GpuOpList ops3;
    const LowerStats ls4 = LowerGnmStateToIR(state, ops3);
    chk(ops3.Empty(), "reset state lowers to an EMPTY list");
    chk(ls4.barrierOps == 0 && ls4.opsEmitted == 0,
        "no barrier, no ops from empty state");

    printf("\n");
    if (fails == 0) {
        // Mandated M6 PASS shape (docs/testing.md "Milestone PASS format").
        printf("M6 PASS\n");
        printf("GPU IR:\n");
        printf("    5 ops lowered\n");
        printf("    5/5 expected ops\n");
        printf("    0 unexpected lowering drops\n");
        return 0;
    }
    printf("FAILED (%d failure%s)\n", fails, fails == 1 ? "" : "s");
    return 1;
}
