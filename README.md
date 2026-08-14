# Experimental-PS5

A native PlayStation 5 emulator for Android ARM64, powered by FEXCore
CPU translation and Vulkan GPU backend.

> Status: experimental research. No commercial PS5 compatibility is
> claimed.

## Architecture

PSX5 is a compatibility and translation stack, not a PS5 binary wrapper.

``` text
PS5 x86-64
    |
    v
decoder -> IR -> optimizer -> ARM64 JIT -> Android ARM64

PS5 GPU semantics
    |
    v
GPU model -> GPU IR -> shader translation -> SPIR-V -> Vulkan -> Android GPU

PS5 audio
    |
    v
ATRAC9 / game-audio decoding -> mixer -> Android audio

PS5 input
    |
    v
input abstraction -> Android / DualSense / SDL
```

The CPU and GPU paths are independent. The project must not assume that
PS5 graphics are D3D12.

## Critical correction: VKD3D-Proton

VKD3D-Proton is NOT the PSX5 GPU backend.

It is a Direct3D 12 to Vulkan translation project and can be used as
research material for:

-   Vulkan resource management
-   descriptors
-   barriers
-   synchronization
-   pipeline state
-   shader translation
-   Vulkan capability handling

PSX5 must instead implement:

``` text
PS5 GPU semantics
        |
        v
PSX5 GPU IR
        |
        v
host Vulkan backend
```

VKD3D-Proton is therefore a research reference, not a core PSX5
dependency.

## Core milestones

``` text
M0  Android/toolchain bootstrap
M1  Native runtime/JNI
M2  Guest memory model
M3  x86-64 decoder
M4  IR
M5  ARM64 JIT
M6  Scheduler/threading
M7  PS5 ABI/platform compatibility
M8  Executable loader
M9  PS5 GPU abstraction
M10 Vulkan backend
M11 Shader translation
M12 Audio
M13 Input/DualSense
M14 System bring-up
M15 Application execution
M16 Compatibility
M17 Optimization
```

## Repository layout

``` text
Experimental-PS5/
├── app/
│   ├── src/main/java/com/px5/emulator/     # Kotlin UI (Jetpack Compose)
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
│   ├── src/main/res/                         # Android resources (fonts, drawables)
│   └── build.gradle.kts                      # App module config
├── third_party/fex/                          # Vendored FEXCore source
├── agent-evidence-engine/                    # Runtime evidence collection engine
├── .github/workflows/                        # CI/CD pipelines
├── build.gradle.kts                          # Root Gradle config
├── settings.gradle.kts
├── ROADMAP.md
└── README.md
```

## Android target

Primary target:

``` text
Android
arm64-v8a
AArch64
Android NDK
Clang/LLVM
C/C++
CMake
Ninja
Gradle
Bionic
Vulkan
```

The emulator core must remain portable. Adreno-specific behavior belongs
behind explicit backend/quirk interfaces.

## CPU/JIT

The PS5 CPU is x86-64/Zen 2 based. Android ARM64 requires dynamic binary
translation.

``` text
x86-64 -> decoder -> IR -> optimization -> ARM64 -> executable code cache
```

Initial CPU state should cover GPRs, RIP, RSP, RFLAGS, SIMD state,
MXCSR, relevant segment state, atomics, and exceptions.

FEX is the main CPU/JIT reference:

https://github.com/FEX-Emu/FEX

FEX primarily targets ARM64 Linux, so Android/Bionic integration must be
proven before making it a dependency.

VIXL is an alternative/reference AArch64 code-generation library:

https://github.com/Linaro/vixl

JIT evidence must distinguish:

``` text
decoded
IR generated
ARM64 generated
cached
linked
reachable
executed
```

Source names, strings, symbols, or static archives do not prove
execution.

## Guest memory

Use an explicit guest address type:

``` cpp
using GuestVA = uint64_t;
```

Implement:

