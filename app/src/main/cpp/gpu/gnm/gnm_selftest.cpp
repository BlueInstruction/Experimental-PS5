// SPDX-License-Identifier: MIT
// PX5 Phase C milestone 1 — GNM decoder self-test (implementation).

#include "gpu/gnm/gnm_selftest.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "gpu/gnm/gnm_selftest.h"
#include "gpu/gnm/gnm_state.h"
#include "gpu/gnm/gnm_submit.h"
#include "gpu/gnm/pm4_decoder.h"
#include "gpu/gnm/pm4_packet.h"

namespace PX5::Gnm {

namespace {

struct SubResult {
    std::string name;
    bool pass = false;
    std::string detail;
};

// ---- Helpers -------------------------------------------------------------

// SET_CONTEXT_REG packet: [0]=space offset, [1..]=values.
void PushSetContextReg(std::vector<uint32_t>& s, uint32_t spaceOffset,
                       const std::vector<uint32_t>& values,
                       uint32_t shaderType = 0) {
    s.push_back(Type3Header::Encode(kItSetContextReg, values.size() + 1,
                                    shaderType));
    s.push_back(spaceOffset);
    for (uint32_t v : values) s.push_back(v);
}

constexpr uint32_t kIndexTypeSel32 = 3;   // public GCN SelIdx32bit
constexpr uint32_t kIndexTypeSel16 = 2;   // public GCN SelIdx16bit

// ---- Subtests ------------------------------------------------------------

// 1. Register writes land in the context bank at absolute addresses.
SubResult TestSetContextReg() {
    SubResult r{"set_context_reg", false, ""};
    std::vector<uint32_t> stream;
    PushSetContextReg(stream, kCtxOffVgtDmaIndexType, {kIndexTypeSel32});
    PushSetContextReg(stream, 0x100, {0xDEAD, 0xBEEF});

    GnmState state; DecodeStats stats; std::vector<StreamError> errors;
    Pm4Decoder d;
    d.Decode(stream.data(), stream.size(), state, stats, &errors);

    bool w1 = false, w2 = false, w3 = false;
    const uint32_t v1 = state.ReadRegister(kRegVgtDmaIndexType, &w1);
    const uint32_t v2 = state.ReadRegister(kContextRegBase + 0x100, &w2);
    const uint32_t v3 = state.ReadRegister(kContextRegBase + 0x101, &w3);
    if (!errors.empty()) { r.detail = "unexpected stream errors: " +
                                       std::to_string(errors.size()); return r; }
    if (v1 != kIndexTypeSel32) { r.detail = "VGT_DMA_INDEX_TYPE wrong"; return r; }
    if (v2 != 0xDEAD || v3 != 0xBEEF) { r.detail = "bank values wrong"; return r; }
    if (stats.streamErrors != 0 || stats.totalPackets != 2) {
        r.detail = "stats mismatch: " + stats.SummaryString(); return r;
    }
    r.pass = true;
    return r;
}

// 2. INDEX_TYPE + NUM_INSTANCES + DRAW_INDEX_AUTO produce a full draw
//    record with captured state.
SubResult TestDrawAccounting() {
    SubResult r{"draw_accounting", false, ""};
    std::vector<uint32_t> stream;
    stream.push_back(Type3Header::Encode(kItIndexType, 1));
    stream.push_back(kIndexTypeSel32);
    stream.push_back(Type3Header::Encode(kItNumInstances, 1));
    stream.push_back(4);
    stream.push_back(Type3Header::Encode(kItDrawIndexAuto, 2));
    stream.push_back(100);   // vertex count
    stream.push_back(0x4);   // initiator raw

    GnmState state; DecodeStats stats; std::vector<StreamError> errors;
    Pm4Decoder d;
    d.Decode(stream.data(), stream.size(), state, stats, &errors);

    if (!errors.empty() || stats.drawCalls != 1) {
        r.detail = "stats mismatch: " + stats.SummaryString(); return r;
    }
    const auto& log = state.DrawLog();
    if (log.size() != 1) { r.detail = "draw log empty"; return r; }
    const auto& dr = log[0];
    if (dr.count != 100 || dr.instances != 4 ||
        dr.indexTypeRaw != kIndexTypeSel32 || dr.indexed) {
        r.detail = "draw record fields wrong"; return r;
    }
    if (state.DrawCalls() != 1 || state.NumInstances() != 4 ||
        state.IndexTypeRaw() != kIndexTypeSel32) {
        r.detail = "state counters wrong"; return r;
    }
    r.pass = true;
    return r;
}

// 3. DISPATCH_DIRECT records dims.
SubResult TestDispatch() {
    SubResult r{"dispatch_direct", false, ""};
    std::vector<uint32_t> stream;
    stream.push_back(Type3Header::Encode(kItDispatchDirect, 3, /*shaderType*/1));
    stream.push_back(8); stream.push_back(4); stream.push_back(2);

    GnmState state; DecodeStats stats; std::vector<StreamError> errors;
    Pm4Decoder d;
    d.Decode(stream.data(), stream.size(), state, stats, &errors);
    uint32_t y = 0, z = 0;
    const uint32_t x = state.LastDispatchDims(&y, &z);
    if (stats.computePackets != 1) { r.detail = "compute bit not counted"; return r; }
    if (state.Dispatches() != 1 || x != 8 || y != 4 || z != 2) {
        r.detail = "dispatch dims wrong"; return r;
    }
    if (!errors.empty()) { r.detail = "unexpected errors"; return r; }
    r.pass = true;
    return r;
}

// 4. Unknown opcodes are counted and skipped by declared body length; the
//    stream stays synchronized afterwards.
SubResult TestUnknownOpcode() {
    SubResult r{"unknown_opcode_skip", false, ""};
    std::vector<uint32_t> stream;
    stream.push_back(Type3Header::Encode(0xEE, 3));   // unknown, 4 body dwords
    stream.push_back(0x11111111);
    stream.push_back(0x22222222);
    stream.push_back(0x33333333);
    stream.push_back(Type3Header::Encode(kItNumInstances, 1));
    stream.push_back(7);

    GnmState state; DecodeStats stats; std::vector<StreamError> errors;
    Pm4Decoder d;
    d.Decode(stream.data(), stream.size(), state, stats, &errors);
    if (!errors.empty()) { r.detail = "unknown opcode flagged as error"; return r; }
    if (stats.unknownPackets != 1 ||
        stats.unknownOpcodeCounts.count(0xEE) != 1 ||
        stats.unknownOpcodeCounts.at(0xEE) != 1) {
        r.detail = "unknown accounting wrong: " + stats.SummaryString(); return r;
    }
    if (state.NumInstances() != 7) { r.detail = "stream desynchronized"; return r; }
    r.pass = true;
    return r;
}

// 5. NOP packets (min and non-min count) pass through cleanly.
SubResult TestNop() {
    SubResult r{"nop_passthrough", false, ""};
    std::vector<uint32_t> stream;
    stream.push_back(Type3Header::Encode(kItNop, 1));   // 1 body dword
    stream.push_back(0);
    stream.push_back(Type3Header::Encode(kItNop, 4));   // 4 body dwords
    for (uint32_t k = 0; k < 4; ++k) stream.push_back(k);

    GnmState state; DecodeStats stats; std::vector<StreamError> errors;
    Pm4Decoder d;
    d.Decode(stream.data(), stream.size(), state, stats, &errors);
    if (!errors.empty()) { r.detail = "NOP produced errors"; return r; }
    if (stats.opcodeCounts.count(kItNop) != 1 ||
        stats.opcodeCounts.at(kItNop) != 2) {
        r.detail = "NOP count wrong: " + stats.SummaryString(); return r;
    }
    r.pass = true;
    return r;
}

// 6. Truncated packet: decoder must stop with a recorded error and must
//    not read out of bounds (the vector below ends mid-body).
SubResult TestTruncation() {
    SubResult r{"truncation_bounds", false, ""};
    std::vector<uint32_t> stream;
    stream.push_back(Type3Header::Encode(kItDrawIndexAuto, 2));
    stream.push_back(100);
    // vector ends here — body declares 3 dwords, only 1 present.
    std::vector<PacketRecord> trace;
    GnmState state; DecodeStats stats; std::vector<StreamError> errors;
    Pm4Decoder d;
    const size_t stopped =
        d.Decode(stream.data(), stream.size(), state, stats, &errors, &trace);
    if (stats.streamErrors != 1 || errors.size() != 1) {
        r.detail = "truncation not reported: " + stats.SummaryString(); return r;
    }
    if (stopped != 0) {
        // Decode stops AT the truncated packet's header offset (0), not
        // past it — the header dword itself is never consumed as body.
        r.detail = "stop offset wrong: " + std::to_string(stopped); return r;
    }
    if (!errors.empty() && errors[0].dwordOffset != 0) {
        r.detail = "error offset wrong"; return r;
    }
    if (errors[0].what.find("truncated") == std::string::npos) {
        r.detail = "error text wrong: " + errors[0].what; return r;
    }
    r.pass = true;
    return r;
}

// 7. Non-Type-3 dword: recorded, skipped, stream continues.
SubResult TestNonType3() {
    SubResult r{"non_type3_resync", false, ""};
    std::vector<uint32_t> stream;
    stream.push_back(0x00000000);   // type 0 — not used by GNM
    stream.push_back(Type3Header::Encode(kItNumInstances, 1));
    stream.push_back(3);

    GnmState state; DecodeStats stats; std::vector<StreamError> errors;
    Pm4Decoder d;
    d.Decode(stream.data(), stream.size(), state, stats, &errors);
    if (stats.streamErrors != 1) { r.detail = "type-0 not reported"; return r; }
    if (state.NumInstances() != 3) { r.detail = "stream desynchronized"; return r; }
    r.pass = true;
    return r;
}

// 8. DRAW_INDEX_2 records an indexed draw (5-dword body).
SubResult TestDrawIndex2() {
    SubResult r{"draw_index_2", false, ""};
    std::vector<uint32_t> stream;
    stream.push_back(Type3Header::Encode(kItIndexType, 1));
    stream.push_back(kIndexTypeSel16);
    stream.push_back(Type3Header::Encode(kItDrawIndex2, 5));
    stream.push_back(0xFFFFF);  // max index count
    stream.push_back(0x1000);   // index buffer lo
    stream.push_back(0x0);      // index buffer hi
    stream.push_back(2048);     // num indices
    stream.push_back(0x2);      // initiator

    GnmState state; DecodeStats stats; std::vector<StreamError> errors;
    Pm4Decoder d;
    d.Decode(stream.data(), stream.size(), state, stats, &errors);
    if (!errors.empty() || stats.drawCalls != 1) {
        r.detail = "stats mismatch: " + stats.SummaryString(); return r;
    }
    const auto& dr = state.DrawLog();
    if (dr.size() != 1 || !dr[0].indexed || dr[0].count != 2048 ||
        dr[0].indexTypeRaw != kIndexTypeSel16) {
        r.detail = "indexed draw record wrong"; return r;
    }
    r.pass = true;
    return r;
}

// 9. Empty stream + SH register space writes.
SubResult TestEmptyAndShSpace() {
    SubResult r{"empty_stream_sh_space", false, ""};
    GnmState state; DecodeStats stats;
    Pm4Decoder d;
    d.Decode(nullptr, 0, state, stats, nullptr);
    if (stats.totalPackets != 0 || stats.streamErrors != 0) {
        r.detail = "empty stream not clean"; return r;
    }

    // SET_SH_REG with compute shaderType bit set.
    std::vector<uint32_t> stream;
    stream.push_back(Type3Header::Encode(kItSetShReg, 3, /*shaderType*/1));
    stream.push_back(0x10);      // space offset
    stream.push_back(0xCAFE);
    stream.push_back(0xF00D);
    std::vector<StreamError> errors;
    d.Decode(stream.data(), stream.size(), state, stats, &errors);
    bool w = false;
    const uint32_t v = state.ReadRegister(kShRegBase + 0x10, &w);
    if (!w || v != 0xCAFE) { r.detail = "SH bank value wrong"; return r; }
    if (state.ReadRegister(kShRegBase + 0x11, &w) != 0xF00D || !w) {
        r.detail = "SH bank second value wrong"; return r;
    }
    if (stats.computePackets != 1) { r.detail = "compute packet not counted"; return r; }
    r.pass = true;
    return r;
}

// 10. Submission pipeline through GnmSubmit (the seam the HLE hook uses):
//     buffer entry decodes into the owned state and evidence accumulates.
SubResult TestSubmitPipeline() {
    SubResult r{"submit_pipeline", false, ""};
    GnmSubmit::GetInstance().ResetForTest();

    std::vector<uint32_t> stream;
    stream.push_back(Type3Header::Encode(kItIndexType, 1));
    stream.push_back(kIndexTypeSel32);
    stream.push_back(Type3Header::Encode(kItNumInstances, 1));
    stream.push_back(2);
    stream.push_back(Type3Header::Encode(kItDrawIndexAuto, 2));
    stream.push_back(60);
    stream.push_back(0x4);

    std::string err;
    if (!GnmSubmit::GetInstance().SubmitBuffer(
            stream.data(), stream.size(), "selftest-cb", &err)) {
        r.detail = "SubmitBuffer rejected: " + err; return r;
    }
    const auto& st = GnmSubmit::GetInstance().Stats();
    if (st.drawCalls != 1 || GnmSubmit::GetInstance().Submissions() != 1 ||
        GnmSubmit::GetInstance().State().NumInstances() != 2) {
        r.detail = "pipeline state wrong: " +
                   GnmSubmit::GetInstance().GetStatsString(); return r;
    }
    if (GnmSubmit::GetInstance().GetStatsString().find("draws=1") ==
        std::string::npos) {
        r.detail = "stats string incomplete"; return r;
    }
    r.pass = true;
    return r;
}

// RAII guard: test-scoped resolver + log tag, restored on EVERY exit path
// (early returns included) — a leaked test resolver would silently change
// production resolution policy for the rest of the process lifetime.
struct TestResolveScope {
    TestResolveScope() {
        GnmSubmit::GetInstance().SetAddressResolverForTest(
            [](uint64_t addr, size_t) -> const void* {
                return reinterpret_cast<const void*>(addr);
            });
        GnmSubmit::GetInstance().SetLogTagForTest("[selftest] ");
    }
    ~TestResolveScope() {
        GnmSubmit::GetInstance().SetAddressResolverForTest(nullptr);
        GnmSubmit::GetInstance().SetLogTagForTest("");
    }
};

// 11. Descriptor path: provisional packing (addr=low48, dwords=high16)
//     resolves a host buffer and decodes it; bad descriptors produce NAMED
//     errors without crashing or decoding garbage.
SubResult TestSubmitDescriptors() {
    SubResult r{"submit_descriptors", false, ""};
    GnmSubmit::GetInstance().ResetForTest();

    // This test holds HOST pointers (std::vector data), not guest VAs. The
    // production resolver only resolves the guest window and returns
    // nullptr for anything else (named EFAULT) — so the test injects
    // identity resolution through the documented seam and tags its log
    // lines. No test may rely on raw pointer fallbacks ever again (that
    // fallback crashed real devices).
    TestResolveScope scope;

    std::vector<uint32_t> stream;
    stream.push_back(Type3Header::Encode(kItNumInstances, 1));
    stream.push_back(9);
    const uint64_t desc =
        (reinterpret_cast<uintptr_t>(stream.data()) & kDescAddrMask) |
        (static_cast<uint64_t>(stream.size()) << 48);
    // SubmitDescriptors takes a POINTER TO a descriptor array.
    const uint64_t descArray[1] = { desc };

    std::string err;
    const int64_t rc = GnmSubmit::GetInstance().SubmitDescriptors(
        1, reinterpret_cast<uintptr_t>(descArray), 0, &err);
    if (rc != 0) { r.detail = "descriptor submit failed: " + err; return r; }
    if (GnmSubmit::GetInstance().State().NumInstances() != 9) {
        r.detail = "descriptor decode wrong"; return r;
    }

    // Bad descriptor VALUE (dwords=0) held at a VALID address -> named
    // EINVAL, state untouched. The badness lives in the descriptor VALUE,
    // not in the array pointer — the array must stay readable or the
    // probe itself would crash (that would prove nothing).
    const uint64_t badDescArray[1] = { 0x0000000000000000ull };
    const int64_t bad = GnmSubmit::GetInstance().SubmitDescriptors(
        1, reinterpret_cast<uintptr_t>(badDescArray), 0, &err);
    if (bad >= 0) { r.detail = "bad descriptor accepted"; return r; }
    if (err.find("implausible") == std::string::npos) {
        r.detail = "error not named: " + err; return r;
    }
    r.pass = true;
    return r;
}

} // namespace

bool RunGnmSelfTest(std::string* report) {
    const SubResult results[] = {
        TestSetContextReg(),
        TestDrawAccounting(),
        TestDispatch(),
        TestUnknownOpcode(),
        TestNop(),
        TestTruncation(),
        TestNonType3(),
        TestDrawIndex2(),
        TestEmptyAndShSpace(),
        TestSubmitPipeline(),
        TestSubmitDescriptors(),
    };

    size_t passed = 0;
    std::string out;
    for (const auto& r : results) {
        passed += r.pass ? 1 : 0;
        out += r.pass ? "  [ok]   " : "  [FAIL] ";
        out += r.name;
        if (!r.pass && !r.detail.empty()) out += " — " + r.detail;
        out += "\n";
    }

    char head[128];
    std::snprintf(head, sizeof(head), "%s (%zu/%zu subtests passed)\n",
                  passed == sizeof(results) / sizeof(results[0])
                      ? "PASS" : "FAIL",
                  passed, sizeof(results) / sizeof(results[0]));
    out = "GNM PM4 decoder self-test: " + std::string(head) + out;

    if (report) *report = out;
    return passed == sizeof(results) / sizeof(results[0]);
}

} // namespace PX5::Gnm
