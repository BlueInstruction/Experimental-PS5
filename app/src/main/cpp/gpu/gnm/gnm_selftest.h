// SPDX-License-Identifier: MIT
// PX5 Phase C milestone 1 — GNM decoder self-test (honest, synthetic).
//
// Builds synthetic PM4 command buffers, decodes them with the REAL
// decoder into a REAL GnmState, and asserts the observable results.
// This proves the decoder/state model mechanics — it does NOT claim any
// game compatibility: real-game streams cannot arrive until SELF
// decryption and libkernel HLE submit paths exist (later milestones).
//
// Platform-independent C++: runs on the host (scripts/gnm_host_test.cpp)
// and inside both APK ABIs via JNI (nativeRunGnmSelfTest).

#ifndef PX5_GPU_GNM_GNM_SELFTEST_H
#define PX5_GPU_GNM_GNM_SELFTEST_H

#include <string>

namespace PX5::Gnm {

// Runs all subtests. Returns true iff every subtest passed. `report`
// (optional) receives a multi-line, log-safe human-readable report whose
// first line begins with "PASS" or "FAIL".
bool RunGnmSelfTest(std::string* report);

} // namespace PX5::Gnm

#endif // PX5_GPU_GNM_GNM_SELFTEST_H
