// SPDX-License-Identifier: MIT
// PX5 Phase C milestone 2b — SceGnmDriver submission shim (real pipeline).
//
// WHAT THIS IS
//   The GPU-side counterpart of the kernel HLE layer: when a guest game
//   calls sceGnmSubmitCommandBuffers (the ONE GNM entry that carries the
//   PM4 command buffers, since GNM is statically linked and this is the
//   submit seam), the buffers flow: descriptor array -> address resolution
//   -> Pm4Decoder -> GnmState. That state becomes the GPU-IR input the
//   later Vulkan-emission phases will consume. Nothing here talks to
//   Vulkan yet — by design (dossier: GPU IR first, Vulkan as backend).
//
// PROVISIONAL ASSUMPTION (named, falsifiable):
//   Each uint64 command-buffer descriptor is decoded as
//       addr   = desc & ((1<<48)-1)
//       dwords = desc >> 48
//   Public RE sources describe the packing differently in places. We
//   therefore VALIDATE every decode (dwords in [1, 4 MiB], address
//   resolvable) and hex-log the raw descriptors of real submissions, so a
//   wrong assumption shows up as named validation errors + raw evidence,
//   never as silently misdecoded streams.
//
// Platform-independent C++ (host-testable); address resolution goes through
// the guest memory window (MemoryManager). An address that does not resolve
// is an ERROR (named EFAULT) — it is never reinterpreted as a raw host
// pointer. The old "evidence fallback" produced dereferenceable garbage and
// crashed the process on real devices (2026-08-29 A750 logs); tests that
// need identity resolution inject it via SetAddressResolverForTest.

#ifndef PX5_GPU_GNM_GNM_SUBMIT_H
#define PX5_GPU_GNM_GNM_SUBMIT_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "gpu/gnm/gnm_state.h"
#include "gpu/gnm/pm4_decoder.h"

namespace PX5::Gnm {

// Descriptor packing constants (see header comment).
constexpr uint64_t kDescAddrMask  = (1ull << 48) - 1;
constexpr uint32_t kMaxSubmitDwords = 4u * 1024 * 1024;   // 16 MB of dwords

class GnmSubmit {
public:
    static GnmSubmit& GetInstance();

    // Address-resolution seam. Default resolves through the guest memory
    // window (MemoryManager) ONLY; unresolvable addresses yield nullptr and
    // become named EFAULT errors. Host unit tests inject a passthrough
    // instead of linking the Android memory backend.
    using AddressResolver = std::function<const void*(uint64_t addr, size_t bytes)>;
    void SetAddressResolverForTest(AddressResolver fn);

    // Prefix for the descriptor hex-log line. Test code sets "[selftest] "
    // so its synthetic submissions can never be misread in a pasted GPU log
    // as real game traffic. Production leaves it empty.

    // HLE-shaped entry: an array of `count` uint64 descriptors.
    // Returns SCE-style 0 on success, negative errno-style on failure
    // with `errOut` carrying the named reason.
    int64_t SubmitDescriptors(uint64_t count, uint64_t descriptorsPtr,
                              uint64_t userDataPtr, std::string* errOut);

    // Direct buffer entry (tests + callers that already hold the dwords).
    bool SubmitBuffer(const uint32_t* dwords, size_t dwordCount,
                      const char* tag, std::string* errOut);

    // Cumulative evidence.
    std::string GetStatsString() const;
    const GnmState&  State() const { return m_state; }
    const DecodeStats& Stats() const { return m_cumulative; }
    uint64_t Submissions() const { return m_submissions; }
    const std::vector<std::string>& LastErrors() const { return m_errors; }
    bool EmptySoFar() const { return m_submissions == 0; }

    void ResetForTest();
    void SetLogTagForTest(const char* tag) { m_logTag = tag ? tag : ""; }

private:
    GnmSubmit() = default;

    // Guest-window resolution (default policy; Android memory backend).
    const void* ResolveDefault(uint64_t addr, size_t bytes) const;
    // Active resolution path (test override or default).
    const void* Resolve(uint64_t addr, size_t bytes) const;

    GnmState    m_state;
    DecodeStats m_cumulative;
    uint64_t    m_submissions = 0;
    uint64_t    m_dwordsDecoded = 0;
    std::vector<std::string> m_errors;   // bounded ring (last 16)
    AddressResolver m_testResolver;      // empty => default policy
    const char* m_logTag = "";           // hex-log prefix ("[selftest] " in tests)
};

} // namespace PX5::Gnm

#endif // PX5_GPU_GNM_GNM_SUBMIT_H
