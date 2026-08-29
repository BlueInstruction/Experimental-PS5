// SPDX-License-Identifier: MIT
// PX5 Phase C milestone 1 — PM4 command-stream decoder (real, bounded).
//
// Decodes a GNM PM4 Type-3 command buffer into the GnmState model,
// collecting per-opcode statistics and an honest error list. Every read is
// bounds-checked: a truncated stream produces a recorded error, never
// out-of-bounds access. Unknown opcodes are counted and skipped by their
// declared body length (the discovery mechanism real RE uses) — semantics
// are never guessed.
//
// Platform-independent C++: unit-runnable on host and inside both ABIs.

#ifndef PX5_GPU_GNM_PM4_DECODER_H
#define PX5_GPU_GNM_PM4_DECODER_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "gpu/gnm/gnm_state.h"

namespace PX5::Gnm {

struct DecodeStats {
    uint64_t totalPackets   = 0;
    uint64_t graphicsPackets = 0;
    uint64_t computePackets  = 0;
    uint64_t unknownPackets  = 0;
    uint64_t drawCalls       = 0;
    uint64_t dispatches      = 0;
    uint64_t streamErrors    = 0;
    // opcode -> occurrence count (known and unknown both; names via
    // ItOpcodeName()).
    std::map<uint32_t, uint32_t> opcodeCounts;
    std::map<uint32_t, uint32_t> unknownOpcodeCounts;

    void Reset();
    std::string SummaryString() const;
};

struct StreamError {
    uint32_t dwordOffset;   // where in the stream
    std::string what;
};

struct PacketRecord {
    uint32_t dwordOffset;
    uint32_t opcode;
    uint32_t bodyDwords;    // actual body length consumed
    uint32_t shaderType;    // 0 gfx / 1 compute
    uint32_t predicate;
};

// Bounded trace of recent packets kept by the decoder (most recent last).
constexpr size_t kMaxPacketTrace = 128;

class Pm4Decoder {
public:
    // Decodes `dwordCount` dwords. Returns the dword offset at which
    // decoding stopped (== dwordCount on a clean run). Errors/stats go to
    // the outputs; `trace` (optional) receives a bounded packet list.
    size_t Decode(const uint32_t* dwords, size_t dwordCount,
                  GnmState& state,
                  DecodeStats& stats,
                  std::vector<StreamError>* errorsOut,
                  std::vector<PacketRecord>* traceOut = nullptr);
};

} // namespace PX5::Gnm

#endif // PX5_GPU_GNM_PM4_DECODER_H
