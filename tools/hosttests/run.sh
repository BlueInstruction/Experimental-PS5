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
# runtime_linker answers import routes only through the v1.47 seam, so the
# loader test links the NID layer but not the kernel HLE floor (which the
# nid_dispatch_test exercises directly).
"$CXX" -std=c++20 -Wall -Wextra -Wno-unused-parameter -pthread \
    -I"$SHIM" -I"$CPP" \
    -o "$OUT/import_trap_test" \
    "$ROOT/tools/hosttests/import_trap_test.cpp" \
    "$CPP/loader/nid.cpp" \
    "$CPP/loader/elf_loader.cpp" \
    "$CPP/loader/guest_abi.cpp" \
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

echo "[hosttests] building nid_dispatch_test (M9 transparent import dispatch)"
# The NID layer (AnyPS5 algorithm, golden vectors), the symbol registry and
# the transparent DispatchImportCall bridge — the seam that lets a guest
# PLT call reach the real HLE services with its call ABI intact.
# gnm_submit now walks the IR + planner per submission: the two
# recompiler-free TUs it needs ride along; PX5_NO_SHADER_SERVICE names the
# capture leg's honest skip (no M8 service in this binary).
"$CXX" -std=c++20 -Wall -Wextra -Wno-unused-parameter -pthread \
    -DPX5_NO_SHADER_SERVICE \
    -I"$SHIM" -I"$CPP" \
    -o "$OUT/nid_dispatch_test" \
    "$ROOT/tools/hosttests/nid_dispatch_test.cpp" \
    "$CPP/loader/nid.cpp" \
    "$CPP/loader/runtime_linker.cpp" \
    "$CPP/kernel/sce_kernel_hle.cpp" \
    "$CPP/kernel/libc_hle.cpp" \
    "$CPP/kernel/pthread_hle.cpp" \
    "$CPP/kernel/syscalls.cpp" \
    "$CPP/loader/guest_abi.cpp" \
    "$CPP/audio/audio_out_hle.cpp" \
    "$CPP/input/pad_hle.cpp" \
    "$CPP/input/controller.cpp" \
    "$CPP/gpu/gnm/gnm_submit.cpp" \
    "$CPP/gpu/ir/gpu_ir.cpp" \
    "$CPP/gpu/vulkan_backend.cpp" \
    "$CPP/gpu/videoout_hle.cpp" \
    "$CPP/gpu/gnm/pm4_decoder.cpp" \
    "$CPP/gpu/gnm/gnm_state.cpp" \
    "$CPP/filesystem/vfs.cpp" \
    "$CPP/memory/memory.cpp" \
    "$CPP/utils/evidence.cpp" \
    "$CPP/utils/logger.cpp" \
    "$CPP/utils/diag_bridge.cpp" \
    "$CPP/utils/breadcrumbs.cpp" \
    "$CPP/utils/crash_handler.cpp" \
    "$SHIM/fexcore_state_stub.cpp"

echo "[hosttests] running nid_dispatch_test"
"$OUT/nid_dispatch_test"

echo "[hosttests] building videoout_hle_test (M10 T1 sceVideoOut HLE)"
# ORBIS video-out semantics ported from AnyPS5 libSceVideoOut: open
# validation order, the 1920x1080 open default, the RegisterBuffers2
# chain, flip status accounting and the honesty boundary (flips retire,
# no display engine claimed).
# Same gnm_submit TU closure as nid_dispatch_test (IR + planner ride along,
# capture leg named-skip via PX5_NO_SHADER_SERVICE).
"$CXX" -std=c++20 -Wall -Wextra -Wno-unused-parameter -pthread \
    -DPX5_NO_SHADER_SERVICE \
    -I"$SHIM" -I"$CPP" \
    -o "$OUT/videoout_hle_test" \
    "$ROOT/tools/hosttests/videoout_hle_test.cpp" \
    "$CPP/gpu/videoout_hle.cpp" \
    "$CPP/kernel/sce_kernel_hle.cpp" \
    "$CPP/kernel/libc_hle.cpp" \
    "$CPP/kernel/pthread_hle.cpp" \
    "$CPP/kernel/syscalls.cpp" \
    "$CPP/loader/guest_abi.cpp" \
    "$CPP/loader/nid.cpp" \
    "$CPP/loader/runtime_linker.cpp" \
    "$CPP/audio/audio_out_hle.cpp" \
    "$CPP/input/pad_hle.cpp" \
    "$CPP/input/controller.cpp" \
    "$CPP/gpu/gnm/gnm_submit.cpp" \
    "$CPP/gpu/ir/gpu_ir.cpp" \
    "$CPP/gpu/vulkan_backend.cpp" \
    "$CPP/gpu/gnm/pm4_decoder.cpp" \
    "$CPP/gpu/gnm/gnm_state.cpp" \
    "$CPP/filesystem/vfs.cpp" \
    "$CPP/memory/memory.cpp" \
    "$CPP/utils/evidence.cpp" \
    "$CPP/utils/logger.cpp" \
    "$CPP/utils/diag_bridge.cpp" \
    "$CPP/utils/breadcrumbs.cpp" \
    "$CPP/utils/crash_handler.cpp" \
    "$SHIM/fexcore_state_stub.cpp"

