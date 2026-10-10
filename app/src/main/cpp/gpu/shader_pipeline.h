#pragma once
// PX5 facade over the vendored shader recompiler (see
// gpu/shader_recompiler/PX5_PORT.md for provenance and license).
//
// Ownership boundary: everything PX5-specific lives here; the vendored tree
// stays upstream-shaped. This is the only header the rest of the engine
// includes — the M7 Vulkan backend asks for SPIR-V through this interface
// and never sees recompiler types.
//
// Failure model: no exceptions cross this boundary. CompileGuestShader
// returns std::nullopt and fills `error` with the recompiler's named
// rejection (the same string the host gate prints).

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace px5::gpu {

// The three stages the M7 render path needs. The recompiler supports more
// (tessellation, mesh, compute variants) — extend this enum when the command
// planner grows the corresponding GpuOp kinds, not before.
enum class ShaderStageKind {
    Vertex,
    Fragment,
    Compute,
};

struct CompiledShader {
    std::vector<std::uint32_t> spirv;
    std::uint64_t variantId = 0;
    bool fromCache = false;
};

// Pin the compiled-shader disk cache to a writable directory (call once at
// engine init with the app files dir). An empty path disables the cache.
// Without this the recompiler would try <apk-lib-dir>/shader_cache, which is
// not writable on Android.
void SetShaderCacheDir(const std::string& path);

// Compile guest RDNA (GFX10.3) shader code to a Vulkan-consumable SPIR-V
// module. Stage context beyond the code bytes (vertex fetch info, pixel
// export info, user data) starts zeroed here; the M7 planner fills it from
// GnmState as those bindings come online.
std::optional<CompiledShader> CompileGuestShader(ShaderStageKind stage,
                                                 std::span<const std::uint32_t> code,
                                                 std::string* error);

// Runtime ABI version of the vendored recompiler this engine was built
// against (RuntimeAbi::Version). Diagnostics and the disk cache key both
// embed it; report it in logs so a stale vendored tree is visible.
std::uint32_t ShaderPipelineAbiVersion();

} // namespace px5::gpu
