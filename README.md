# PSX5 Android ARM64

Experimental research project for a PlayStation 5 emulator targeting Android ARM64.

The long-term objective is to build a native Android application capable of executing legally obtained PlayStation 5 software through a compatibility and translation stack rather than relying on direct execution of PS5 binaries on Android.

> Status: Experimental research. PS5 commercial game compatibility is not currently claimed.

## Architecture

```text
                    PSX5 Android APK
                           |
              +------------+------------+
              |                         |
          Android UI                Native Core
                                        |
                              +---------+---------+
                              |                   |
                           CPU Core            GPU Core
                              |                   |
                         x86-64 Guest         PS5 GPU Model
                              |                   |
                              v                   v
                           FEX/IR             GPU IR
                              |                   |
                              v                   v
                         ARM64 JIT             SPIR-V
                              |                   |
                              +---------+---------+
                                        |
                                      Vulkan
                                        |
                                        v
                                  Android GPU
```

The fundamental execution path is:

```text
PS5 x86-64 code
      |
      v
x86-64 decoding
      |
      v
IR
      |
      v
optimization
      |
      v
ARM64 JIT
      |
      v
Android ARM64 execution
```

The graphics path is independent:

```text
PS5 GPU model
      |
      v
GPU command translation
      |
      v
GPU IR
      |
      v
shader translation
      |
      v
SPIR-V
      |
      v
Vulkan
      |
      v
Android GPU
```

## Core principles

PSX5 treats every layer as an independently verifiable subsystem.

The project distinguishes:

1. Source compilation.
2. Static and dynamic linking.
3. Final ELF contents.
4. Code reachability.
5. Runtime initialization.
6. Runtime execution.
7. Application compatibility.

A successful build does not prove runtime correctness.

Likewise, the presence of strings, symbols, object files, or static archives does not prove that executable code was incorporated into the final binary or that the code was actually executed.

Evidence should therefore be classified as:

```text
DIRECTLY PROVEN
STRONGLY INDICATED
NOT PROVEN
```

## Target platform

Primary target:

```text
Android ARM64
arm64-v8a
ARMv8-A or newer
Android NDK
Clang/LLVM
CMake
Ninja
Gradle
Vulkan
C/C++
```

Initial hardware validation should focus on modern Qualcomm Snapdragon and Adreno devices while keeping the architecture portable.

The core emulator must not depend on a single Snapdragon model or vendor-specific implementation.

## Repository structure

```text
PSX5/
├── app/
│   └── src/main/
│       ├── java/
│       ├── cpp/
│       │   ├── psx5/
│       │   ├── cpu/
│       │   ├── kernel/
│       │   ├── memory/
│       │   ├── scheduler/
│       │   ├── gpu/
│       │   ├── vulkan/
│       │   ├── android/
│       │   ├── debugger/
│       │   └── tests/
│       └── assets/
├── core/
│   ├── cpu/
│   ├── kernel/
│   ├── memory/
│   ├── scheduler/
│   └── gpu/
├── third_party/
│   ├── fex/
│   ├── vulkan/
│   ├── spirv/
│   ├── fmt/
│   ├── xxhash/
│   └── softfloat/
├── tools/
├── tests/
├── docs/
├── CMakeLists.txt
├── settings.gradle
├── build.gradle
├── ROADMAP.md
└── README.md
```

## Development roadmap

The detailed project roadmap is maintained separately in [`ROADMAP.md`](ROADMAP.md).

The major development sequence is:

```text
M0  Build bootstrap
M1  Android native runtime
M2  x86-64 CPU
M3  ARM64 JIT
M4  Kernel/platform compatibility
M5  GPU abstraction
M6  Vulkan backend
M7  Shader translation
M8  System bring-up
M9  Application execution
M10 Compatibility
M11 Optimization
```

The project should not skip directly from APK build success to game testing.

## Phase 0: Android build bootstrap

The first milestone is intentionally small:

```text
Android APK
    |
    +-- lib/arm64-v8a/libpsx5.so
```

No PS5 emulation is required at this stage.

Build:

```bash
./gradlew assembleDebug
```

Inspect the APK:

```bash
unzip -l app/build/outputs/apk/debug/*.apk
```

The native library must exist at:

```text
lib/arm64-v8a/libpsx5.so
```

Inspect the native artifact:

```bash
file libpsx5.so
readelf -h libpsx5.so
readelf -d libpsx5.so
readelf -Ws libpsx5.so
```

## Phase 1: Android native runtime

Implement:

- JNI
- native logging
- Android lifecycle
- native worker threads
- memory allocation
- mmap/mprotect
- timers
- file I/O
- controller/input abstraction
- Vulkan initialization

Initial runtime target:

```text
APK
 |
 v
JNI
 |
 v
libpsx5.so
 |
 v
worker thread
 |
 v
Vulkan
 |
 v
Android surface
```

