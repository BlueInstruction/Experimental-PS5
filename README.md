# Experimental-PS5 (PSX5)

An experimental **PS5 compatibility layer for Android ARM64**, built around
real engine technology rather than mockups:

* **CPU:** [FEXCore](https://github.com/FEX-Emu/FEX) x86-64 → ARM64 JIT,
  built from a pinned upstream release and statically linked into the app.
* **GPU:** Vulkan, targeting Qualcomm Adreno A7xx–A8xx phones with optional
  patched Mesa Turnip drivers via
  [libadrenotools](https://github.com/bylaws/libadrenotools).
* **OS surface:** an original guest-memory manager, syscall layer, ELF64
  loader, and a minimal kernel-HLE seam — no glibc runtime in between.

> ⚠️ This is early engineering work. It does not run commercial games.
> The project refuses to fake features: see the honest status ledger in
> [AGENT.md §5](AGENT.md#5-subsystem-status-honesty-ledger).

## Current status (evidence-based)

| Area | Today |
|------|-------|
| FEXCore bring-up | ✅ static build for arm64-v8a; guest instructions execute and results are observed at runtime |
| Memory / syscalls / loader | 🟡 real implementations with self-tests; hardening ongoing |
| Vulkan device layer | 🟡 instance/device/submission loop proven in smoke tests |
| GNM → Vulkan graphics translation | ❌ not started |
| Audio · input · UI shell | 🟡 functional wrappers + Compose shell with real preferences |
| Driver switching | 🟡 slot manager present; on-device `vulkaninfo` verification next |

For agents working on this repo, **AGENT.md is the operating contract**:
architecture map, pinned-dependency rules, patch policy, testing ladder,
and the workflow law of the repository.

## Build

Requirements: JDK 17 · Gradle 8.9 · SDK API 35 · NDK 27.3.13750724 · CMake 3.22.1

```bash
./tools/fetch_fexcore.sh          # materializes pinned upstream FEX sources
export PX5_FEXCORE_ROOT="$PWD/../deps/FEX"
gradle assembleDebug --no-daemon
```

CI (`PX5 Build APK`) performs exactly this sequence on every push to
`main`, alongside `android-lint`, `cppcheck`, and `clang-tidy` analysis.

## Legal

This repository contains only original glue code and open-source engines.
It must not be used to distribute Sony firmware, proprietary keys, or
commercial game assets.
