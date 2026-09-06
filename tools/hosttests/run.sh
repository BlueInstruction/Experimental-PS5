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

echo "[hosttests] all host tests passed"
