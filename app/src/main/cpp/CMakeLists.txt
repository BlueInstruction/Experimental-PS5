cmake_minimum_required(VERSION 3.22.1)

project("px5")

# FEXCore headers use C++20 concepts (requires clauses, std::is_invocable_r_v)
# so the px5 target MUST be compiled with C++20. FEX's own CMakeLists sets this
# only for FEX's subdirectory scope, which does not propagate up to this file.
# Setting it here at parent scope propagates DOWN to FEX's subdirectory too.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ----------------------------------------------------------------------------
# std::atomic_ref polyfill (force-include)
# ----------------------------------------------------------------------------
# NDK 26/27 libc++ does NOT export std::atomic_ref<T> in <atomic> on Bionic,
# even with -std=c++20. FEXCore headers (SpinWaitLock.h, WritePriorityMutex.h)
# heavily use std::atomic_ref. We provide a minimal polyfill using __atomic_*
# builtins and force-include it in EVERY C++ translation unit (px5 + FEXCore)
# via add_compile_options() BEFORE add_subdirectory(fex) so it applies to the
# FEX subdirectory as well.
#
# Note: -include and the path MUST be passed as two separate arguments; the
# generator-expression form below produces two separate compiler options.
set(PX5_ATOMIC_REF_POLYFILL
        "${CMAKE_CURRENT_LIST_DIR}/compat/std_atomic_ref_polyfill.h")
add_compile_options(
        $<$<COMPILE_LANGUAGE:CXX>:-include>
        $<$<COMPILE_LANGUAGE:CXX>:${PX5_ATOMIC_REF_POLYFILL}>)

# ----------------------------------------------------------------------------
# Linker + LTO settings
# ----------------------------------------------------------------------------
# FEX defaults ENABLE_LTO=TRUE, which makes CMake invoke `ld.gold` with the
# LLVMgold.so plugin. NDK 27 does NOT ship LLVMgold.so, so the link fails:
#   /usr/bin/ld.gold: error: LLVMgold.so: could not load plugin library
#   /usr/bin/ld.gold: fatal error: crtbegin_so.o: unsupported ELF machine number 183
# We disable LTO entirely (debug builds don't benefit from it anyway) and
# force the modern lld linker, which is bundled with the NDK and fully
# supports ARM64 (machine number 183).
set(ENABLE_LTO OFF CACHE BOOL "" FORCE)
add_link_options($<$<COMPILE_LANGUAGE:CXX>:-fuse-ld=lld>)

# ----------------------------------------------------------------------------
# ABI routing
# ----------------------------------------------------------------------------
# arm64-v8a : full guest engine (FEXCore JIT x86-64 -> ARM64).
# x86_64    : UI smoke-test library only (see stub/ui_smoke_stub.cpp). The CI
#             Emulator Smoke Test boots an x86_64 AVD; an arm-only APK cannot
#             even be installed there. We ship a symbol-compatible stub so
#             Compose surfaces can be validated end-to-end without faking a
#             CPU backend.
if(ANDROID_ABI STREQUAL "x86_64")
    add_library(
            px5
            SHARED
            stub/ui_smoke_stub.cpp
            gpu/vulkan_device.cpp
            gpu/driver_manager.cpp
            utils/crash_handler.cpp
            utils/logger.cpp)
    find_library(log-lib log)
    find_library(android-lib android)
    target_include_directories(px5 PRIVATE ${CMAKE_CURRENT_LIST_DIR})
    target_compile_features(px5 PRIVATE cxx_std_20)
    target_link_libraries(px5 ${log-lib} ${android-lib})
    return()
endif()

# ----------------------------------------------------------------------------
# Upstream engine sources (FEXCore)
# ----------------------------------------------------------------------------
# The upstream FEX-Emu/FEX tree is deliberately NOT vendored inside this repo.
# It is materialized by tools/fetch_fexcore.sh at a pinned release commit and
# located through PX5_FEXCORE_ROOT (set by Gradle from environment or by hand):
#
#     cmake -DPX5_FEXCORE_ROOT=/path/to/fex ...
#
# Android-specific options below are applied BEFORE entering FEX's CMake
# project so no glibc allocator is enabled.
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_FEXCONFIG OFF CACHE BOOL "" FORCE)
set(BUILD_THUNKS OFF CACHE BOOL "" FORCE)
set(ENABLE_FEX_ALLOCATOR OFF CACHE BOOL "" FORCE)
set(ENABLE_JEMALLOC_GLIBC_ALLOC OFF CACHE BOOL "" FORCE)
set(ENABLE_GLIBC_ALLOCATOR_HOOK_FAULT OFF CACHE BOOL "" FORCE)
set(ENABLE_VIXL_DISASSEMBLER OFF CACHE BOOL "" FORCE)
set(ENABLE_VIXL_SIMULATOR OFF CACHE BOOL "" FORCE)
set(ENABLE_ZYDIS OFF CACHE BOOL "" FORCE)
set(ENABLE_FEXCORE_PROFILER OFF CACHE BOOL "" FORCE)
if(NOT DEFINED PX5_FEXCORE_ROOT)
    message(FATAL_ERROR
        "PX5_FEXCORE_ROOT is not set. Upstream FEX sources are not vendored "
        "inside this repository — run tools/fetch_fexcore.sh first (see "
        "AGENT.md), or pass -DPX5_FEXCORE_ROOT=<path-to-FEX-tree>.")