## Phase 2: Guest memory

Create an explicit guest virtual-address abstraction.

```cpp
using GuestVA = uint64_t;
```

Required concepts:

- Guest virtual address space.
- Page permissions.
- Mappings.
- Guard pages.
- Executable mappings.
- Shared mappings.
- Code-cache mappings.
- Guest-to-host address translation.

A guest address must never be assumed to be equivalent to an Android host pointer.

## Phase 3: x86-64 CPU emulation

The PlayStation 5 uses an AMD Zen 2 x86-64 CPU architecture while Android ARM64 devices use AArch64.

The practical architecture is dynamic binary translation:

```text
x86-64 instructions
        |
        v
decoder
        |
        v
IR
        |
        v
optimization
        |
        v
register allocation
        |
        v
ARM64 code generation
        |
        v
executable code cache
```

Initial CPU state includes:

- GPRs
- RIP
- RSP
- RFLAGS
- XMM/YMM state
- MXCSR
- relevant segment state
- atomic and memory-ordering state
- exception state

Initial instruction coverage should prioritize:

```text
MOV
ADD
SUB
CMP
TEST
LEA
JMP
CALL
RET
conditional branches
PUSH
POP
```

SIMD and additional ISA requirements should be expanded from executable tests rather than assumptions.

## FEX integration

FEX is an important reference and potential technology source for x86/x86-64 to ARM64 translation.

Repository:

https://github.com/FEX-Emu/FEX

FEX provides concepts and components related to:

- x86/x86-64 frontend
- IR
- optimization
- JIT
- ARM64 code generation
- code caching
- syscall handling
- signal handling
- memory management

FEX primarily targets ARM64 Linux. Android uses Bionic and has different platform and security behavior.

Therefore PSX5 should separate:

```text
FEX CPU/JIT technology
```

from:

```text
Android runtime
PS5 ABI
PS5 syscalls
PS5 platform semantics
```

If FEXCore or another static archive is linked into PSX5, final ELF inspection must verify what actually entered the executable.

Useful inspection commands:

```bash
readelf -Ws libpsx5.so
readelf -S libpsx5.so
readelf -r libpsx5.so
objdump -d libpsx5.so
nm -C libFEXCore.a
```

A linker map should be generated where practical.

## Phase 4: JIT

Required components:

- Translation blocks.
- Guest RIP lookup.
- x86-64 decoding.
- IR generation.
- Optimization.
- ARM64 lowering.
- Register allocation.
- Code cache.
- Block linking.
- Invalidation.
- Self-modifying-code detection.

Suggested representation:

```text
TranslationBlock
├── guest address
├── guest size
├── host address
├── host size
├── generation
└── dependencies
```

JIT verification must distinguish:

```text
compiled
linked
present in final ELF
reachable
executed
```

`strings` output is not proof of JIT execution.

## Phase 5: Scheduler and threads

Implement:

- guest process
- guest thread
- CPU context
- scheduler
- TLS
- mutexes
- events
- semaphores
- condition variables
- atomics
- synchronization primitives

Initial implementation can use Android host threads.

Later optimization can introduce:

- worker pools
- CPU affinity
- JIT workers
- GPU workers
- I/O workers

## Phase 6: PS5 kernel/platform compatibility

The emulator requires a PS5-oriented compatibility layer.

```text
Guest syscall/API
        |
        v
PS5 compatibility layer
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
Android implementation
```

PS5 system behavior must not be assumed to be equivalent to ordinary Linux syscalls.

Implement behavior from documented and reproducible research and legally usable test material.

## Phase 7: Executable loading

Implement a controlled loader for the supported guest executable format.

Required concepts:

- headers
- segments
- permissions
- relocations
- symbol resolution where required
- dynamic dependencies
- TLS
- entry point
- memory mappings

Guest ABI handling must remain separate from Android's ELF loader.

## Phase 8: PS5 GPU abstraction

The PS5 GPU is based on AMD RDNA-family technology, while Android devices may use Adreno or other GPU architectures.

PSX5 should not attempt direct execution of PS5 GPU instructions on an Android GPU.

Instead:

```text
PS5 GPU abstraction
        |
        v
GPU command translation
        |
        v
GPU IR
        |
        v
Vulkan backend
```

The GPU abstraction should independently model:

- GPU virtual memory
- resources
- buffers
- images
- render targets
- depth/stencil
- samplers
- descriptors
- pipelines
- command buffers
- queues
- fences
- semaphores
- events
- barriers

## Phase 9: Vulkan backend

Vulkan is the initial host graphics backend.

References:

- https://www.vulkan.org/
- https://github.com/KhronosGroup/Vulkan-Hpp
- https://github.com/KhronosGroup/glslang

The backend should cover:

