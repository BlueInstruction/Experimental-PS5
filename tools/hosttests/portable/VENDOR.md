# AnyPS5 shader recompiler (vendored)

Source: `BlueInstruction/AnyPS5` @ `c72d5ce112194139b32f5587634c7cbaf4a6f9f3`
(2026-10-10, merge of PR #2537).

License: **GPL-2.0-only** — same as PX5. Keep the upstream `LICENSE.upstream`
file next to the tree; preserve copyright headers when updating.

## What is vendored (verbatim, no edits)

- `core/shader/recompiler/` — RDNA → IR → SPIR-V recompiler library
  (~52k LOC: RndaDecoder, ControlFlow, IntermediateRepresentation, Translation,
  Optimization, SrtWalker, SpirvBackend, ShaderDiskCache).
- `core/libs/prx/libc/include/HostThreadLocal.hpp` — the one include that
  escapes the recompiler subtree upstream.
- `3rdparty/SPIRV-Headers/include/spirv/unified1/{spirv.hpp,GLSL.std.450.h}` —
  Khronos headers (MIT), pinned by content, the only third-party includes the
  recompiler needs without SPIRV-Tools.

## PX5 port layer (`portable/`, NOT upstream files)

- `ShaderCacheVersion.hpp` — substitutes the CMake-generated header.
- `px5_spirv_optimizer.cpp` — implements `ValidateAndOptimizeSpirv` with
  structural validation (no SPIRV-Tools). Upstream compiles its own optimizer
  only under `ANYPS5_ENABLE_SPIRV_TOOLS=1`; upstream CI uses =ON. Replace this
  file's body with a SPIRV-Tools-backed implementation when strict validation
  is needed; the vendor tree stays untouched.

## Build (host tests)

`tools/hosttests/run.sh` compiles the subtree with g++ directly:
everything except `tests/DummyShaders.cpp` (needs glslang — the only upstream
consumer) and `SpirvBackend/src/SpirvOptimizer.cpp` (needs SPIRV-Tools), plus
the port layer. Compile definition `ANYPS5_ENABLE_SPIRV_TOOLS=0` is required.

## Syncing a new upstream revision

```
git clone --depth 1 https://github.com/BlueInstruction/AnyPS5 /tmp/anyps5
rsync -a --delete /tmp/anyps5/core/shader/recompiler/ core/shader/recompiler/
rsync -a /tmp/anyps5/core/libs/prx/libc/include/HostThreadLocal.hpp \
      core/libs/prx/libc/include/
# update the pin commit here + SourceVersion in portable/ShaderCacheVersion.hpp
```
