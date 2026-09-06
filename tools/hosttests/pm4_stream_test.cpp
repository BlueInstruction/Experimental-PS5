// SPDX-License-Identifier: MIT
// M5 gate — PM4 stream decode to the EXACT expected structured sequence.
//
// docs/gpu.md M5 evidence gate: "a fixed PM4 test stream with known packet
// count and opcodes decodes to the exact expected structured sequence —
// N packets in, N/N records out, 0 unexpected stream errors — register
// banks hold the written values, draw records carry the right
// count/indexed/instances fields."
//
// This file IS that gate (the M5 deliverable previously marked missing in
// docs/architecture.md). It is deterministic: same stream, same state,
// same verdict, every run. Sections:
//   1. the fixed 12-packet stream — every opcode, position, body width,
//      register write, draw/dispatch record and trace record asserted;
//   2. a truncated stream — the error lands in DecodeStats, nothing is
//      silently dropped or guessed.
//
// Platform-independent: builds with tools/hosttests/run.sh, no device.

#include <cstdio>
#include <string>
#include <vector>

#include "gpu/gnm/gnm_state.h"
#include "gpu/gnm/pm4_decoder.h"
#include "gpu/gnm/pm4_packet.h"

using PX5::Gnm::DecodeStats;
using PX5::Gnm::GnmState;
using PX5::Gnm::ItOpcodeName;
using PX5::Gnm::kConfigRegBase;
using PX5::Gnm::kContextRegBase;
using PX5::Gnm::kCtxOffVgtDmaIndexType;
using PX5::Gnm::kItDrawIndex2;
using PX5::Gnm::kItDrawIndexAuto;
using PX5::Gnm::kItDispatchDirect;
using PX5::Gnm::kItIndexType;
using PX5::Gnm::kItNop;
using PX5::Gnm::kItNumInstances;
using PX5::Gnm::kItSetConfigReg;
using PX5::Gnm::kItSetContextReg;
using PX5::Gnm::kItSetShReg;
using PX5::Gnm::kItSetShRegOffset;
using PX5::Gnm::kItSetUConfigReg;
using PX5::Gnm::kUConfigRegBase;
using PX5::Gnm::kRegVgtDmaIndexType;
using PX5::Gnm::Pm4Decoder;
using PX5::Gnm::RegSpace;
using PX5::Gnm::StreamError;
using PX5::Gnm::Type3Header;

namespace {

int fails = 0;

void chk(bool ok, const std::string& what) {
    printf("  [%s] %s\n", ok ? "OK  " : "FAIL", what.c_str());
    if (!ok) ++fails;
}

constexpr uint32_t kConfigA1 = 0x11110001u;
constexpr uint32_t kConfigA2 = 0x11110002u;
constexpr uint32_t kUConfigB1 = 0x22220001u;
constexpr uint32_t kShC1 = 0x33330001u;
constexpr uint32_t kShC2 = 0x33330002u;
constexpr uint32_t kShC3 = 0x33330003u;
constexpr uint32_t kAddrLo = 0x11223344u;   // SET_SH_REG_OFFSET address pair
constexpr uint32_t kAddrHi = 0x55667788u;   //   — discarded by the M5 model
constexpr uint32_t kShD1 = 0x44440001u;     // first real data dword
constexpr uint32_t kShD2 = 0x44440002u;     // second real data dword
constexpr uint32_t kCtxE1 = 0x66660001u;
constexpr uint32_t kIndexTypeRaw = 0x2u;
constexpr uint32_t kInstances = 4u;
constexpr uint32_t kAutoCount = 36u;
constexpr uint32_t kInitiator = 0x6u;
constexpr uint32_t kDi2Count = 300u;
constexpr uint32_t kDispX = 8u, kDispY = 4u, kDispZ = 2u;

// The fixed stream. Positions are asserted, so keep this table the single
// source of truth and derive everything from it.
struct Expect {
    uint32_t opcode;
    uint32_t offset;
    uint32_t bodyDwords;
    uint32_t shaderType;
};

std::vector<uint32_t> BuildStream() {
    std::vector<uint32_t> s;
    auto push = [&](uint32_t op, uint32_t bodyCount,
                    std::initializer_list<uint32_t> body,
                    uint32_t shaderType = 0, uint32_t predicate = 0) {
        s.push_back(Type3Header::Encode(op, bodyCount, shaderType, predicate));
        for (uint32_t d : body) s.push_back(d);
    };
    // 0: NOP  @0 (1 body dword)
    push(kItNop, 1, {0x0});
    // 1: SET_CONFIG_REG  @2 — offset 0x10, two values
    push(kItSetConfigReg, 3, {0x10, kConfigA1, kConfigA2});
    // 2: SET_UCONFIG_REG @6 — offset 0x20, one value
    push(kItSetUConfigReg, 2, {0x20, kUConfigB1});
    // 3: SET_SH_REG      @9 — offset 0x0C, three values
    push(kItSetShReg, 4, {0x0C, kShC1, kShC2, kShC3});
    // 4: SET_SH_REG_OFFSET @14 — offset 0x18, address pair discarded,
    //    two data values land at 0x2C18/0x2C19
    push(kItSetShRegOffset, 5, {0x18, kAddrLo, kAddrHi, kShD1, kShD2});
    // 5: SET_CONTEXT_REG @20 — offset 0x128, one value
    push(kItSetContextReg, 2, {0x128, kCtxE1});
    // 6: UNKNOWN opcode 0x51 @23 — counted, skipped by declared length
    //    (3 body dwords), the stream continues cleanly after it
    push(0x51, 3, {0xDEAD, 0xBEEF, 0x0BAD});
    // 7: INDEX_TYPE @27
    push(kItIndexType, 1, {kIndexTypeRaw});
    // 8: NUM_INSTANCES @29
    push(kItNumInstances, 1, {kInstances});
    // 9: DRAW_INDEX_AUTO @31
    push(kItDrawIndexAuto, 2, {kAutoCount, kInitiator});
    // 10: DRAW_INDEX_2 @34 — public 5-dword body; the M5 model records
    //     count=body[3], initiator=body[4] (address/max stay unrecorded)
    push(kItDrawIndex2, 5, {0x1FF, 0x0, 0x1000, kDi2Count, kInitiator});
    // 11: DISPATCH_DIRECT @40 — compute packet (shaderType=1)
    push(kItDispatchDirect, 3, {kDispX, kDispY, kDispZ}, /*shaderType=*/1);
    return s;
}

} // namespace