-   guest virtual address space
-   page permissions
-   mappings
-   guard pages
-   executable mappings
-   code cache
-   guest-to-host translation
-   invalidation
-   memory faults

Guest virtual addresses must not be treated as Android pointers.

## PS5 platform compatibility

Use a compatibility layer:

``` text
PS5 guest API/syscall
        |
        v
PSX5 compatibility layer
        |
        +-- process
        +-- thread
        +-- virtual memory
        +-- synchronization
        +-- filesystem
        +-- timing
        +-- IPC
        +-- networking
        |
        v
Android/Bionic
```

Do not translate PS5 behavior directly into arbitrary Linux syscalls
without modeling the required semantics.

## GPU

The PS5 GPU must be represented by a PSX5-specific GPU abstraction.

Model as required:

-   GPU virtual memory
-   buffers
-   images
-   render targets
-   depth/stencil
-   samplers
-   descriptors
-   pipelines
-   command buffers
-   queues
-   fences
-   semaphores
-   barriers
-   coherency

Do not assume one PS5 GPU command equals one Vulkan command.

## Vulkan

Primary host graphics API:

https://github.com/KhronosGroup/Vulkan-Headers

https://github.com/KhronosGroup/Vulkan-Loader

https://github.com/KhronosGroup/Vulkan-Docs

https://github.com/KhronosGroup/Vulkan-Hpp

https://github.com/KhronosGroup/Vulkan-Guide

https://github.com/KhronosGroup/Vulkan-Samples

SPIR-V:

https://github.com/KhronosGroup/SPIRV-Headers

https://github.com/KhronosGroup/SPIRV-Tools

https://github.com/KhronosGroup/SPIRV-Registry

https://github.com/KhronosGroup/SPIRV-Cross

https://github.com/KhronosGroup/SPIRV-LLVM-Translator

https://github.com/KhronosGroup/glslang

https://github.com/microsoft/DirectXShaderCompiler

DXC is optional shader infrastructure; PS5 shaders must not be assumed
to be HLSL source.

Vulkan memory allocation reference:

https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator

VMA manages host Vulkan memory; it does not replace the guest PS5 memory
manager.

Validation/debugging:

https://github.com/KhronosGroup/VK-GL-CTS

https://github.com/KhronosGroup/Vulkan-ExtensionLayer

https://github.com/baldurk/renderdoc

https://github.com/ValveSoftware/Fossilize

https://github.com/ARM-software/perfdoc

## Adreno and Android graphics

libadrenotools:

https://github.com/bylaws/libadrenotools

Forks/related implementations:

https://github.com/eden-emulator/libadrenotools

https://github.com/Pipetto-crypto/libadrenotools

https://github.com/TouseefX/libadrenotools-native

https://github.com/xodiosx/libadrenotools

Android linker namespace research:

https://github.com/bylaws/liblinkernsbypass

https://github.com/Pipetto-crypto/liblinkernsbypass

Important distinction:

``` text
libadrenotools = Android/Adreno driver loading/replacement support
Turnip         = Mesa Vulkan driver
PSX5 GPU       = PS5 GPU model + translation
```

libadrenotools is not Turnip and neither is a PS5 GPU emulator.

Android Mesa container reference:

https://github.com/lfdevs/mesa-for-android-container

Qualcomm Vulkan/OpenGL examples:

https://github.com/SnapdragonGameStudios/adreno-gpu-vulkan-code-sample-framework

https://github.com/SnapdragonGameStudios/adreno-gpu-opengl-es-code-sample-framework

Qualcomm Windows driver research:

https://github.com/WOA-Project/Qualcomm-Reference-Drivers

Winlator graphics references:

https://github.com/brunodev85/vortek

https://github.com/brunodev85/gladio

These are references, not PSX5 GPU implementations.

## Upscaling and post-processing

Do not label generic upscalers as Sony PSSR implementations.

Preferred optional host-side technologies:

https://github.com/SnapdragonGameStudios/snapdragon-gsr

