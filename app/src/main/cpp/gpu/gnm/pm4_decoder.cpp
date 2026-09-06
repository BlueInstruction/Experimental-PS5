// SPDX-License-Identifier: MIT
// PX5 Phase C milestone 1 — PM4 command-stream decoder (implementation).
//
// Semantics decoded in this milestone (high-confidence public knowledge):
//   SET_CONFIG_REG / SET_CONTEXT_REG / SET_UCONFIG_REG / SET_SH_REG /
//   SET_SH_REG_OFFSET  -> register-bank writes in GnmState
//   INDEX_TYPE         -> index-type capture (raw + size decode)
//   NUM_INSTANCES      -> instance-count capture
//   DRAW_INDEX_AUTO    -> draw record (count + current instances/indexType)
//   DRAW_INDEX_2       -> indexed draw record (5-dword body)
//   DISPATCH_DIRECT    -> dispatch record (dimX/Y/Z)
//   NOP                -> skipped
// Everything else: recognized-name lookup, counted, body skipped by count.

#include "gpu/gnm/pm4_decoder.h"

#include <cstdio>

#include "gpu/gnm/pm4_packet.h"

namespace PX5::Gnm {

namespace {

void AddError(std::vector<StreamError>* errorsOut, uint32_t offset,
              const std::string& what, DecodeStats& stats) {
    ++stats.streamErrors;
    if (errorsOut && errorsOut->size() < 64) {
        errorsOut->push_back({offset, what});
    }
}

// Packet body layouts (public GCN-lineage; dwords after the header).
constexpr uint32_t kBodySetRegMin     = 1;  // [0]=offset, then values
constexpr uint32_t kBodyIndexType     = 1;
constexpr uint32_t kBodyNumInstances  = 1;
constexpr uint32_t kBodyDrawIndexAuto = 2;  // [0]=count [1]=initiator
constexpr uint32_t kBodyDrawIndex2    = 5;  // max,count | addrLo,addrHi | count,init
constexpr uint32_t kBodyDispatch      = 3;  // dimX dimY dimZ

} // namespace

const char* ItOpcodeName(uint32_t opcode) {
    switch (opcode) {
        case kItNop:               return "NOP";
        case kItClearState:        return "CLEAR_STATE";
        case kItDispatchDirect:    return "DISPATCH_DIRECT";
        case kItDispatchIndirect:  return "DISPATCH_INDIRECT";
        case kItIndirectBufferEnd: return "INDIRECT_BUFFER_END";
        case kItContextControl:    return "CONTEXT_CONTROL";
        case kItIndexType:         return "INDEX_TYPE";
        case kItDrawIndexAuto:     return "DRAW_INDEX_AUTO";
        case kItDrawIndex2:        return "DRAW_INDEX_2";
        case kItNumInstances:      return "NUM_INSTANCES";
        case kItWaitRegMem:        return "WAIT_REG_MEM";
        case kItIndirectBuffer:    return "INDIRECT_BUFFER";
        case kItEventWrite:        return "EVENT_WRITE";
        case kItEventWriteEop:     return "EVENT_WRITE_EOP";
        case kItEventWriteEos:     return "EVENT_WRITE_EOS";
        case kItDmaData:           return "DMA_DATA";
        case kItAcquireMem:        return "ACQUIRE_MEM";
        case kItSetConfigReg:      return "SET_CONFIG_REG";
        case kItSetContextReg:     return "SET_CONTEXT_REG";
        case kItSetUConfigReg:     return "SET_UCONFIG_REG";
        case kItSetShReg:          return "SET_SH_REG";
        case kItSetShRegOffset:    return "SET_SH_REG_OFFSET";
        default:                   return "UNKNOWN";
    }
}

void DecodeStats::Reset() {
    totalPackets = graphicsPackets = computePackets = 0;
    unknownPackets = drawCalls = dispatches = streamErrors = 0;
    opcodeCounts.clear();
    unknownOpcodeCounts.clear();
}

std::string DecodeStats::SummaryString() const {
    char line[256];
    std::snprintf(line, sizeof(line),
                  "packets=%llu gfx=%llu compute=%llu unknown=%llu "
                  "draws=%llu dispatches=%llu errors=%llu opcodes=%u",
                  static_cast<unsigned long long>(totalPackets),
                  static_cast<unsigned long long>(graphicsPackets),
                  static_cast<unsigned long long>(computePackets),
                  static_cast<unsigned long long>(unknownPackets),
                  static_cast<unsigned long long>(drawCalls),
                  static_cast<unsigned long long>(dispatches),
                  static_cast<unsigned long long>(streamErrors),
                  static_cast<unsigned>(opcodeCounts.size()));
    return std::string(line);
}

size_t Pm4Decoder::Decode(const uint32_t* dwords, size_t dwordCount,
                          GnmState& state,
                          DecodeStats& stats,
                          std::vector<StreamError>* errorsOut,
                          std::vector<PacketRecord>* traceOut) {
    size_t i = 0;
    while (i < dwordCount) {
        const uint32_t headerRaw = dwords[i];
        const Type3Header header(headerRaw);

        if (header.Type() != Type3Header::kType3) {
            // GNM streams are pure Type-3. Non-Type-3 dwords are legacy
            // PM4 classes — record honestly and advance one dword so a
            // single pad dword cannot desynchronize the whole walk.
            AddError(errorsOut, static_cast<uint32_t>(i),
                     "non-type3 dword (type field = " +
                         std::to_string(header.Type()) + ")",
                     stats);
            ++i;
            continue;
        }

        const uint32_t opcode     = header.Opcode();
        const uint32_t bodyNeed   = header.BodyDwords();

        // Bounds: the declared body must fit in the stream. A truncated
        // packet ends the walk with a recorded error (never reads OOB).
        if (i + 1 + bodyNeed > dwordCount) {
            AddError(errorsOut, static_cast<uint32_t>(i),
                     "truncated packet: opcode 0x" +
                         [] (uint32_t op) {
                             char b[16]; std::snprintf(b, sizeof(b), "%X", op);
                             return std::string(b);
                         }(opcode) +
                         " declares " + std::to_string(bodyNeed) +
                         " body dwords but only " +
                         std::to_string(dwordCount - i - 1) + " remain",
                     stats);
            return i;
        }

        const uint32_t* body = &dwords[i + 1];

        ++stats.totalPackets;
        ++stats.opcodeCounts[opcode];
        if (header.ShaderType() != 0) ++stats.computePackets;
        else ++stats.graphicsPackets;

        bool unknown = false;
        switch (opcode) {
        case kItSetConfigReg:
        case kItSetContextReg:
        case kItSetUConfigReg:
        case kItSetShReg: {
            if (bodyNeed >= kBodySetRegMin + 1) {  // offset + >=1 value
                const uint32_t spaceOffset = body[0];
                uint32_t base = 0;
                if (opcode == kItSetConfigReg)  base = kConfigRegBase;
                if (opcode == kItSetContextReg) base = kContextRegBase;
                if (opcode == kItSetUConfigReg) base = kUConfigRegBase;
                if (opcode == kItSetShReg)      base = kShRegBase;
                for (uint32_t v = 1; v < bodyNeed; ++v) {
                    state.WriteRegister(base + spaceOffset + (v - 1), body[v]);
                }
            } else {
                AddError(errorsOut, static_cast<uint32_t>(i),
                         std::string(ItOpcodeName(opcode)) +
                             " body too short for offset+value",
                         stats);
            }
            break;
        }
        case kItSetShRegOffset: {
            // Same register space as SET_SH_REG plus an address pair
            // (body[1]=addrLo, body[2]=addrHi in the public GCN layout).
            // Milestone-1 stores only the register VALUES: the address pair
            // is discarded — both dwords, not one — and the packet trace
            // carries no address field yet. Partial semantics, documented as
            // such in docs/gpu.md; the pm4_stream_test locks this contract.
            if (bodyNeed >= kBodySetRegMin + 2) {
                const uint32_t spaceOffset = body[0];
                for (uint32_t v = 3; v < bodyNeed; ++v) {
                    state.WriteRegister(kShRegBase + spaceOffset + (v - 3),
                                        body[v]);
                }
            } else {
                AddError(errorsOut, static_cast<uint32_t>(i),
                         "SET_SH_REG_OFFSET body too short", stats);
            }
            break;
        }
        case kItIndexType: {
            if (bodyNeed >= kBodyIndexType) {
                state.SetIndexTypeRaw(body[0]);
                state.WriteRegister(kRegVgtDmaIndexType, body[0]);
            }
            break;
        }
        case kItNumInstances: {
            if (bodyNeed >= kBodyNumInstances) {
                state.SetNumInstances(body[0]);
            }
            break;
        }
        case kItDrawIndexAuto: {
            if (bodyNeed >= kBodyDrawIndexAuto) {
                GnmState::DrawRecord r{};
                r.packetOffset = static_cast<uint32_t>(i);
                r.count = body[0];
                r.initiator = body[1];
                r.instances = state.NumInstances();
                r.indexTypeRaw = state.IndexTypeRaw();
                r.indexed = false;
                state.RecordDraw(r);
                ++stats.drawCalls;
            }
            break;
        }
        case kItDrawIndex2: {
            if (bodyNeed >= kBodyDrawIndex2) {
                GnmState::DrawRecord r{};
                r.packetOffset = static_cast<uint32_t>(i);
                r.count = body[3];       // num_indices (public layout)
                r.initiator = body[4];   // draw_initiator
                r.instances = state.NumInstances();
                r.indexTypeRaw = state.IndexTypeRaw();
                r.indexed = true;
                state.RecordDraw(r);
                ++stats.drawCalls;
            } else {
                AddError(errorsOut, static_cast<uint32_t>(i),
                         "DRAW_INDEX_2 body shorter than 5 dwords", stats);
            }
            break;
        }
        case kItDispatchDirect: {
            if (bodyNeed >= kBodyDispatch) {
                state.RecordDispatch(body[0], body[1], body[2]);
                ++stats.dispatches;
            }
            break;
        }
        case kItNop:
            break;
        default: {
            unknown = true;
            ++stats.unknownPackets;
            ++stats.unknownOpcodeCounts[opcode];
            break;
        }
        }

        if (traceOut && traceOut->size() < kMaxPacketTrace) {
            PacketRecord rec{};
            rec.dwordOffset = static_cast<uint32_t>(i);
            rec.opcode = opcode;
            rec.bodyDwords = bodyNeed;
            rec.shaderType = header.ShaderType();
            rec.predicate = header.Predicate();
            traceOut->push_back(rec);
        }
        (void)unknown;

        i += 1 + bodyNeed;
    }
    return i;
}

} // namespace PX5::Gnm
