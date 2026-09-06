// SPDX-License-Identifier: MIT
// PX5 M7 — Vulkan backend: the layer between the GPU IR and Vulkan.
//
// WHAT THIS IS
//   docs/gpu.md's layer chain names "vulkan_backend (M7) — render ops →
//   Vulkan". This header commits that layer's platform-independent core:
//
//     GpuOpList (M6 IR)  ->  PlanVulkanCommands  ->  VulkanCommandPlan
//                                                    |
//                                        vulkan_device.cpp materializes
//                                        the plan with real Vulkan calls
//                                        and verifies the readback with
//                                        VerifyClearReadback.
//
//   The backend consumes the M6 IR types (GpuOp/GpuOpList) and NEVER any
//   GNM/PM4 type — the layer contract stays: PM4 -> GnmState -> IR ->
//   backend, and the backend does not know GNM existed.
//
// WHY THE PLAN IS VULKAN-FREE C++
//   The op->command mapping is exactly the part that must be provable
//   WITHOUT a device (docs/testing.md: M7's readback gate is T3/on-device,
//   but the mapping itself is deterministic logic). Keeping the plan types
//   free of Vulkan headers lets tools/hosttests/vulkan_backend_test.cpp
//   lock the mapping byte-for-byte on host — the same T1 pattern as M5/M6.
//   The device side (vulkan_device.cpp, which includes vulkan headers)
//   executes THIS plan; it cannot drift from what the host gate locked.
//
// HONEST SCOPE (what the planner materializes today)
//   kClear    -> one kPipelineBarrier (UNDEFINED -> TRANSFER_DST_OPTIMAL)
//                + one kClearColorImage command, per Clear op.
//   kBarrier  (scope 0, the submit boundary the M6 lowering emits) -> one
//                kSubmitBoundary command (end -> submit -> fence).
//   kSetScissor / kDraw / kDrawIndexed / kDispatch -> DEFERRED, counted
//                per kind in BackendPlanStats. They need pipelines /
//                dynamic state that do not exist until the decoder deepens
//                their register semantics (M5 PARTIAL notes) and M8 lands
//                shader binaries. Counted, never guessed into commands.
//   Anything else (vocabulary ops with no emitter, barrier scopes != 0)
//                -> unknownOps, counted, never guessed.
//
//   The M6 lowering emits no Clear today — a Clear op reaching this backend
//   comes from an explicitly-labelled synthetic IR list (the M7 device
//   proof builds exactly that, and its detail line says so). That proves
//   the BACKEND gate, not the decoder; docs/milestones.md keeps the two
//   claims separate.

#ifndef PX5_GPU_VULKAN_BACKEND_H
#define PX5_GPU_VULKAN_BACKEND_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "gpu/ir/gpu_ir.h"

namespace PX5::Gpu {

// ---------------------------------------------------------------------------
// One planned Vulkan command. Plain POD with default initializers (same
// safety rule as GpuOp: every field has a defined value for every command).
// ---------------------------------------------------------------------------
struct VulkanCommand {
    enum class Kind : uint32_t {
        kPipelineBarrier = 0,  // UNDEFINED -> TRANSFER_DST_OPTIMAL image
                               // barrier, before the clear (or after it,
                               // when the consumer transitions for readback)
        kClearColorImage,      // clearColor -> the target image
        kSubmitBoundary,       // end -> submit -> fence-wait (IR Barrier op)
    };

