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
// so the synthetic container it feeds must be byte-identical to the one
// the extractor self-test validates against. Duplicating the builder
// would let the two drift apart and quietly weaken one of the proofs.
//
// The layout is the one documented in loader/self_extract.h (the same
// provisional public-RE lineage). It is a TEST FIXTURE format, not a
// claim about retail SELF files: encrypted retail containers are still
// refused by name at the extractor boundary.

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

// Builds a SELF container image from raw segment payloads per the
// documented layout: header @0x00, metadata @0x20, segment table @0x40,
// data at headerSize (0x100, aligned). One entry per payload, in order.
BuiltSelf BuildSelfContainer(
        const std::vector<std::vector<uint8_t>>& payloads,
        const std::vector<uint64_t>& flags,
        const std::vector<uint64_t>& memSizes);

} // namespace PX5::SelfFixtures

#endif // PX5_LOADER_SELF_FIXTURES_H
