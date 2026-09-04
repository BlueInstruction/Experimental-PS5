# Contributing to Experimental-PS5 (PSX5)

Development documentation for this repository: project rules, repository
layout, build steps, the engine dependency contract, subsystem status,
and commit conventions. Read the relevant section before changing code.

---

## 1. Project rules

PSX5 is a **research PS5 compatibility layer for Android ARM64** targeting
Qualcomm Snapdragon devices (Adreno A7xx–A8xx class GPUs). It is not a
production emulator and runs no commercial titles today.

1. **No placeholder implementations.** Every function does what its name
   claims. No stubs that return success, no invented addresses, no dead
   TODO code paths, no UI that simulates functionality. Dead code and
   placeholders are removed on sight.
2. **Runtime evidence.** Compilation is not execution; APK install is not
   runtime success. Behavior claims must cite a runtime probe (logcat
   marker, exit code, round-trip data check). Status claims without a
   cited check are not accepted.
3. **Main branch only.** All work happens directly on `main`. Force-pushes
   and history rewrites require repository-owner approval.
4. **What never gets committed:** secrets (`*.token`, `*.pat`, `.creds/`,
   `.env`, keys), build artifacts, logs, or vendored upstream trees. The
   `.gitignore` at the repository root is the authoritative list.
5. **Dependency policy:** upstream source trees are never committed; they
   are materialized by the pinned bootstrap (§4).

---

## 2. Repository layout

``` text
app/src/main/cpp/
├── CMakeLists.txt          # root native build; routes PX5_FEXCORE_ROOT
├── compat/                 # std::atomic_ref polyfill + platform shims
├── core/                   # emulator orchestration state machine
├── memory/                 # guest address space manager (PageTable/mprotect)
├── kernel/                 # syscall handler + HLE surface (sce_kernel_hle)
├── loader/                 # ELF64 loader + SELF container handling
├── fexcore_wrapper.cpp     # FEXCore context creation & lifecycle glue
├── fexcore_integration.cpp # JIT compile/run path + guest dispatch bridge
├── gpu/                    # vulkan_device (instance/device/submission),
│                           # driver_manager (slot-based driver selection)
├── audio/                  # AAudio-backed stream wrapper
├── input/                  # controller mapping w/ atomic state
├── filesystem/             # vfs abstraction
├── media/                  # MediaCodec HLE seam
├── stub/ui_smoke_stub.cpp  # x86_64 symbol-compatible library (CI only)
└── tests/                  # host-side unit/self-tests

tools/
├── fetch_fexcore.sh        # deterministic engine bootstrap (pin + submodules
│                           # + overlay patches); see §4
├── verify_evidence.py      # offline recomputation of the load-evidence ledger
└── patches/fex-*.patch     # managed deltas applied onto pristine FEX tree

.github/workflows/          # exactly four files:
├── build.yml               # PX5 Build APK (arm64-v8a full engine, NDK 27.3)
├── android-lint.yml        # Kotlin/Lint static analysis
├── cppcheck.yml            # C/C++ static analysis (SARIF)
└── clang-tidy.yml          # C++ linting via NDK clang-tidy + compile DB
```

---

## 3. Building locally

Prerequisites (must match CI exactly):

| Tool | Version |
|------|---------|
| JDK | 17 (Temurin or equivalent) |
| Gradle | 8.9 (wrapper config pins it) |
| Android SDK | API 35 |
| NDK | 27.3.13750724 |
| CMake | 3.22.1 |

```bash
./tools/fetch_fexcore.sh                       # materializes $PX5_FEXCORE_ROOT
export PX5_FEXCORE_ROOT="$PWD/../deps/FEX"     # default location of step 1
gradle assembleRelease --no-daemon             # release APK (both ABIs)
```

The CMake layer fails fast with an explanatory error if
`PX5_FEXCORE_ROOT` is unset or does not contain a valid patched FEX tree.

---

## 4. Engine dependency contract (FEXCore)

