// SPDX-License-Identifier: MIT
// PX5 Phase C milestone 1 — GNM GPU state model SKELETON (honest scope).
//
// WHAT THIS IS
//   The intermediate representation the user's research dossier (Task 9 in
//   worklog) insists on: PM4 -> PS5 GPU state -> GNM semantics -> Vulkan —
//   NOT PM4 -> Vulkan directly. Milestone 1 implements the skeleton of the
//   state half: bounded register banks for the four public GNM/GCN register
//   spaces plus real draw/dispatch accounting. Named-register semantics are
//   limited to a handful of high-confidence public registers; everything
//   else is raw-but-tracked storage with written-mask accounting.
//
// WHAT THIS IS NOT
//   Not a command processor, not a shader backend, not tied to Vulkan in
//   any way. Downstream phases (Vulkan state emission, shader IR) will be
//   new code reading from this model.
//
// Platform-independent C++: unit-runnable on host and inside both ABIs.

#ifndef PX5_GPU_GNM_GNM_STATE_H
#define PX5_GPU_GNM_GNM_STATE_H

#include <cstdint>
#include <vector>

namespace PX5::Gnm {

enum class RegSpace : uint32_t {
    kConfig  = 0,   // 0x2000..
    kUConfig = 1,   // 0x3000..
    kSh      = 2,   // 0x2C00..
    kContext = 3,   // 0x28000..
    kSpaceCount = 4,
};

struct BankInfo {
    uint32_t base;        // first absolute MMIO dword address
    uint32_t sizeDwords;  // bank length
    const char* name;
};

class GnmState {
public:
    static constexpr uint32_t kConfigSize  = 0x1000;  // 0x2000..0x2FFF
    static constexpr uint32_t kUConfigSize = 0x1000;  // 0x3000..0x3FFF
    static constexpr uint32_t kShSize      = 0x0400;  // 0x2C00..0x2FFF
    static constexpr uint32_t kContextSize = 0x3000;  // 0x28000..0x2AFFF

    // Bounded history of recent draw records (most recent last).
    static constexpr size_t kMaxDrawLog = 64;

    struct DrawRecord {
        uint32_t packetOffset;   // dword offset of the draw packet in its cb
        uint32_t count;          // vertex/index count from the draw packet
        uint32_t initiator;      // raw DRAW_INITIATOR dword (decoded elsewhere)
        uint32_t instances;      // current VGT_NUM_INSTANCES
        uint32_t indexTypeRaw;   // current INDEX_TYPE dword
        bool     indexed;        // true for DRAW_INDEX_2-style packets
    };

    GnmState();

    static const BankInfo& Bank(RegSpace space);

    // Absolute-address write. Returns false (and records nothing) if the
    // address falls outside the modeled space — the caller counts that as
    // a stream error instead of silently dropping it.
    bool WriteRegister(uint32_t absoluteAddress, uint32_t value);

    // Absolute-address read. Unwritten registers read as 0; `*wasWritten`
    // (optional) reports whether a packet ever wrote it.
    uint32_t ReadRegister(uint32_t absoluteAddress, bool* wasWritten = nullptr) const;

    // ---- Draw/dispatch accounting (real state machine) -------------------
    void SetIndexTypeRaw(uint32_t raw)  { index_type_raw_ = raw; }
    void SetNumInstances(uint32_t n)    { num_instances_ = n; }
    void RecordDraw(const DrawRecord& r);
    void RecordDispatch(uint32_t x, uint32_t y, uint32_t z);

    uint64_t DrawCalls()      const { return draw_calls_; }
    uint64_t Dispatches()     const { return dispatches_; }
    uint32_t IndexTypeRaw()   const { return index_type_raw_; }
    uint32_t NumInstances()   const { return num_instances_; }
    const std::vector<DrawRecord>& DrawLog() const { return draw_log_; }
    uint32_t LastDispatchDims(uint32_t* y, uint32_t* z) const;

    // ---- Bank introspection (diagnostics) ---------------------------------
    // Count of registers written at least once per space + total writes.
    uint64_t TotalRegisterWrites() const { return reg_writes_; }
    uint64_t OutOfRangeWrites()    const { return out_of_range_writes_; }
    uint32_t WrittenRegisters(RegSpace space) const;

    void Reset();

private:
    std::vector<uint32_t> banks_[static_cast<size_t>(RegSpace::kSpaceCount)];
    std::vector<bool>     written_[static_cast<size_t>(RegSpace::kSpaceCount)];

    uint64_t reg_writes_ = 0;
    uint64_t out_of_range_writes_ = 0;

    uint64_t draw_calls_ = 0;
    uint64_t dispatches_ = 0;
    uint32_t index_type_raw_ = 0;
    uint32_t num_instances_ = 0;
    uint32_t dispatch_x_ = 0, dispatch_y_ = 0, dispatch_z_ = 0;
    std::vector<DrawRecord> draw_log_;
};

} // namespace PX5::Gnm

#endif // PX5_GPU_GNM_GNM_STATE_H
