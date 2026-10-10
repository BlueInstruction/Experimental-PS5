cmake_minimum_required(VERSION 3.22.1)
project(px5_shader_recompiler_host CXX)

# Host gate for the vendored shader recompiler. Builds the ENTIRE vendored
# library on the host toolchain plus the PX5 facade, then runs a synthetic
# RDNA program through the full pipeline (decode -> IR -> control flow ->
# SPIR-V) and verifies the module. Run before pushing changes that touch
# app/src/main/cpp/gpu/shader_recompiler/:
#
#   cmake -S tools/shader_recompiler_host -B /tmp/srhost
#   cmake --build /tmp/srhost -j
#   /tmp/srhost/recompiler_smoke
#
# Exits 0 only when the ABI handshake, the facade boundary and the direct
# pipeline probe all emit valid SPIR-V.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(PX5_CPP_DIR "${CMAKE_CURRENT_LIST_DIR}/../../app/src/main/cpp")

add_subdirectory(${PX5_CPP_DIR}/gpu/shader_recompiler shader_recompiler_build)

add_executable(recompiler_smoke smoke.cpp)
target_compile_features(recompiler_smoke PRIVATE cxx_std_20)
target_sources(recompiler_smoke PRIVATE ${PX5_CPP_DIR}/gpu/shader_pipeline.cpp)
target_include_directories(recompiler_smoke PRIVATE ${PX5_CPP_DIR}/gpu)
target_link_libraries(recompiler_smoke px5_shader_recompiler)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(recompiler_smoke PRIVATE -Wno-unused-result)
endif()
