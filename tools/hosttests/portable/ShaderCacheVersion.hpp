// PX5 port layer — substitutes the upstream CMake-generated header
// (core/shader/recompiler/ShaderCacheVersion.cmake) for the host build,
// which does not go through upstream CMake. Content mirrors the
// generated schema: a source version and the recompiler switch list.
#ifndef CORE_SHADER_RECOMPILER_SHADERCACHEVERSION_HPP
#define CORE_SHADER_RECOMPILER_SHADERCACHEVERSION_HPP

#include <cstdint>
#include <string_view>

namespace ShaderRecompiler::ShaderDiskCache::Generated {

// PX5 vendor pin: BlueInstruction/AnyPS5 @c72d5ce1 (2026-10-10), recompiler subtree.
inline constexpr std::uint64_t SourceVersion = 0x505835c72d5ce1ull;

inline constexpr std::string_view RecompilerSwitches[] = {
    "PX5_HOST_TEST",
};

}

#endif
