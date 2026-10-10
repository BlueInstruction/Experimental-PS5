#!/usr/bin/env bash
# ============================================================================
# Deterministic bootstrap of the AnyPS5 RDNA shader recompiler (M8 engine).
#
# Same model as fetch_fexcore.sh: the upstream sources are NOT committed —
# they materialize at a pinned commit of BlueInstruction/AnyPS5 (the project
# owner's fork; GPL-2.0-only, same license as PX5) into .deps/anyps5/.
#
#   Local :  ./tools/fetch_shader_recompiler.sh
#   CI    :  ./tools/fetch_shader_recompiler.sh
#
# Extracted: core/shader/recompiler (the whole recompiler library) plus the
# single file that escapes it upstream (prx/libc HostThreadLocal.hpp).
# PX5's port layer lives in-tree at tools/hosttests/portable/ and substitutes
# the two upstream build-only contracts (ShaderCacheVersion.hpp generation
# and the SPIRV-Tools-backed SpirvOptimizer) for the host test build.
#
# To upgrade the pin: bump PIN_SHA below to the new BlueInstruction/AnyPS5
# commit, re-run, and update SourceVersion in portable/ShaderCacheVersion.hpp.
# ============================================================================
set -euo pipefail

PIN_SHA="c72d5ce112194139b32f5587634c7cbaf4a6f9f3"
UPSTREAM="https://github.com/BlueInstruction/AnyPS5.git"
DEST="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/.deps/anyps5"

if [ -d "$DEST/core/shader/recompiler" ] && [ -f "$DEST/.pin" ] && [ "$(cat "$DEST/.pin")" = "$PIN_SHA" ]; then
    echo "[fetch_shader_recompiler] already materialized at $PIN_SHA"
    exit 0
fi

rm -rf "$DEST"
mkdir -p "$DEST"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo "[fetch_shader_recompiler] fetching $UPSTREAM @ $PIN_SHA"
git clone --filter=blob:none --no-checkout "$UPSTREAM" "$TMP/repo" 2>/dev/null
git -C "$TMP/repo" checkout "$PIN_SHA" -- \
    core/shader/recompiler \
    core/libs/prx/libc/include/HostThreadLocal.hpp \
    LICENSE 2>/dev/null

mkdir -p "$DEST/core"
mv "$TMP/repo/core/shader" "$DEST/core/"
mkdir -p "$DEST/core/libs/prx/libc/include"
mv "$TMP/repo/core/libs/prx/libc/include/HostThreadLocal.hpp" "$DEST/core/libs/prx/libc/include/"
mv "$TMP/repo/LICENSE" "$DEST/LICENSE.upstream"

echo "$PIN_SHA" > "$DEST/.pin"
echo "[fetch_shader_recompiler] materialized $(find "$DEST/core/shader/recompiler" -name '*.cpp' | wc -l) .cpp + headers at $DEST"
