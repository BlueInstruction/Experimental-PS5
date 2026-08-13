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

# FEXCore is built from the vendored upstream source. Android-specific options
# are set before entering FEX's CMake project so no glibc allocator is enabled.
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
list(APPEND CMAKE_MODULE_PATH
        ${CMAKE_CURRENT_LIST_DIR}/../../../../third_party/fex/Data/CMake)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../../../third_party/fex
                 ${CMAKE_CURRENT_BINARY_DIR}/fex)

add_library(
        px5
        SHARED
        fexcore_wrapper.cpp
        fexcore_integration.cpp
        core/emulator.cpp
        memory/memory.cpp
        memory/memory_map.cpp
        kernel/syscalls.cpp
        kernel/signals.cpp
        kernel_hle.cpp
        loader/elf_loader.cpp
        loader/self_loader.cpp
        gpu/vulkan_device.cpp
        gpu/shader_cache.cpp
        gnm_vulkan_renderer.cpp
        audio/audio.cpp
        audio_input_native.cpp
        input/controller.cpp
        filesystem/vfs.cpp
        utils/logger.cpp
        utils/linker_ns_bypass.cpp
        media/media_codec_hle.cpp
        turnip_hook.cpp)

target_include_directories(px5 PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../../../../third_party/fex/FEXCore/include
        ${CMAKE_CURRENT_BINARY_DIR}/fex/FEXCore/include
        ${CMAKE_CURRENT_BINARY_DIR}/fex/generated)

# Belt and suspenders: explicitly require C++20 on the px5 target itself
# so any consumer of px5 inherits the requirement.
target_compile_features(px5 PRIVATE cxx_std_20)

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
