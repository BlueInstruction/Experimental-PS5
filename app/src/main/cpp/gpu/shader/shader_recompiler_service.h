// SPDX-License-Identifier: GPL-2.0-only
// PX5 M8 T3 — guest shader recompiler service (app-side wiring).
//
// WHAT THIS IS
//   The app-owned boundary to the vendored AnyPS5 RDNA recompiler
//   (.deps/anyps5, GPL-2.0-only, materialized by tools/fetch_shader_recompiler.sh):
//   RDNA shader dwords in, SPIR-V words out, with an honest stats record and
//   a bounded, mutex-guarded recompile cache.
//
//   Layer contract (docs/gpu.md chain): this TU is the ONLY app file that
//   includes recompiler headers. It never names a Vulkan type — VkShaderModule
//   creation from the produced words is the Vulkan device layer's job (next
//   step, M8 T4). GpuOp/GnmState know nothing about this module.
//
// WHAT THIS IS NOT
//   No PM4 SET_SH plumbing yet (M8 T4): the service takes raw stage + code
//   words, exactly the fixture shape proven in M8 T1/T2. Compute/vertex/
//   fragment stage contexts are the minimal honest defaults (T1/T2 fixtures);
//   user-data SGPR layouts arrive with the real PS5 register plumbing.
//
// Failure policy: any recompiler exception becomes (false, error) plus a
// stats failure — never a crash, never a guessed module. License note: this
// TU links the vendored GPL-2.0-only recompiler, hence its SPDX tag.
#ifndef PX5_GPU_SHADER_GUEST_SHADER_RECOMPILER_H
#define PX5_GPU_SHADER_GUEST_SHADER_RECOMPILER_H

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace PX5::Gpu {

enum class GuestShaderStage : uint32_t {
    Compute = 0,
    Vertex,
    Fragment,
};

struct GuestShaderMeta {
    GuestShaderStage stage = GuestShaderStage::Compute;
    // RDNA machine code (dwords). Must stay valid for the call.
    const uint32_t* code = nullptr;
    uint32_t codeDwords = 0;
    // User-data SGPRs (descriptor roots; the caller owns the memory for the
    // call). Compute dispatches always carry user data on PS5 — the
    // workgroup-id SGPRs land after them (ShaderInputInfoBuilder ABI), so an
    // empty list with group ids enabled is rejected instead of guessed.
    std::span<const uint32_t> userData;
    // Compute: declared local size (PS5 thread block).
    uint32_t threadsX = 64, threadsY = 1, threadsZ = 1;
    // Graphics stages: false = wave64 (PS5 graphics default), true = wave32.
    bool wave32 = false;
    // Fragment: number of SPI_PS_IN_CONTROL interpolators the guest declares
    // (reads beyond the count fail honestly in the recompiler).
    uint32_t fragmentInterpolators = 0;
};

struct GuestShaderStats {
    uint64_t recompiles = 0;      // full recompiler runs (cache misses)
    uint64_t cacheHits = 0;       // served from the bounded cache
    uint64_t failures = 0;        // recompiler rejected the input
    uint64_t cacheEvictions = 0;  // cache-clear events at capacity
    uint64_t cachedEntries = 0;   // current cache population

    void Reset() { *this = GuestShaderStats{}; }
    std::string SummaryString() const;
};

class GuestShaderRecompiler {
public:
    static GuestShaderRecompiler& Instance();

    GuestShaderRecompiler(const GuestShaderRecompiler&) = delete;
    GuestShaderRecompiler& operator=(const GuestShaderRecompiler&) = delete;

    // Returns true and fills `spirv` on success; returns false and fills
    // `error` on failure (stats updated either way). Identical
    // (stage, wave size, threads, code) requests are served from cache.
    bool Recompile(const GuestShaderMeta& meta, std::vector<uint32_t>& spirv,
                   std::string& error);

    GuestShaderStats Stats() const;

    // Test/telemetry hook: drops all cached modules (stats survive).
    void ClearCache();

private:
    GuestShaderRecompiler();
    ~GuestShaderRecompiler();
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace PX5::Gpu

#endif // PX5_GPU_SHADER_GUEST_SHADER_RECOMPILER_H
