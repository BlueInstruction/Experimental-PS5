// SPDX-License-Identifier: MIT
// PX5 M7 — Vulkan backend, platform-independent core.
// See vulkan_backend.h for the layer contract and the honest per-op mapping.

#include "gpu/vulkan_backend.h"

#include <cstdio>

namespace PX5::Gpu {

uint8_t ClearFloatToUnorm8(float f) {
    // NaN fails both comparisons in the "normal" path, so it is caught by
    // the first branch: !(NaN > 0.0f) is true -> 0. Negative clamps to 0,
    // anything >= 1 clamps to 255, and the in-range path is the contracted
    // round-half-up scale.
    if (!(f > 0.0f)) return 0u;
    if (f >= 1.0f) return 255u;
    return static_cast<uint8_t>(f * 255.0f + 0.5f);
}

void BackendPlanStats::Reset() { *this = BackendPlanStats{}; }

std::string BackendPlanStats::SummaryString() const {
    char b[160];
    snprintf(b, sizeof(b),
             "ops=%llu clear=%llu boundary=%llu deferred=%llu unknown=%llu",
             static_cast<unsigned long long>(opsReceived),
             static_cast<unsigned long long>(clearOps),
             static_cast<unsigned long long>(boundaryOps),
             static_cast<unsigned long long>(DeferredTotal()),
             static_cast<unsigned long long>(unknownOps));
    return std::string(b);
}

VulkanCommandPlan PlanVulkanCommands(const GpuOpList& ops) {
    VulkanCommandPlan plan;

    for (const GpuOp& op : ops.Ops()) {
        BackendPlanStats& st = plan.stats;
        ++st.opsReceived;

        switch (op.kind) {
        case OpKind::kClear: {
            ++st.clearOps;

            // The clear needs the image in TRANSFER_DST_OPTIMAL; the
            // barrier that gets it there is part of the op's semantics,
            // so the backend plans it — the device materializer never
            // has to remember one.
            VulkanCommand barrier{};
            barrier.kind = VulkanCommand::Kind::kPipelineBarrier;
            barrier.seq  = op.seq;
            plan.commands.push_back(barrier);

            VulkanCommand clear{};
            clear.kind = VulkanCommand::Kind::kClearColorImage;
            clear.seq  = op.seq;
            for (int i = 0; i < 4; ++i)
                clear.clearColor[i] = op.clearColor[i];
            plan.commands.push_back(clear);

            // Last clear wins (ordered work on the same image): the
            // readback expectation tracks the final color seen here.
            plan.hasClear = true;
            for (int i = 0; i < 4; ++i) {
                plan.clearF32[i]  = op.clearColor[i];
                plan.clearRgba[i] = ClearFloatToUnorm8(op.clearColor[i]);
            }
            break;
        }

        case OpKind::kBarrier:
            // Scope 0 is the submit boundary — the only barrier the M6
            // lowering emits. It materializes as end -> submit -> fence.
            if (op.barrierScope == 0) {
                ++st.boundaryOps;
                VulkanCommand boundary{};
                boundary.kind = VulkanCommand::Kind::kSubmitBoundary;
                boundary.seq  = op.seq;
                plan.commands.push_back(boundary);
            } else {
                // Reserved scope bits: no named semantics yet, no guessed
                // command.
                ++st.unknownOps;
            }
            break;

        // Deferred: the ops exist in the committed vocabulary and the
        // lowering emits them, but their Vulkan materialization needs
        // pipelines / dynamic state that nothing creates yet. Counting is
        // the honest behaviour — executing a draw without a pipeline would
        // be a silent lie.
        case OpKind::kSetScissor:   ++st.scissorOpsDeferred;   break;
        case OpKind::kDraw:         ++st.drawOpsDeferred;      break;
        case OpKind::kDrawIndexed:  ++st.drawIndexedOpsDeferred; break;
        case OpKind::kDispatch:     ++st.dispatchOpsDeferred;  break;

        // kSetRenderTarget / kSetViewport / kBindPipeline / kBindResource /
        // kCopyImage have no emitter today (M6 honest scope), so they
        // cannot reach the planner from the lowering. If one ever does,
        // count it as unknown — never guess a materialization. This also
        // catches kOpKindCount, which is never a valid op.
        default:                    ++st.unknownOps;           break;
        }
    }

    return plan;
}

ReadbackCheck VerifyClearReadback(const uint8_t* data, size_t size,
                                  uint32_t width, uint32_t height,
                                  const uint8_t expectedRgba[4]) {
    ReadbackCheck out;

    const uint64_t need =
        static_cast<uint64_t>(width) * static_cast<uint64_t>(height) * 4ull;
    if (data == nullptr || width == 0 || height == 0 ||
        static_cast<uint64_t>(size) < need) {
        // Unusable input reports itself as zero pixels checked — never as
        // a pass, never as a guessed partial count.
        return out;
    }

    const size_t pixelCount =
        static_cast<size_t>(width) * static_cast<size_t>(height);
    out.pixelsTotal = static_cast<uint32_t>(pixelCount);

    for (size_t p = 0; p < pixelCount; ++p) {
        const uint8_t* px = data + p * 4;
        const bool match = px[0] == expectedRgba[0] &&
                           px[1] == expectedRgba[1] &&
                           px[2] == expectedRgba[2] &&
                           px[3] == expectedRgba[3];
        if (match) {
            ++out.pixelsMatch;
        } else if (out.firstBadByte == SIZE_MAX) {
            for (int c = 0; c < 4; ++c) {
                if (px[c] != expectedRgba[c]) {
                    out.firstBadByte = p * 4 + static_cast<size_t>(c);
                    break;
                }
            }
        }
    }

    out.allMatch = out.pixelsMatch == out.pixelsTotal;
    return out;
}

} // namespace PX5::Gpu
