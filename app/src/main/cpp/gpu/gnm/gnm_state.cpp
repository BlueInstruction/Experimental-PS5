// SPDX-License-Identifier: MIT
// PX5 Phase C milestone 1 — GNM GPU state model skeleton (implementation).

#include "gpu/gnm/gnm_state.h"

#include "gpu/gnm/pm4_packet.h"

namespace PX5::Gnm {

namespace {
constexpr BankInfo kBanks[] = {
    {kConfigRegBase,  GnmState::kConfigSize,  "config"},
    {kUConfigRegBase, GnmState::kUConfigSize, "uconfig"},
    {kShRegBase,      GnmState::kShSize,      "sh"},
    {kContextRegBase, GnmState::kContextSize, "context"},
};
} // namespace

GnmState::GnmState() {
    banks_[static_cast<size_t>(RegSpace::kConfig)].assign(kConfigSize, 0);
    banks_[static_cast<size_t>(RegSpace::kUConfig)].assign(kUConfigSize, 0);
    banks_[static_cast<size_t>(RegSpace::kSh)].assign(kShSize, 0);
    banks_[static_cast<size_t>(RegSpace::kContext)].assign(kContextSize, 0);
    written_[static_cast<size_t>(RegSpace::kConfig)].assign(kConfigSize, false);
    written_[static_cast<size_t>(RegSpace::kUConfig)].assign(kUConfigSize, false);
    written_[static_cast<size_t>(RegSpace::kSh)].assign(kShSize, false);
    written_[static_cast<size_t>(RegSpace::kContext)].assign(kContextSize, false);
}

const BankInfo& GnmState::Bank(RegSpace space) {
    return kBanks[static_cast<size_t>(space)];
}

bool GnmState::WriteRegister(uint32_t absoluteAddress, uint32_t value) {
    for (size_t i = 0; i < static_cast<size_t>(RegSpace::kSpaceCount); ++i) {
        const BankInfo& b = kBanks[i];
        if (absoluteAddress >= b.base &&
            absoluteAddress < b.base + b.sizeDwords) {
            banks_[i][absoluteAddress - b.base] = value;
            if (!written_[i][absoluteAddress - b.base]) {
                written_[i][absoluteAddress - b.base] = true;
            }
            ++reg_writes_;
            // Named-register journal: the only writes whose semantics the
            // model names today. Bounded, drop-oldest; each entry carries
            // the shared event seq so the GPU-IR lowering can interleave
            // state changes with draws/dispatches exactly. BR records also
            // carry the TL bank value effective at write time, so journal
            // eviction can never lose the scissor pair (bot-review P1);
            // the cumulative counters below keep the honesty accounting
            // exact for any stream length.
            if (absoluteAddress == kRegPaScScreenScissorTL ||
                absoluteAddress == kRegPaScScreenScissorBR ||
                absoluteAddress == kRegVgtDmaIndexType) {
                RegWriteRecord w;
                w.seq = ++seq_counter_;
                w.absoluteAddress = absoluteAddress;
                w.value = value;
                ++named_write_total_;
                if (absoluteAddress == kRegPaScScreenScissorBR) {
                    const size_t tlIdx =
                        kRegPaScScreenScissorTL - kContextRegBase;
                    w.pairedValue =
                        banks_[static_cast<size_t>(RegSpace::kContext)][tlIdx];
                    w.pairedValid = written_[static_cast<size_t>(
                        RegSpace::kContext)][tlIdx];
                } else if (absoluteAddress == kRegVgtDmaIndexType) {
                    ++carried_write_total_;
                }
                named_write_log_.push_back(w);
                if (named_write_log_.size() > kMaxNamedWriteLog) {
                    named_write_log_.erase(named_write_log_.begin());
                }
            }
            return true;
        }
    }
    ++out_of_range_writes_;
    return false;
}

uint32_t GnmState::ReadRegister(uint32_t absoluteAddress,
                                bool* wasWritten) const {
    if (wasWritten) *wasWritten = false;
    for (size_t i = 0; i < static_cast<size_t>(RegSpace::kSpaceCount); ++i) {
        const BankInfo& b = kBanks[i];
        if (absoluteAddress >= b.base &&
            absoluteAddress < b.base + b.sizeDwords) {
            const size_t idx = absoluteAddress - b.base;
            if (wasWritten) *wasWritten = written_[i][idx];
            return banks_[i][idx];
        }
    }
    return 0;
}

void GnmState::RecordDraw(const DrawRecord& r) {
    ++draw_calls_;
    DrawRecord stamped = r;
    stamped.seq = ++seq_counter_;
    draw_log_.push_back(stamped);
    if (draw_log_.size() > kMaxDrawLog) {
        draw_log_.erase(draw_log_.begin());   // bounded: drop oldest
    }
}

void GnmState::RecordDispatch(uint32_t x, uint32_t y, uint32_t z,
                              uint32_t packetOffset) {
    ++dispatches_;
    dispatch_x_ = x; dispatch_y_ = y; dispatch_z_ = z;
    DispatchRecord rec;
    rec.seq = ++seq_counter_;
    rec.packetOffset = packetOffset;
    rec.x = x; rec.y = y; rec.z = z;
    dispatch_log_.push_back(rec);
    if (dispatch_log_.size() > kMaxDispatchLog) {
        dispatch_log_.erase(dispatch_log_.begin());   // bounded: drop oldest
    }
}

uint32_t GnmState::LastDispatchDims(uint32_t* y, uint32_t* z) const {
    if (y) *y = dispatch_y_;
    if (z) *z = dispatch_z_;
    return dispatch_x_;
}

uint32_t GnmState::WrittenRegisters(RegSpace space) const {
    const auto& w = written_[static_cast<size_t>(space)];
    uint32_t n = 0;
    for (bool b : w) n += b ? 1u : 0u;
    return n;
}

void GnmState::Reset() {
    for (size_t i = 0; i < static_cast<size_t>(RegSpace::kSpaceCount); ++i) {
        banks_[i].assign(kBanks[i].sizeDwords, 0);
        written_[i].assign(kBanks[i].sizeDwords, false);
    }
    reg_writes_ = 0;
    out_of_range_writes_ = 0;
    draw_calls_ = 0;
    dispatches_ = 0;
    index_type_raw_ = 0;
    num_instances_ = 0;
    dispatch_x_ = dispatch_y_ = dispatch_z_ = 0;
    seq_counter_ = 0;
    named_write_total_ = 0;
    carried_write_total_ = 0;
    draw_log_.clear();
    dispatch_log_.clear();
    named_write_log_.clear();
}

} // namespace PX5::Gnm