https://github.com/GPUOpen-Effects/FidelityFX-FSR

https://github.com/DadSchoorse/vkBasalt

https://github.com/crosire/reshade

Snapdragon GSR 1 is spatial upscaling/sharpening and GSR 2 is temporal
upscaling for Adreno. These are presentation technologies, not PS5 GPU
emulation.

Pipeline:

``` text
PSX5 Vulkan output -> optional upscaler/post-process -> Android surface
```

## Audio

ATRAC9 and game-audio references:

https://github.com/Thealexbarney/LibAtrac9

https://github.com/Thealexbarney/VGAudio

General media/audio:

https://github.com/FFmpeg/FFmpeg

Audio should be independent:

``` text
PS5 audio data -> decoder -> PCM/mixer -> Android audio backend
```

## Input

SDL:

https://github.com/libsdl-org/SDL

DualSense Linux research:

https://github.com/nowrep/dualsensectl

Use a platform-neutral input abstraction. DualSense-specific behavior
remains a guest compatibility concern.

## PS5 emulator references

SharpEmu:

https://github.com/sharpemu/sharpemu

SharpEmu is an experimental PS5 emulator and is a strong PS5-specific
architecture reference. Its current targets are Windows/Linux/macOS, so
it is not an Android dependency.

KytyPS5:

https://github.com/KytyPS5/KytyPS5

PS5-specific research and emulator architecture reference.

shadPS4:

https://github.com/shadps4-emu/shadPS4

PS4 emulator reference for Vulkan, shader compilation, compatibility
testing, debugging, and console architecture. It is not a PS5 emulator.

RPCS3:

https://github.com/RPCS3/rpcs3

Useful for CPU translation, memory, kernel compatibility, scheduling,
GPU abstraction, debugging, and testing.

ARMSX3:

https://github.com/ARMSX2/ARMSX3

Useful for Android ARM64 emulator porting and packaging research.

Bachata-S4:

https://github.com/JICA98/Bachata-S4

Android PS4-emulator reference.

## PS5 Linux research

https://github.com/ps5-linux/ps5-linux-tools

https://github.com/ps5-linux/ps5-linux-image

https://github.com/ps5-linux/ps5-linux-patches

https://github.com/ps5-linux/ps5-linux-loader

These projects are useful for real PS5 hardware/Linux behavior and
low-level research. They are not PSX5 user-space emulator dependencies.

## Kernel/low-level references

Linux:

https://github.com/torvalds/linux

Qualcomm mainline:

https://github.com/linux-msm/mainline-status

ARM Trusted Firmware:

https://github.com/ARM-software/arm-trusted-firmware

Historical Samsung Android kernel:

https://github.com/coolya/android_kernel_samsung

The Samsung kernel is device-specific and historical; it is only a
reference.

## Graphics architecture references

AMD PAL:

https://github.com/GPUOpen-Drivers/pal

The Forge:

https://github.com/ConfettiFX/The-Forge

GLM:

https://github.com/g-truc/glm

GLI:

https://github.com/g-truc/gli

OpenGL samples:

https://github.com/g-truc/ogl-samples

ANARI:

https://github.com/KhronosGroup/ANARI-SDK

UnityGLTF:

https://github.com/KhronosGroup/UnityGLTF

Sascha Willems Vulkan examples:

https://github.com/SaschaWillems/Vulkan

ARM Vulkan SDK:

https://github.com/ARM-software/vulkan-sdk

MoltenVK:

https://github.com/KhronosGroup/MoltenVK

MoltenVK is an Apple Metal portability layer and is not part of the
Android backend.

OpenCL headers:

https://github.com/KhronosGroup/OpenCL-Headers

These are optional/general graphics references and should not
automatically enter third_party/.

## Console/general research

libnx:

https://github.com/switchbrew/libnx

libnx is a Nintendo Switch homebrew library. It is not a PS5 component
and should only be used as a general console API/reference.

Game Console Dev Guide:

