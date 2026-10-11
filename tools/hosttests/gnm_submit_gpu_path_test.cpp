// GPU-path gate: the REAL submit path (GnmSubmit::SubmitBuffer — the TU the
// APK's sceGnmSubmitCommandBuffers serves) now walks the full chain that
// ends at Vulkan, per submission:
//
//   PM4 stream -> Pm4Decoder -> GnmState  (M5, existing)
//              -> LowerGnmStateToIR       (M6 lowering, was host-test-only)
//              -> PlanVulkanCommands      (M7 planner accounting)
//              -> ShaderCapture::CaptureAndRecompile (M8, real vendored
//                                                     recompiler, SPIR-V out)
//
// What this gate proves: the decoded state reaches the IR and the planner
// during normal (non-test) submissions, with per-kind deferral counters,
// and guest-programmed shader state (SET_SH_REG PGM pair + user data)
// reaches the recompiler through the capture layer.
//
// What this gate does NOT prove (labels stay honest): the shaders here are
// the M8 T1/T2 FIXTURE programs delivered through real PM4 packets — a
// synthetic stream, not a game dump. Nothing renders, no Vulkan call is
// made, and no frame exists. The deferral counters ARE the evidence that
// draws stop at the planner today.
#include "gpu/gnm/gnm_submit.h"
#include "gpu/gnm/pm4_packet.h"
#include "gpu/gnm/sh_registers.h"
#include "gpu/ir/gpu_ir.h"
#include "gpu/shader/shader_recompiler_service.h"
#include "gpu/vulkan_backend.h"

#include <cstdint>
#include <cstdio>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using PX5::Gnm::GnmSubmit;
using PX5::Gnm::Type3Header;
using PX5::Gpu::OpKind;
using PX5::Gpu::GuestShaderStage;

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

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error("submit gpu path: " + message);
}

// Map-backed guest shader memory (the capture reader seam).
using GuestMemory = std::map<uint64_t, std::vector<uint32_t>>;

PX5::Gpu::ShaderCapture::MemoryReader MapReader(GuestMemory& mem) {
    return [&mem](uint64_t vaddr, uint32_t maxDwords,
                  std::vector<uint32_t>& out) {
        const auto it = mem.find(vaddr);
        if (it == mem.end()) return false;
        out = it->second;
        if (out.size() > maxDwords) out.resize(maxDwords);
        return true;
    };
}

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

void Append(std::vector<uint32_t>& stream,
            const std::vector<uint32_t>& tail) {
    stream.insert(stream.end(), tail.begin(), tail.end());
}

// Compute block: NUM_THREAD + PGM pair + RSRC1/2 + user data + DISPATCH.
std::vector<uint32_t> BuildComputeBlock(uint64_t codeVaddr) {
    std::vector<uint32_t> stream;
    Append(stream, SetShReg(PX5::Gnm::Sh::kComputeNumThreadX, {64u, 1u, 1u}));
    const uint32_t pgmLo =
        static_cast<uint32_t>((codeVaddr >> 8) & 0xFFFFFFFFu);
    const uint32_t pgmHiJunk = 0xABCD0000u |
        static_cast<uint32_t>((codeVaddr >> 40) & 0xFFu);
    Append(stream, SetShReg(PX5::Gnm::Sh::kComputePgmLo, {pgmLo, pgmHiJunk}));
    Append(stream, SetShReg(PX5::Gnm::Sh::kComputeRsrc1, {0u, 9u << 1}));
    Append(stream, SetShReg(PX5::Gnm::Sh::kComputeUserData0,
                            {0x11223344u, 0x00040010u, 64u, 0x01016facu,
                             0x55667788u, 0x00040010u, 64u, 0x01016facu,
                             1u}));
    // DISPATCH_DIRECT: real four-dword body; initiator bit 15 = wave32.
    stream.push_back(
        Type3Header::Encode(PX5::Gnm::kItDispatchDirect, 4, 1u));
    stream.push_back(64u);
    stream.push_back(1u);
    stream.push_back(1u);
    stream.push_back(0x8000u);   // wave32
    return stream;
}

