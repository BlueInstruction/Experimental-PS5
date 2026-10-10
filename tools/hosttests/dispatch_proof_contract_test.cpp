// SPDX-License-Identifier: GPL-2.0-only
// PX5 M8 T7 (contract half) host gate — the dispatch proof's expected bytes,
// locked. This is the double-entry ledger for the on-device compute proof:
// the dword tables and the case table must stay verbatim, the fills must
// produce the exact documented bytes, and the verifier must name the first
// differing thread, refuse short windows, and pass perfect runs silently.
#include "gpu/shader/dispatch_proof_contract.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace PX5::Gpu::DispatchProof;

namespace {

int g_failures = 0;

void Check(bool ok, const std::string& what) {
    if (ok) {
        std::printf("  ok  %s\n", what.c_str());
        return;
    }
    ++g_failures;
    std::printf("FAIL  %s\n", what.c_str());
}

bool Contains(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}

}  // namespace

int main() {
    // ---- 1. geometry pins ----------------------------------------------------
    Check(kThreads == 64 && kStride == 4 && kSelectorRegister == 8 &&
              kUserDataSize == 9 && kOutputFill == 0xdeadbeefu,
          "proof geometry pins (64 threads, stride 4, selector SGPR 8)");

    // ---- 2. program tables verbatim (spot dwords from both ends) -------------
    Check(kTailBlockCode.size() == 20 && kTailBlockCode[0] == 0x34020082u &&
              kTailBlockCode[3] == 0xbf8c3f70u &&
              kTailBlockCode[6] == 0x4a080881u &&
              kTailBlockCode[10] == 0x4a080882u &&
              kTailBlockCode[15] == 0xbf85fffau &&
              kTailBlockCode[16] == 0x4a080883u &&
              kTailBlockCode[19] == 0xbf810000u,
          "TailBlockCode dwords verbatim (20 dwords)");
    Check(kLoopBodyCode.size() == 18 && kLoopBodyCode[0] == 0x34020082u &&
              kLoopBodyCode[4] == 0xbe890380u &&
              kLoopBodyCode[7] == 0x4a0808c0u &&
              kLoopBodyCode[11] == 0x4a080881u &&
              kLoopBodyCode[12] == 0x80098109u &&
              kLoopBodyCode[14] == 0xbf85fffcu &&
              kLoopBodyCode[17] == 0xbf810000u,
          "LoopBodyCode dwords verbatim (18 dwords)");

    // ---- 3. the six cases, in the upstream order ------------------------------
    const auto& cases = Cases();
    Check(cases.size() == 6, "exactly six proof cases");
    Check(std::string(cases[0].name) == "return before the tail blocks" &&
              cases[0].code == kTailBlockCode.data() &&
              cases[0].codeDwords == 20u &&
              cases[0].selector == 0u && cases[0].added == 1u,
          "case 0: tail blocks, selector 0, added 1");
    Check(std::string(cases[1].name) ==
                  "tail block entered from the tail block after it" &&
              cases[1].selector == 1u && cases[1].added == 2u,
          "case 1: tail-block fallthrough, selector 1, added 2");
    Check(std::string(cases[2].name) == "last tail block" &&
              cases[2].selector == 2u && cases[2].added == 3u,
          "case 2: last tail block, selector 2, added 3");
    Check(std::string(cases[3].name) == "return before the loop" &&
              cases[3].code == kLoopBodyCode.data() &&
              cases[3].codeDwords == 18u &&
              cases[3].selector == 0u && cases[3].added == 64u,
          "case 3: loop skipped, selector 0, added 64");
    Check(std::string(cases[4].name) ==
                  "loop body entered once from the test after it" &&
              cases[4].selector == 1u && cases[4].added == 1u,
          "case 4: one loop iteration, selector 1, added 1");
    Check(std::string(cases[5].name) ==
                  "loop body entered three times from the test after it" &&
              cases[5].selector == 3u && cases[5].added == 3u,
          "case 5: three loop iterations, selector 3, added 3");
    for (const auto& c : cases) {
        Check(c.code != nullptr && c.codeDwords > 0u && c.name != nullptr,
              "case well-formed (has code and a name)");
    }

    // ---- 4. fills --------------------------------------------------------------
    std::vector<uint32_t> input(size_t{kThreads} * kStride, 0u);
    std::vector<uint32_t> output(size_t{kThreads} * kStride, 0u);
    FillInput(input);
    FillOutput(output);
    Check(input[0] == 7u && input[1 * kStride] == 0x01010108u &&
              input[17 * kStride] == 17u * 0x01010101u + 7u &&
              input[63 * kStride] == 63u * 0x01010101u + 7u,
          "input fill: thread t carries t*0x01010101+7");
    Check(output[0] == 0xdeadbeefu &&
              output[size_t{kThreads} * kStride - 1] == 0xdeadbeefu,
          "output fill: every dword 0xdeadbeef");

    // ---- 5. buffer descriptor shape --------------------------------------------
    {
        const auto d = BufferDescriptor(0x0000123456789abcull, 256u);
        Check(d[0] == 0x56789abcu && d[1] == (0x1234u | (4u << 16u)) &&
                  d[2] == 256u && d[3] == 0x01016facu,
              "buffer descriptor: lo/hi|stride4/count/dst_sel word");
    }

    // ---- 6. verifier -------------------------------------------------------------
    {
        // Perfect run for every case: output[tid] = input[tid] + added.
        for (const auto& c : cases) {
            for (uint32_t tid = 0; tid < kThreads; ++tid)
                output[tid * kStride] = input[tid * kStride] + c.added;
            const std::string err = VerifyOutput(input, output, c);
            Check(err.empty(), std::string("perfect run passes: ") + c.name);
        }
    }
    {
        // Corrupted thread 17 (case 4): the verifier names THAT thread with
        // its actual and expected values.
        const auto& c = cases[4];
        for (uint32_t tid = 0; tid < kThreads; ++tid)
            output[tid * kStride] = input[tid * kStride] + c.added;
        output[17 * kStride] = input[17 * kStride] + c.added + 1u;
        const std::string err = VerifyOutput(input, output, c);
        Check(Contains(err, "thread 17") &&
                  Contains(err, std::string("'") + c.name + "'") &&
                  Contains(err, "expected " +
                                    std::to_string(
                                        input[17 * kStride] + c.added)),
              "corrupted thread 17 named with actual and expected");
    }
    {
        // Thread 0 differs (the most common real failure: the kernel never
        // ran and the output is still the fill).
        const auto& c = cases[0];
        FillOutput(output);  // simulate the kernel never having run
        const std::string err = VerifyOutput(input, output, c);
        Check(Contains(err, "thread 0") && Contains(err, "expected 8"),
              "unrun kernel reported at thread 0 with expected value");
    }
    {
        // Short windows are a refusal, not a pass.
        std::vector<uint32_t> shortIn(10u, 0u), shortOut(10u, 0u);
        const std::string err =
            VerifyOutput(shortIn, shortOut, cases[0]);
        Check(Contains(err, "too short"),
              "short window refused (never reads as a pass)");
    }

    if (g_failures == 0) {
        std::printf("dispatch_proof_contract_test: ALL PASS\n");
        return 0;
    }
    std::printf("dispatch_proof_contract_test: %d FAILURES\n", g_failures);
    return 1;
}