endif()
if(NOT EXISTS "${PX5_FEXCORE_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR
        "PX5_FEXCORE_ROOT (${PX5_FEXCORE_ROOT}) does not contain a FEX "
        "source tree. Run tools/fetch_fexcore.sh first.")
endif()

list(APPEND CMAKE_MODULE_PATH ${PX5_FEXCORE_ROOT}/Data/CMake)
add_subdirectory(${PX5_FEXCORE_ROOT}
                 ${CMAKE_CURRENT_BINARY_DIR}/fex)

# ----------------------------------------------------------------------------
# Disable FEX targets we don't need on Android
# ----------------------------------------------------------------------------
# FEXCore_shared is fundamentally broken on Android: FEXCore/Source/CMakeLists.txt
# only links FEXCore_Base (which carries fmt, xxhash, Allocator, Config, LogManager
# as PUBLIC deps) into FEXCore_shared when MINGW is set. On Linux/Android, the
# shared library is built without those deps and fails at link time with
# -Wl,--no-undefined (NDK default) due to undefined symbols:
#   fmt::v12::detail::vformat_to, FEXCore::Config::detail::*, FEXCore::Allocator::*,
#   LogMan::*, XXH3_64bits, ...
# We only use the STATIC FEXCore target (linked into libpx5.so), so we just
# exclude FEXCore_shared from the default build set.
#
# We also exclude the host tools (FEX, FEXBash, FEXServer, FEXGetConfig,
# FEXOfflineCompiler, FEXRootFSFetcher, FEXpidof, CodeSizeValidation) since
# they are Linux host binaries that cannot run on Android and would fail to
# link against Android's Bionic libc anyway.
set(_PX5_FEX_TARGETS_TO_EXCLUDE
        FEXCore_shared
        FEX
        FEXBash
        FEXServer
        FEXGetConfig
        FEXOfflineCompiler
        FEXRootFSFetcher
        FEXpidof
        CodeSizeValidation)
foreach(_t ${_PX5_FEX_TARGETS_TO_EXCLUDE})
        if(TARGET ${_t})
                set_target_properties(${_t} PROPERTIES EXCLUDE_FROM_ALL TRUE)
        endif()
endforeach()

add_library(
        px5
        SHARED
        fexcore_wrapper.cpp
        fexcore_integration.cpp
        core/emulator.cpp
        memory/memory.cpp
        kernel/syscalls.cpp
        kernel/sce_kernel_hle.cpp
        kernel/signals.cpp
        loader/elf_loader.cpp
        loader/self_loader.cpp
        gpu/vulkan_device.cpp
        gpu/driver_manager.cpp
        audio/audio.cpp
        audio_input_native.cpp
        input/controller.cpp
        filesystem/vfs.cpp
        utils/logger.cpp
        utils/crash_handler.cpp
        utils/linker_ns_bypass.cpp
        media/media_codec_hle.cpp)

target_include_directories(px5 PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${PX5_FEXCORE_ROOT}/FEXCore/include
        ${CMAKE_CURRENT_BINARY_DIR}/fex/FEXCore/include
        ${CMAKE_CURRENT_BINARY_DIR}/fex/generated)

# Belt and suspenders: explicitly require C++20 on the px5 target itself
# so any consumer of px5 inherits the requirement.
target_compile_features(px5 PRIVATE cxx_std_20)

# The logger still routes PX5_LOG* macros to Android logcat; no file backend.
find_library(log-lib log)
find_library(android-lib android)
find_library(aaudio-lib aaudio)

target_link_libraries(
        px5
        ${log-lib}
        ${android-lib}
        ${aaudio-lib}
        FEXCore
        FEXCore_Base
        JemallocDummy)

# ----------------------------------------------------------------------------
# libadrenotools — real Turnip/custom-driver loading (linker-namespace hook)
# ----------------------------------------------------------------------------
# Materialized outside the repo by tools/fetch_adrenotools.sh and routed
# through PX5_ADRENOTOOLS_ROOT. When absent the build still succeeds and
# driver switching honestly reports system-ICD-only mode.
set(PX5_ADRENOTOOLS_ROOT "" CACHE PATH "Path to libadrenotools source")
if(PX5_ADRENOTOOLS_ROOT AND EXISTS "${PX5_ADRENOTOOLS_ROOT}/CMakeLists.txt")
    set(ADRENOTOOLS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory(${PX5_ADRENOTOOLS_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/adrenotools)
    target_link_libraries(px5 adrenotools)
    target_compile_definitions(px5 PRIVATE PX5_HAVE_ADRENOTOOLS=1)
    message(STATUS "PX5: adrenotools enabled (${PX5_ADRENOTOOLS_ROOT})")
else()
    message(STATUS "PX5: adrenotools NOT provided — system-ICD-only driver mode")
endif()