// Vertex block: PGM pair + RSRC1/2 (USER_SGPR=2) + user data + one
// DRAW_INDEX_AUTO, with a scissor set mid-stream (the named-write op).
std::vector<uint32_t> BuildVertexBlock(uint64_t codeVaddr, uint32_t count) {
    std::vector<uint32_t> stream;
    const uint32_t pgmLo =
        static_cast<uint32_t>((codeVaddr >> 8) & 0xFFFFFFFFu);
    Append(stream, SetShReg(PX5::Gnm::Sh::kVsBase + PX5::Gnm::Sh::kOffPgmLo,
                            {pgmLo, 0u}));
    Append(stream, SetShReg(PX5::Gnm::Sh::kVsBase + PX5::Gnm::Sh::kOffRsrc1,
                            {0u, 2u << 1}));
    Append(stream, SetShReg(PX5::Gnm::Sh::kVsBase +
                                PX5::Gnm::Sh::kOffUserData0,
                            {0x0BADF00Du, 0x00040010u}));
    Append(stream, SetReg(PX5::Gnm::kItSetContextReg,
                          PX5::Gnm::kCtxOffPaScScreenScissorTL,
                          {0x00200010u}));
    Append(stream, SetReg(PX5::Gnm::kItSetContextReg,
                          PX5::Gnm::kCtxOffPaScScreenScissorBR,
                          {0x00400180u}));
    stream.push_back(
        Type3Header::Encode(PX5::Gnm::kItDrawIndexAuto, 2));
    stream.push_back(count);
    stream.push_back(2u);
    return stream;
}

const PX5::Gnm::GnmSubmit::CaptureOutcome* FindOutcome(
    const std::vector<PX5::Gnm::GnmSubmit::CaptureOutcome>& outcomes,
    uint32_t stage, bool ok) {
    for (const auto& o : outcomes) {
        if (o.stage == stage && o.ok == ok) return &o;
    }
    return nullptr;
}

} // namespace

