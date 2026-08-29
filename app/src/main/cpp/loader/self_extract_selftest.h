// SPDX-License-Identifier: MIT
// PX5 Phase C milestone 2a — SELF extractor self-test (synthetic round-trip).
//
// Builds synthetic SELF containers with the layout documented in
// self_extract.h, extracts them with the REAL extractor, and asserts the
// results: plain segment, zlib-compressed segment, encrypted-segment
// refusal, bad-magic refusal. Proves the container mechanics end to end —
// real-dump layout confirmation remains future evidence-driven work.

#ifndef PX5_LOADER_SELF_EXTRACT_SELFTEST_H
#define PX5_LOADER_SELF_EXTRACT_SELFTEST_H

#include <string>

namespace PX5::SelfExtract {

// Runs all subtests; returns true iff every one passed. `report` (optional)
// receives a multi-line report whose first line begins PASS or FAIL.
bool RunSelfExtractSelfTest(std::string* report);

} // namespace PX5::SelfExtract

#endif // PX5_LOADER_SELF_EXTRACT_SELFTEST_H
