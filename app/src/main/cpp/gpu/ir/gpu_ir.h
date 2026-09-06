// SPDX-License-Identifier: MIT
// PX5 M6 — GPU command IR: the committed op list between GnmState and Vulkan.
//
// WHAT THIS IS
//   The intermediate representation docs/gpu.md mandates: GnmState + draw
//   records lower to a bounded, ordered GpuOp list. The Vulkan backend (M7)
//   executes that list WITHOUT ever knowing PM4 or GNM existed, and this
//   header NEVER names a Vulkan type — enforced by the include guard below.
//
//   Layer contract (docs/architecture.md, "Data paths"):
//     guest PM4 stream -> Pm4Decoder -> GnmState -> [this IR] -> Vulkan backend
//   GnmState knows nothing about GpuOp; GpuOp knows nothing about Vulkan.
//
// WHAT THE LOWERING EMITS TODAY (honest scope)
//   The full op vocabulary below is COMMITTED (M7 is built against it), but
//   the current lowering only emits ops the state model can honestly back:
//     SetScissor   — from journaled PA_SC_SCREEN_SCISSOR_TL/BR register writes
//                    (public PS4/PS5 RE consensus: context packet offsets
//                    0xC/0xD, X[15:0] Y[31:16]; Kyty + RPCSX agree)
//     Draw         — from GnmState::DrawRecord (indexed=false)
//     DrawIndexed  — from GnmState::DrawRecord (indexed=true)
//     Dispatch     — from GnmState::DispatchRecord
//     Barrier      — exactly one, submit boundary, after the timeline
//   SetRenderTarget / SetViewport / BindPipeline / BindResource / CopyImage /
//   Clear are vocabulary-only until the decoder deepens named-register
//   semantics (the M5 PARTIAL notes in docs/gpu.md). Register writes with no
//   named semantics are counted (LowerStats::unmappedRegisterWrites), never
//   turned into guessed ops.
//
// Ordering contract: ops are emitted in GnmState's event-sequence order
// (seq stamped at record time), so draws, dispatches and named state
// changes interleave exactly as the guest stream recorded them.
//
// Platform-independent C++: no Android headers, no Vulkan headers;
// unit-runnable on host (tools/hosttests/gpu_ir_test.cpp, the M6 gate)
// and inside both ABIs.

#if defined(VULKAN_H_) || defined(VULKAN_CORE_H_)
#error "GPU IR must not see Vulkan headers — layer contract, docs/gpu.md"
#endif

#ifndef PX5_GPU_IR_GPU_IR_H
#define PX5_GPU_IR_GPU_IR_H

#include <cstdint>
#include <string>
#include <vector>

#include "gpu/gnm/gnm_state.h"

namespace PX5::Gpu {

// ---------------------------------------------------------------------------
// The committed op vocabulary. Names are the docs/gpu.md planned list, with
// one addition the state model already backs: DrawIndexed (GnmState records
// indexed draws via DRAW_INDEX_2, so a single "Draw" op would erase a real
// distinction the guest stream carries).
// ---------------------------------------------------------------------------
enum class OpKind : uint32_t {
    kSetRenderTarget = 0,
    kSetViewport,
    kSetScissor,
    kBindPipeline,
    kBindResource,
    kDraw,
    kDrawIndexed,
    kDispatch,
    kCopyImage,
    kClear,
    kBarrier,
    kOpKindCount,
};

const char* OpKindName(OpKind kind);

// ---------------------------------------------------------------------------
// One IR op. Plain aggregate with default member initializers (no union, no
// UB, no Vulkan; the initializers are the safety feature — every field has a
// defined value for every op). Most fields are zero for any given op; the
// "read by" comment per field names its ops.
// ---------------------------------------------------------------------------
struct GpuOp {
    OpKind    kind = OpKind::kBarrier;

    // Provenance. `seq` is GnmState's event-sequence stamp (monotonic across
    // draws/dispatches/named writes); `packetOffset` is the source packet's
    // dword offset when the op comes from a packet record (draws/dispatches),
    // 0 for state-derived ops (scissor) and the submit-boundary Barrier.
    uint64_t  seq = 0;
    uint32_t  packetOffset = 0;

    // Read by kDraw / kDrawIndexed: vertex (auto) or index count.
    uint32_t  count = 0;
    // Read by kDraw / kDrawIndexed: current VGT_NUM_INSTANCES at draw time,
    // carried verbatim from the state model (0 when the guest never set it).
    uint32_t  instances = 0;
    // Read by kDrawIndexed: raw guest INDEX_TYPE dword (host decode is the
    // backend's business; the IR carries the guest encoding untouched).
    uint32_t  indexTypeRaw = 0;
    // Read by kDraw / kDrawIndexed: raw DRAW_INITIATOR dword from the draw
    // packet, carried verbatim (part of the verbatim-payload contract).
    uint32_t  initiatorRaw = 0;

