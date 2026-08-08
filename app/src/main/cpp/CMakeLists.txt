cmake_minimum_required(VERSION 3.22.1)

project("px5")

add_library(
        px5
        SHARED
        fexcore_wrapper.cpp
        core/emulator.cpp
        cpu/fex/fex_wrapper.cpp
        cpu/fex/fex_guest_engine.cpp
        cpu/fex/fex_guest_cpu.cpp
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

find_library(log-lib log)
find_library(android-lib android)
find_library(aaudio-lib aaudio)

target_link_libraries(
        px5
        ${log-lib}
        ${android-lib}
        ${aaudio-lib})
