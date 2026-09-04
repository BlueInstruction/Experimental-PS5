# Experimental-PS5 (PSX5)

An experimental **PS5 compatibility layer for Android ARM64**:

* **CPU:** [FEXCore](https://github.com/FEX-Emu/FEX) x86-64 → ARM64 JIT,
  built from a pinned upstream release and statically linked into the app.
* **GPU:** Vulkan, targeting Qualcomm Adreno A7xx–A8xx phones with optional
  patched Mesa Turnip drivers via
  [libadrenotools](https://github.com/bylaws/libadrenotools).
* **OS surface:** an original guest-memory manager, syscall layer, ELF64
  loader, and a minimal kernel-HLE seam — no glibc runtime in between.
  The PS5 OS surface is reimplemented natively against Android bionic:
  there is no Wine-like compatibility environment and no Linux runtime
  container.

PSX5 is an independent implementation; external projects are engineering
references only (see Credits).

> This is early engineering research. It does not run commercial games.
> The status table below reflects runtime evidence only.

## Current status

| Area | Status |
|------|--------|
| FEXCore bring-up | working: static build for arm64-v8a; guest instructions execute and results are observed at runtime |
| Memory / syscalls / loader | partial: real implementations with self-tests; hardening ongoing |
| Vulkan device layer | partial: instance/device/submission loop proven in smoke tests |
| GNM → Vulkan graphics translation | not started |
| Audio / input / UI shell | partial: functional wrappers + Compose shell with real preferences |
| Driver switching | partial: slot manager present; on-device `vulkaninfo` verification next |

## Build

Requirements: JDK 17, Gradle 8.9, SDK API 35, NDK 27.3.13750724, CMake 3.22.1

```bash
./tools/fetch_fexcore.sh          # materializes pinned upstream FEX sources
export PX5_FEXCORE_ROOT="$PWD/../deps/FEX"
gradle assembleRelease --no-daemon
```

CI (`PX5 Build APK`) performs exactly this sequence on every push to
`main`, alongside `android-lint`, `cppcheck`, and `clang-tidy` analysis.

## Documentation

[AGENTS.md](AGENTS.md) covers the repository layout, the pinned FEXCore
dependency contract, and commit message conventions.

## Legal

This repository contains only original glue code and open-source engines.
It must not be used to distribute Sony firmware, proprietary keys, or
commercial game assets.

## Credits

Thanks to the projects this work builds on and learns from:

* **[FEX-Emu](https://github.com/FEX-Emu/FEX)** — FEXCore, the x86-64 →
  ARM64 CPU translation engine that powers PSX5 (MIT), and a host-layer
  reference for running x86-64 guests on ARM64 Linux-style platforms.
* **[prosper](https://github.com/mattias800/prosper)** — engineering
  reference for the PS5 OS surface: SELF → loader → NID dispatch → HLE →
  AGC/PM4 → RDNA2 → SPIR-V.
* **[shadPS4](https://github.com/shadps4-emu/shadPS4)** — PS4-era loader and
  GNM design reference, and SELF container format cross-checks.
* **[Kyty](https://github.com/InoriRus/Kyty)** and
  **[GPCS4](https://github.com/Inori/GPCS4)** — PS4/PS5 loader, HLE, and
  shader-recompiler references.
* **[CLRX](https://github.com/CLRX/CLRX-mirror)** — AMD GCN ISA
  documentation reference.
* **[libadrenotools](https://github.com/bylaws/libadrenotools)** — custom
  Vulkan driver injection on Adreno.
* **Mesa Turnip** — the open Adreno Vulkan driver our driver slots are
  built around.
* **[Winlator](https://github.com/brunodev85/winlator)** and its Bionic
  Ludashi builds ([StevenMXZ/Winlator-Ludashi](https://github.com/StevenMXZ/Winlator-Ludashi))
  — the proof that native-bionic x86-64 guest execution on Android is a
  viable model.

Android PS4/PS5 attempts such as
[sharpdroid](https://github.com/mircowuffwuff/sharpdroid) and
[Bachata S4](https://github.com/JICA98/Bachata-S4) are studied as comparison
points; no code is taken from them.