    // Read by kDispatch: grid dims.
    uint32_t  gridX = 0, gridY = 0, gridZ = 0;

    // Read by kSetRenderTarget / kBindPipeline / kBindResource / kClear /
    // kCopyImage: target or binding slot (vocabulary-only today — no
    // lowering emits these until register semantics name them).
    uint32_t  slot = 0;

    // Read by kSetViewport / kSetScissor: box, guest-decoded raw values
    // (scissor: X[15:0] Y[31:16] of TL/BR dwords, as public RE agrees).
    uint32_t  xMin = 0, yMin = 0, xMax = 0, yMax = 0;

    // Read by kClear: clear color (host float, NOT a Vulkan type).
    float     clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    // Read by kBarrier: 0 = submit boundary (the only scope the lowering
    // emits today). Bit meanings stay reserved until event packets
    // (EVENT_WRITE_EOP/EOS family) gain named semantics.
    uint32_t  barrierScope = 0;
};

// ---------------------------------------------------------------------------
// Bounded op list. Capacity is fixed at construction so the lowering can
// never grow without bound on adversarial state; rejections are counted in
// LowerStats::droppedOps, never silent.
// ---------------------------------------------------------------------------
constexpr size_t kDefaultGpuOpCapacity = 4096;

class GpuOpList {
public:
    explicit GpuOpList(size_t capacity = kDefaultGpuOpCapacity)
        : capacity_(capacity) { ops_.reserve(capacity_ < 64 ? capacity_ : 64); }

    size_t Capacity() const { return capacity_; }
    size_t Size()    const { return ops_.size(); }
    bool   Empty()   const { return ops_.empty(); }
    const std::vector<GpuOp>& Ops() const { return ops_; }

    // Appends `op` if below capacity (returns true), otherwise drops it and
    // returns false. The caller (lowering) owns the drop accounting.
    bool Push(const GpuOp& op) {
        if (ops_.size() >= capacity_) return false;
        ops_.push_back(op);
        return true;
    }

    void Clear() { ops_.clear(); }

private:
    size_t capacity_;
    std::vector<GpuOp> ops_;
};

// ---------------------------------------------------------------------------
// Lowering evidence (docs/testing.md: numbers or it did not happen).
// ---------------------------------------------------------------------------
struct LowerStats {
    uint64_t opsEmitted  = 0;   // ops that entered the list
    uint64_t drawOps       = 0;
    uint64_t drawIndexedOps = 0;
    uint64_t dispatchOps   = 0;
    uint64_t scissorOps    = 0;
    uint64_t barrierOps    = 0;
    // Journaled named writes that produce no op because their semantics are
    // carried elsewhere: VGT_DMA_INDEX_TYPE lands inside Draw/DrawIndexed
    // payloads (indexTypeRaw), so it is accounted here, not "unmapped".
    uint64_t carriedRegisterWrites = 0;
    // Register writes with no named semantics: counted, never guessed into
    // ops. = TotalRegisterWrites() - (journaled writes).
    uint64_t unmappedRegisterWrites = 0;
    // Ops rejected because the list was full.
    uint64_t droppedOps = 0;

    void Reset();
    std::string SummaryString() const;
};

// The M6 lowering: GnmState (+ its draw/dispatch/named-write records) ->
// ordered GpuOp list. Clears `out` first. Returns the stats.
//
// Rules (locked by tools/hosttests/gpu_ir_test.cpp):
//   1. Timeline: draws, dispatches and journaled named-register writes
//      merge in `seq` order (each log is strictly seq-ascending; see
//      GnmState).
//   2. PA_SC_SCREEN_SCISSOR_BR write -> one kSetScissor, whose TL corner is
//      the TL value effective at write time (the journal record pairs it —
//      journal eviction cannot lose the pair) and whose BR corner is the
//      record's own value. A TL write emits nothing by itself (it is
//      consumed through the pairing). A BR with no TL ever written lowers
//      with the register reset value (0,0) — honest hardware behaviour.
//   3. VGT_DMA_INDEX_TYPE write -> carriedRegisterWrites (no op; the value
//      already lives in every draw record's indexTypeRaw). Accounting is
//      cumulative (GnmState counters), immune to journal eviction.
//   4. DrawRecord -> kDraw or kDrawIndexed (indexed flag), payload verbatim
//      (count/instances/indexTypeRaw/initiatorRaw).
//   5. DispatchRecord -> kDispatch.
//   6. If and only if the list is non-empty afterwards: exactly one
//      kBarrier (submit boundary, barrierScope=0, seq = last EMITTED seq —
//      non-emitting named writes never advance it).
//   7. List-full rejections count in droppedOps and never abort the walk.
LowerStats LowerGnmStateToIR(const PX5::Gnm::GnmState& state, GpuOpList& out);

} // namespace PX5::Gpu

#endif // PX5_GPU_IR_GPU_IR_H
