// SPDX-License-Identifier: GPL-2.0-only
// PX5 M8 T7 (contract half) — the compute dispatch proof's expected bytes.
//
// WHAT THIS IS
//   The RDNA program pair and the exact expected output of the branch-past-
//   endpgm execution test, lifted from AnyPS5's upstream execution test
//   (core/libs/prx/libSceAgcDriver/tests/execution/BranchPastEndpgm.cpp,
//   GPL-2.0-only) and locked as DATA so the on-device proof has a fixed
//   target: the recompiler must make a guest compute program whose control
//   flow branches past an endpgm (tail blocks, loop bodies — exactly the
//   shape real game shaders use) produce these bytes, or the failure names
//   the first thread that differs.
//
//   Six cases cover both programs' control-flow shapes: early return, tail
//   block fall-through, last tail block, loop-skip, one loop iteration,
//   three loop iterations.
//
// WHAT THIS IS NOT
//   No Vulkan, no recompiler, no dispatch — the device side (vulkan_device's
//   M8 compute readback proof) consumes this contract; this file only owns
//   the program bytes, the input fill, and the verifier.
#ifndef PX5_GPU_SHADER_DISPATCH_PROOF_CONTRACT_H
#define PX5_GPU_SHADER_DISPATCH_PROOF_CONTRACT_H

#include <array>
#include <cstdint>
#include <span>
#include <string>

namespace PX5::Gpu::DispatchProof {

// Fixed proof geometry (same numbers as the upstream execution test).
constexpr uint32_t kThreads = 64;             // threadblock width (x)
constexpr uint32_t kStride = 4;               // u32 slots per thread
constexpr uint32_t kSelectorRegister = 8;     // user-data SGPR carrying the case selector
constexpr uint32_t kUserDataSize = kSelectorRegister + 1;  // 9 dwords
constexpr uint32_t kOutputFill = 0xdeadbeefu;

// User-data layout: [0..3] input buffer descriptor, [4..7] output buffer
// descriptor, [8] selector. Descriptors are the 4-dword PS5 GPU VA form the
// recompiler's GuestBuffers consumes.
constexpr uint32_t kInputDescriptorSlot = 0;
constexpr uint32_t kOutputDescriptorSlot = 4;

/// One proof case: RDNA program + which branch the selector takes + how much
/// each thread's value must GROW by running the program.
struct Case {
    const char* name;
    const uint32_t* code;
    uint32_t codeDwords;
    uint32_t selector;
    uint32_t added;  // expected: Output[tid] == Input[tid] + added
};

// The two RDNA programs (dwords verbatim from the upstream test).
// TailBlockCode: three endpgm-terminated tail blocks; the selector picks
// which block the thread enters (0 = return before the tail blocks).
// LoopBodyCode: a counted loop body; the selector picks the trip count
// (0 = return before the loop).
inline constexpr std::array<uint32_t, 20> kTailBlockCode{
    0x34020082u, 0xe0302000u, 0x80000401u, 0xbf8c3f70u, 0xbf068008u,
    0xbf840008u, 0x4a080881u, 0xe0702000u, 0x80010401u, 0xbf810000u,
    0x4a080882u, 0xe0702000u, 0x80010401u, 0xbf810000u, 0xbf068108u,
    0xbf85fffau, 0x4a080883u, 0xe0702000u, 0x80010401u, 0xbf810000u,
};
inline constexpr std::array<uint32_t, 18> kLoopBodyCode{
    0x34020082u, 0xe0302000u, 0x80000401u, 0xbf8c3f70u, 0xbe890380u,
    0xbf068008u, 0xbf840006u, 0x4a0808c0u, 0xe0702000u, 0x80010401u,
    0xbf810000u, 0x4a080881u, 0x80098109u, 0xbf0a0809u, 0xbf85fffcu,
    0xe0702000u, 0x80010401u, 0xbf810000u,
};

/// The six proof cases, in the upstream test's order.
const std::array<Case, 6>& Cases();

/// Input fill: Input[tid * kStride] = tid * 0x01010101 + 7 (every thread a
/// distinct non-trivial value). `input` must hold kThreads * kStride dwords.
void FillInput(std::span<uint32_t> input);

/// Output fill: every dword 0xdeadbeef (a thread that never stored keeps the
/// fill — indistinguishable from "did not run", which is the point).
void FillOutput(std::span<uint32_t> output);

/// PS5 GPU VA buffer descriptor for the proof's user-data roots (4 dwords,
/// verbatim shape from the upstream test's BufferDescriptor).
std::array<uint32_t, 4> BufferDescriptor(uint64_t deviceAddress,
                                         uint32_t dwordCount);

/// Verifies Output[tid] == Input[tid] + added for all kThreads threads.
/// Returns an empty string on a perfect run; otherwise names the FIRST
/// failing thread with its actual and expected values, e.g.
///   "case '<name>': thread 17 is 4370, expected 4369"
/// Refuses (non-empty, before any comparison) spans smaller than the fixed
/// proof geometry — a short window must never read as a pass.
std::string VerifyOutput(std::span<const uint32_t> input,
                         std::span<const uint32_t> output,
                         const Case& c);

}  // namespace PX5::Gpu::DispatchProof

#endif  // PX5_GPU_SHADER_DISPATCH_PROOF_CONTRACT_H