- instance
- physical device
- logical device
- queues
- command pools
- command buffers
- descriptor layouts
- descriptor pools
- buffers
- images
- memory allocation
- samplers
- fences
- semaphores
- timeline synchronization
- shader modules
- pipeline cache

Actual device features and extensions must be queried at runtime rather than assumed.

## Phase 10: Qualcomm Adreno and Turnip

Qualcomm Android devices are an important initial hardware target.

Turnip and related tooling are useful research references.

Repository:

https://github.com/bylaws/libadrenotools

The emulator should detect:

- vendor ID
- device ID
- GPU generation
- Vulkan version
- extensions
- features
- memory heaps
- queue families
- descriptor capabilities
- shader capabilities

Vendor-specific workarounds should remain isolated:

```text
gpu/
└── quirks/
    ├── adreno.cpp
    └── turnip.cpp
```

The core emulator must not hard-code one Snapdragon device.

## Phase 11: Shader translation

The graphics path requires a shader translation pipeline:

```text
PS5 shader
    |
    v
decoder
    |
    v
shader IR
    |
    v
optimizer
    |
    v
SPIR-V
    |
    v
Vulkan
```

Unit tests should cover:

- arithmetic
- branches
- texture operations
- samplers
- derivatives
- barriers
- atomics
- subgroup operations
- FP16
- FP32
- integer operations

## Phase 12: GPU synchronization

Model:

```text
PS5 queue
    |
    v
PSX5 synchronization
    |
    +-- fence
    +-- semaphore
    +-- event
    +-- barrier
    +-- ownership
    |
    v
Vulkan synchronization
```

Avoid global GPU waits as a compatibility shortcut. Excessive global synchronization can hide correctness problems and destroy performance.

## Phase 13: Memory and GPU budget

Track independently:

```text
guest RAM
GPU allocations
JIT cache
shader cache
pipeline cache
staging buffers
textures
render targets
system allocations
```

Expose diagnostic counters.

Memory values should not be spoofed unless the behavior is understood and isolated behind an explicit compatibility layer.

## Phase 14: VKD3D-Proton research

VKD3D-Proton is useful as a reference for modern Vulkan translation architecture.

Repository:

https://github.com/HansKristian-Work/vkd3d-proton

Relevant areas include:

- descriptor management
- D3D12/Vulkan abstraction
- resource barriers
- pipeline state
- shader translation
- SPIR-V
- synchronization
- Vulkan capability detection

VKD3D-Proton is not a PS5 graphics implementation.

Previous project research:

https://github.com/SeaNaxxx/VKD3D-Proton-QSA-Mesa-Turnip-QSA

Earlier research included:

- Adreno quirks
- UE5 compatibility
- Wave32
- FP16
- metadata pools
- UMA memory fallback
- KGSL-related behavior
- memory budget behavior
- Turnip interaction

These techniques must be independently verified before reuse.

## Phase 15: Android emulator references

aPS3e is not a PS5 emulator, but it is useful for Android emulator engineering.

Repository:

https://github.com/aenu1/aps3e

Relevant areas:

- Android packaging
- Gradle
- C++
- native libraries
- emulator configuration
- input
- graphics integration
- Android build workflow

PS3-specific emulation assumptions should not be copied into PSX5.

## Phase 16: Additional emulator references

RPCS3:

https://github.com/RPCS3/rpcs3

Other useful architectural references include PCSX2, Dolphin, Ryujinx, and Xenia.

These projects can inform:

- CPU emulation
- JIT
- IR
- memory management
- scheduling
- GPU abstraction
- shader systems
- debugging
- testing

They are references, not PS5 implementation sources.

## Phase 17: Virtualization research

Virtualization is an optional research path.

Android Virtualization Framework:

https://android.googlesource.com/platform/packages/modules/Virtualization/

AVF documentation:

https://android.googlesource.com/platform/packages/modules/Virtualization/+/HEAD/docs/getting_started.md

crosvm:

https://github.com/google/crosvm

Potential architecture:

```text
Android
   |
   v
AVF
   |
   v
crosvm
   |
   v
Gunyah
   |
   v
guest environment
```

Virtualization does not automatically solve PS5 GPU emulation. GPU virtualization or passthrough is a separate problem.

The core emulator must remain independent of AVF, crosvm, and Gunyah.

## Testing hierarchy

Testing should progress in this order:

```text
1.  unit tests
2.  x86-64 decoder tests
3.  IR tests
4.  JIT tests
5.  memory tests
6.  scheduler tests
7.  syscall tests
8.  Vulkan tests
9.  shader tests
10. GPU translation tests
11. executable-loader tests
12. system bring-up
13. application compatibility
```

Do not treat application launch as proof that the lower layers are correct.

## Performance profiling

Measure:

