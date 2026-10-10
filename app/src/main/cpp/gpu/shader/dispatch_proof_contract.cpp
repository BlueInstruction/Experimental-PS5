// SPDX-License-Identifier: GPL-2.0-only
// PX5 M8 T7 (contract half) — the dispatch proof's expected bytes, as data.
// The tables live in the header (inline constexpr, verbatim upstream dwords);
// this TU owns the case table, the fills, the descriptor builder, and the
// verifier — the pieces the device proof and the host gate share.
#include "dispatch_proof_contract.h"

#include <cstdio>

namespace PX5::Gpu::DispatchProof {
namespace {

const std::array<Case, 6> kCases{{
    // name                                code             dwords        selector  added
    {"return before the tail blocks",      kTailBlockCode.data(), 20u, 0u,  1u},
    {"tail block entered from the tail block after it",
                                           kTailBlockCode.data(), 20u, 1u,  2u},
    {"last tail block",                    kTailBlockCode.data(), 20u, 2u,  3u},
    {"return before the loop",             kLoopBodyCode.data(),  18u, 0u, 64u},
    {"loop body entered once from the test after it",
                                           kLoopBodyCode.data(),  18u, 1u,  1u},
    {"loop body entered three times from the test after it",
                                           kLoopBodyCode.data(),  18u, 3u,  3u},
}};

constexpr size_t kInputDwords = size_t{kThreads} * size_t{kStride};

}  // namespace

const std::array<Case, 6>& Cases() { return kCases; }

void FillInput(std::span<uint32_t> input) {
    if (input.size() < kInputDwords) return;  // caller error; verifier refuses
    for (uint32_t tid = 0; tid < kThreads; ++tid) {
        input[tid * kStride] = tid * 0x01010101u + 7u;
    }
}

void FillOutput(std::span<uint32_t> output) {
    for (uint32_t& v : output) v = kOutputFill;
}

std::array<uint32_t, 4> BufferDescriptor(uint64_t deviceAddress,
                                         uint32_t dwordCount) {
    // Verbatim shape from the upstream test's BufferDescriptor: base lo,
    // (base hi & 0xffff) | (stride 4 << 16), count, the fixed dst_sel/num_format word.
    return {static_cast<uint32_t>(deviceAddress),
            static_cast<uint32_t>((deviceAddress >> 32u) & 0xffffu) |
                (4u << 16u),
            dwordCount, 0x01016facu};
}

std::string VerifyOutput(std::span<const uint32_t> input,
                         std::span<const uint32_t> output, const Case& c) {
    if (input.size() < kInputDwords || output.size() < kInputDwords) {
        return std::string("case '") + (c.name ? c.name : "?") +
               "': proof windows too short — need " +
               std::to_string(kInputDwords) + " dwords, got " +
               std::to_string(input.size()) + "/" + std::to_string(output.size());
    }
    for (uint32_t tid = 0; tid < kThreads; ++tid) {
        const uint32_t in = input[tid * kStride];
        const uint32_t expected = in + c.added;
        const uint32_t actual = output[tid * kStride];
        if (actual != expected) {
            char b[160];
            snprintf(b, sizeof(b),
                     "case '%s': thread %u is %u, expected %u",
                     c.name ? c.name : "?", tid, actual, expected);
            return std::string(b);
        }
    }
    return {};
}

}  // namespace PX5::Gpu::DispatchProof