https://github.com/mikeroyal/Game-Console-Dev-Guide

General console development reference.

## Android framework and tooling

https://github.com/LineageOS/android_frameworks_native

Android native framework reference.

## Security research

mast1c0re:

https://github.com/McCaulay/mast1c0re

Security/exploit research reference only. It is not required for normal
PSX5 emulation.

Magisk:

https://github.com/topjohnwu/Magisk

Optional device-development/root tooling. Root must not be a PSX5
requirement.

PS5ish:

https://github.com/davidkgriggs/PS5ish

PS5 UI/theme reference only.

## Optional virtualization

Android Virtualization Framework:

https://android.googlesource.com/platform/packages/modules/Virtualization/

AVF getting started:

https://android.googlesource.com/platform/packages/modules/Virtualization/+/HEAD/docs/getting_started.md

crosvm:

https://github.com/google/crosvm

Potential architecture:

``` text
Android -> AVF -> crosvm -> Gunyah -> guest
```

Virtualization does not solve PS5 GPU emulation. The PSX5 core must
remain independent of AVF, crosvm, and Gunyah.

## Dependencies that must NOT be misclassified

These are not PSX5 core GPU/CPU layers:

``` text
VKD3D-Proton
MoltenVK
libnx
Samsung Android kernels
Linux kernel
ARM Trusted Firmware
GPUOpen PAL
ANARI
UnityGLTF
ReShade
vkBasalt
Magisk
PS5 Linux loader
mast1c0re
Vortek
Gladio
Qualcomm Windows reference drivers
```

They belong to research, tooling, optional platform integration,
security research, or presentation categories.

## Dependency policy

Before adding any repository to `third_party/`, answer:

``` text
1. Which PSX5 subsystem needs it?
2. Is it runtime code or reference material?
3. Is the license compatible?
4. Does it support Android ARM64?
5. Does it support Bionic?
6. Does it require Linux-only facilities?
7. Does it require x86 host execution?
8. Does it require a different graphics API?
9. Can it be isolated behind an interface?
10. Is it demonstrably exercised at runtime?
```

## Testing hierarchy

``` text
1.  unit tests
2.  x86-64 decoder tests
3.  IR tests
4.  JIT tests
5.  memory tests
6.  scheduler tests
7.  platform compatibility tests
8.  executable-loader tests
9.  Vulkan tests
10. shader tests
11. GPU translation tests
12. audio tests
13. input tests
14. system bring-up
15. application execution
16. compatibility tests
```

## Runtime evidence

Use explicit evidence states:

``` text
BUILD_SUCCESS
BINARY_VALID
NATIVE_LOAD_SUCCESS
VULKAN_INITIALIZED
CPU_RUNTIME_ACTIVE
JIT_ACTIVE
GPU_RUNTIME_ACTIVE
AUDIO_RUNTIME_ACTIVE
INPUT_RUNTIME_ACTIVE
APPLICATION_REACHED
APPLICATION_EXECUTED
```

Evidence levels:

``` text
DIRECTLY PROVEN
STRONGLY INDICATED
NOT PROVEN
```

Compilation, symbol presence, static linkage, APK installation, or
strings output do not prove runtime execution.

## Binary inspection

``` bash
file libpsx5.so
readelf -h libpsx5.so
readelf -S libpsx5.so
readelf -d libpsx5.so
readelf -Ws libpsx5.so
readelf -r libpsx5.so
objdump -d libpsx5.so
```

For static libraries:

``` bash
nm -C libFEXCore.a
```

Verify ABI, ELF type, dependencies, undefined symbols, relocations,
RELRO/BIND_NOW, build ID, and actual code inclusion.

## Reproducible builds

Record:

``` text
Git commit
dependency revisions
NDK
CMake
Clang
Gradle
Android API
compiler flags
linker flags
```

Recommended artifacts:

