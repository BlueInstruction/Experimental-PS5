#!/usr/bin/env bash
# Build and run the shader-recompiler host gate (tools/shader_recompiler_host).
# Fails (exit != 0) when the vendored recompiler or the PX5 facade cannot
# compile, link, or emit valid SPIR-V for a synthetic RDNA program.
#
# Preferred path: cmake. If cmake is not installed, a direct g++ invocation
# over the same source list + include roots is used instead (same gate).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
CPP_DIR="$(cd "$HERE/../../app/src/main/cpp" && pwd)"
SR_DIR="$CPP_DIR/gpu/shader_recompiler"
BUILD="${HERE}/build"

if command -v cmake >/dev/null 2>&1; then
    cmake -S "$HERE" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$BUILD" -j "$(nproc 2>/dev/null || echo 4)"
    SMOKE="$BUILD/recompiler_smoke"
else
    echo "cmake not found — direct g++ gate"
    mkdir -p "$BUILD"
    SRCS=$(find "$SR_DIR" -name "*.cpp" \
        ! -path "*/tests/*" ! -path "*/px5_shim/*" ! -path "*/generated/*" \
        ! -name "Deserialize.cpp" | sort)
    g++ -std=c++20 -O1 -Wno-unused-result \
        -I"$SR_DIR" -I"$SR_DIR/generated" \
        -I"$SR_DIR/RdnaDecoder/include" \
        -I"$SR_DIR/ControlFlow/include" \
        -I"$SR_DIR/IntermediateRepresentation/include" \
        -I"$SR_DIR/Translation/include" \
        -I"$SR_DIR/Optimization/include" \
        -I"$SR_DIR/SpirvBackend/include" \
        -I"$SR_DIR/px5_shim" \
        -I"$CPP_DIR/third_party/spirv-headers/include" \
        -I"$CPP_DIR/gpu" \
        "$HERE/smoke.cpp" "$CPP_DIR/gpu/shader_pipeline.cpp" \
        $SRCS \
        -o "$BUILD/recompiler_smoke"
    SMOKE="$BUILD/recompiler_smoke"
fi

rm -rf "$BUILD/gate_cache"
mkdir -p "$BUILD/gate_cache"
cd "$BUILD/gate_cache"
"$SMOKE"
