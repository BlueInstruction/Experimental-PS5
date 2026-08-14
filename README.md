# Experimental-PS5

A native PlayStation 5 emulator for Android ARM64, powered by FEXCore
CPU translation and Vulkan GPU backend.

> **Status:** Experimental research. No commercial PS5 compatibility
> is claimed. This project exists to explore ARM64 emulation
> architecture, not to play commercial games.

## Architecture

```
PS5 x86-64 guest code
        |
        v
  FEXCore JIT (x86-64 → ARM64)
        |
        v
  Android ARM64 host (Bionic, ARM64 CPU)

PS5 GPU semantics (GNM/GNMX)
        |
        v
  GPU IR → SPIR-V → Vulkan 1.3 → Android GPU (Adreno/Turnip)

PS5 audio (ATRAC9, Tempest)
        |
        v
  Audio decoder → Android AAudio

PS5 input (DualSense)
        |
        v
  Input abstraction → Android touch / Bluetooth controller
```

The CPU and GPU paths are independent. FEXCore handles x86-64 → ARM64
instruction translation; the GPU path translates PS5 GNM command
streams to SPIR-V shaders consumed by Vulkan.

## Repository layout

```
Experimental-PS5/
├── app/
│   ├── src/main/java/com/px5/emulator/     # Kotlin UI (Compose)
│   ├── src/main/cpp/                         # C++ native core
│   │   ├── core/                             # Emulator core
│   │   ├── memory/                           # Memory manager
│   │   ├── kernel/                           # Kernel HLE / syscalls
│   │   ├── loader/                           # ELF/SELF loader
│   │   ├── gpu/                              # Vulkan device + shaders
│   │   ├── audio/                            # Audio backend
│   │   ├── input/                            # Controller input
│   │   ├── filesystem/                       # Virtual filesystem
│   │   ├── utils/                            # Logger + utilities
│   │   └── CMakeLists.txt                    # Native build config
│   ├── src/main/res/                         # Android resources
│   └── build.gradle.kts                      # App module config
├── third_party/fex/                          # Vendored FEXCore
├── .github/workflows/                        # CI/CD pipelines
├── build.gradle.kts                          # Root Gradle config
├── settings.gradle.kts
├── ROADMAP.md
└── README.md
```

## Build

### Prerequisites

- Android SDK (API 35)
- Android NDK 27.3.13750724
- CMake 3.22.1
- JDK 17
- Gradle 8.9

### Build the debug APK

```bash
gradle assembleDebug --no-daemon
```

Output: `app/build/outputs/apk/debug/app-debug.apk`

### Target

- **ABI:** arm64-v8a only (FEXCore requires 64-bit)
- **minSdk:** 28 (Android 9)
- **targetSdk:** 35 (Android 15)

## FEXCore integration

The vendored FEXCore source is at `third_party/fex/` (commit
`fd141ed6d721d03062619e4702bca1a0c93b6dd9`).

Android-specific patches:
- C++20 forced at parent CMake scope
- `std::atomic_ref` polyfill for NDK 27 libc++
- LTO disabled (NDK 27 lacks `LLVMgold.so`)
- `lld` linker forced (ARM64 support)
- `FEXCore_shared` + host tools excluded (broken on Android)
- `jemalloc_glibc` disabled (Bionic doesn't provide glibc allocator ABI)

## Validation: agent-evidence-engine

This project is the first consumer of
[agent-evidence-engine](https://github.com/SeaNaxxx/agent-evidence-engine),
a deterministic runtime evidence collection engine that prevents
"blind development" — the situation where an AI agent writes code,
sees `BUILD SUCCESSFUL`, and assumes the app works, while the user
sees a crash on their phone.

The evidence engine enforces this pipeline:

```
Source Code → Build → Install → Launch → Runtime Probes → Evidence → Policy
                                                                          |
                                                                          +-- PASS
                                                                          +-- FAIL
                                                                          +-- INCONCLUSIVE
                                                                                    |
                                                                                    v
                                                                          agent-report.json
```

The agent **interprets** evidence and suggests fixes.
The engine **decides** PASS / FAIL / INCONCLUSIVE.

See `ROADMAP.md` for the v1/v2/v3 integration plan.

## CI/CD

| Workflow | Purpose |
|----------|---------|
| `build.yml` | Build debug APK |
| `diagnostic.yml` | Verbose build log capture |
| `emulator-test.yml` | Android Emulator smoke test |
| `android-lint.yml` | Kotlin/Java static analysis |
| `cppcheck.yml` | C/C++ static analysis |
| `clang-tidy.yml` | C++ linting |
| `codeql.yml` | Security analysis |
| `codacy.yml` | Third-party security scan |
| `python-scripts.yml` | Python helper script linting |

## Legal boundary

This project does not distribute:
- Sony firmware or proprietary keys
- Commercial games or copyrighted assets
- Proprietary system binaries

Only legally obtained, user-owned material is supported.
