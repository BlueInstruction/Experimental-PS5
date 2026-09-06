// SPDX-License-Identifier: MIT
// M7 host gate (T1) — the GPU IR -> Vulkan command mapping, locked on host.
//
// docs/milestones.md M7 gate is the DEVICE readback chain (T3): instance ->
// device -> queue -> image -> clear -> submit -> fence -> readback ->
// expected pixels. This file is NOT that gate and never claims it. What it
// locks is the deterministic core the device run executes:
//
//   1. the float -> UNORM-8 clear conversion — the exact bytes the device
//      readback will compare against (one rule, literal expectations);
//   2. the planner against the REAL M6 lowering output (the M6 gate
//      stream): every emitted op maps to its honest disposition —
//      deferred-by-kind for pipeline-needing ops, one submit boundary;
//   3. a synthetic one-Clear IR list — the exact command sequence the M7
//      device proof plans: barrier + clear, payload and seq carried;
//   4. last-clear-wins readback expectation (ordered clears);
//   5. a mixed draw/clear list — executable commands and deferred counts
//      coexist, nothing guessed;
//   6. the empty list lowers to an empty plan;
//   7. VerifyClearReadback — full match, single-byte mismatch attribution,
//      unusable-input guards.
//
// Platform-independent: builds with tools/hosttests/run.sh, no device, no
// Vulkan headers — vulkan_backend.h keeps the plan Vulkan-free on purpose.

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

#include "gpu/gnm/pm4_decoder.h"
#include "gpu/gnm/pm4_packet.h"
#include "gpu/ir/gpu_ir.h"
#include "gpu/vulkan_backend.h"

using PX5::Gpu::BackendPlanStats;
using PX5::Gpu::ClearFloatToUnorm8;
using PX5::Gpu::GpuOp;
using PX5::Gpu::GpuOpList;
using PX5::Gpu::LowerGnmStateToIR;
using PX5::Gpu::LowerStats;
using PX5::Gpu::OpKind;
using PX5::Gpu::PlanVulkanCommands;
using PX5::Gpu::ReadbackCheck;
using PX5::Gpu::VerifyClearReadback;
using PX5::Gpu::VulkanCommand;
using PX5::Gpu::VulkanCommandPlan;
using PX5::Gnm::DecodeStats;
using PX5::Gnm::GnmState;
using PX5::Gnm::kCtxOffPaScScreenScissorBR;
using PX5::Gnm::kCtxOffPaScScreenScissorTL;
using PX5::Gnm::kItDrawIndex2;
using PX5::Gnm::kItDrawIndexAuto;
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

// The planner's honesty invariant, asserted on EVERY plan in this file.
bool StatsInvariant(const BackendPlanStats& s) {
    return s.opsReceived ==
           s.clearOps + s.boundaryOps + s.DeferredTotal() + s.unknownOps;
}

bool RgbaEquals(const uint8_t a[4], const uint8_t b[4]) {
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3];
}

// ---- the M6 gate stream (same fixture gpu_ir_test.cpp locks) --------------
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

std::vector<uint32_t> BuildM6GateStream() {
    std::vector<uint32_t> s;
    auto push = [&](uint32_t op, uint32_t bodyCount,
                    std::initializer_list<uint32_t> body,
                    uint32_t shaderType = 0) {
        s.push_back(Type3Header::Encode(op, bodyCount, shaderType));
        for (uint32_t d : body) s.push_back(d);
    };
    push(kItNop, 1, {0x0});
    push(kItSetContextReg, 2, {kCtxOffPaScScreenScissorTL, kScTL});
    push(kItSetContextReg, 2, {kCtxOffPaScScreenScissorBR, kScBR});
    push(kItSetConfigReg, 2, {0x10, kConfigA});
    push(kItSetShReg, 4, {0x0C, kShA, kShB, kShC});
    push(kItIndexType, 1, {kIndexTypeRaw});
    push(kItNumInstances, 1, {kInstances});
    push(kItDrawIndexAuto, 2, {kAutoCount, kInitiator});
    push(kItDrawIndex2, 5, {0x1FF, 0x0, 0x1000, kDi2Count, kInitiator});
    push(kItDispatchDirect, 3, {kDispX, kDispY, kDispZ}, /*shaderType=*/1);
    return s;
}

