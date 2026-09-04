# AGENTS.md

Notes for contributors and coding agents working on PSX5.

## Project Overview

PSX5 is a research PS5 compatibility layer for Android ARM64, targeting
Qualcomm Snapdragon devices. Guest x86-64 code runs on FEXCore
(https://github.com/FEX-Emu/FEX), built from a pinned upstream release.
The PS5 side (SELF/ELF loader, guest memory, syscalls, kernel HLE) is
original code in this repository.

Early research: no commercial game runs, and nothing is claimed to work
without runtime evidence behind it.

## Rules

1. Every function does what its name says. No stubs that return success,
   no invented addresses, no dead code, no UI that pretends to work.
2. Compiling is not running. When a commit claims a behavior change, the
   body cites how it was checked: a log line, an exit code, or a data
   round-trip.
3. Device capabilities come from measurements (vulkaninfo, driver logs),
   not from chip names or guesswork.
4. Fetched upstream trees are never committed and never edited by hand.
   Engine changes go in as numbered patches under `tools/patches/`.

## Repository Layout

```text
app/src/main/cpp/
├── core/                   # emulator state machine
├── memory/                 # guest address space manager
├── kernel/                 # syscall dispatch + kernel HLE
├── loader/                 # ELF64 loader + SELF container handling
├── fexcore_wrapper.cpp     # FEXCore context lifecycle
├── fexcore_integration.cpp # JIT compile/run path, guest dispatch
├── gpu/                    # Vulkan device, driver slots, GNM/PM4
├── audio/ input/ filesystem/ media/    # subsystem wrappers
├── stub/ui_smoke_stub.cpp  # x86_64 symbol-compatible stub (CI only)
└── tests/                  # host-side self-tests

tools/
├── fetch_fexcore.sh        # fetches the pinned FEX tree, patches it
├── verify_evidence.py      # rechecks the load-evidence ledger offline
└── patches/fex-*.patch     # patches applied onto the FEX tree

.github/workflows/          # APK build (arm64-v8a), lint, cppcheck, clang-tidy
```

## Build

| Tool | Version |
|------|---------|
| JDK | 17 |
| Gradle | 8.9 |
| Android SDK | API 35 |
| NDK | 27.3.13750724 |
| CMake | 3.22.1 |

```bash
./tools/fetch_fexcore.sh
export PX5_FEXCORE_ROOT="$PWD/../deps/FEX"
gradle assembleRelease --no-daemon
```

CMake stops with an error if `PX5_FEXCORE_ROOT` is unset or is not a
patched FEX tree. Compile before pushing (local or CI).

## Engine Dependency

FEX is pinned (`FEX-2608`, commit `e869aa64`). `tools/fetch_fexcore.sh`
fetches and verifies the pin; engine changes are made as numbered patches
in `tools/patches/` that apply cleanly against it. Anything edited
directly in the fetched tree is lost on refetch.

## Commit Messages

Subjects follow `area: summary`: short, imperative, lowercase area
prefix. Areas in use: loader, runtime, cpu, gpu, kernel, diag, crash, ui,
exec, probe, test, build, ci, docs, fix, revert, chore.

Version numbers stay out of subjects. Releases are git tags; the release
commit records versionName/versionCode in its body.

The body is optional. A non-trivial change states what changed, why, and
how it was verified, in plain prose. Small fixes need only a subject.

Example:

```
loader: apply DT_RELA relative relocations before guest dispatch

A DYN-style ORBIS image keeps absolute data pointers at link-time values
until DT_RELA is applied; the current image carries 120,333 RELA entries
(119,961 R_X86_64_RELATIVE, per DT_RELACOUNT).
...
Build: versionName 1.43 (versionCode 44).
```

## Legal

No Sony firmware, keys, or game assets here. The project runs legally
obtained software on hardware the owner controls. Reference projects are
credited in README.
