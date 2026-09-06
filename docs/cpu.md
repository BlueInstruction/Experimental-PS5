# CPU: x86-64 → FEXCore → ARM64

> M1 contract. FEXCore is the CPU translation backend and nothing else.
> PX5 owns the PS5 execution environment around it. The two never
> share responsibilities.

## Division of responsibility

```
FEXCore (fetched, .deps/FEX)          PX5 (app/src/main/cpp/)
  x86-64 frontend (decode)              guest virtual memory (memory/)
  FEX IR + optimizations                guest threads + lifecycle
  ARM64 JIT backend                     TLS / FSBASE / GDT-LDT install
  CPU context registers                 ORBIS entry ABI (argc block, auxv)
                                        syscall trap routing (kernel/)
                                        NID gate for libSce* imports
```

FEXCore is NOT vendored: `tools/fetch_fexcore.sh` materializes the
pinned upstream tree at `<repo>/.deps/FEX` (gitignored) — the same
path `app/build.gradle.kts` and CI default to. The directory does not
exist until you fetch it.

PX5 never patches FEXCore semantics for PS5 convenience; if a guest
behaviour needs kernel-side help, the fix belongs in PX5's syscall /
memory layers, driven through FEXCore's public host interfaces.

## The execution path

```
eboot.bin (SELF) → self_extract → elf_loader → guest memory segments
  → runtime_linker (relocs, imports → NID ledger)
  → entry point: RIP = the image's loaded e_entry, stack, auxv,
    FSBASE installed
  → FEXCore context → ExecuteThread → ARM64 JIT dispatch
```

## M1 evidence gate (deterministic, on device)

The nine fixtures below are the M1 DELIVERABLE — planned, not yet in
tree (the runner and fixtures get committed under `tools/hosttests/`
with the gate attempt). Each is a fixed byte sequence with a known
result, executed through the real FEXCore JIT on the ARM64 device:

| fixture | proves | pass shape |
|---|---|---|
| test_x86_basic | mov/add/hlt decode+exec | `result = 42, exit = 42` |
| test_arithmetic | flags, widths, imm forms | expected register file dump |
| test_branch | jcc loops, deterministic exit | fixed iteration count in R14-class reg |
| test_stack | push/pop/rsp discipline | stack pointer invariant restored |
| test_call_return | call/ret + shadow space | return address honored |
| test_memory | guest-VA load/store via PX5 maps | bytes round-trip |
| test_tls | FSBASE-relative access | TIB slot round-trip |
| test_syscall_trap | syscall → PX5 dispatcher → HLE | named syscall handled, RAX = contract value |
| test_elf_execution | minimal PX5-owned ELF → entry → exit | exit code 42 |

Rule: a fixture that passes on the CI x86_64 smoke build but not on
the ARM64 device is NOT passed. Determinism means same input, same
registers, same exit, every run, logged.

## The known blocker (recorded, not hidden)

Since v1.15, attempts to dispatch guest code on the device —
fork-isolated OR in-process — faulted at `ExecuteThread` entry with
SIGSEGV `si_addr=0x4`, `si_code=1`, after the full setup chain
succeeded (Initialize 0-6, guest window mapped prot=7, "Guest thread
created"). The chain, by version:

- v1.20: in-process run (pid = app itself — fork exonerated).
- v1.21–v1.22: evidence-first crash reports (raw_pc first,
  guest-stack-safe handler, per-thread altstack, trail prefix).
- v1.24 (2026-08-31, vc24 auto-run): the 0x4 crash SYMBOLIZED —
  `pc=libpx5.so+0x3809e4`,
  `FEXCore::Frontend::Decoder::DecodeInstructionsAtEntry +0xc4`
  (Frontend.cpp:1395): `CSSegment->L` with `GetSegmentFromIndex()`
  returning `&NULL[0]` — `CPUState::segment_arrays[]` left NULL by an
  uninitialized host (CoreState.h:158). FIX: PX5 installs the GDT/LDT
  arrays after CreateThread, as every FEX host does.
- v1.25: next stage — `Frontend.cpp:1396` assert
  `Is64BitMode == Config.Is64BitMode` fired: `IS64BIT_MODE` defaults
  FALSE and `SetConfigKey` never mapped it. FIX:
  `CONFIG_IS64BIT_MODE=1` at context construction.
- v1.40: kernel-side TLS/TCB built and FSBASE set BEFORE entry (the
  vc40 session's LDAR-through-null-FS evidence).
- v1.46 (vc47): ORBIS entry ABI at dispatch — RDI = initial SP (argc
  block), RSI/RDX = 0; FEXCore zeroes all GPRs, so `_start`'s first
  load through RDI=0 faulted.

Each fix exposed the next stage; the gate stays unmet until the nine
fixtures pass on device. The next diagnostic run must produce a
crash-free fixture suite — any new fault resolves through
`tools/symbolize_px5.py` against the CI `px5-native-symbols-arm64`
artifact (unstripped libpx5.so).

M1 is done when ALL nine fixtures pass deterministically on the
device. Nothing downstream (loader validation beyond parse, GPU IR
consumption from guest streams, HLE behaviour tests) may claim
device truth until then — with the sole exception of M5-M7 GPU
work, which is deliberately host-side and progresses independently
(see docs/gpu.md).