The CPU translation engine is upstream [FEX-Emu/FEX](https://github.com/FEX-Emu/FEX)
built from source, never vendored, never tracked by git.

* Current pin: tag `FEX-2608`, commit
  `e869aa644a16e4332cdc15c1ea0b4d13d482385d`.
* `tools/fetch_fexcore.sh` performs: shallow clone of the exact pin →
  SHA verification against `PIN_SHA` → submodule materialization
  (fmt / xxhash / range-v3 / unordered_dense …) → application of all
  `tools/patches/fex-*.patch` overlays → atomic move into place with a
  `.fex-pin` marker file.
* **Patch discipline:** when integration requires a change to upstream's
  tree, commit it as a numbered patch under `tools/patches/`. Patches must
  apply cleanly against the current pin (`git apply --check` gates it).
  Never hand-edit `$PX5_FEXCORE_ROOT`; those edits are lost on refetch.
* **Upgrading the pin:** resolve the new release tag's commit SHA via the
  GitHub API, update `PIN_TAG`/`PIN_SHA`, run a full local configure, then
  rebase any rejected patches onto the new tree before pushing.

Android-specific adaptations applied via cache variables in
`app/src/main/cpp/CMakeLists.txt` or overlay patches:

* C++20 forced at parent scope (upstream sets it only inside its subdir);
* `std::atomic_ref` force-included polyfill (NDK/Bionic libc++ lacks it);
* LTO disabled — NDK ships no `LLVMgold.so` plugin;
* `lld` forced — gold cannot link ARM64 Android correctly here;
* static `FEXCore` target only; `FEXCore_shared` and every host tool
  (FEX, FEXBash, FEXServer, …) excluded from the build;
* `jemalloc_glibc` allocator hooking disabled (Bionic provides neither the
  glibc allocator ABI nor its hooks);
* the platform gate accepts `CMAKE_SYSTEM_NAME == Android`
  (patch `fex-0001-android-platform-gate.patch`).

---

## 5. Subsystem status

Statuses are updated only with runtime-proven evidence, citing how it was
proven (test name, log probe, CI job).

| Subsystem | State |
|-----------|-------|
| FEXCore static linkage + context bring-up | working — minimal guest instructions execute, results observable |
| Guest memory model (reserve → page-map within reservation) | implemented; mmap round-trip self-test |
| Syscall layer | real handlers behind a dispatch table |
| ELF loader | reads program headers, maps segments, entry bridged to JIT |
| Runtime linker + NID gate (guest → bionic HLE) | implemented (v1.31); reserved-syscall gate into the export registry; PT_DYNAMIC reader |
| DYN-style eboot loading (PS5 0xFE10 inner ELF) | implemented (v1.32); two-phase load since v1.34 (map RWX → copy → seal to ELF flags) |
| DT_RELA processing | v1.43: R_X86_64_RELATIVE applied before dispatch; undefined-symbol imports counted as the HLE/NID worklist |
| PS5 XOM text (PF_X without PF_R) | v1.38 `HostReadableExec` seal rule — executable guest pages are never sealed below-read |
| Guest process environment (initial stack + TLS) | v1.39 FEXLoader-style initial stack + real `arch_prctl`; v1.40 pre-entry FSBASE (FreeBSD/ORBIS TCB) + auxv from parse truth |
| Evidence layer | v1.41/v1.42: every load is SHA-256-bound (whole stream + per-PT_LOAD), 32-byte entry proof hashed from both mapped memory and source stream, append-only ledger (`px5_evidence.log`), inner-ELF dump to `<logs>/elfdumps/`; `tools/verify_evidence.py` recomputes every ledger claim offline (stdlib-only, exit 1 on mismatch) |
| Vulkan device init | instance/device/surface live, submission loop proven |
| GPU command translation (GNM → Vulkan) | **not started** — PM4 decoder + GPU state model exist (Phase C m1) |
| Shader compiler seam | **not started** |
| libkernel HLE | minimal surface v1 |
| Audio / input | functional wrappers, lock-free input state |
| Driver switching (Turnip ↔ vendor) | v1.36 slot manager + adrenotools hook; importer bundles non-public platform DT_NEEDED deps |
| UI shell | Steam-Deck-style Compose shell, functional preferences |

Forbidden status vocabulary: "works", "supported", "runs" without an
evidence citation attached in the PR/commit message.

---

## 6. Target device envelope

* CPU target: Android ARM64 only (`arm64-v8a`). The x86_64 ABI exists solely
  to boot the CI UI-smoke AVD and ships a symbol-compatible stub library —
  it deliberately contains no engine.
* GPU targets: Qualcomm Adreno A7xx–A8xx. Capabilities come from observed
  truth: device `vulkaninfo` dumps, Mesa Turnip release notes
  ([mesa.freedesktop.org](https://www.mesa3d.org)), freedreno changelogs,
  and libadrenotools documentation. Do not construct support matrices from
  probability or marketing names (Snapdragon model ≠ driver capability).
* Driver strategy: patched Turnip installed per-app via
  [libadrenotools](https://github.com/bylaws/libadrenotools); selection
  logic stays isolated so driver choice cannot leak into renderer logic.
* Root access is not required and must never become one.

---

## 7. Architecture direction (GPU path)

The PS5 graphics pipeline is modeled after what the research emulators
demonstrate publicly ([Kyty](https://github.com/KytyPS5/KytyPS5),
[SharpEmu](https://github.com/sharpemu/sharpemu),
[prosper](https://github.com/mattias800/prosper)) plus shadPS4's proven
PS4-era design ([shadPS4](https://github.com/shadps4-emu/shadPS4)):

1. guest command buffers are translated into an internal GPU IR;
2. the IR lowers to Vulkan command streams — no assumption that one guest
   opcode equals one Vulkan call;
3. shaders arrive as GNM/PSG-era bytecode-or-equivalent and lower through
   the internal IR to SPIR-V;
4. memory-export/render-target aliasing handled via explicit barriers;
   VMA may manage *host* allocations but never replaces the guest VM model.

Reference links (curated):

* Vulkan ecosystem: KhronosGroup/Vulkan-Headers · Vulkan-Loader ·
  Vulkan-Docs · SPIRV-Tools · SPIRV-Cross · glslang ·
  GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator · VK-GL-CTS
* Debugging/validation: baldurk/renderdoc · ValveSoftware/Fossilize
* Adreno ecosystem: bylaws/libadrenotools (+ eden-emulator /
  Pipetto-crypto forks), bylaws/liblinkernsbypass
* Emulator references: prosper (PS5 OS/HLE/AGC/RDNA2→SPIR-V architecture),
  RPCS3 (CPU/memory/kernel patterns),
  JICA98/Bachata-S4 (Android PS4 attempt), ps5-linux/* (hardware research)

Not engine components (research/tooling contexts only): VKD3D-Proton,
MoltenVK, libnx, Magisk, Vortek/Gladio, Qualcomm Windows drivers,
ReShade/vkBasalt, mast1c0re, PS5 Linux loader.

Before adding a new dependency, document: subsystem owner,
runtime-vs-reference role, license, ARM64 status, Bionic compatibility,
Linux-only facilities, x86-host needs, alternate graphics API, interface
isolation, and how it will be exercised at runtime.

---

## 8. Testing ladder (enforced ordering)

Unit tests → decoder tests → IR tests → JIT tests → memory tests →
scheduler tests → loader tests → Vulkan init → shader tests → GPU
translation → audio/input → system bring-up → application execution →
compatibility corpus.

Each rung may only claim progress once the previous rung has a repeatable,
automated check.

---

## 9. Development workflow

1. Review this document and recent `git log` before starting work.
2. Implement, then immediately verify: configure AND compile locally (or
   push and watch CI when the local toolchain is unavailable). Never leave
   unverified diffs on `main`.
3. When something breaks an upstream dependency assumption, add/adjust a
   patch + pin bump rather than editing fetched trees.
4. When CI fails: reproduce locally if possible; fix; push; re-watch. Two
   consecutive network-flake failures warrant rerunning the workflow once
   before suspecting code.

---

## 10. Commit message conventions

Commit history is engineering documentation. Someone running
`git log --oneline` a year from now must be able to follow the project's
technical evolution without context from any development conversation.

* **Subject:** `area: summary` — imperative, ≤ 72 chars, no trailing
  period. Areas in use: `loader:`, `runtime:`, `cpu:`, `gpu:`, `kernel:`,
  `diag:`, `crash:`, `ui:`, `exec:`, `probe:`, `test:`, `vendor:`,
  `build:`, `ci:`, `docs:`, `fix:`, `feat:`, `revert:`, `chore:`.
* **No version numbers in the subject.** Releases are marked with tags
  (`git tag v1.43`); build identity (versionName/versionCode) is recorded
  in the commit body.
* **Body:** explains the observable behavior change, the root cause, and
  the verification evidence (test counts, log lines, device-session
  findings). Written in neutral technical third person.
* **Never include:** references to development conversations, assistant/
  developer narration, first-person anecdotes, or rhetorical framing.
  Describe the code and the evidence, nothing else.

Example:

```
loader: apply DT_RELA relative relocations before guest dispatch

A DYN-style ORBIS image keeps absolute data pointers at link-time
values until DT_RELA is applied; the current image carries 120,333
RELA entries (119,961 R_X86_64_RELATIVE, per DT_RELACOUNT).
...
Build: versionName 1.43 (versionCode 44).
```

---

## 11. Legal boundary

This project must never distribute or encourage distributing Sony firmware,
proprietary keys, commercial game assets, or copyrighted system binaries.
It exists to run legally obtained homebrew/software on hardware the owner
controls, using open-source engines (FEX, Mesa/Turnip, Android NDK) glued
by original code in this repository.

External repositories are used as engineering references. Core PSX5
components are original implementations; external projects that are
studied, linked, or used are acknowledged in the README Credits section.
