// SPDX-License-Identifier: GPL-2.0-only
// PX5 M8 T3 — guest shader recompiler service, implementation.
// See shader_recompiler_service.h for the layer contract and failure policy.

#include "gpu/shader/shader_recompiler_service.h"

#include "Recompiler.hpp"
#include "BdaAbi.hpp"

#include <array>
#include <cstdio>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace PX5::Gpu {

namespace {

using ShaderRecompiler::RecompileRequest;
using ShaderRecompiler::ShaderStage;
// NOTE: ShaderRecompiler::Recompile is called fully qualified below — inside
// this class the unqualified name resolves to the member function.

constexpr size_t kMaxCacheEntries = 256;

// Vendored DescriptorKind -> STABLE PUBLIC VkDescriptorType numeric value
// (M8 T5). The numbers are fixed by the Vulkan specification's
// VkDescriptorType enum; the host gate locks them without Vulkan headers.
uint32_t DescriptorTypeValue(const ShaderRecompiler::DescriptorKind kind) {
    switch (kind) {
    case ShaderRecompiler::DescriptorKind::Sampler:            return 0u;   // SAMPLER
    case ShaderRecompiler::DescriptorKind::SampledImage:       return 2u;   // SAMPLED_IMAGE
    case ShaderRecompiler::DescriptorKind::StorageImage:       return 3u;   // STORAGE_IMAGE
    case ShaderRecompiler::DescriptorKind::UniformTexelBuffer: return 5u;   // UNIFORM_TEXEL_BUFFER
    case ShaderRecompiler::DescriptorKind::StorageTexelBuffer: return 6u;   // STORAGE_TEXEL_BUFFER
    case ShaderRecompiler::DescriptorKind::StorageBuffer:      return 7u;   // STORAGE_BUFFER
    case ShaderRecompiler::DescriptorKind::UniformBuffer:
    default:                                                    return 4u;   // UNIFORM_BUFFER
    }
}

// Vendored DescriptorRole -> stable role id for diagnostics (order of the
// vendored enum: GuestBuffers, GuestImages, GuestSamplers, Gds,
// BdaPagetable, FaultBuffer, FlattenedSrt, ShaderData).
uint32_t DescriptorRoleId(const ShaderRecompiler::DescriptorRole role) {
    return static_cast<uint32_t>(role);
}

// Vendored reflection -> app-owned GuestShaderReflection (M8 T5). Runs
// inside the ONLY TU that sees the vendored types — the layer contract
// (docs/gpu.md chain) stays intact.
GuestShaderReflection TranslateReflection(
    const ShaderRecompiler::RecompileResult& result,
    const ShaderRecompiler::BindingLayout& layout) {
    GuestShaderReflection out;
    out.pushConstantSizeBytes = layout.pushConstantSizeBytes;
    out.bindings.reserve(result.bindings.size());
    for (const ShaderRecompiler::DescriptorBinding& b : result.bindings) {
        GuestShaderBinding out2;
        out2.type = DescriptorTypeValue(b.kind);
        out2.set = b.descriptorSet;
        out2.binding = b.binding;
        out2.count = b.count;
        out2.readOnly = b.readOnly;
        out2.role = DescriptorRoleId(b.role);
        out.bindings.push_back(out2);
    }
    return out;
}

ShaderStage ToRecompilerStage(GuestShaderStage stage) {
    switch (stage) {
    case GuestShaderStage::Vertex:   return ShaderStage::Vertex;
    case GuestShaderStage::Fragment: return ShaderStage::Fragment;
    case GuestShaderStage::Compute:
    default:                          return ShaderStage::Compute;
    }
}

// Adreno-class host target — the same values the M8 T1/T2 gates pin (Vulkan
// 1.3, SPIR-V 1.3, 64-wide subgroups, BDA ABI v1). The fragment path keeps
// barycentric off: the GetAttribute interpolation path works on every host.
ShaderRecompiler::SpirvTarget HostTarget() {
    static const std::array<uint32_t, 12> capabilities{
        1,     // Shader
        2,     // Tessellation (declared available; unused)
        4,     // Geometry
        11,    // ImageBuffer
        12,    // StorageImageExtendedFormats
        17,    // Int64
        22,    // Int16
        23,    // TessellationPointSize
        4138,  // PhysicalStorageBufferAddresses (BDA)
        4463,  // FragmentShaderBarycentric
        4423,  // ShaderFloatControls2
        5307,  // ShaderInvocationReorder
    };
    ShaderRecompiler::SpirvTarget target{};
    target.vulkanVersion = 0x00403000u;  // VK_API_VERSION_1_3
    target.spirvVersion = 0x00010300u;   // SPIR-V 1.3
    target.subgroupSize = 64u;
    target.bdaAbiVersion = ShaderRecompiler::BdaAbi::Version;
    target.supportedCapabilities = capabilities;
    target.supportedExtensions = {};
    target.fragmentShaderBarycentricEnabled = false;
    target.maxWorkgroupSize = {1024u, 1024u, 1024u};
    target.maxWorkgroupInvocations = 1024u;
    target.maxWorkgroupSharedMemoryBytes = 32768u;
    target.nonConstantImageOffsets = false;
    target.srgbDecodeFormats = 0u;
    target.narrowSubgroupClock = false;
    return target;
}

// FNV-1a 64: cache key over everything the recompile depends on.
uint64_t HashMeta(const GuestShaderMeta& meta) {
    uint64_t h = 1469598103934665603ull;
    const auto mix = [&h](uint64_t v) {
        h ^= v;
        h *= 1099511628211ull;
    };
    mix(static_cast<uint64_t>(meta.stage));
    mix(meta.wave32 ? 1u : 0u);
    mix(meta.threadsX);
    mix(meta.threadsY);
    mix(meta.threadsZ);
    mix(meta.codeDwords);
    if (meta.code != nullptr) {
        for (uint32_t i = 0; i < meta.codeDwords; ++i) mix(meta.code[i]);
    }
    mix(meta.userData.size());
    for (uint32_t word : meta.userData) mix(word);
    mix(meta.fragmentInterpolators);
    return h;
}

} // namespace

