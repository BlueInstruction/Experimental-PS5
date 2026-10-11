// SPDX-License-Identifier: MIT
// PX5 Phase C milestone 2b — SceGnmDriver submission shim (real pipeline).
//
// WHAT THIS IS
//   The GPU-side counterpart of the kernel HLE layer: when a guest game
//   calls sceGnmSubmitCommandBuffers (the ONE GNM entry that carries the
//   PM4 command buffers, since GNM is statically linked and this is the
//   submit seam), the buffers flow: descriptor array -> address resolution
//   -> Pm4Decoder -> GnmState. That state becomes the GPU-IR input the
//   later Vulkan-emission phases will consume. Nothing here talks to
//   Vulkan yet — by design (dossier: GPU IR first, Vulkan as backend).
//
// PROVISIONAL ASSUMPTION (named, falsifiable):
//   Each uint64 command-buffer descriptor is decoded as
//       addr   = desc & ((1<<48)-1)
//       dwords = desc >> 48
//   Public RE sources describe the packing differently in places. We
//   therefore VALIDATE every decode (dwords in [1, 4 MiB], address
//   resolvable) and hex-log the raw descriptors of real submissions, so a
//   wrong assumption shows up as named validation errors + raw evidence,
//   never as silently misdecoded streams.
//
// Platform-independent C++ (host-testable); address resolution goes through
// the guest memory window (MemoryManager). An address that does not resolve
// is an ERROR (named EFAULT) — it is never reinterpreted as a raw host
// pointer. The old "evidence fallback" produced dereferenceable garbage and
// crashed the process on real devices (2026-08-29 A750 logs); tests that
// need identity resolution inject it via SetAddressResolverForTest.

#ifndef PX5_GPU_GNM_GNM_SUBMIT_H
#define PX5_GPU_GNM_GNM_SUBMIT_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "gpu/gnm/gnm_state.h"
#include "gpu/gnm/pm4_decoder.h"
// Vulkan-free app layers the submit path now feeds (docs/gpu.md chain):
// the M6 lowering (GnmState -> GpuOpList), the M7 planner (IR -> command
// plan, accounting only) and the M8 capture (GnmState -> SPIR-V). None of
// these headers names a Vulkan type — the IR header guard enforces it.
#include "gpu/ir/gpu_ir.h"
#include "gpu/shader/shader_capture.h"
#include "gpu/vulkan_backend.h"

namespace PX5::Gnm {

// Descriptor packing constants (see header comment).
constexpr uint64_t kDescAddrMask  = (1ull << 48) - 1;
constexpr uint32_t kMaxSubmitDwords = 4u * 1024 * 1024;   // 16 MB of dwords

class GnmSubmit {
public:
    static GnmSubmit& GetInstance();

    // Address-resolution seam. Default resolves through the guest memory
    // window (MemoryManager) ONLY; unresolvable addresses yield nullptr and
    // become named EFAULT errors. Host unit tests inject a passthrough
    // instead of linking the Android memory backend.
    using AddressResolver = std::function<const void*(uint64_t addr, size_t bytes)>;
    void SetAddressResolverForTest(AddressResolver fn);

    // Prefix for the descriptor hex-log line. Test code sets "[selftest] "
    // so its synthetic submissions can never be misread in a pasted GPU log
    // as real game traffic. Production leaves it empty.

    // HLE-shaped entry: an array of `count` uint64 descriptors.
    // Returns SCE-style 0 on success, negative errno-style on failure
    // with `errOut` carrying the named reason.
    int64_t SubmitDescriptors(uint64_t count, uint64_t descriptorsPtr,
                              uint64_t userDataPtr, std::string* errOut);

    // Direct buffer entry (tests + callers that already hold the dwords).
    bool SubmitBuffer(const uint32_t* dwords, size_t dwordCount,
                      const char* tag, std::string* errOut);

    // Cumulative evidence.
    std::string GetStatsString() const;
    const GnmState&  State() const { return m_state; }
    const DecodeStats& Stats() const { return m_cumulative; }
    uint64_t Submissions() const { return m_submissions; }
    const std::vector<std::string>& LastErrors() const { return m_errors; }
    bool EmptySoFar() const { return m_submissions == 0; }

    // ---- GPU-path evidence (the M6/M7/M8 wiring, run per submission) ------
    //
    // After every decoded buffer the submit path now walks the REAL chain
    // that ends at Vulkan — accounting only, nothing renders yet:
    //   GnmState --LowerGnmStateToIR--> GpuOpList      (M6 lowering)
    //            --PlanVulkanCommands--> BackendPlanStats (M7, deferred draws
    //                                     counted, never materialized)
    //            --ShaderCapture::CaptureAndRecompile--> SPIR-V or a named
    //                                     failure per stage (M8)
    // The GpuOpList and the plan are the LAST submission's (cumulative
    // state lowers cumulatively — the lists are bounded); capture outcomes
    // accumulate below.
    const PX5::Gpu::LowerStats&       LastLowerStats() const { return m_lastLower; }
    const PX5::Gpu::BackendPlanStats& LastPlanStats()  const { return m_lastPlan; }
    uint64_t Lowerings()     const { return m_lowerings; }
    uint64_t CaptureAttempts() const { return m_captureAttempts; }
    uint64_t CaptureOk()       const { return m_captureOk; }
    uint64_t CaptureSkips()    const { return m_captureSkips; }