    Kind     kind = Kind::kSubmitBoundary;
    // Provenance: the seq of the GpuOp this command came from. One op can
    // yield two commands (a Clear yields barrier + clear) — both carry the
    // op's seq so the device log can attribute every command to its op.
    uint64_t seq = 0;
    // Read by kClearColorImage: the clear color as host floats, verbatim
    // from the GpuOp (NOT a Vulkan type — vulkan_device.cpp copies it into
    // a VkClearColorValue at materialization time).
    float    clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
};

// ---------------------------------------------------------------------------
// Planner evidence (docs/testing.md: numbers or it did not happen).
// Invariant, locked by the host gate:
//   opsReceived == clearOps + boundaryOps + DeferredTotal() + unknownOps
// ---------------------------------------------------------------------------
struct BackendPlanStats {
    uint64_t opsReceived = 0;            // GpuOps walked (== GpuOpList::Size()
                                         //  when the list was not mutated)
    uint64_t clearOps      = 0;          // -> barrier + clear commands
    uint64_t boundaryOps   = 0;          // Barrier scope 0 -> kSubmitBoundary
    uint64_t scissorOpsDeferred   = 0;   // needs dynamic state on a pipeline
    uint64_t drawOpsDeferred      = 0;   // needs a bound graphics pipeline
    uint64_t drawIndexedOpsDeferred = 0; // pipeline + index buffer binding
    uint64_t dispatchOpsDeferred  = 0;   // needs a compute pipeline
    // Barrier scope != 0 (reserved in the IR), or an op kind with no named
    // materialization. Counted, never guessed into commands.
    uint64_t unknownOps = 0;

    uint64_t DeferredTotal() const {
        return scissorOpsDeferred + drawOpsDeferred + drawIndexedOpsDeferred +
               dispatchOpsDeferred;
    }

    void Reset();
    std::string SummaryString() const;
};

// ---------------------------------------------------------------------------
// The planned command sequence plus the readback expectation it implies.
// ---------------------------------------------------------------------------
struct VulkanCommandPlan {
    std::vector<VulkanCommand> commands;
    BackendPlanStats stats;

    // Readback expectation: the LAST Clear op in the list wins. Clears are
    // ordered work on the same image; after the sequence, the image holds
    // the final clear's color. Locked by the host gate (two-clear case).
    bool    hasClear = false;
    float   clearF32[4]  = {0.0f, 0.0f, 0.0f, 0.0f};  // verbatim from the op
    uint8_t clearRgba[4] = {0, 0, 0, 0};  // UNORM-8, byte order R, G, B, A
                                          // (VK_FORMAT_R8G8B8A8_UNORM memory
                                          //  layout = byte per channel, R first)
};

// float -> UNORM-8: clamp to [0,1] (NaN clamps to 0), scale by 255, round
// half up, truncate. Deterministic by contract: the device readback and
// this conversion MUST agree byte-for-byte, so the rule lives here and
// nowhere else. IEEE-754 single-precision makes the float expression
// (f * 255.0f + 0.5f) exact and compiler-independent for every float input.
uint8_t ClearFloatToUnorm8(float f);

// The M7 planner: GpuOpList -> ordered VulkanCommandPlan. Does not mutate
// `ops`. See the header comment for the honest per-op mapping.
VulkanCommandPlan PlanVulkanCommands(const GpuOpList& ops);

// ---------------------------------------------------------------------------
// Readback verification. `data` is the CPU-mapped copy of the target image
// (vkCmdCopyImageToBuffer into a host-visible buffer, then vkMapMemory):
// tightly packed width*height pixels, 4 bytes per pixel, byte order
// R,G,B,A. The check is exact — no tolerance: the M7 gate is "expected
// pixels", and a clear is the one render op whose bytes are exactly known.
// ---------------------------------------------------------------------------
struct ReadbackCheck {
    uint32_t pixelsTotal = 0;      // width*height, 0 when the input is unusable
    uint32_t pixelsMatch = 0;      // pixels whose 4 bytes all match expected
    bool     allMatch   = false;
    // Byte offset of the first mismatching BYTE (not pixel), SIZE_MAX when
    // none. Names the exact channel a failure differs on.
    size_t   firstBadByte = SIZE_MAX;
};

ReadbackCheck VerifyClearReadback(const uint8_t* data, size_t size,
                                  uint32_t width, uint32_t height,
                                  const uint8_t expectedRgba[4]);

} // namespace PX5::Gpu

#endif // PX5_GPU_VULKAN_BACKEND_H