std::string GuestShaderStats::SummaryString() const {
    char b[192];
    snprintf(b, sizeof(b),
             "recompiles=%llu hits=%llu failures=%llu evictions=%llu cached=%llu",
             static_cast<unsigned long long>(recompiles),
             static_cast<unsigned long long>(cacheHits),
             static_cast<unsigned long long>(failures),
             static_cast<unsigned long long>(cacheEvictions),
             static_cast<unsigned long long>(cachedEntries));
    return std::string(b);
}

struct GuestShaderRecompiler::Impl {
    std::mutex mutex;
    struct CacheEntry {
        std::vector<uint32_t> words;
        GuestShaderReflection reflection;
    };
    std::unordered_map<uint64_t, CacheEntry> cache;
    GuestShaderStats stats;
};

GuestShaderRecompiler::GuestShaderRecompiler() : impl_(new Impl{}) {}
GuestShaderRecompiler::~GuestShaderRecompiler() { delete impl_; }

GuestShaderRecompiler& GuestShaderRecompiler::Instance() {
    static GuestShaderRecompiler instance;
    return instance;
}

GuestShaderStats GuestShaderRecompiler::Stats() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->stats;
}

void GuestShaderRecompiler::ClearCache() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->cache.clear();
    impl_->stats.cachedEntries = 0;
}

bool GuestShaderRecompiler::Recompile(const GuestShaderMeta& meta,
                                      std::vector<uint32_t>& spirv,
                                      std::string& error) {
    GuestShaderReflection unusedReflection;
    return Recompile(meta, spirv, unusedReflection, error);
}

