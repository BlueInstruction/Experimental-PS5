# SharpDroid v7 field baseline vs PSX5 — same game, same phone, same driver

Purpose: a measured baseline to compare PSX5 against, and the v1.70 decisive
test protocol. Numbers here are parsed from the user-supplied
`sharpdroid-v7-log.txt` (Dreaming Sarah PPSA02929, uploaded 2026-10-12) and
from the PSX5 v1.69 device logs. Nothing below is an agent claim; every line
cites its source log.

## 1. SharpDroid v7 — what the log actually proves

| item | value (from sharpdroid-v7-log.txt) |
|---|---|
| device class | Adreno 610 phone, 8 cores, midr=0x51af8014 (Qualcomm), LSE atomics=0 |
| driver | Turnip v26.3.0-R6, Vulkan 1.4.359, injected via adrenotools |
| FEXCore probe | `AVX=1 SVE128=0 SVE256=0` — ARM64 host probe |
| FEX flags | TSOEnabled=1, HalfBarrierTSO=1, Multiblock=1, DynamicL1Cache=1, DisableL2Cache=1, VectorTSO=0 |
| eboot.bin | 120,776 relocations, **484 import stubs** |
| libc.prx | **real module executed**: 1,896 relocs, 102 imports, **5,130 symbols**, `dt_init` ran |
| import setup | 586 stubs "direct bridge, lle_redirects=**139**" |
| guest threads | **31 scheduled** (spi_main_thread, AudioOutThread, TrophyManager, saveThreadProc, 12+ ratamedia streamer, …) |
| unhandled syscalls | 34 lines (239/309) — logged, ignored, game continued |
| Vulkan | android surface, driver swapchain, VideoOut ready 2341x1080 R8G8B8A8Unorm |
| boot | `10 of 10 checkpoints, first frame at 53.054 s` |
| present | `presented frame 1` @3840x2160 (game RT), guest frames @2880x1620 (=0.75 render scale), then `frame 300`, `frame 600` |
| audio | guest stream 48 kHz, 99.2% fill, 1 xrun total |
| guest stdout | engine's own lines: `Loading 19 event sheets and 58 layouts` → `Done loading` → background ogg/flac loads → `ON SAVEGAME MISSING` |
| user-measured | ~20 FPS (user statement, not from log) |

## 2. Why this matters for the AVX fix (PSX5 v1.70)

- FEX-2608's own ARM64 host probe sets `SupportsAVX = true` unconditionally
  (`Source/Common/HostFeatures.cpp`, ARM64 section of the pinned tree).
  SharpDroid inherited that: probe shows `AVX=1` with SVE off — exactly the
  config PSX5 v1.70 now sets by hand (`VEXTableOps_AVX128`, paired-NEON
  emulation).
- PSX5 v1.69 hand-built `HostFeatures` and zeroed AVX → no VEX decode table
  (FEX-2608 Frontend.cpp:82-87) → the first VEX instruction in the game's
  statically linked code (`vxorps` @0x1402e3305) raised a guest SIGILL.
- So v1.70 restores what FEX itself would have done. Corroborated by the
  field log — **not yet proven on-device**.

## 3. PSX5 v1.69 baseline (same game, same eboot sha 0e95ecd2, same phone)

119,961 relocations applied, 815 import slots trapped (370 abiResolved data
objects), 326 HLE symbols (libc slice, binary not executed), real guest ran
7.54 ms: `_init_env` → `atexit(NULL)` → `atexit(func=0x1402e7070)` → SIGILL at
0x1402e3305 (`vxorps`). draws=0, frames=0. Verdict tool: gates 2–5 red.

## 4. v1.70 decisive test protocol (A610 phone, same game, no [SYNTH])

**T1 — CPU/FEXCore layer**
- PASS: HostFeatures log line shows `AVX=1`, AND the last real-guest RIP is
  strictly past the atexit sequence — no trap at 0x1402e3305.
- If a new stop appears: record RIP + guest bytes + dissection, then stop.
- FAIL: RIP still 0x1402e3305 (fix didn't remove the wall), even with AVX=1.

**T2 — PS5 system layer**
- PASS: distinct IMPORT-CALL hits > 2 (beyond `_init_env`/`atexit`), real-guest
  `syscalls total > 3`, runtime >> 7.54 ms, guest thread scheduling visible.
- FAIL: same three calls then stop.

**T3 — GPU/present layer**
- PASS: evidence log `REAL-GUEST … gpu-stats submits>0 draws>0`, then
  `Render loop stopped (frames=N>0)` — a guest frame, not host surface setup.
- Explicitly NOT evidence: Vulkan device init, swapchain creation,
  `AVX=1` itself, HLE symbol counts, [SYNTH] passes.

**Decision rules (binding):**
1. RIP unchanged → fix failed; review evidence + code before proposing another.
2. RIP moved → limited, proven progress — report as such, nothing more.
3. No release notes, no success claims, until real v1.70 logs exist.
4. The SIGILL at rip=0x140400000 in the main log is the synthetic ud2 test —
   never counts (verdict tool already attributes it).

## 5. Architectural gap to decide AFTER T1/T2 (not started)

SharpDroid executes the **real** `libc.prx` (5,130 symbols; 139 lle_redirects)
inside its JIT; PSX5 reimplements a 326-symbol HLE slice and never runs the
module binary. Two paths forward once the boot cascade moves:
(a) keep expanding HLE, guided strictly by real import-call hits; or
(b) load and run the real libc.prx module through PSX5's loader (SharpDroid
model — the CRT bugs come pre-solved, but the loader gains module machinery).
Decision deferred until T1/T2 results exist. Also noted: SharpDroid tolerated
34 unhandled syscalls by log-and-continue; PSX5's ledger must behave the same
for unknown non-fatal calls when the cascade reaches that stage.
