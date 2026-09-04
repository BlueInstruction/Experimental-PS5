# AGENT.md — Operating Manual for AI Coding Agents

This file is the authoritative working contract for any agent (human or AI)
that modifies the Experimental-PS5 repository ("PSX5"). Read it fully before
writing code. The legacy research README was converted into this document;
the public-facing overview now lives in `README.md`.

---

## 1. Mission and non-negotiable rules

PSX5 is a **research PS5 compatibility layer for Android ARM64** targeting
Qualcomm Snapdragon devices (Adreno A7xx–A8xx class GPUs). It is not a
production emulator and runs no commercial titles today.

Hard rules:

1. **No fake code.** Every function must do what its name claims. No stubs
   that return success, no invented addresses, no dead "TODO" code paths,
   no UI that fakes functionality. Dead code and vibe-coded placeholders are
   removed on sight.
2. **Evidence over assumptions.** Compilation is not execution. APK install
   is not runtime success. Claims about behavior must be backed by runtime
   probes (logcat markers, exit codes, round-trip data checks).
3. **Only main branch.** All work happens directly on `main`. Do not create
   feature branches unless explicitly instructed. History cleanup operations
   (rewrites) must be approved by the repo owner first.
4. **Never commit secrets** (`*.token`, `*.pat`, `.creds/`, `.env`, keys),
   build artifacts, logs, or vendored upstream trees. The `.gitignore` at the
   repository root is the enforced list.
5. **Vendor policy:** upstream source trees are never committed into this
   repository. See §4 for the pinned bootstrap contract.

---

## 2. Repository layout (post-debloat)

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
└── patches/fex-*.patch     # managed deltas applied onto pristine FEX tree

.github/workflows/          # EXACTLY four files, nothing else:
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

Steps:

```bash
./tools/fetch_fexcore.sh                       # materializes $PX5_FEXCORE_ROOT
export PX5_FEXCORE_ROOT="$PWD/../deps/FEX"     # default location of step 1
gradle assembleRelease --no-daemon             # produces release APK (both ABIs, debug-keystore signed; v1.41 CI artifact)
```

The CMake layer fails fast with an explanatory error if
`PX5_FEXCORE_ROOT` is unset or does not contain a valid patched FEX tree.

---

## 4. Engine dependency contract (FEXCore)