bool GuestShaderRecompiler::Recompile(const GuestShaderMeta& meta,
                                      std::vector<uint32_t>& spirv,
                                      GuestShaderReflection& reflection,
                                      std::string& error) {
    spirv.clear();
    reflection = GuestShaderReflection{};
    error.clear();

    if (meta.code == nullptr || meta.codeDwords == 0) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        ++impl_->stats.failures;
        error = "no shader code";
        return false;
    }

    const uint64_t key = HashMeta(meta);
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        auto it = impl_->cache.find(key);
        if (it != impl_->cache.end()) {
            ++impl_->stats.cacheHits;
            spirv = it->second.words;
            reflection = it->second.reflection;
            return true;
        }
    }

    RecompileRequest request{};
    request.shader = ShaderRecompiler::ShaderBinary{
        ToRecompilerStage(meta.stage),
        reinterpret_cast<std::uintptr_t>(meta.code),
        std::span<const uint32_t>(meta.code, meta.codeDwords),
        0u, {},
    };
    if (meta.stage == GuestShaderStage::Compute) {
        // Workgroup-id SGPRs live AFTER the user data (ShaderInputInfoBuilder
        // ABI). An empty user-data list would park them at s0, colliding with
        // the descriptor base — the recompiler rejects that shape with an
        // opaque BDA error, so the service refuses it with a clear one.
        if (meta.userData.empty()) {
            std::lock_guard<std::mutex> lock(impl_->mutex);
            ++impl_->stats.failures;
            error = "compute shader requires user data (workgroup register ABI)";
            return false;
        }
        ShaderRecompiler::ShaderComputeStageInfo compute{};
        compute.numThreads = {meta.threadsX, meta.threadsY, meta.threadsZ};
        compute.ldsSizeDwords = 0u;
        compute.groupIdEnable = {true, true, true};
        compute.tgSizeEnable = false;
        compute.threadIdComponentCount = 3u;
        compute.scratchDwords = 0u;
        request.context.compute = compute;
    } else if (meta.stage == GuestShaderStage::Vertex) {
        // No embedded vertex resources: the M8 T4 register plumbing owns the
        // fetch descriptor setup when real SET_SH packets arrive.
        request.context.vertex = ShaderRecompiler::ShaderVertexStageInfo{};
    } else {
        // Fragment: interpolator count comes from the guest's
        // SPI_PS_IN_CONTROL (meta). A shader that READS beyond the count
        // fails honestly in the recompiler (counted as a failure) instead
        // of fabricating data. Input plumbing arrives with M8 T4.
        ShaderRecompiler::ShaderPixelStageInfo pixel{};
        pixel.interpolatorCount = meta.fragmentInterpolators;
        pixel.conservativeZExport = ShaderRecompiler::ConservativeZExport::AnyZ;
        request.context.pixel = pixel;
    }
    request.context.waveSize = meta.wave32 ? 32u : 64u;
    request.context.userDataBaseRegister = 0u;
    request.context.userData = meta.userData;
    // Guest code bytes back the program-counter regions the recompiler walks.
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{
        {reinterpret_cast<uint64_t>(meta.code),
         std::as_bytes(std::span<const uint32_t>(meta.code, meta.codeDwords))},
    }};
    request.context.memory = memory;
    request.target = HostTarget();
    request.layout = ShaderRecompiler::BindingLayout{0u, 0u, 0u, 128u};
    request.useCache = false;  // this service owns the app-side cache

    try {
        const ShaderRecompiler::RecompileResult result = ShaderRecompiler::Recompile(request);
        std::vector<uint32_t> words = result.spirv.Words();
        // Reflection translation runs inside this TU (layer contract) and
        // travels with the module through the cache (M8 T5).
        GuestShaderReflection translated =
            TranslateReflection(result, request.layout);
        std::lock_guard<std::mutex> lock(impl_->mutex);
        ++impl_->stats.recompiles;
        if (impl_->cache.size() >= kMaxCacheEntries) {
            impl_->cache.clear();
            ++impl_->stats.cacheEvictions;
        }
        Impl::CacheEntry entry;
        entry.words = words;
        entry.reflection = translated;
        impl_->cache.emplace(key, std::move(entry));
        impl_->stats.cachedEntries = impl_->cache.size();
        spirv = std::move(words);
        reflection = std::move(translated);
        return true;
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        ++impl_->stats.failures;
        error = e.what();
        return false;
    } catch (...) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        ++impl_->stats.failures;
        error = "unknown recompiler failure";
        return false;
    }
}

} // namespace PX5::Gpu