``` text
build/
├── build.log
├── compile_commands.json
├── link-map.txt
├── compiler.txt
├── linker.txt
├── dependencies.txt
├── elf-header.txt
├── elf-sections.txt
├── elf-dynamic.txt
├── elf-symbols.txt
└── elf-relocations.txt
```

## CI

Build:

``` text
Debug arm64-v8a
Release arm64-v8a
```

Run:

``` text
unit tests
decoder tests
IR tests
JIT tests
shader tests
Vulkan initialization
ELF inspection
APK validation
native-load tests
```

Fail CI if:

``` text
libpsx5.so is missing
wrong ABI is produced
unexpected shared dependency appears
required symbols are unresolved
required assets are missing
native loading fails
```

## FEXCore integration

The vendored FEXCore source is at `third_party/fex/` (commit
`fd141ed6d721d03062619e4702bca1a0c93b6dd9`).

Android-specific patches applied to the build:

- C++20 forced at parent CMake scope (FEX only sets it at subdirectory level)
- `std::atomic_ref` polyfill for NDK 27 libc++ (Bionic doesn't export it)
- LTO disabled (NDK 27 lacks `LLVMgold.so` plugin)
- `lld` linker forced (gold doesn't fully support ARM64 Android)
- `FEXCore_shared` + host tools excluded (broken on Android, only need static lib)
- `jemalloc_glibc` disabled (Bionic doesn't provide glibc allocator ABI)

FEX's own `.github/workflows/` have been removed to prevent FEX's CI
(ccpp.yml, mingw_build.yml, wine_build, etc.) from running in our repo.

## Validation: agent-evidence-engine

This project includes `agent-evidence-engine/` — a deterministic runtime
evidence collection engine that prevents "blind development": the situation
where an AI agent writes code, sees `BUILD SUCCESSFUL`, and assumes the
app works, while the user sees a crash on their phone.

The evidence engine enforces this pipeline:

``` text
Source Code -> Build -> Install -> Launch -> Runtime Probes -> Evidence -> Policy
                                                                          |
                                                                          +-- PASS
                                                                          +-- FAIL
                                                                          +-- INCONCLUSIVE
                                                                                    |
                                                                                    v
                                                                          agent-report.json
```

The AI agent **interprets** evidence and suggests fixes.
The engine **decides** PASS / FAIL / INCONCLUSIVE.

See `agent-evidence-engine/README.md` and `ROADMAP.md` for the v1/v2/v3
integration plan.

## CI/CD workflows

| Workflow | Purpose |
|----------|---------|
| `build.yml` | Build debug APK (arm64-v8a) |
| `diagnostic.yml` | Verbose build log capture + commit to repo |
| `emulator-test.yml` | Android Emulator smoke test (install + launch + logcat) |
| `android-lint.yml` | Kotlin/Java static analysis (Google Lint) |
| `cppcheck.yml` | C/C++ static analysis |
| `clang-tidy.yml` | C++ linting (modernize, bugprone, performance) |
| `codeql.yml` | Security analysis (C++, Kotlin, Python, Actions) |
| `codacy.yml` | Third-party security scan (SARIF) |
| `python-scripts.yml` | Python helper script linting (flake8 + mypy + bandit) |


## Legal boundary

Do not distribute:

-   Sony firmware
-   proprietary keys
-   commercial games
-   copyrighted game assets
-   proprietary system binaries

Support legally obtained user-owned material where required.

## Final engineering chain

``` text
source
  -> compile
  -> link
  -> final ELF
  -> binary verification
  -> Android native initialization
  -> x86-64 decode
  -> IR
  -> ARM64 JIT
  -> PS5 platform compatibility
  -> PS5 GPU model
  -> GPU IR
  -> SPIR-V
  -> Vulkan
  -> Android GPU
  -> audio/input
  -> application execution
  -> compatibility testing
```

Every transition is an independent milestone and must be independently
verified.

The objective is not merely to produce an APK. The objective is a
reproducible, observable, testable, progressively verifiable PS5
compatibility stack for Android ARM64.