- guest instructions per second
- translation time
- JIT compilation time
- JIT cache hit rate
- code-cache size
- GPU translation time
- shader compilation time
- pipeline creation time
- Vulkan CPU overhead
- GPU frame time
- synchronization stalls
- memory bandwidth
- allocation counts

Optimization should follow measured bottlenecks.

## Native binary forensics

Every release candidate should be inspected.

```bash
file libpsx5.so
readelf -h libpsx5.so
readelf -S libpsx5.so
readelf -d libpsx5.so
readelf -Ws libpsx5.so
readelf -r libpsx5.so
objdump -d libpsx5.so
```

Verify:

- AArch64
- ELF64
- Android-compatible dependencies
- unexpected DT_NEEDED entries
- undefined symbols
- SONAME
- RELRO
- BIND_NOW
- build ID
- stripped/unstripped state

Forensic conclusions must distinguish between:

```text
DIRECTLY PROVEN
STRONGLY INDICATED
NOT PROVEN
```

For example, the presence of `JIT.cpp` in `strings` output does not prove that JIT code exists in the final executable or that the JIT was executed.

## Reproducible builds

Record:

```text
Git commit
FEX revision
Mesa/Turnip revision
VKD3D revision
Vulkan headers revision
NDK version
CMake version
Clang version
Gradle version
Android API level
compiler flags
linker flags
```

Recommended evidence artifacts:

```text
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

## CI and runtime evidence

CI should build:

```text
Debug arm64-v8a
Release arm64-v8a
```

And run:

- unit tests
- decoder tests
- IR tests
- JIT tests
- shader tests
- Vulkan initialization
- ELF inspection
- APK structure validation
- native loading checks

CI should fail if:

- `libpsx5.so` is missing
- the wrong ABI is produced
- an unexpected shared dependency appears
- required native symbols are unresolved
- required assets are missing
- the native library cannot load

### Agent evidence

PSX5 development should use runtime evidence rather than build-only claims.

The project is intended to integrate with an external evidence engine that can collect:

```text
source
  |
  v
build
  |
  v
final APK
  |
  v
native ELF
  |
  v
Android runtime
  |
  v
logcat / probes
  |
  v
structured evidence
  |
  v
agent report
```

A diagnostic system should identify the failing subsystem rather than merely matching arbitrary log strings.

The evidence model should distinguish:

```text
BUILD_SUCCESS
BINARY_VALID
NATIVE_LOAD_SUCCESS
VULKAN_INITIALIZED
CPU_RUNTIME_ACTIVE
GPU_RUNTIME_ACTIVE
APPLICATION_REACHED
APPLICATION_EXECUTED
```

A green build is therefore not equivalent to a green runtime.

## Legal boundary

The repository must not distribute:

- copyrighted Sony firmware
- proprietary keys
- commercial games
- copyrighted game assets
- proprietary system binaries

The project should provide mechanisms for legally obtained user-owned material where required.

Test fixtures should be legally distributable.

## Current status

PSX5 is an experimental research project.

Current development should prioritize engineering foundations:

```text
Android build
    |
    v
native runtime
    |
    v
memory model
    |
    v
CPU/JIT
    |
    v
kernel/platform layer
    |
    v
GPU abstraction
    |
    v
Vulkan
    |
    v
shader translation
    |
    v
system bring-up
    |
    v
application compatibility
```

The project should not claim compatibility based on compilation, static linkage, symbol presence, or application launch alone.

## References

FEX:

https://github.com/FEX-Emu/FEX

VKD3D-Proton:

https://github.com/HansKristian-Work/vkd3d-proton

aPS3e:

https://github.com/aenu1/aps3e

libadrenotools:

https://github.com/bylaws/libadrenotools

PSX5 VKD3D/QSA research:

https://github.com/SeaNaxxx/VKD3D-Proton-QSA-Mesa-Turnip-QSA

crosvm:

https://github.com/google/crosvm

Android Virtualization Framework:

https://android.googlesource.com/platform/packages/modules/Virtualization/

Vulkan-Hpp:

https://github.com/KhronosGroup/Vulkan-Hpp

glslang:

https://github.com/KhronosGroup/glslang

Proton:

https://github.com/ValveSoftware/Proton

RPCS3:

https://github.com/RPCS3/rpcs3

Android NDK:

https://developer.android.com/ndk/guides

## Final engineering principle

The critical development chain is:

```text
source
 ->
compile
 ->
link
 ->
final ELF
 ->
binary verification
 ->
runtime initialization
 ->
x86-64 execution
 ->
ARM64 JIT
 ->
PS5 platform compatibility
 ->
GPU translation
 ->
Vulkan
 ->
Android GPU
 ->
application compatibility
```

Every transition is an independent milestone and must be independently verified.

The objective is not merely to produce an APK.

The objective is to build a reproducible, observable, testable, and progressively verifiable PS5 compatibility stack for Android ARM64.
