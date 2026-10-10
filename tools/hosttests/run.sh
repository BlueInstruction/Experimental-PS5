#!/usr/bin/env bash
# ============================================================================
# Host-side regression tests for logic that needs no device.
#
# WHY THIS EXISTS: the memory manager's address queries were key lookups into
# a map keyed by block base, so every address past a block's first page was
# reported unmapped. Four CI workflows were green throughout, because all of
# them only compile. Compiling is not running -- so this runs.
#
# These tests link the REAL sources (no reimplementation) against a tiny shim
# for the two Android headers the code touches, and assert behaviour.
#
#   ./tools/hosttests/run.sh
# ============================================================================
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CPP="$ROOT/app/src/main/cpp"
SHIM="$ROOT/tools/hosttests/shim"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

CXX="${CXX:-g++}"
echo "[hosttests] compiler: $($CXX --version | head -1)"

echo "[hosttests] building memory_range_test"
"$CXX" -std=c++20 -Wall -Wextra -Wno-unused-parameter -pthread \
    -I"$SHIM" -I"$CPP" \
    -o "$OUT/memory_range_test" \
    "$ROOT/tools/hosttests/memory_range_test.cpp" \
    "$CPP/memory/memory.cpp" \
    "$CPP/utils/logger.cpp" \
    "$CPP/utils/diag_bridge.cpp"

echo "[hosttests] running memory_range_test"
"$OUT/memory_range_test"

echo "[hosttests] building import_trap_test"
"$CXX" -std=c++20 -Wall -Wextra -Wno-unused-parameter -pthread \
    -I"$SHIM" -I"$CPP" \
    -o "$OUT/import_trap_test" \
    "$ROOT/tools/hosttests/import_trap_test.cpp" \
    "$CPP/loader/elf_loader.cpp" \
    "$CPP/loader/self_extract.cpp" \
    "$CPP/loader/runtime_linker.cpp" \
    "$CPP/memory/memory.cpp" \
    "$CPP/utils/evidence.cpp" \
    "$CPP/utils/logger.cpp" \
    "$CPP/utils/diag_bridge.cpp" \
    "$CPP/utils/breadcrumbs.cpp" \
    "$CPP/utils/crash_handler.cpp" \
    "$SHIM/fexcore_state_stub.cpp" \
    -lz

echo "[hosttests] running import_trap_test"
"$OUT/import_trap_test" "$OUT/import_trap_ledger.log"

echo "[hosttests] building pm4_stream_test (M5 gate)"
"$CXX" -std=c++20 -Wall -Wextra -Wno-unused-parameter -pthread \
    -I"$SHIM" -I"$CPP" \
    -o "$OUT/pm4_stream_test" \
    "$ROOT/tools/hosttests/pm4_stream_test.cpp" \
    "$CPP/gpu/gnm/pm4_decoder.cpp" \
    "$CPP/gpu/gnm/gnm_state.cpp"

echo "[hosttests] running pm4_stream_test (M5 gate)"
"$OUT/pm4_stream_test"

echo "[hosttests] building gpu_ir_test (M6 gate)"
"$CXX" -std=c++20 -Wall -Wextra -Wno-unused-parameter -pthread \
    -I"$SHIM" -I"$CPP" \
    -o "$OUT/gpu_ir_test" \
    "$ROOT/tools/hosttests/gpu_ir_test.cpp" \
    "$CPP/gpu/ir/gpu_ir.cpp" \
    "$CPP/gpu/gnm/pm4_decoder.cpp" \
    "$CPP/gpu/gnm/gnm_state.cpp"

echo "[hosttests] running gpu_ir_test (M6 gate)"
"$OUT/gpu_ir_test"

echo "[hosttests] building vulkan_backend_test (M7 T1 gate)"
"$CXX" -std=c++20 -Wall -Wextra -Wno-unused-parameter -pthread \
    -I"$SHIM" -I"$CPP" \
    -o "$OUT/vulkan_backend_test" \
    "$ROOT/tools/hosttests/vulkan_backend_test.cpp" \
    "$CPP/gpu/vulkan_backend.cpp" \
    "$CPP/gpu/ir/gpu_ir.cpp" \
    "$CPP/gpu/gnm/pm4_decoder.cpp" \
    "$CPP/gpu/gnm/gnm_state.cpp"

