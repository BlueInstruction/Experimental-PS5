# AGENTS.md

Guidance for AI coding agents and contributors working in this
repository. Read the relevant section before changing code.

## Project Overview

PSX5 is a research PS5 compatibility layer for Android ARM64, targeting
Qualcomm Snapdragon devices. x86-64 guest code is translated by
[FEXCore](https://github.com/FEX-Emu/FEX), built from a pinned upstream
release; the SELF/ELF loader, guest memory manager, syscall surface, and
subsystem seams are original code in this repository.

It is an early research platform. It runs no commercial titles today,
and no status claim is made beyond what runtime evidence supports.

## Rules

1. **No placeholder implementations.** Every function does what its name
   claims. No stubs that return success, no invented addresses, no dead
   code paths, no UI that simulates functionality.
2. **Runtime evidence.** Compilation is not execution. A behavior claim
   must cite a runtime probe — a logcat marker, an exit code, or a
   round-trip data check — referenced in the commit body.
3. **Observed truth only.** Device capabilities come from measurements
   (`vulkaninfo`, driver logs), never from marketing names or inference.
4. **Dependency contract.** Fetched upstream trees are never committed
   and never hand-edited; integration changes land as numbered patches
   under `tools/patches/`.

## Repository Layout

```text
app/src/main/cpp/
├── core/                   # emulator orchestration state machine
├── memory/                 # guest address space manager
├── kernel/                 # syscall dispatch + HLE surface
├── loader/                 # ELF64 loader + SELF container handling
├── fexcore_wrapper.cpp     # FEXCore context lifecycle glue
├── fexcore_integration.cpp # JIT compile/run path + guest dispatch bridge
├── gpu/                    # Vulkan device, driver slots, GNM/PM4 research
├── audio/  input/  filesystem/  media/   # subsystem seams
├── stub/ui_smoke_stub.cpp  # x86_64 symbol-compatible stub (CI only)
└── tests/                  # host-side self-tests

tools/
├── fetch_fexcore.sh        # pinned engine bootstrap
├── verify_evidence.py      # offline recomputation of the evidence ledger
└── patches/fex-*.patch     # managed deltas onto the pristine FEX tree

.github/workflows/          # APK build (arm64-v8a), lint, cppcheck, clang-tidy
```

## Build

JDK 17 · Gradle 8.9 · Android SDK API 35 · NDK 27.3.13750724 · CMake 3.22.1

```bash
./tools/fetch_fexcore.sh
export PX5_FEXCORE_ROOT="$PWD/../deps/FEX"
gradle assembleRelease --no-daemon
```

CMake fails fast if `PX5_FEXCORE_ROOT` is unset or is not a valid patched
FEX tree. Verify that changes compile (locally or via CI) before pushing;
never leave unverified diffs on `main`.

## Engine Dependency

Upstream FEX is pinned (`FEX-2608`, commit `e869aa64`), fetched and
verified by `tools/fetch_fexcore.sh`, and patched only through numbered
files in `tools/patches/` that apply cleanly against the pin. Hand edits
to the fetched tree are discarded on refetch.

## Commit Conventions

Commit history is engineering documentation: `git log --oneline` must
read as the project's technical evolution, with no reference to any
development conversation.

* **Subject:** `area: summary` — imperative, ≤ 72 chars. Areas: `loader`,
  `runtime`, `cpu`, `gpu`, `kernel`, `diag`, `crash`, `ui`, `exec`,
  `probe`, `test`, `build`, `ci`, `docs`, `fix`, `revert`, `chore`.
* **No version numbers in subjects.** Releases are git tags; versionName
  and versionCode go in the commit body.
* **Body:** the observable behavior change, the root cause, and the
  verification evidence, in neutral technical prose.
* **Never:** conversation references, narration, first-person anecdotes,
  or rhetorical framing.

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

This project must never distribute Sony firmware, proprietary keys, or
copyrighted game assets. It exists to run legally obtained software on
hardware its owner controls. External projects used as references are
acknowledged in the README Credits section.
