// SPDX-License-Identifier: MIT
// PX5 M6 — GPU command IR (implementation): GnmState -> ordered GpuOp list.
//
// The lowering rules are documented in gpu_ir.h and locked by
// tools/hosttests/gpu_ir_test.cpp. This file adds no semantics of its own:
// everything it emits is backed by the state model's records, and every
// write it cannot name is counted instead of guessed.

#include "gpu/ir/gpu_ir.h"

#include <algorithm>

#include "gpu/gnm/pm4_packet.h"

namespace PX5::Gpu {

namespace {

using PX5::Gnm::GnmState;

// Event kinds in the merged timeline. Register writes live in a separate
// log from draws/dispatches, so the merge walk tags each entry first.
enum class EntryKind : uint8_t { kNamedWrite, kDraw, kDispatch };

struct TimelineEntry {
    uint64_t seq;
    EntryKind kind;
    size_t index;   // position in its own source log
};

} // namespace

const char* OpKindName(OpKind kind) {
    switch (kind) {
        case OpKind::kSetRenderTarget: return "SetRenderTarget";
        case OpKind::kSetViewport:     return "SetViewport";
        case OpKind::kSetScissor:      return "SetScissor";
        case OpKind::kBindPipeline:    return "BindPipeline";
        case OpKind::kBindResource:    return "BindResource";
        case OpKind::kDraw:            return "Draw";
        case OpKind::kDrawIndexed:     return "DrawIndexed";
        case OpKind::kDispatch:        return "Dispatch";
        case OpKind::kCopyImage:       return "CopyImage";
        case OpKind::kClear:           return "Clear";
        case OpKind::kBarrier:         return "Barrier";
        default:                       return "Unknown";
    }
}

void LowerStats::Reset() {
    opsEmitted = drawOps = drawIndexedOps = dispatchOps = 0;
    scissorOps = barrierOps = 0;
    carriedRegisterWrites = unmappedRegisterWrites = droppedOps = 0;
}

std::string LowerStats::SummaryString() const {
    char line[256];
    std::snprintf(line, sizeof(line),
                  "ops=%llu draw=%llu drawIndexed=%llu dispatch=%llu "
                  "scissor=%llu barrier=%llu carried=%llu unmapped=%llu "
                  "dropped=%llu",
                  static_cast<unsigned long long>(opsEmitted),
                  static_cast<unsigned long long>(drawOps),
                  static_cast<unsigned long long>(drawIndexedOps),
                  static_cast<unsigned long long>(dispatchOps),
                  static_cast<unsigned long long>(scissorOps),
                  static_cast<unsigned long long>(barrierOps),
                  static_cast<unsigned long long>(carriedRegisterWrites),
                  static_cast<unsigned long long>(unmappedRegisterWrites),
                  static_cast<unsigned long long>(droppedOps));
    return std::string(line);
}

LowerStats LowerGnmStateToIR(const GnmState& state, GpuOpList& out) {
    LowerStats stats;
    out.Clear();

    const auto& draws   = state.DrawLog();
    const auto& dispatches = state.DispatchLog();
    const auto& writes  = state.NamedWriteLog();

    // ---- build the merged timeline ---------------------------------------
    // Each source log is strictly seq-ascending (GnmState appends with a
    // monotonic counter and only drops from the front), so a three-way merge
    // by seq restores the guest's event order exactly.
    std::vector<TimelineEntry> timeline;
    timeline.reserve(draws.size() + dispatches.size() + writes.size());
    for (size_t i = 0; i < draws.size(); ++i) {
        timeline.push_back({draws[i].seq, EntryKind::kDraw, i});
    }
    for (size_t i = 0; i < dispatches.size(); ++i) {
        timeline.push_back({dispatches[i].seq, EntryKind::kDispatch, i});
    }
    for (size_t i = 0; i < writes.size(); ++i) {
        timeline.push_back({writes[i].seq, EntryKind::kNamedWrite, i});
    }
    std::stable_sort(timeline.begin(), timeline.end(),
                     [](const TimelineEntry& a, const TimelineEntry& b) {
                         return a.seq < b.seq;
                     });

    // ---- walk the timeline ------------------------------------------------
    // Pending scissor box: TL write updates xMin/yMin, BR write updates
    // xMax/yMax and emits. Defaults are the guest's unwritten values (0).
    uint32_t scMinX = 0, scMinY = 0, scMaxX = 0, scMaxY = 0;
    uint64_t lastSeq = 0;

    auto push = [&](const GpuOp& op) {
        if (out.Push(op)) {
            ++stats.opsEmitted;
        } else {
            ++stats.droppedOps;
        }
    };

    for (const TimelineEntry& e : timeline) {
        lastSeq = e.seq;
        switch (e.kind) {
        case EntryKind::kNamedWrite: {
            const auto& w = writes[e.index];
            GpuOp op;
            op.seq = w.seq;
            if (w.absoluteAddress == PX5::Gnm::kRegPaScScreenScissorTL) {
                scMinX = w.value & 0xFFFFu;
                scMinY = (w.value >> 16) & 0xFFFFu;
            } else if (w.absoluteAddress ==
                       PX5::Gnm::kRegPaScScreenScissorBR) {
                scMaxX = w.value & 0xFFFFu;
                scMaxY = (w.value >> 16) & 0xFFFFu;
                op.kind = OpKind::kSetScissor;
                op.xMin = scMinX;
                op.yMin = scMinY;
                op.xMax = scMaxX;
                op.yMax = scMaxY;
                ++stats.scissorOps;
                push(op);
            } else if (w.absoluteAddress ==
                       PX5::Gnm::kRegVgtDmaIndexType) {
                // Carried: every Draw/DrawIndexed payload already holds this
                // value via DrawRecord.indexTypeRaw.
                ++stats.carriedRegisterWrites;
            }
            break;
        }
        case EntryKind::kDraw: {
            const auto& d = draws[e.index];
            GpuOp op;
            op.kind = d.indexed ? OpKind::kDrawIndexed : OpKind::kDraw;
            op.seq = d.seq;
            op.packetOffset = d.packetOffset;
            op.count = d.count;
            op.instances = d.instances;
            op.indexTypeRaw = d.indexTypeRaw;
            if (d.indexed) ++stats.drawIndexedOps; else ++stats.drawOps;
            push(op);
            break;
        }
        case EntryKind::kDispatch: {
            const auto& dp = dispatches[e.index];
            GpuOp op;
            op.kind = OpKind::kDispatch;
            op.seq = dp.seq;
            op.packetOffset = dp.packetOffset;
            op.gridX = dp.x;
            op.gridY = dp.y;
            op.gridZ = dp.z;
            ++stats.dispatchOps;
            push(op);
            break;
        }
        }
    }

    // Unmapped accounting: every register write that was neither journaled
    // (named) nor absent. The journal IS the named set, so:
    //   unmapped = total writes - journaled writes.
    const uint64_t totalWrites = state.TotalRegisterWrites();
    const uint64_t journaled = writes.size();
    stats.unmappedRegisterWrites =
        (totalWrites >= journaled) ? (totalWrites - journaled) : 0;

    // ---- submit boundary --------------------------------------------------
    if (!out.Empty()) {
        GpuOp barrier;
        barrier.kind = OpKind::kBarrier;
        barrier.seq = lastSeq;
        barrier.barrierScope = 0;
        ++stats.barrierOps;
        push(barrier);
    }

    return stats;
}

} // namespace PX5::Gpu