GpuOp MakeClear(uint64_t seq, float r, float g, float b, float a) {
    GpuOp op{};
    op.kind = OpKind::kClear;
    op.seq  = seq;
    op.clearColor[0] = r;
    op.clearColor[1] = g;
    op.clearColor[2] = b;
    op.clearColor[3] = a;
    return op;
}

GpuOp MakeBarrier(uint64_t seq) {
    GpuOp op{};
    op.kind = OpKind::kBarrier;
    op.seq  = seq;
    op.barrierScope = 0;
    return op;
}

} // namespace

int main() {
    printf("M7 host gate: GPU IR -> Vulkan command mapping (T1)\n");

    // ==== Section 1: the UNORM conversion rule =============================
    printf("\nClear float -> UNORM-8 (the bytes readback compares):\n");
    struct F2B { float f; uint8_t b; const char* what; };
    const F2B convCases[] = {
        {0.0f,   0,   "0.0 -> 0"},
        {1.0f,   255, "1.0 -> 255"},
        {2.0f,   255, "2.0 clamps -> 255"},
        {-0.5f,  0,   "-0.5 clamps -> 0"},
        {0.5f,   128, "0.5 -> 128 (round half up)"},
        {0.25f,  64,  "0.25 -> 64"},
        {0.08f,  20,  "0.08 -> 20 (the suite's teal-blue R)"},
        {0.72f,  184, "0.72 -> 184 (its G)"},
        {0.7f,   179, "0.7 -> 179 (0.7f*255.0f rounds to exactly 178.5f)"},
        {std::numeric_limits<float>::quiet_NaN(), 0,
         "NaN clamps -> 0 (deterministic, never garbage)"},
    };
    for (const F2B& c : convCases) {
        chk(ClearFloatToUnorm8(c.f) == c.b, c.what);
    }

    // ==== Section 2: the planner against the REAL M6 lowering ==============
    printf("\nPlanner vs the M6 gate stream's lowered ops:\n");
    const std::vector<uint32_t> stream = BuildM6GateStream();
    GnmState state;
    DecodeStats stats;
    std::vector<StreamError> errors;
    Pm4Decoder dec;
    const size_t consumed =
        dec.Decode(stream.data(), stream.size(), state, stats, &errors);
    chk(consumed == stream.size() && stats.streamErrors == 0,
        "M6 gate stream decodes clean");

    GpuOpList irOps;
    const LowerStats lowerStats = LowerGnmStateToIR(state, irOps);
    chk(irOps.Size() == 5,
        "M6 lowering emitted its 5 locked ops (scissor,draw,di,dispatch,barrier)");
    chk(lowerStats.opsEmitted == 5 && lowerStats.droppedOps == 0,
        "M6 lowering stats clean (5 emitted, 0 dropped)");

    const VulkanCommandPlan plan6 = PlanVulkanCommands(irOps);
    chk(StatsInvariant(plan6.stats), "plan6 honesty invariant holds");
    chk(plan6.stats.opsReceived == 5, "plan6 walked all 5 ops");
    chk(plan6.stats.scissorOpsDeferred == 1 &&
            plan6.stats.drawOpsDeferred == 1 &&
            plan6.stats.drawIndexedOpsDeferred == 1 &&
            plan6.stats.dispatchOpsDeferred == 1 &&
            plan6.stats.DeferredTotal() == 4,
        "all 4 pipeline-needing ops DEFERRED by kind, never guessed");
    chk(plan6.stats.boundaryOps == 1 && plan6.stats.clearOps == 0 &&
            plan6.stats.unknownOps == 0,
        "one submit boundary, no clear, nothing unknown");
    chk(plan6.commands.size() == 1 &&
            plan6.commands[0].kind ==
                VulkanCommand::Kind::kSubmitBoundary,
        "plan6 = exactly one kSubmitBoundary command");
    chk(!plan6.hasClear, "plan6 has NO readback expectation (no clear)");

    // ==== Section 3: the synthetic one-Clear list (the M7 device input) ====
    printf("\nSynthetic one-Clear list (M7 device proof's IR input):\n");
    GpuOpList oneClear;
    oneClear.Push(MakeClear(7u, 1.0f, 0.0f, 0.0f, 1.0f));
    oneClear.Push(MakeBarrier(7u));
    const VulkanCommandPlan planClear = PlanVulkanCommands(oneClear);
    chk(StatsInvariant(planClear.stats), "planClear honesty invariant holds");
    chk(planClear.stats.opsReceived == 2 && planClear.stats.clearOps == 1 &&
            planClear.stats.boundaryOps == 1 &&
            planClear.stats.DeferredTotal() == 0 &&
            planClear.stats.unknownOps == 0,
        "2 ops in -> 1 clear + 1 boundary, 0 deferred, 0 unknown");
    chk(planClear.commands.size() == 3,
        "planClear = 3 commands (barrier, clear, submit boundary)");
    bool seqOk = planClear.commands.size() == 3;
    for (const VulkanCommand& c : planClear.commands)
        seqOk = seqOk && c.seq == 7u;
    chk(seqOk, "every command carries its GpuOp's seq (provenance)");
    if (planClear.commands.size() == 3) {
        chk(planClear.commands[0].kind ==
                VulkanCommand::Kind::kPipelineBarrier &&
            planClear.commands[1].kind ==
                VulkanCommand::Kind::kClearColorImage &&
            planClear.commands[2].kind ==
                VulkanCommand::Kind::kSubmitBoundary,
            "command order: barrier -> clear -> submit boundary");
        const VulkanCommand& cc = planClear.commands[1];
        chk(cc.clearColor[0] == 1.0f && cc.clearColor[1] == 0.0f &&
                cc.clearColor[2] == 0.0f && cc.clearColor[3] == 1.0f,
            "clear color carried verbatim (floats untouched)");
    } else {
        chk(false, "command-order assertions (plan incomplete)");
    }
    const uint8_t expectRed[4] = {255, 0, 0, 255};
    chk(planClear.hasClear && RgbaEquals(planClear.clearRgba, expectRed),
        "readback expectation = RGBA8 bytes (255,0,0,255)");

    // ==== Section 4: last clear wins =======================================
    printf("\nTwo ordered clears (expectation = the LAST one):\n");
    GpuOpList twoClears;
    twoClears.Push(MakeClear(1u, 0.0f, 0.0f, 0.0f, 1.0f));   // opaque black
    twoClears.Push(MakeClear(2u, 1.0f, 0.0f, 0.0f, 1.0f));   // then red
    const VulkanCommandPlan plan2 = PlanVulkanCommands(twoClears);
    chk(StatsInvariant(plan2.stats), "plan2 honesty invariant holds");
    chk(plan2.stats.clearOps == 2 && plan2.commands.size() == 4,
        "2 clears -> 4 commands (barrier+clear, barrier+clear)");
    if (plan2.commands.size() == 4) {
        chk(plan2.commands[1].clearColor[0] == 0.0f &&
                plan2.commands[3].clearColor[0] == 1.0f,
            "both clear commands kept in order (black first, red second)");
    }
    chk(plan2.hasClear && plan2.clearF32[0] == 1.0f &&
            RgbaEquals(plan2.clearRgba, expectRed),
        "readback expectation tracks the LAST clear (red), not the first");

    // ==== Section 5: mixed draw/clear list =================================
    printf("\nMixed list (draw, clear, drawIndexed, boundary):\n");
    GpuOpList mixed;
    GpuOp draw{};
    draw.kind = OpKind::kDraw;
    draw.seq = 1u;
    draw.count = 3u;
    mixed.Push(draw);
    mixed.Push(MakeClear(2u, 0.0f, 1.0f, 0.0f, 1.0f));
    GpuOp di{};
    di.kind = OpKind::kDrawIndexed;
    di.seq = 3u;
    di.count = 9u;
    mixed.Push(di);
    mixed.Push(MakeBarrier(3u));
    const VulkanCommandPlan planM = PlanVulkanCommands(mixed);
    chk(StatsInvariant(planM.stats), "planM honesty invariant holds");
    chk(planM.stats.opsReceived == 4 &&
            planM.stats.drawOpsDeferred == 1 &&
            planM.stats.drawIndexedOpsDeferred == 1 &&
            planM.stats.DeferredTotal() == 2 &&
            planM.stats.clearOps == 1 && planM.stats.boundaryOps == 1,
        "deferred draws AND the clear coexist, each counted by kind");
    chk(planM.commands.size() == 3 &&
            planM.commands[0].kind == VulkanCommand::Kind::kPipelineBarrier &&
            planM.commands[1].kind == VulkanCommand::Kind::kClearColorImage &&
            planM.commands[2].kind == VulkanCommand::Kind::kSubmitBoundary,
        "planM executable subset: barrier, clear, boundary (no draw commands)");
    const uint8_t expectGreen[4] = {0, 255, 0, 255};
    chk(planM.hasClear && RgbaEquals(planM.clearRgba, expectGreen),
        "planM readback expectation = the green clear");

    // ==== Section 6: empty list ============================================
    printf("\nEmpty list:\n");
    GpuOpList empty;
    const VulkanCommandPlan planE = PlanVulkanCommands(empty);
    chk(planE.commands.empty() && planE.stats.opsReceived == 0 &&
            !planE.hasClear,
        "empty IR list -> empty plan, no expectation");
    chk(StatsInvariant(planE.stats), "planE honesty invariant holds (0=0)");

    // ==== Section 7: VerifyClearReadback ===================================
    printf("\nReadback verification:\n");
    constexpr uint32_t W = 64, H = 64;
    constexpr size_t kBytes = W * H * 4;
    std::vector<uint8_t> buf(kBytes);
    for (size_t p = 0; p < W * H; ++p) {
        buf[p * 4 + 0] = 255;
        buf[p * 4 + 1] = 0;
        buf[p * 4 + 2] = 0;
        buf[p * 4 + 3] = 255;
    }
    ReadbackCheck full = VerifyClearReadback(buf.data(), buf.size(), W, H,
                                             expectRed);
    chk(full.pixelsTotal == 4096 && full.pixelsMatch == 4096 &&
            full.allMatch && full.firstBadByte == SIZE_MAX,
        "64x64 red readback: 4096/4096 pixels match");

    buf[130 * 4 + 2] = 1;   // one wrong B channel at pixel 130
    ReadbackCheck oneBad = VerifyClearReadback(buf.data(), buf.size(), W, H,
                                               expectRed);
    chk(oneBad.pixelsTotal == 4096 && oneBad.pixelsMatch == 4095 &&
            !oneBad.allMatch && oneBad.firstBadByte == 130u * 4 + 2,
        "single wrong byte: 4095/4096, firstBadByte names byte 522");

    ReadbackCheck shortBuf = VerifyClearReadback(buf.data(), kBytes / 2, W, H,
                                                 expectRed);
    chk(shortBuf.pixelsTotal == 0 && !shortBuf.allMatch,
        "undersized buffer reports 0 pixels checked, never a fake pass");

    ReadbackCheck nullBuf = VerifyClearReadback(nullptr, 0, W, H, expectRed);
    chk(nullBuf.pixelsTotal == 0 && !nullBuf.allMatch,
        "null buffer reports 0 pixels checked");

    ReadbackCheck zeroDim = VerifyClearReadback(buf.data(), buf.size(), 0, H,
                                                expectRed);
    chk(zeroDim.pixelsTotal == 0 && !zeroDim.allMatch,
        "zero width reports 0 pixels checked");

    // 2x2 hand-checked buffer, mismatch on the A channel of the last pixel.
    const uint8_t tiny[16] = {10, 20, 30, 40,  10, 20, 30, 40,
                              10, 20, 30, 40,  10, 20, 30, 41};
    const uint8_t tinyExp[4] = {10, 20, 30, 40};
    ReadbackCheck tinyChk = VerifyClearReadback(tiny, sizeof(tiny), 2, 2,
                                                tinyExp);
    chk(tinyChk.pixelsTotal == 4 && tinyChk.pixelsMatch == 3 &&
            !tinyChk.allMatch && tinyChk.firstBadByte == 15,
        "2x2 buffer: 3/4 match, firstBadByte=15 (A channel, pixel 3)");

    printf("\n");
    if (fails == 0) {
        // Honest shape: this is the T1 core gate, NOT the M7 milestone gate
        // (docs/milestones.md M7 = the on-device readback chain).
        printf("M7-T1 PASS\n");
        printf("Vulkan backend planner:\n");
        printf("    clear plan = barrier + clear + submit boundary\n");
        printf("    deferred ops counted by kind, never guessed\n");
        printf("    readback rule: exact bytes, last clear wins\n");
        printf("M7 device gate (T3 readback): NOT CLAIMED HERE — pending\n");
        printf("the on-device proof run (foundation suite step 8b).\n");
        return 0;
    }
    printf("FAILED (%d failure%s)\n", fails, fails == 1 ? "" : "s");
    return 1;
}
