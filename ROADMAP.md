# Experimental-PS5 Roadmap

> A native Android PlayStation 5 emulator focused on incremental
> implementation, runtime validation, and measurable compatibility
> progress.

## Core Development Phases

| Phase | Component | Description | Status |
|:-----:|-----------|-------------|:------:|
| 0 | Foundation | Project structure, build system, FEXCore integration | ✅ |
| 1 | Android Framework | Compose UI, application lifecycle, Vulkan surface | 🔄 |
| 2 | FEXCore | FEXCore ARM64/x86-64 execution bridge | 🔄 |
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

## Evidence Engine Integration

The [agent-evidence-engine](https://github.com/SeaNaxxx/agent-evidence-engine)
operates alongside the core emulator phases. It is not a sequential
emulator phase — it is the measurement and validation layer that
prevents blind development.

### v1 — Evidence Collection (current)

| Component | Purpose | Status |
|-----------|---------|:------:|
| Project Contract | Machine-readable PX5 build and runtime requirements | ⬜ |
| Build Probe | Validate Android + native build outputs (APK, ABI, .so files) | ⬜ |
| Native Loader Probe | Validate native libraries exist in APK AND load at runtime | ⬜ |
| Vulkan Dummy Probe | Validate Vulkan instance creation + physical device enumeration | ⬜ |
| Evidence Model | Structured runtime state captured from the Android device | ⬜ |
| Agent Report | Compact `agent-report.json` with PASS/FAIL/INCONCLUSIVE verdict | ⬜ |
| PR Policy | Block PRs with P0/P1 runtime failures | ⬜ |

### v2 — Regression History

| Component | Purpose | Status |
|-----------|---------|:------:|
| Evidence Database | SQLite store of probe results per commit | ⬜ |
| Fingerprinting | Content-addressed identification of probe outputs | ⬜ |
| Regression Detection | "Last known good: commit A → First known bad: commit B" | ⬜ |
| Probe Diff | "native-loader: PASS → FAIL" between commits | ⬜ |

### v3 — Governance

| Component | Purpose | Status |
|-----------|---------|:------:|
| Severity Policy | P0=block, P1=block, P2=warning, P3=informational | ⬜ |
| Request More Evidence | Engine can run additional probes on demand | ⬜ |
| Human Report | Compact text report for PR review | ⬜ |

### PS5-Specific Probes (future)

| Probe | Validates |
|-------|-----------|
| `elf_probe` | PS5 ELF/SELF format validity |
| `syscall_probe` | Syscall table coverage |
| `memory_probe` | Memory contract between PX5 and FEX |
| `fex_probe` | FEXCore JIT execution |
| `gpu_probe` | GNM → SPIR-V → Vulkan pipeline |
| `runtime_probe` | End-to-end guest code execution |

## Validation Principle

PX5 progress is considered complete only when the implementation is
supported by **runtime evidence**, not when `BUILD SUCCESSFUL` appears
in CI logs.

```
Source Code
    |
    v
Build  ──────► Build Probe ──► Evidence
    |                              |
    v                              v
Install ──────► Native Probe ──► Evidence
    |                              |
    v                              v
Launch  ──────► Vulkan Probe ──► Evidence
    |                              |
    v                              v
Runtime ──────► PS5 Probes ────► Evidence
                                   |
                                   v
                               Policy
                                   |
                                   +---- PASS
                                   +---- FAIL
                                   +---- INCONCLUSIVE
                                             |
                                             v
                                    agent-report.json
```

The AI agent **interprets** evidence and suggests fixes.
The engine **decides** the verdict.