echo "[hosttests] running vulkan_backend_test (M7 T1 gate)"
"$OUT/vulkan_backend_test"

echo "[hosttests] running JNI symbol parity check"
python3 "$ROOT/tools/check_jni_symbols.py"


if [ ! -f "$ROOT/.deps/anyps5/.pin" ]; then
    echo "[hosttests] materializing AnyPS5 recompiler pin"
    "$ROOT/tools/fetch_shader_recompiler.sh"
fi

echo "[hosttests] building shader_recompiler_test (M8 T1)"
REC="$ROOT/.deps/anyps5"
PORTABLE="$ROOT/tools/hosttests/portable"
SPIRVHDRS="$ROOT/tools/hosttests/spirv"
# Vendored AnyPS5 recompiler: whole subtree except DummyShaders.cpp (glslang)
# and SpirvOptimizer.cpp (SPIRV-Tools); port layer supplies both contracts.
find "$REC/core/shader/recompiler" -name '*.cpp' ! -name 'DummyShaders.cpp' ! -name 'SpirvOptimizer.cpp' ! -name 'Deserialize.cpp' | sort > "$OUT/rec_sources.txt"
# shellcheck disable=SC2046
"$CXX" -std=c++20 -Wall -Wextra -Wno-unused-parameter -pthread -DANYPS5_ENABLE_SPIRV_TOOLS=0 \
    -I"$SHIM" -I"$CPP" \
    -I"$REC/core/libs" \
    -I"$REC/core/shader/recompiler" \
    -I"$REC/core/shader/recompiler/RdnaDecoder/include" \
    -I"$REC/core/shader/recompiler/ControlFlow/include" \
    -I"$REC/core/shader/recompiler/IntermediateRepresentation/include" \
    -I"$REC/core/shader/recompiler/Translation/include" \
    -I"$REC/core/shader/recompiler/Optimization/include" \
    -I"$REC/core/shader/recompiler/SpirvBackend/include" \
    -I"$SPIRVHDRS" \
    -I"$PORTABLE" \
    -o "$OUT/shader_recompiler_test" \
    "$ROOT/tools/hosttests/shader_recompiler_test.cpp" \
    "$PORTABLE/px5_spirv_optimizer.cpp" \
    $(cat "$OUT/rec_sources.txt")

echo "[hosttests] running shader_recompiler_test (M8 T1)"
"$OUT/shader_recompiler_test"

echo "[hosttests] building shader_recompiler_stages_test (M8 T2)"
# Same vendored-subtree compile as T1, second entry point exercising the
# graphics stages (vertex + fragment) through the real recompiler.
"$CXX" -std=c++20 -Wall -Wextra -Wno-unused-parameter -pthread -DANYPS5_ENABLE_SPIRV_TOOLS=0 \
    -I"$SHIM" -I"$CPP" \
    -I"$REC/core/libs" \
    -I"$REC/core/shader/recompiler" \
    -I"$REC/core/shader/recompiler/RdnaDecoder/include" \
    -I"$REC/core/shader/recompiler/ControlFlow/include" \
    -I"$REC/core/shader/recompiler/IntermediateRepresentation/include" \
    -I"$REC/core/shader/recompiler/Translation/include" \
    -I"$REC/core/shader/recompiler/Optimization/include" \
    -I"$REC/core/shader/recompiler/SpirvBackend/include" \
    -I"$SPIRVHDRS" \
    -I"$PORTABLE" \
    -o "$OUT/shader_recompiler_stages_test" \
    "$ROOT/tools/hosttests/shader_recompiler_stages_test.cpp" \
    "$PORTABLE/px5_spirv_optimizer.cpp" \
    $(cat "$OUT/rec_sources.txt")

echo "[hosttests] running shader_recompiler_stages_test (M8 T2)"
"$OUT/shader_recompiler_stages_test"