The CPU translation engine is upstream [FEX-Emu/FEX](https://github.com/FEX-Emu/FEX)
built from source, **never vendored**, never tracked by git.

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
  rebase any rejected patches to the new tree before pushing.

Android-specific adaptations currently applied either via our cache
variables in `app/src/main/cpp/CMakeLists.txt` or via overlay patches:

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

## 5. Subsystem status honesty ledger

Statuses below are commitments, not aspirations. Update them only with
runtime-proven evidence, citing how it was proven (test name, logcat probe,
CI job).

| Subsystem | State | Proof requirement next step |
|-----------|-------|------------------------------|
| FEXCore static linkage + context bring-up | working (minimal guest instructions execute, result observable) | long-running guest program + crash-free teardown |
| Guest memory model (reserve → page-map within reservation) | implemented in-tree | mmap round-trip self-test green in CI artifact log |
| Syscall layer | real handlers behind a dispatch table | strace-style trace dump diffed against expected list |
| ELF loader | reads program headers, maps segments, entry bridged to JIT | run our signed-off test ELF end-to-end |
| Vulkan device init | instance/device/surface live, submission loop proven | clear+present visible frame on-device screenshot |
| GPU command translation (GNM-ish queue → Vulkan) | **not started** — design in §7 | first triangle through GNM-to-Vulkan path |
| Shader compiler seam | **not started** | SPIR-V emitted for one micro-shader |
| libkernel HLE | minimal surface v1 | syscall-count parity vs Kyty reference tables |
| Runtime linker + NID gate (guest -> bionic HLE) | implemented in-tree (v1.31): reserved-syscall gate into the export registry; PT_DYNAMIC reader; linker self-test runs on both ABIs | foundation step 10 `[PASS]` (exit 42) in the next on-device report + CI smoke green |
| DYN-style eboot loading (PS5 0xFE10 inner ELF) | implemented in-tree (v1.32; v1.34 two-phase load: map RWX → copy → seal to ELF flags, fixing the vc34 write-to-RO-text ACCERR) | vc35 device session: the game's OWN code path (guest crash with RIP inside the game text range or its first real syscall) + foundation 5b/6/10 `[PASS]` |
| Game boot UX | v1.35: the game's own cover card leads the 0→100 boot pipeline; failure = symbolic title id + one clean centered card, the loader's detailed reason lives only in the Logs screen; Eden-style in-game menu | vc35 device session: cover renders from the library entry, boot overlay completes without a diagnostics wall |
| PS5 XOM text segments (PF_X, no PF_R) | v1.38: loader/memory seal rule `HostReadableExec` — executable guest pages are never sealed below-read (AArch64 refuses loads from X-only pages; FEXCore's decoder must fetch bytes); W absence honored | vc39 device session: execution probe passes the eboot entry byte fetch (no SIGSEGV si_code=2 at DecodeInstructionsAtEntry) and the guest reports its own crash/exit |
| Guest process environment (initial stack + TLS) | v1.39: FEXLoader-style initial guest stack (argc/argv/envp/auxv incl. AT_PHDR/AT_ENTRY/AT_RANDOM read from the mapped image); real `arch_prctl` (ARCH_SET_FS/GS into the live CPUState — the old no-op left fs=0 and fs-relative atomics hit address 0, the vc39 LDAXR[0] crash); crash report now carries guest_state (rip/rsp/fs/gs + guest bytes @rip) | vc40 device session: the game's crt proceeds past early init (no null atomic at dispatch; either clean run, guest syscall, or a NEW named guest RIP) |
| ORBIS entry contract (pre-entry FSBASE + phdr-table-in-guest-VA) | v1.40: PS5 crt never issues arch_prctl (vc40: zero arch_prctl log lines, fs_base=0x0, first block died on LDAR w3,[x11] x11=0) — ExecuteLoadedGuest now builds the FreeBSD/ORBIS TCB ([tls init][pad][tcb_self/dtv/pthread][dtv]) and ExecuteAtHostRip pre-sets `fs_cached` before dispatch; auxv AT_PHDR/AT_PHNUM come from parse truth + a mapped phdr-table copy page (SELF-extracted images map segment content only — the vc40 auxv read text bytes and got AT_PHDR=0x4c1e… garbage); loader logs the FULL phdr table + pre-dispatch entry bytes; crash-report guest bytes read via process_vm_readv (API 36 SELinux denies /proc/self/mem) | vc41 device session: guest_state fs_base != 0, entry bytes logged sane, and the first guest block either executes onward or traps with a NAMED guest RIP + visible instruction bytes |
| Evidence layer (off-device verifiability) | v1.41: the 2026-09-04 trust review ruled that logs alone cannot carry the load — foundation-suite PASS proves the engine, not game execution, and a logger can print anything. Every real-game load is now SHA-256-bound (whole parsed stream + every PT_LOAD's file bytes), the 32 entry bytes are hashed from BOTH mapped memory and the source stream (match=1 pins the dispatch target to the game file), the first 16 guest syscalls land in an append-only ledger (px5_evidence.log, separate from the main log), and any crash report ATTRIBUTES the guest RIP to its PT_LOAD with the file offset to compare bytes against. Log namespaces: [SYNTH] = built-in fixtures, [GUEST] = the user-selected executable; foundation suite step 11 proves the SHA-256 primitive on-device (FIPS 180-4 KATs). v1.42: the ledger alone is now SUFFICIENT for offline verification — full segment hashes, entry-proof, container hash AND container size are ledger events; SELF loads dump the executed inner-ELF stream to `<logs>/elfdumps/` (Vita3K dump_elfs pattern) and bind `stream=inner_elf`; `tools/verify_evidence.py` recomputes EVERY claim from the user's own files (stdlib-only, exit 1 on any mismatch) — the whole procedure needs zero trust in the app or the agent; host demo `scripts/evidence_host_test.cpp` runs the production loader + evidence path end-to-end and proved the verifier both ways (clean file 13/13 OK, corrupted file REJECTED) and caught 2 real bugs (phdr-copy page write fault; SELF stream mislabel). Log format moved to the Eden contract the user supplied: `[  1.234567] Loader <Info> loader/elf_loader.cpp:142:Fn: msg` with automatic file:line:func capture, per-class filter, LOG_ONCE anti-spam; missing-NID accounting (Vita3K pattern): distinct unseen NIDs counted, logged once each, ledgered, summarized hot-first. Acceptance criterion is UNCHANGED and external: the game visibly runs | vc43 device session: `python3 tools/verify_evidence.py --ledger px5_evidence.log --file <your eboot>` prints 0 FAIL for every load of your own dump; entry-proof match=1 INDEPENDENTLY CONFIRMED by the script, not by the log |
| Audio/input | functional wrappers, atomics-honest input | on-device gamepad echo test |
| Driver switching (Turnip ↔ vendor) | v1.36: slot manager + adrenotools hook; the importer bundles the package's non-public platform DT_NEEDED deps (libhardware.so class) into the slot dir and the driver namespaces search the platform lib dirs as fallback | vc37 device session: the imported Turnip v26.x pack reports `driverVerified=yes` with its soname mapped in /proc/self/maps |
| UI shell | Steam-Deck-style Compose shell, functional preferences | no dead buttons; each control mutates a real setting |

Forbidden status vocabulary: "works", "supported", "runs" without an
evidence citation attached in the PR/commit message.

---

## 6. Target device envelope (facts, not guesses)

* CPU target: Android ARM64 only (`arm64-v8a`). The x86_64 ABI exists solely
  to boot the CI UI-smoke AVD and ships a symbol-compatible stub library —
  it deliberately contains no engine.
* GPU targets: Qualcomm Adreno A7xx–A8xx. Capabilities must come from
  observed truth: device `vulkaninfo` dumps, Mesa Turnip release notes
  ([mesa.freedesktop.org](https://www.mesa3d.org)), freedreno changelogs,
  and libadrenotools documentation. NEVER fabricate support matrices from
  probability or marketing names (Snapdragon model ≠ driver capability).
* Driver strategy: prefer patched Turnip installed per-app via
  [libadrenotools](https://github.com/bylaws/libadrenotools); keep the
  selection logic isolated so driver choice cannot leak into renderer logic.
* Root access is not required and must never become one.

---

## 7. Architecture direction (GPU path)

The PS5 graphics pipeline will be modeled after what the research emulators
demonstrate publicly ([Kyty](https://github.com/KytyPS5/KytyPS5),
[SharpEmu](https://github.com/sharpemu/sharpemu),
[prosper](https://github.com/mattias800/prosper)) plus shadPS4's proven
PS4-era design ([shadPS4](https://github.com/shadps4-emu/shadPS4)):

1. guest command buffers are translated into an internal GPU IR;
2. the IR lowers to Vulkan command streams — **no assumption that one guest
   opcode equals one Vulkan call**;
3. shaders arrive as GNM/PSG-era bytecode-or-equivalent and lower through
   the internal IR to SPIR-V;
4. memory-export/render-target aliasing handled via explicit barriers;
   VMA may manage *host* allocations but never replaces the guest VM model.

Reference links retained from the old README (curated):

* Vulkan ecosystem: KhronosGroup/Vulkan-Headers · Vulkan-Loader ·
  Vulkan-Docs · SPIRV-Tools · SPIRV-Cross · glslang ·
  GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator · VK-GL-CTS
* Debugging/validation: baldurk/renderdoc · ValveSoftware/Fossilize
* Adreno ecosystem: bylaws/libadrenotools (+ eden-emulator /
  Pipetto-crypto forks), bylaws/liblinkernsbypass
* Emulator references: prosper (PS5 OS/HLE/AGC/RDNA2→SPIR-V architecture),
  RPCS3 (CPU/memory/kernel patterns),
  JICA98/Bachata-S4 (Android PS4 attempt), ps5-linux/* (hardware research)

Misclassification guard-list (NOT our layers — do not import as engine
components): VKD3D-Proton, MoltenVK, libnx, Magisk, Vortek/Gladio,
Qualcomm Windows drivers, ReShade/vkBasalt, mast1c0re, PS5 Linux loader.
They belong to research/tooling contexts only.

Before adding ANY new dependency answer the ten-question gate from the old
README verbatim (subsystem owner, runtime-vs-reference, license, ARM64,
Bionic, Linux-only facilities, x86-host needs, alternate graphics API,
interface isolation, runtime exercise).

---

## 8. Testing ladder (enforced ordering)

Unit tests → decoder tests → IR tests → JIT tests → memory tests →
scheduler tests → loader tests → Vulkan init → shader tests → GPU
translation → audio/input → system bring-up → application execution →
compatibility corpus.

Each rung may only claim progress once the previous rung has a repeatable,
automated check.

---

## 9. Workflow rules for agents

1. Read this file + recent `git log` before every session.
2. Implement, then immediately verify: configure AND compile locally
   (or push and babysit CI when local toolchain unavailable). Never leave
   unverified diffs on `main`.
3. Commit messages describe **observable behavior changes**, not vibes.
4. When something breaks upstream dependency assumptions, add/adjust a
   patch + pin bump rather than editing fetched trees.
5. If asked to fake anything (feature presence, performance numbers,
   compatibility claims), refuse and cite this document.
6. When CI fails: reproduce locally if possible; fix; push; re-watch. Two
   consecutive network-flake failures warrant rerunning the workflow once
   before suspecting code.

---

## 10. Legal boundary

This project must never distribute or encourage distributing Sony firmware,
proprietary keys, commercial game assets, or copyrighted system binaries.
It exists to run legally obtained homebrew/software on hardware the user
owns, using open-source engines (FEX, Mesa/Turnip, Android NDK) glued by
original code in this repository.

---

## 11. External references & credits posture

1. External repositories are used primarily as **engineering references**.
   Studying architecture, file formats, algorithms, APIs, and observable
   behavior is free and is never blocked by a repository's license state
   (known, unknown, restrictive, or incompatible).
2. Designs are derived from observed behavior, public specifications,
   binary formats, interfaces, and multiple independent implementations.
   Core PSX5 components are clean-room reimplementations.
3. Copying or vendoring external source is a deliberate exception, not a
   default. It triggers a license check at the moment of copying — nothing
   more. Acknowledgment then belongs in the README **Credits** section
   (normal GitHub practice), plus a one-line license note in the fetch
   script if a dependency is vendored. No separate license ledgers, reuse
   maps, or reference documents are maintained.
4. The README Credits section stays truthful: only projects actually used,
   linked, or studied as references are listed.
