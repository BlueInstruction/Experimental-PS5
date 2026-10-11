// SPDX-License-Identifier: MIT
// PX5 Phase C milestone 2b — SceGnmDriver submission shim (implementation).
//
// Address resolution: on device the default path resolves guest VAs through
// the MemoryManager window (raw-host-pointer fallback mirrors the kernel
// HLE evidence-test policy). Host unit tests compile with PX5_HOST_TEST
// (identity fallback) and may inject a resolver via
// SetAddressResolverForTest; device behavior is byte-identical code.

#include "gpu/gnm/gnm_submit.h"

#include <cstdio>

#ifndef PX5_HOST_TEST
#include <memory/memory.h>
#endif
#include <utils/logger.h>

namespace {

using PX5::Gpu::GuestShaderStage;

[[maybe_unused]] const char* StageName(uint32_t stage) {
    switch (static_cast<GuestShaderStage>(stage)) {
        case GuestShaderStage::Compute:  return "CS";
        case GuestShaderStage::Vertex:   return "VS";
        case GuestShaderStage::Fragment: return "PS";
        default:                         return "?";
    }
}

} // namespace

namespace PX5::Gnm {

GnmSubmit& GnmSubmit::GetInstance() {
    static GnmSubmit inst;
    return inst;
}

void GnmSubmit::SetAddressResolverForTest(AddressResolver fn) {
    m_testResolver = std::move(fn);
}

const void* GnmSubmit::ResolveDefault(uint64_t addr, size_t bytes) const {
#ifdef PX5_HOST_TEST
    (void)bytes;
    return reinterpret_cast<const void*>(addr);
#else
    auto& mem = MemoryManager::GetInstance();
    if (mem.IsValidAddress(addr, bytes ? bytes : 1)) {
        return mem.GetHostPointer(addr);
    }
    // NO fallback. The previous "evidence-test fallback" reinterpreted the
    // raw guest VA as a host pointer and the first dereference killed the
    // whole process on real devices (SIGSEGV, no named error, no evidence).
    // An address outside the guest window is simply unresolvable: nullptr
    // -> caller emits a named EFAULT. Tests that legitimately hold HOST
    // pointers inject identity resolution via SetAddressResolverForTest.
    return nullptr;
#endif
}

const void* GnmSubmit::Resolve(uint64_t addr, size_t bytes) const {
    if (m_testResolver) return m_testResolver(addr, bytes);
    return ResolveDefault(addr, bytes);
}

void GnmSubmit::ResetForTest() {
    m_state.Reset();
    m_cumulative.Reset();
    m_submissions = 0;
    m_dwordsDecoded = 0;
    m_errors.clear();
    m_logTag = "";
    m_irScratch.Clear();
    m_lastLower.Reset();
    m_lastPlan.Reset();
    m_lastRpPlan.Reset();
    m_lowerings = 0;
    m_captureAttempts = 0;
    m_captureOk = 0;
    m_captureSkips = 0;
    for (auto& memo : m_stageMemo) memo = StageMemo{};
    m_captureOutcomes.clear();
    m_testShaderReader = nullptr;
    // m_shaderModuleSink deliberately survives ResetForTest: it is engine
    // configuration (the production module path), not test evidence.
    m_modulesMaterialized = 0;
    m_moduleFailures = 0;
}

bool GnmSubmit::SubmitBuffer(const uint32_t* dwords, size_t dwordCount,
                             const char* tag, std::string* errOut) {
    const char* name = tag ? tag : "submit";
    if (!dwords || dwordCount == 0) {
        const std::string what = std::string(name) + ": empty buffer";
        if (errOut) *errOut = what;
        if (m_errors.size() < 16) m_errors.push_back(what);
        return false;
    }
    if (dwordCount > kMaxSubmitDwords) {
        const std::string what = std::string(name) + ": buffer too large (" +
                                 std::to_string(dwordCount) + " dwords)";
        if (errOut) *errOut = what;
        if (m_errors.size() < 16) m_errors.push_back(what);
        return false;
    }

    // Decode into the owned state; run stats fold directly into the
    // cumulative evidence (counters add, opcode maps accumulate).
    std::vector<StreamError> errors;
    Pm4Decoder d;
    d.Decode(dwords, dwordCount, m_state, m_cumulative, &errors);

    ++m_submissions;
    m_dwordsDecoded += dwordCount;

    for (const auto& e : errors) {
        const std::string line = std::string(name) +
                                 " @" + std::to_string(e.dwordOffset) +
                                 ": " + e.what;
        if (m_errors.size() < 16) m_errors.push_back(line);
    }
    // Stream errors are evidence, not fatal: partial decode of a malformed
    // buffer still yields real stats. The error ring carries the reasons.

    // GPU-path evidence: the decoded state now walks the REAL chain that
    // ends at Vulkan — M6 lowering, M7 planner accounting, M8 shader
    // capture. Accounting only: nothing materializes, nothing renders
    // (the planner defers pipeline-needing ops by design; that deferral
    // count IS the honest distance to the first guest-driven frame).
    RunGpuPathEvidence();

    return true;
}

void GnmSubmit::RunGpuPathEvidence() {
    // ---- M6: cumulative state -> ordered IR (bounded list) ----------------
    m_lastLower = PX5::Gpu::LowerGnmStateToIR(m_state, m_irScratch);
    ++m_lowerings;

    // ---- M7: IR -> command-plan accounting (Vulkan-free) ------------------
    // The plan is NOT executed here: executing clear ops needs a target
    // image the runtime path does not own yet, and every pipeline-needing
    // op is deferred by the M7-T1-locked mapping. The stats ARE the point:
    // they turn "a game submitted N draws" into per-kind counters the
    // device log can quote, instead of a silent end at GnmState.
    m_lastPlan = PX5::Gpu::PlanVulkanCommands(m_irScratch).stats;

    // ---- M7-TBDR: render-pass structure over the same ops ---------------
    // GMEM region planning for tiled GPUs (every Adreno generation). Runs
    // BEFORE any vkCmdDraw exists: draws are still deferred, and when they
    // materialize they join this structure instead of naive immediate
    // submissions.
    m_lastRpPlan = PX5::Gpu::PlanRenderPasses(m_irScratch);

    // ---- M8: capture + recompile what the stream programmed ---------------
    // Binaries that ship no shader service (the x86_64 UI-smoke stub, and
    // the host HLE tests that link gnm_submit without the M8 stack) compile
    // this leg down to a named skip: lowering + planning still run,
    // captures stay at zero. Every real ABI (arm64 engine, the full host
    // gates) runs the full leg.
#ifndef PX5_NO_SHADER_SERVICE
    for (uint32_t s = 0; s < kCaptureStages; ++s) {
        const auto stage = static_cast<GuestShaderStage>(s);
        StageMemo& memo = m_stageMemo[s];

        const PX5::Gpu::ShaderCaptureResult captured =
            PX5::Gpu::ShaderCapture::ExtractShader(m_state, stage);
        if (!captured.hasProgram) continue;   // stream never loaded it

        // Memoization: same program + pinned success — the pin requires the
        // capture AND, when a sink is registered, the module hop — or the
        // retry budget spent -> skip. Never re-read a stable program per
        // submit; a module failure keeps the leg alive until the budget
        // pins it (bounded, named each time).
        const bool sameVaddr = memo.tried && memo.vaddr == captured.codeVaddr;
        const bool capturePinned = sameVaddr && memo.ok &&
                                   (!m_shaderModuleSink || memo.moduleOk);
        const bool budgetSpent   = sameVaddr &&
                                   memo.attempts >= kMaxCaptureAttemptsPerVaddr;
        if (capturePinned || budgetSpent) {
            ++m_captureSkips;
            continue;
        }

        if (!memo.tried || memo.vaddr != captured.codeVaddr) {
            memo = StageMemo{};
            memo.vaddr = captured.codeVaddr;
            memo.tried = true;
        }
        ++memo.attempts;
        ++m_captureAttempts;

        // Guest-memory reader: the test seam when installed, otherwise the
        // same resolution policy descriptors use (MemoryManager window on
        // device; all-or-nothing range check, named failure otherwise).
        PX5::Gpu::ShaderCapture::MemoryReader reader = m_testShaderReader;
        if (!reader) {
            reader = [this](uint64_t vaddr, uint32_t maxDwords,
                            std::vector<uint32_t>& out) {
                const size_t bytes =
                    static_cast<size_t>(maxDwords) * sizeof(uint32_t);
                const void* host = Resolve(vaddr, bytes);
                if (!host) return false;
                const uint32_t* dwords =
                    static_cast<const uint32_t*>(host);
                out.assign(dwords, dwords + maxDwords);
                return true;
            };
        }

        std::vector<uint32_t> spirv;
        std::string error;
        CaptureOutcome outcome;
        outcome.stage = s;
        outcome.codeVaddr = captured.codeVaddr;
        outcome.ok = PX5::Gpu::ShaderCapture::CaptureAndRecompile(
            m_state, stage, reader, spirv, error);
        if (outcome.ok) {
            outcome.spirvWords = static_cast<uint32_t>(spirv.size());
            memo.ok = true;
            ++m_captureOk;
            PX5_LOGI(LogCategory::GPU,
                     "%sGPU path: %s @0x%llX -> SPIR-V (%u words)",
                     m_logTag, StageName(s),
                     (unsigned long long)outcome.codeVaddr,
                     outcome.spirvWords);
            // Module hop: the guest SPIR-V crosses to a real VkShaderModule
            // through the registered sink (CreateShaderModuleFromWords).
            // With no sink this leg is a named absence, not a silence.
            if (m_shaderModuleSink) {
                std::string modErr;
                const bool modOk = m_shaderModuleSink(
                    s, outcome.codeVaddr, spirv.data(), spirv.size(),
                    modErr);
                if (modOk) {
                    memo.moduleOk = true;
                    ++m_modulesMaterialized;
                    PX5_LOGI(LogCategory::GPU,
                             "%sGPU path: %s @0x%llX -> VkShaderModule "
                             "(%u words)",
                             m_logTag, StageName(s),
                             (unsigned long long)outcome.codeVaddr,
                             outcome.spirvWords);
                } else {
                    ++m_moduleFailures;
                    PX5_LOGW(LogCategory::GPU,
                             "%sGPU path: %s @0x%llX VkShaderModule "
                             "failed: %s",
                             m_logTag, StageName(s),
                             (unsigned long long)outcome.codeVaddr,
                             modErr.c_str());
                }
            }
        } else {
            outcome.error = error;
            PX5_LOGI(LogCategory::GPU,
                     "%sGPU path: %s @0x%llX capture failed: %s",
                     m_logTag, StageName(s),
                     (unsigned long long)outcome.codeVaddr,
                     error.c_str());
        }
        if (m_captureOutcomes.size() >= kMaxCaptureOutcomes) {
            m_captureOutcomes.erase(m_captureOutcomes.begin());
        }
        m_captureOutcomes.push_back(std::move(outcome));
    }
#endif // PX5_UI_SMOKE_STUB
}

int64_t GnmSubmit::SubmitDescriptors(uint64_t count, uint64_t descriptorsPtr,
                                     uint64_t userDataPtr,
                                     std::string* errOut) {
    (void)userDataPtr;  // parsed; semantics land in later phases

    if (count == 0 || count > 32) {
        const std::string what = "sceGnmSubmitCommandBuffers: count=" +
                                 std::to_string(count) + " outside [1..32]";
        if (errOut) *errOut = what;
        if (m_errors.size() < 16) m_errors.push_back(what);
        return -static_cast<int64_t>(0x16 /*EINVAL*/);
    }

    const size_t arrayBytes = static_cast<size_t>(count) * sizeof(uint64_t);
    const void* descHost = Resolve(descriptorsPtr, arrayBytes);
    if (!descHost) {
        const std::string what =
            "sceGnmSubmitCommandBuffers: descriptor array unresolvable @0x" +
            [](uint64_t v) {
                char b[32]; std::snprintf(b, sizeof(b), "%llX",
                                          (unsigned long long)v);
                return std::string(b);
            }(descriptorsPtr);
        if (errOut) *errOut = what;
        if (m_errors.size() < 16) m_errors.push_back(what);
        return -static_cast<int64_t>(0xE /*EFAULT*/);
    }

    const uint64_t* descs = static_cast<const uint64_t*>(descHost);

    // Evidence-first: hex-log raw descriptors of every submission
    // (bounded). If the provisional packing is wrong, THIS is what
    // corrects it from real-device evidence.
    {
        std::string hex;
        for (uint64_t i = 0; i < count && i < 8; ++i) {
            char b[32];
            std::snprintf(b, sizeof(b), "%s%016llX", i ? " " : "",
                          (unsigned long long)descs[i]);
            hex += b;
        }
        PX5_LOGI(LogCategory::GPU,
                 "%ssceGnmSubmitCommandBuffers: count=%llu descriptors=[%s%s]",
                 m_logTag,
                 (unsigned long long)count, hex.c_str(),
                 count > 8 ? " ..." : "");
    }

    int64_t rc = 0;
    for (uint64_t i = 0; i < count; ++i) {
        const uint64_t desc    = descs[i];
        const uint64_t addr    = desc & kDescAddrMask;
        const uint64_t ndwords = desc >> 48;

        if (ndwords == 0 || ndwords > kMaxSubmitDwords) {
            const std::string what = "descriptor " + std::to_string(i) +
                                     ": dword count " +
                                     std::to_string(ndwords) +
                                     " implausible (packing may be wrong — "
                                     "raw desc logged above)";
            if (errOut) *errOut = what;
            if (m_errors.size() < 16) m_errors.push_back(what);
            rc = -static_cast<int64_t>(0x16 /*EINVAL*/);
            continue;
        }

        const size_t bytes = static_cast<size_t>(ndwords) * sizeof(uint32_t);
        const void* host = Resolve(addr, bytes);
        if (!host) {
            const std::string what = "descriptor " + std::to_string(i) +
                                     ": buffer address unresolvable @0x" +
                                     [](uint64_t v) {
                                         char b[32];
                                         std::snprintf(b, sizeof(b), "%llX",
                                                       (unsigned long long)v);
                                         return std::string(b);
                                     }(addr);
            if (errOut) *errOut = what;
            if (m_errors.size() < 16) m_errors.push_back(what);
            rc = -static_cast<int64_t>(0xE /*EFAULT*/);
            continue;
        }

        std::string subErr;
        SubmitBuffer(static_cast<const uint32_t*>(host),
                     static_cast<size_t>(ndwords),
                     ("cb[" + std::to_string(i) + "]").c_str(), &subErr);
    }
    return rc;
}

std::string GnmSubmit::GetStatsString() const {
    char head[192];
    std::snprintf(head, sizeof(head),
                  "submits=%llu dwords=%llu draws=%llu dispatches=%llu "
                  "regWrites=%llu streamErrors=%llu",
                  (unsigned long long)m_submissions,
                  (unsigned long long)m_dwordsDecoded,
                  (unsigned long long)m_cumulative.drawCalls,
                  (unsigned long long)m_cumulative.dispatches,
                  (unsigned long long)m_state.TotalRegisterWrites(),
                  (unsigned long long)m_cumulative.streamErrors);
    std::string out(head);
    // GPU-path evidence: the last lowering/planning pass over the real
    // submissions, plus lifetime capture accounting. These numbers are the
    // per-kind "deferred draw" truth — the honest distance to a frame.
    out += " | ir[last: ";
    out += m_lastLower.SummaryString();
    out += "] | plan[last: ";
    out += m_lastPlan.SummaryString();
    out += "] | rp[last: ";
    out += m_lastRpPlan.SummaryString();
    out += "] | captures=";
    out += std::to_string(m_captureAttempts);
    out += " ok=";
    out += std::to_string(m_captureOk);
    out += " skips=";
    out += std::to_string(m_captureSkips);
    // Per-stage capture ring (last 8, oldest first): "ok <words>w" or the
    // exact named failure. This is the T2 evidence a device session reads
    // straight from the stats line — no other log needed.
    out += " | cap[";
    for (size_t i = 0; i < m_captureOutcomes.size(); ++i) {
        const CaptureOutcome& o = m_captureOutcomes[i];
        if (i) out += ",";
        char stageHead[32];
        std::snprintf(stageHead, sizeof(stageHead), "%s@%llX:",
                      StageName(o.stage), (unsigned long long)o.codeVaddr);
        out += stageHead;
        if (o.ok) {
            out += std::to_string(o.spirvWords);
            out += "w";
        } else {
            out += "fail:";
            out += o.error.substr(0, 48);
        }
    }
    out += "]";
    out += " | modules=";
    out += std::to_string(m_modulesMaterialized);
    out += " moduleFails=";
    out += std::to_string(m_moduleFailures);
    return out;
}

} // namespace PX5::Gnm