echo "[hosttests] building guest_shader_recompiler_test (M8 T3)"
# The app-side shader service (gpu/shader/shader_recompiler_service.cpp) is
# the TU the arm64 APK compiles; this gate proves it end to end on the host.
"$CXX" -std=c++20 -Wall -Wextra -Wno-unused-parameter -pthread -DANYPS5_ENABLE_SPIRV_TOOLS=0 \
    -I"$CPP" \
    -I"$REC/core/libs" \
    -I"$REC/core/shader/recompiler" \
    -I"$REC/core/shader/recompiler/RdnaDecoder/include" \
    -I"$REC/core/shader/recompiler/ControlFlow/include" \
    -I"$REC/core/shader/recompiler/IntermediateRepresentation/include" \
    -I"$REC/core/shader/recompiler/Translation/include" \
    -I"$REC/core/shader/recompiler/Optimization/include" \
    -I"$REC/core/shader/recompiler/SpirvBackend/include" \
    -I"$SPIRVHDRS" \
    -I"$PORTABLE" \
    -o "$OUT/guest_shader_recompiler_test" \
    "$ROOT/tools/hosttests/guest_shader_recompiler_test.cpp" \
    "$CPP/gpu/shader/shader_recompiler_service.cpp" \
    "$PORTABLE/px5_spirv_optimizer.cpp" \
    $(cat "$OUT/rec_sources.txt")

echo "[hosttests] running guest_shader_recompiler_test (M8 T3)"
"$OUT/guest_shader_recompiler_test"

echo "[hosttests] building guest_shader_capture_test (M8 T4)"
# M8 T4: real PM4 SET_SH streams decode into GnmState and the shader
# capture layer turns the register state into SPIR-V via the T3 service.
# Same TU set the arm64 APK compiles, plus the GNM decoder/state sources.
"$CXX" -std=c++20 -Wall -Wextra -Wno-unused-parameter -pthread -DANYPS5_ENABLE_SPIRV_TOOLS=0 \
    -I"$CPP" \
    -I"$REC/core/libs" \
    -I"$REC/core/shader/recompiler" \
    -I"$REC/core/shader/recompiler/RdnaDecoder/include" \
    -I"$REC/core/shader/recompiler/ControlFlow/include" \
    -I"$REC/core/shader/recompiler/IntermediateRepresentation/include" \
    -I"$REC/core/shader/recompiler/Translation/include" \
    -I"$REC/core/shader/recompiler/Optimization/include" \
    -I"$REC/core/shader/recompiler/SpirvBackend/include" \
    -I"$SPIRVHDRS" \
    -I"$PORTABLE" \
    -o "$OUT/guest_shader_capture_test" \
    "$ROOT/tools/hosttests/guest_shader_capture_test.cpp" \
    "$CPP/gpu/shader/shader_capture.cpp" \
    "$CPP/gpu/shader/shader_recompiler_service.cpp" \
    "$CPP/gpu/gnm/pm4_decoder.cpp" \
    "$CPP/gpu/gnm/gnm_state.cpp" \
    "$PORTABLE/px5_spirv_optimizer.cpp" \
    $(cat "$OUT/rec_sources.txt")

echo "[hosttests] running guest_shader_capture_test (M8 T4)"
"$OUT/guest_shader_capture_test"

echo "[hosttests] building graphics_pipeline_desc_test (M8 T6)"
# Vulkan-free desc/validator/merge — pure data + pure functions, no recompiler.
"$CXX" -std=c++20 -Wall -Wextra -Wno-unused-parameter -pthread \
    -I"$CPP" \
    -o "$OUT/graphics_pipeline_desc_test" \
    "$ROOT/tools/hosttests/graphics_pipeline_desc_test.cpp" \
    "$CPP/gpu/shader/graphics_pipeline_desc.cpp"

echo "[hosttests] running graphics_pipeline_desc_test (M8 T6)"
"$OUT/graphics_pipeline_desc_test"

echo "[hosttests] all host tests passed"