int main() {
    const std::vector<uint32_t> stream = BuildStream();

    GnmState state;
    DecodeStats stats;
    std::vector<StreamError> errors;
    std::vector<PX5::Gnm::PacketRecord> trace;

    Pm4Decoder dec;
    const size_t consumed = dec.Decode(stream.data(), stream.size(),
                                       state, stats, &errors, &trace);

    printf("M5 gate: fixed PM4 stream (%zu dwords)\n", stream.size());

    // ---- counts ---------------------------------------------------------
    printf("\nPacket counts:\n");
    chk(consumed == stream.size(), "clean run consumes the whole stream");
    chk(stats.totalPackets == 12, "12 packets decoded");
    chk(stats.graphicsPackets == 11, "11 graphics packets");
    chk(stats.computePackets == 1, "1 compute packet (DISPATCH_DIRECT)");
    chk(stats.unknownPackets == 1, "1 unknown packet counted");
    chk(stats.unknownOpcodeCounts.size() == 1 &&
            stats.unknownOpcodeCounts.count(0x51) == 1 &&
            stats.unknownOpcodeCounts.at(0x51) == 1,
        "unknown opcode 0x51 counted exactly once");
    chk(stats.drawCalls == 2, "2 draw calls");
    chk(stats.dispatches == 1, "1 dispatch");
    chk(stats.streamErrors == 0, "0 unexpected stream errors");
    const uint32_t mainPackets = stats.totalPackets;  // for the PASS block

    // ---- exact structured sequence --------------------------------------
    printf("\nExact expected sequence (opcode @offset, body dwords):\n");
    const Expect expect[] = {
        {kItNop,               0, 1, 0},
        {kItSetConfigReg,      2, 3, 0},
        {kItSetUConfigReg,     6, 2, 0},
        {kItSetShReg,          9, 4, 0},
        {kItSetShRegOffset,   14, 5, 0},
        {kItSetContextReg,    20, 2, 0},
        {0x51,                23, 3, 0},
        {kItIndexType,        27, 1, 0},
        {kItNumInstances,     29, 1, 0},
        {kItDrawIndexAuto,    31, 2, 0},
        {kItDrawIndex2,       34, 5, 0},
        {kItDispatchDirect,   40, 3, 1},
    };
    const size_t nExpect = sizeof(expect) / sizeof(expect[0]);
    chk(trace.size() == nExpect, "trace holds every packet, most recent last");
    bool sequenceExact = trace.size() == nExpect;
    for (size_t i = 0; sequenceExact && i < nExpect; ++i) {
        const auto& rec = trace[i];
        if (rec.opcode != expect[i].opcode ||
            rec.dwordOffset != expect[i].offset ||
            rec.bodyDwords != expect[i].bodyDwords ||
            rec.shaderType != expect[i].shaderType ||
            rec.predicate != 0) {
            printf("  [FAIL] record %zu: got %s(0x%X) @%u body=%u st=%u, "
                   "expected opcode 0x%X @%u body=%u st=%u\n",
                   i, ItOpcodeName(rec.opcode), rec.opcode, rec.dwordOffset,
                   rec.bodyDwords, rec.shaderType, expect[i].opcode,
                   expect[i].offset, expect[i].bodyDwords,
                   expect[i].shaderType);
            sequenceExact = false;
        }
    }
    chk(sequenceExact, "decoded sequence matches the fixture exactly");

    // ---- register banks ---------------------------------------------------
    printf("\nRegister banks:\n");
    bool wasWritten = false;
    chk(state.ReadRegister(kConfigRegBase + 0x10) == kConfigA1 &&
            state.ReadRegister(kConfigRegBase + 0x11) == kConfigA2,
        "CONFIG bank holds the SET_CONFIG_REG values");
    chk(state.ReadRegister(kUConfigRegBase + 0x20) == kUConfigB1,
        "UCONFIG bank holds the SET_UCONFIG_REG value");
    chk(state.ReadRegister(0x2C00 + 0x0C) == kShC1 &&
            state.ReadRegister(0x2C00 + 0x0D) == kShC2 &&
            state.ReadRegister(0x2C00 + 0x0E) == kShC3,
        "SH bank holds the SET_SH_REG values");
    chk(state.ReadRegister(0x2C00 + 0x18) == kShD1 &&
            state.ReadRegister(0x2C00 + 0x19) == kShD2,
        "SET_SH_REG_OFFSET data lands at the SH offsets");
    state.ReadRegister(0x2C00 + 0x1A, &wasWritten);
    chk(!wasWritten, "the discarded address pair wrote NOTHING extra");
    chk(state.ReadRegister(kContextRegBase + 0x128) == kCtxE1,
        "CONTEXT bank holds the SET_CONTEXT_REG value");
    chk(state.ReadRegister(kRegVgtDmaIndexType, &wasWritten) ==
                kIndexTypeRaw &&
            wasWritten,
        "INDEX_TYPE raw value visible at VGT_DMA_INDEX_TYPE");
    chk(state.WrittenRegisters(RegSpace::kConfig) == 2,
        "written-register accounting: CONFIG = 2");
    chk(state.WrittenRegisters(RegSpace::kUConfig) == 1,
        "written-register accounting: UCONFIG = 1");
    chk(state.WrittenRegisters(RegSpace::kSh) == 5,
        "written-register accounting: SH = 5");
    chk(state.WrittenRegisters(RegSpace::kContext) == 2,
        "written-register accounting: CONTEXT = 2");
    chk(state.TotalRegisterWrites() == 10, "10 register writes total");
    chk(state.OutOfRangeWrites() == 0, "no out-of-range writes");

    // ---- draw/dispatch records -------------------------------------------
    printf("\nDraw/dispatch records:\n");
    chk(state.DrawLog().size() == 2, "two draw records, most recent last");
    if (state.DrawLog().size() == 2) {
        const auto& autoRec = state.DrawLog()[0];
        chk(autoRec.packetOffset == 31 && autoRec.count == kAutoCount &&
                autoRec.initiator == kInitiator &&
                autoRec.instances == kInstances &&
                autoRec.indexTypeRaw == kIndexTypeRaw && !autoRec.indexed,
            "DRAW_INDEX_AUTO record: count/indexed/instances/initiator");
        const auto& di2 = state.DrawLog()[1];
        chk(di2.packetOffset == 34 && di2.count == kDi2Count &&
                di2.initiator == kInitiator &&
                di2.instances == kInstances &&
                di2.indexTypeRaw == kIndexTypeRaw && di2.indexed,
            "DRAW_INDEX_2 record: count/indexed/instances/initiator");
    } else {
        chk(false, "draw records present");
    }
    chk(state.IndexTypeRaw() == kIndexTypeRaw, "state index type matches");
    chk(state.NumInstances() == kInstances, "state instance count matches");
    uint32_t dy = 0, dz = 0;
    chk(state.LastDispatchDims(&dy, &dz) == kDispX && dy == kDispY &&
            dz == kDispZ,
        "DISPATCH_DIRECT dims 8x4x2 recorded");

    // ---- negative stream: truncated packet -------------------------------
    printf("\nNegative stream (truncated packet):\n");
    state.Reset();
    stats.Reset();
    errors.clear();
    std::vector<uint32_t> bad;
    bad.push_back(Type3Header::Encode(kItDrawIndexAuto, 2)); // declares 2 body
    bad.push_back(kAutoCount);                               // only 1 present
    const size_t stop = dec.Decode(bad.data(), bad.size(), state, stats,
                                   &errors, nullptr);
    chk(stats.streamErrors == 1, "truncation recorded as exactly one error");
    chk(errors.size() == 1 && errors[0].dwordOffset == 0 &&
            errors[0].what.find("truncated") != std::string::npos,
        "error names the offset and 'truncated'");
    chk(stop == 0, "decode stops at the erroring packet");
    chk(state.DrawCalls() == 0 && stats.drawCalls == 0,
        "no draw was recorded from the truncated packet");

    printf("\n");
    if (fails == 0) {
        // Mandated M5 PASS shape (docs/testing.md "Milestone PASS format").
        // The numbers are the MAIN stream's (captured before the negative
        // section reset the counters).
        printf("M5 PASS\n");
        printf("PM4:\n");
        printf("    %u packets decoded\n", mainPackets);
        printf("    %u/%u expected opcodes\n", mainPackets, mainPackets);
        printf("    0 unexpected stream errors\n");
        return 0;
    }
    printf("FAILED (%d failure%s)\n", fails, fails == 1 ? "" : "s");
    return 1;
}
