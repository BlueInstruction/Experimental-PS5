# PX5 Roadmap

> A native Android PlayStation 5 emulator focused on incremental implementation, runtime validation, and measurable compatibility progress.

## Core Development

| Phase | Component | Description | Status |
|:-----:|-----------|-------------|:------:|
| 0 | Foundation | Project structure, build system, CI foundation | ✅ |
| 1 | Android Framework | UI, application lifecycle, Vulkan surface | 🔄 |
| 2 | FEXCore | FEXCore integration and ARM64/AMD64 execution bridge | ⬜ |
| 3 | PS5 Loader | PS5 ELF/SELF loader and executable validation | ⬜ |
| 4 | Memory | Virtual memory manager and PS5 memory model | ⬜ |
| 5 | Kernel HLE | PS5 kernel HLE and syscall layer | ⬜ |
| 6 | System Libraries | PS5 system libraries and runtime services | ⬜ |
| 7 | GNM/GNMX | GNM/GNMX API translation layer | ⬜ |
| 8 | Shader Pipeline | PSSL shader pipeline and SPIR-V translation | ⬜ |
| 9 | Vulkan Backend | Vulkan GPU backend and Adreno/Turnip integration | ⬜ |
| 10 | Audio | Tempest Engine and audio subsystem | ⬜ |
| 11 | Input | Touch, controllers and DualSense support | ⬜ |
| 12 | Filesystem | Game data, saves, patches and virtual mounts | ⬜ |
| 13 | UI | Game library, settings, diagnostics and debug tools | ⬜ |
| 14 | Optimization | Profiling, performance tuning and runtime optimization | ⬜ |
| 15 | Compatibility | Game compatibility layer and per-game workarounds | ⬜ |
| 16 | Advanced | Save states, cheats and advanced debugging facilities | ⬜ |
| 17 | Release Candidate | Stability, compatibility and regression validation | ⬜ |
| 18 | Stable v1.0 | Production release | ⬜ |

## Engineering Infrastructure

The following systems operate alongside the core emulator phases rather than as sequential emulator phases.

| Component | Purpose | Status |
|-----------|---------|:------:|
| `agent-evidence-engine` | Runtime evidence collection and deterministic validation | 🔄 |
| Project Contract | Machine-readable PX5 build and runtime requirements | ⬜ |
| Build Probe | Validate Android and native build outputs | ⬜ |
| Native Loader Probe | Validate native libraries and runtime loading | ⬜ |
| Vulkan Probe | Validate Vulkan initialization and GPU visibility | ⬜ |
| Runtime Evidence | Capture structured runtime state from the Android device | ⬜ |
| Agent Report | Generate compact `agent-report.json` for AI agents | ⬜ |
| Regression History | Detect behavioral regressions between commits | ⬜ |
| PR Policy | Block critical runtime regressions | ⬜ |
| PS5 Probes | ELF, memory, FEX, syscall and GPU validation | ⬜ |

## Validation Principle

PX5 progress is considered complete only when the implementation is supported by runtime evidence.

```text
Source Code
    |
    v
Build
    |
    v
Install
    |
    v
Runtime
    |
    v
Probes
    |
    v
Evidence
    |
    v
Validation
    |
    +---- PASS
    +---- FAIL
    +---- INCONCLUSIVE
