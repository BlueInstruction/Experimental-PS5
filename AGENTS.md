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

.github/workflows/          # APK build (arm64-v8a), lint, cppcheck
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
# tools/fetch_fexcore.sh defaults to <repo>/.deps/FEX; set this only
# to override that location.
# export PX5_FEXCORE_ROOT="/custom/path/FEX"
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

This repo is GPL-2.0-only (SPDX: GPL-2.0-only, relicensed from
GPL-3.0 by the copyright holder, 2026-10) — chosen to match the PS5
ecosystem (AnyPS5, shadPS4 are GPL-2.0-only). Merging code from
same-license open-source projects needs NO per-copy permission; the
license itself grants it. The obligations ride along automatically:

- Keep copyright/license headers of copied files intact.
- Credit the source project in README Credits.

Compatibility map (verified against upstream LICENSE files 2026-10):
mergeable without friction = AnyPS5, shadPS4 (both GPL-2.0-only),
FEX/FEXCore, box64, Kyty, fmt, ankerl_unordered_map (MIT),
libadrenotools, liblinkernsbypass (BSD-2-Clause). NOT mergeable =
GPL-3-only or Apache-2.0-only sources (some Google projects) — verify
the SPDX of anything new before importing it.

## CI Review Policy

Applies to any AI agent (or human) working on this repository.

Before a task is called complete:

1. Read the CI results for the exact commit being reviewed — never assume
   a check is green without seeing its conclusion. Same-repo agents with a
   read token can fetch everything through the GitHub API:
   - `commits/<sha>/check-runs` — status and conclusion per check
   - `check-runs/<id>/annotations` — inline findings
   - `actions/jobs/<job_id>/logs` — full job log, including the
     "Publish lint summary" and "Publish cppcheck summary" steps, which
     carry the structured findings as plain text
   - `actions/artifacts` — cppcheck.xml / lint reports / APK
   - `code-scanning/alerts` — SARIF alerts from the Security tab
   The run page Step Summary mirrors the publish steps; either source is
   fine. If results are unreachable, say so — do not invent findings.
2. For every finding, open the reported file and line before proposing a
   fix; don't fix blind from the warning text. Fix priority: memory
   safety / UB / resource lifetime / thread safety > API misuse >
   style / performance.
3. Never suppress a finding just to turn CI green. Suppress only a
   demonstrated false positive or a documented intentional exception, and
   state the reason in the suppression comment itself.
4. Do not edit the fetched engine tree (see `tools/fetch_fexcore.sh`) to
   silence findings — engine changes go through numbered patches.
5. Severity: cppcheck `error` / Lint `Error` blocks the merge (the gate
   step fails); `warning` findings are reviewed one by one; style,
   portability and performance notes are opportunistic and must not drive
   scope.
6. A green CI run means "no gate-blocking findings were detected": the
   cppcheck gate fails only on `error` severity, so `warning` findings
   may still be present in the publish summary (see item 5). A green run
   does NOT prove CPU translation, ELF loading, or Vulkan correctness —
   see Rules 2 and 3: behavior claims need runtime evidence from the
   device logs.
