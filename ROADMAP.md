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

A separate evidence-engine repository was planned to operate alongside the
core emulator phases as a measurement and validation layer. It does not
exist yet: the link that used to be here (`BlueInstruction/agent-evidence-engine`)
returns 404, and every component in the table below is still ⬜, so nothing
in this section has been built.

What DOES exist and runs today is `tools/hosttests/run.sh` — host-side
regression tests with no toolchain dependency, wired into the build
workflow.

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

Verdicts are computed by the engine itself; evidence interpretation
and fix suggestions are downstream consumers of `agent-report.json`.

## Device Matrix & Handheld Strategy (2026-08)

The target is not phones alone. Android gaming handhelds are the primary
class for this emulator: they ship console-class SoCs, active cooling in
the flagship tier, and — critically — built-in physical controls that
Android already exposes through the standard GAMEPAD/JOYSTICK APIs.

| Device class | SoC examples | Notes for PX5 |
|---|---|---|
| Android handheld, flagship | Snapdragon 8 Gen 2/3 (AYN Odin 2 class), Snapdragon G3x Gen 2 | Primary target tier. Custom-driver path (adrenotools/Turnip) applies; physical pad pass-through is the default input surface; performance presets realistic. |
| Android handheld, upper-mid | Snapdragon 865-class (Retroid Pocket 5 class) | Same software path; resolution scale + Safe/Balanced presets recommended; thermal envelopes are smaller. |
| Android handheld, entry | Dimensity / Helio class | Vulkan 1.1+ baseline; custom-driver loading is Qualcomm/adrenotools-specific and reported as unavailable elsewhere; no speculative Mali/Xclipse claims. |
| Phones, flagship | Snapdragon 8 series | Touch overlay + BT pads; same engine path as handheld tier. |
| Future vendor diversity | MediaTek Mali, Exynos Xclipse | The GraphicsDriverManager contract (meta.json → libraryName → loader) is vendor-generic by design; a Mali/Xclipse backend is future work and will not be claimed to exist before it exists. |

Concrete capabilities this strategy already landed:

* `PhysicalControllerBridge` — hardware gamepad pass-through into the same
  native input atomics the touch overlay uses (handhelds need zero setup).
* Per-slot driver sonames from `meta.json` (`libraryName` is the authority;
  file naming in the wild is not stable). `minApi` is honored at import.
* FEXCore presets applied through FEXCore's real layered config, separated
  into configuration (presets), runtime (native apply), and diagnostics
  (live counters) — no UI switch claims an effect it cannot verify.
* Explicit swapchain present-mode selection, validated against the device
  before use, with loud fallback.
* Fork-isolated self-tests: a JIT fault produces a crash dump and a full
  report instead of killing the app.

Next planned steps for the device matrix (order reflects dependency, not
preference): real-device validation of the driver harness on an Odin 2-class
device; per-device default profiles (thermal/RAM aware); per-game setting
overrides; sustained-performance/thermal-headroom adaptation.
