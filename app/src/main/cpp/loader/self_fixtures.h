// SPDX-License-Identifier: MIT
// PX5 Phase C milestone 3 — shared synthetic SELF container builder.
//
// One builder, two consumers:
//   * loader/self_extract_selftest.cpp  (extractor round-trip subtests)
//   * core/emulator.cpp                 (foundation self-test step 5b:
//                                        SELF container -> real LoadSelf
//                                        -> FEXCore guest execution)
//
// WHY a shared builder: the foundation self-test must exercise the REAL
// production path (ElfLoader::LoadSelf -> SelfExtract::ExtractInnerElf),
// so the synthetic container it feeds must be byte-layout-identical to
// the one the extractor self-test validates against. Duplicating the
// builder would let the two drift apart and quietly weaken one of the
// proofs.
//
// v1.29: the builder emits the ORBIS layout shadPS4's production parser
// uses (see self_extract.h): verified magic, segment table at 0x20, the
// inner ELF header + phdrs directly after the table, segment payloads at
// absolute file offsets. It is a TEST FIXTURE format, not a claim about
// retail signatures: encrypted retail containers are still refused by
// name at the extractor boundary.

#ifndef PX5_LOADER_SELF_FIXTURES_H
#define PX5_LOADER_SELF_FIXTURES_H

#include <cstdint>
#include <vector>

#include "loader/self_extract.h"

namespace PX5::SelfFixtures {

struct BuiltSelf {
    std::vector<uint8_t> bytes;
    std::vector<PX5::SelfExtract::SegmentInfo> entries;
};

// Builds a SELF container from the inner ELF's header + phdr table plus
// one payload per PT_LOAD phdr (in phdr order), per the orbis layout:
//   0x00 self_header, 0x20 segment table, then inner ehdr + phdrs,
//   payload data from headerSize (0x100, 16-aligned).
BuiltSelf BuildSelfContainer(
        const std::vector<uint8_t>& innerEhdr,
        const std::vector<uint8_t>& innerPhdrs,
        const std::vector<std::vector<uint8_t>>& payloads,
        const std::vector<uint64_t>& flags,
        const std::vector<uint64_t>& memSizes);

// Convenience wrapper for the common fixture shape: split a WHOLE ELF
// file image (ehdr + phdrs + payload at p_offset) and wrap it. One
// flag per PT_LOAD phdr, in order. v1.30: each PT_LOAD's entry is
// emitted as a Blocked entry whose id field names that phdr's index —
// the resolution shape shadPS4's Elf::LoadSegment requires (the vc30
// device log proved real containers do NOT pair entries with phdrs by
// order: 12 entries vs 14 phdrs).
BuiltSelf BuildSelfFromWholeElf(const std::vector<uint8_t>& elfFile,
                                const std::vector<uint64_t>& flags);

} // namespace PX5::SelfFixtures

#endif // PX5_LOADER_SELF_FIXTURES_H
