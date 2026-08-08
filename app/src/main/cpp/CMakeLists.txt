cmake_minimum_required(VERSION 3.22.1)

project("px5")

add_library(
        px5
        SHARED
        fexcore_wrapper.cpp
        kernel_hle.cpp
        gnm_vulkan_renderer.cpp
        audio_input_native.cpp
        turnip_hook.cpp)

find_library(log-lib log)
find_library(android-lib android)
find_library(aaudio-lib aaudio)

target_link_libraries(
        px5
        ${log-lib}
        ${android-lib}
        ${aaudio-lib})