    // One bounded capture outcome (last 8, most recent last). ok=false
    // carries the exact named failure — the evidence a device session
    // needs to tell "no shader yet" from "code unreadable" from
    // "recompiler rejected".
    struct CaptureOutcome {
        bool     ok = false;
        uint32_t stage = 0;          // GuestShaderStage value
        uint64_t codeVaddr = 0;
        uint32_t spirvWords = 0;
        std::string error;
    };
    const std::vector<CaptureOutcome>& CaptureOutcomes() const {
        return m_captureOutcomes;
    }

    void ResetForTest();
    void SetLogTagForTest(const char* tag) { m_logTag = tag ? tag : ""; }
    // Test seam: override the guest-memory reader the capture uses (device
    // default resolves through the same window as descriptors).
    void SetShaderMemoryReaderForTest(
        PX5::Gpu::ShaderCapture::MemoryReader fn) {
        m_testShaderReader = std::move(fn);
    }

    // ---- Shader-module sink (M8 T5 caller seam) -------------------------
    //
    // VulkanGpuDevice::CreateShaderModuleFromWords existed with NO caller:
    // guest SPIR-V stopped at the capture leg. The engine registers this
    // sink (Emulator::Initialize) and every successful REAL capture then
    // materializes a VkShaderModule from the guest words — the exact
    // "SPIR-V from the real guest -> shader module" hop, counted and
    // logged per stage. Absent sink = named absence (accounting only);
    // a failing sink is evidence too (named error, bounded retries via
    // the same per-vaddr memo budget as the capture).
    // Not cleared by ResetForTest: it is configuration, not evidence —
    // an in-process self-test must not silently un-register the
    // production path for the next real boot in the same process.
    using ShaderModuleSink = std::function<bool(
        uint32_t stage, uint64_t codeVaddr, const uint32_t* spirvWords,
        size_t wordCount, std::string& err)>;
    void SetShaderModuleSink(ShaderModuleSink fn) {
        m_shaderModuleSink = std::move(fn);
    }
    uint64_t ModulesMaterialized() const { return m_modulesMaterialized; }
    uint64_t ModuleFailures()     const { return m_moduleFailures; }

private:
    GnmSubmit() = default;

    // Guest-window resolution (default policy; Android memory backend).
    const void* ResolveDefault(uint64_t addr, size_t bytes) const;
    // Active resolution path (test override or default).
    const void* Resolve(uint64_t addr, size_t bytes) const;

    // The per-submission GPU-path walk (lowering + planner + capture).
    void RunGpuPathEvidence();

    // Per-stage capture memoization: a (vaddr -> attempts/ok) record so a
    // stable program is not re-read from guest memory on every submit.
    // Success pins the vaddr until the stream loads a different program;
    // failure retries are bounded (kMaxCaptureAttemptsPerVaddr) so an
    // unreadable page cannot turn into per-submit cost.
    struct StageMemo {
        uint64_t vaddr = 0;
        uint32_t attempts = 0;
        bool     ok = false;
        bool     tried = false;
        bool     moduleOk = false;   // sink delivered a VkShaderModule
    };
    static constexpr uint32_t kCaptureStages = 3;   // CS, VS, PS
    static constexpr uint32_t kMaxCaptureAttemptsPerVaddr = 3;
    static constexpr size_t   kMaxCaptureOutcomes = 8;

    GnmState    m_state;
    DecodeStats m_cumulative;
    uint64_t    m_submissions = 0;
    uint64_t    m_dwordsDecoded = 0;
    std::vector<std::string> m_errors;   // bounded ring (last 16)
    AddressResolver m_testResolver;      // empty => default policy
    const char* m_logTag = "";           // hex-log prefix ("[selftest] " in tests)

    // GPU-path evidence state (see the accessors above).
    PX5::Gpu::GpuOpList        m_irScratch{1024};  // bounded lowering target
    PX5::Gpu::LowerStats       m_lastLower;
    PX5::Gpu::BackendPlanStats m_lastPlan;
    uint64_t         m_lowerings = 0;
    uint64_t         m_captureAttempts = 0;
    uint64_t         m_captureOk = 0;
    uint64_t         m_captureSkips = 0;
    StageMemo        m_stageMemo[kCaptureStages];
    std::vector<CaptureOutcome> m_captureOutcomes;  // bounded ring (last 8)
    PX5::Gpu::ShaderCapture::MemoryReader m_testShaderReader;  // empty => default
    ShaderModuleSink m_shaderModuleSink;             // empty => accounting only
    uint64_t         m_modulesMaterialized = 0;
    uint64_t         m_moduleFailures = 0;
};

} // namespace PX5::Gnm

#endif // PX5_GPU_GNM_GNM_SUBMIT_H