int main() {
    try {
        auto& submit = GnmSubmit::GetInstance();
        submit.ResetForTest();

        GuestMemory mem;
        constexpr uint64_t kCsVaddr = 0x112345600ull;   // 256B aligned
        constexpr uint64_t kVsVaddr = 0x22000000ull;    // 256B aligned
        mem[kCsVaddr] =
            std::vector<uint32_t>(ComputeCode.begin(), ComputeCode.end());
        mem[kVsVaddr] =
            std::vector<uint32_t>(VertexCode.begin(), VertexCode.end());
        submit.SetShaderMemoryReaderForTest(MapReader(mem));
        submit.SetLogTagForTest("[submit-gpu-path] ");

        // ---- Submission 1: CS + dispatch, then VS + scissor + draw -------
        std::vector<uint32_t> stream1 = BuildComputeBlock(kCsVaddr);
        Append(stream1, BuildVertexBlock(kVsVaddr, 6u));
        std::string err;
        Require(submit.SubmitBuffer(stream1.data(), stream1.size(),
                                    "cb[0]", &err),
                "submission 1 rejected: " + err);

        Require(submit.Lowerings() == 1u, "one lowering after one submit");

        // IR: Dispatch, SetScissor, Draw, Barrier (seq order) = 4 ops.
        const PX5::Gpu::LowerStats& ls = submit.LastLowerStats();
        Require(ls.opsEmitted == 4u, "expected 4 IR ops, got " +
                                         std::to_string(ls.opsEmitted));
        Require(ls.dispatchOps == 1u && ls.drawOps == 1u &&
                    ls.scissorOps == 1u && ls.barrierOps == 1u,
                "per-kind lowering counts wrong");
        Require(ls.droppedOps == 0u, "ops dropped at capacity");

        // Plan: every op accounted, none guessed into commands.
        const PX5::Gpu::BackendPlanStats& ps = submit.LastPlanStats();
        Require(ps.opsReceived == 4u, "planner received 4 ops");
        Require(ps.boundaryOps == 1u, "one submit boundary");
        Require(ps.drawOpsDeferred == 1u && ps.dispatchOpsDeferred == 1u &&
                    ps.scissorOpsDeferred == 1u,
                "pipeline-needing ops deferred by kind");
        Require(ps.DeferredTotal() == 3u, "deferred total = 3");
        Require(ps.unknownOps == 0u && ps.clearOps == 0u,
                "no unknown/clear ops in this stream");

        // Captures: CS + VS through the REAL vendored recompiler.
        Require(submit.CaptureAttempts() == 2u,
                "two capture attempts (CS + VS)");
        Require(submit.CaptureOk() == 2u, "both captures recompiled");
        const auto& outcomes = submit.CaptureOutcomes();
        Require(outcomes.size() == 2u, "two outcomes recorded");
        const auto* csOut = FindOutcome(
            outcomes, static_cast<uint32_t>(GuestShaderStage::Compute), true);
        const auto* vsOut = FindOutcome(
            outcomes, static_cast<uint32_t>(GuestShaderStage::Vertex), true);
        Require(csOut && csOut->spirvWords > 5u && csOut->codeVaddr == kCsVaddr,
                "CS SPIR-V words/vaddr wrong");
        Require(vsOut && vsOut->spirvWords > 5u && vsOut->codeVaddr == kVsVaddr,
                "VS SPIR-V words/vaddr wrong");

        // ---- Submission 2: the SAME stream — memoized, no re-read --------
        Require(submit.SubmitBuffer(stream1.data(), stream1.size(),
                                    "cb[0]", &err),
                "submission 2 rejected");
        Require(submit.Lowerings() == 2u, "second lowering ran");
        // Cumulative state: 2 dispatches + 2 scissors + 2 draws + barrier.
        Require(submit.LastLowerStats().opsEmitted == 7u,
                "cumulative lowering = 7 ops");
        Require(submit.LastPlanStats().opsReceived == 7u &&
                    submit.LastPlanStats().DeferredTotal() == 6u,
                "cumulative plan accounts all ops");
        Require(submit.CaptureAttempts() == 2u,
                "no new capture attempts for stable programs");
        Require(submit.CaptureSkips() == 2u, "two memoized skips (CS + VS)");
        Require(submit.CaptureOk() == 2u, "capture successes unchanged");

        // ---- Submissions 3-6: unreadable CS code — bounded retries -------
        // New CS vaddr NOT in the map: the reader fails with a named error.
        // Retry budget per vaddr = 3 attempts, then skip. VS stays pinned.
        constexpr uint64_t kMissingVaddr = 0x33000000ull;
        std::vector<uint32_t> streamBad = BuildComputeBlock(kMissingVaddr);
        for (int i = 0; i < 4; ++i) {
            Require(submit.SubmitBuffer(streamBad.data(), streamBad.size(),
                                        "cb[bad]", &err),
                    "submission (bad) rejected");
        }
        // Attempts: 2 (S1) + 3 (bad CS, capped) = 5. Skips: 2 (S2) +
        // 1 (S3, VS pinned) + 1 (S4) + 1 (S5) + 2 (S6: CS budget + VS) = 7.
        Require(submit.CaptureAttempts() == 5u,
                "retry budget capped at 3 per vaddr (attempts=" +
                    std::to_string(submit.CaptureAttempts()) + ")");
        Require(submit.CaptureSkips() == 7u,
                "memoized skips counted (skips=" +
                    std::to_string(submit.CaptureSkips()) + ")");
        Require(submit.CaptureOk() == 2u, "no false successes");

        const auto* badOut = FindOutcome(
            submit.CaptureOutcomes(),
            static_cast<uint32_t>(GuestShaderStage::Compute), false);
        Require(badOut && badOut->codeVaddr == kMissingVaddr &&
                    badOut->error.find("unreadable") != std::string::npos,
                "failure outcome carries the named unreadable-code reason");

        // Outcome ring stays bounded (last 8).
        Require(submit.CaptureOutcomes().size() <= 8u, "outcome ring bounded");

        // ---- Stats string carries the evidence ---------------------------
        // plan[last:] names each deferred kind (T2: "deferred per type"),
        // cap[...] names the per-stage capture outcomes (SPIR-V words or
        // the named error).
        const std::string stats = submit.GetStatsString();
        Require(stats.find("ir[last:") != std::string::npos &&
                    stats.find("plan[last:") != std::string::npos &&
                    stats.find("captures=5") != std::string::npos &&
                    stats.find("ok=2") != std::string::npos,
                "stats string carries the GPU-path evidence");
        // Last plan is cumulative (2 dispatches from stream1 + 4 from
        // streamBad = 6 dispatches, 2 scissors, 2 draws, 1 boundary).
        Require(stats.find("deferred=10(scissor=2 draw=2 drawIndexed=0 "
                           "dispatch=6)") != std::string::npos,
                "plan summary splits the deferral by kind (" + stats + ")");
        Require(stats.find("cap[CS@") != std::string::npos &&
                    stats.find("w,VS@") != std::string::npos &&
                    stats.find("w,") != std::string::npos &&
                    stats.find("fail:shader capture: code unreadable @0x") !=
                        std::string::npos,
                "stats string carries the per-stage capture ring (" +
                    stats + ")");

        submit.ResetForTest();
        Require(submit.Lowerings() == 0u && submit.CaptureAttempts() == 0u,
                "reset clears the GPU-path evidence");

        // ---- Module hop: the orphaned layer gets its caller -------------
        // CreateShaderModuleFromWords had no caller; the sink is the seam.
        // Success path: fresh stream -> CS + VS captures -> two sink calls
        // with the SPIR-V words; second identical submit is pinned (capture
        // AND module) and never reaches the sink again.
        std::vector<std::pair<uint32_t, size_t>> moduleCalls;
        bool moduleFail = false;
        submit.SetShaderMemoryReaderForTest(MapReader(mem));
        submit.SetLogTagForTest("[submit-gpu-path] ");
        submit.SetShaderModuleSink(
            [&](uint32_t stage, uint64_t vaddr, const uint32_t* words,
                size_t n, std::string& err) -> bool {
                (void)vaddr;
                Require(words != nullptr && n > 5u,
                        "sink receives the SPIR-V words");
                moduleCalls.push_back({stage, n});
                if (moduleFail) {
                    err = "hosttest: fake device unavailable";
                    return false;
                }
                return true;
            });

        Require(submit.SubmitBuffer(stream1.data(), stream1.size(),
                                    "cb[0]", &err),
                "sink-path submission 1 rejected: " + err);
        Require(moduleCalls.size() == 2u,
                "sink called once per captured stage (CS + VS), got " +
                    std::to_string(moduleCalls.size()));
        Require(submit.ModulesMaterialized() == 2u &&
                    submit.ModuleFailures() == 0u,
                "module counters track the sink");

        const size_t callsAfterFirst = moduleCalls.size();
        Require(submit.SubmitBuffer(stream1.data(), stream1.size(),
                                    "cb[0]", &err),
                "sink-path submission 2 rejected");
        Require(moduleCalls.size() == callsAfterFirst,
                "pinned capture + module skips the sink on replay");
        Require(submit.CaptureSkips() == 2u, "replay counted as skips");

        // Failure path: a module failure is NOT pinned — the leg retries
        // within the same per-vaddr budget (3), then pins by budget.
        moduleFail = true;
        constexpr uint64_t kSinkVaddr = 0x44000000ull;
        mem[kSinkVaddr] =
            std::vector<uint32_t>(ComputeCode.begin(), ComputeCode.end());
        std::vector<uint32_t> streamSink =
            BuildComputeBlock(kSinkVaddr) /* CS only */;
        for (int i = 0; i < 5; ++i) {
            Require(submit.SubmitBuffer(streamSink.data(),
                                        streamSink.size(),
                                        "cb[sink]", &err),
                    "sink-fail submission rejected");
        }
        // First sink stream made 2 calls (CS + VS success path); the
        // failing CS added exactly 3 (budget), replays added none.
        Require(moduleCalls.size() == 5u,
                "module failure retries bounded to the budget (calls=" +
                    std::to_string(moduleCalls.size()) + ")");
        Require(submit.ModulesMaterialized() == 2u,
                "successes unchanged by the failing stage");
        Require(submit.ModuleFailures() == 3u,
                "module failures counted (fails=" +
                    std::to_string(submit.ModuleFailures()) + ")");

        const std::string stats3 = submit.GetStatsString();
        Require(stats3.find("modules=2 moduleFails=3") != std::string::npos,
                "stats string carries the module counters (" + stats3 + ")");
        // TBDR/GMEM plan over the cumulative IR: the stream order is
        // dispatch, scissor, draw (x2) then five dispatches + the barrier,
        // so the scissor+draw regions are TWO passes (each closed by the
        // next dispatch/boundary); every dispatch stays outside (compute
        // has no GMEM).
        Require(stats3.find("rp[last: passes=2 drawOps=2 clearOps=0 "
                            "inPasses=4 outside=8") != std::string::npos,
                "stats string carries the render-pass plan (" + stats3 + ")");

        submit.SetShaderModuleSink(nullptr);
        submit.ResetForTest();

        printf("\nGNM submit GPU-path gate PASS\n");
        printf("  IR: 4 ops (dispatch/scissor/draw/barrier) from a real "
               "PM4 stream\n");
        printf("  plan: 3 pipeline-needing ops DEFERRED by kind, 1 boundary,"
               " 0 guessed\n");
        printf("  captures: CS + VS fixtures -> SPIR-V via the vendored "
               "recompiler (synthetic programs, not game shaders)\n");
        printf("  memoization: stable programs skip; unreadable code "
               "retries <= 3 then skips, named error kept\n");
        printf("  labels: accounting evidence — no Vulkan call, no pixels, "
               "no frame\n");
        return 0;
    } catch (const std::exception& e) {
        printf("GNM submit GPU-path gate FAIL: %s\n", e.what());
        return 1;
    }
}