echo "[hosttests] running videoout_hle_test"
"$OUT/videoout_hle_test"

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

echo "[hosttests] building dispatch_proof_contract_test (M8 T7 contract)"
# The dispatch proof's expected bytes locked as data — the on-device proof's
# fixed target, verified here before any device session spends time on it.
"$CXX" -std=c++20 -Wall -Wextra -Wno-unused-parameter -pthread \
    -I"$CPP" \
    -o "$OUT/dispatch_proof_contract_test" \
    "$ROOT/tools/hosttests/dispatch_proof_contract_test.cpp" \
    "$CPP/gpu/shader/dispatch_proof_contract.cpp"

echo "[hosttests] running dispatch_proof_contract_test (M8 T7 contract)"
"$OUT/dispatch_proof_contract_test"

echo "osttests] building gnm_submit_gpu_path_test (submit-path wiring gate)"
# The REAL submit TU (GnmSubmit - what sceGnmSubmitCommandBuffers serves)
# walking the full chain per submission: PM4 decode -> GnmState -> M6 IR ->
# M7 planner accounting -> M8 capture -> SPIR-V through the vendored
# recompiler. Fixtures are synthetic; the TUs and the flow are the APK's.
"$CXX" -std=c++20 -Wall -Wextra -Wno-unused-parameter -pthread -DPX5_HOST_TEST \
    -DANYPS5_ENABLE_SPIRV_TOOLS=0 \
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
    -o "$OUT/gnm_submit_gpu_path_test" \
    "$ROOT/tools/hosttests/gnm_submit_gpu_path_test.cpp" \
    "$CPP/gpu/gnm/gnm_submit.cpp" \
    "$CPP/gpu/gnm/gnm_state.cpp" \
    "$CPP/gpu/gnm/pm4_decoder.cpp" \
    "$CPP/gpu/ir/gpu_ir.cpp" \
    "$CPP/gpu/vulkan_backend.cpp" \
    "$CPP/gpu/shader/shader_capture.cpp" \
    "$CPP/gpu/shader/shader_recompiler_service.cpp" \
    "$CPP/utils/logger.cpp" \
    "$CPP/utils/diag_bridge.cpp" \
    "$PORTABLE/px5_spirv_optimizer.cpp" \
    $(cat "$OUT/rec_sources.txt")

echo "osttests] running gnm_submit_gpu_path_test (submit-path wiring gate)"
"$OUT/gnm_submit_gpu_path_test"

echo "[hosttests] building audio_out_hle_test (sceAudioOut HLE)"
# ORBIS audio-out semantics: port table, format decode, volume/Std-mix math,
# virtual-clock pacing — locked before the AAudio backend ever lands.
"$CXX" -std=c++20 -Wall -Wextra -Wno-unused-parameter -pthread \
    -I"$SHIM" -I"$CPP" \
    -o "$OUT/audio_out_hle_test" \
    "$ROOT/tools/hosttests/audio_out_hle_test.cpp" \
    "$CPP/audio/audio_out_hle.cpp" \
    "$CPP/utils/logger.cpp" \
    "$CPP/utils/diag_bridge.cpp"

echo "[hosttests] running audio_out_hle_test (sceAudioOut HLE)"
"$OUT/audio_out_hle_test"

echo "[hosttests] building pad_hle_test (scePad HLE)"
# ORBIS pad semantics: PadData layout (the guest ABI), button-bit mapping,
# stick/trigger byte conversion, read/handle rules — locked against the
# InputManager bridge.
"$CXX" -std=c++20 -Wall -Wextra -Wno-unused-parameter -pthread \
    -I"$SHIM" -I"$CPP" \
    -o "$OUT/pad_hle_test" \
    "$ROOT/tools/hosttests/pad_hle_test.cpp" \
    "$CPP/input/pad_hle.cpp" \
    "$CPP/input/controller.cpp" \
    "$CPP/utils/logger.cpp" \
    "$CPP/utils/diag_bridge.cpp"

echo "[hosttests] running pad_hle_test (scePad HLE)"
"$OUT/pad_hle_test"


echo "osttests] building pthread_hle_test (v1.65 pthread tranche)"
# ORBIS pthread semantics: 27-symbol surface, errorcheck/recursive mutexes,
# cond timedwait (usec contract), guest-thread lifecycle through the
# launcher seam (host launcher; the FEXCore device half is shared with the
# main dispatch path the device sessions prove).
"$CXX" -std=c++20 -Wall -Wextra -Wno-unused-parameter -pthread \
    -I"$SHIM" -I"$CPP" \
    -o "$OUT/pthread_hle_test" \
    "$ROOT/tools/hosttests/pthread_hle_test.cpp" \
    "$CPP/kernel/pthread_hle.cpp" \
    "$CPP/memory/memory.cpp" \
    "$CPP/utils/logger.cpp" \
    "$CPP/utils/diag_bridge.cpp"

echo "osttests] running pthread_hle_test"
"$OUT/pthread_hle_test"

echo "osttests] all host tests passed"
