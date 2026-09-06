# CPU: x86-64 → FEXCore → ARM64

> M1 contract. FEXCore is the CPU translation backend and nothing else.
> PX5 owns the PS5 execution environment around it. The two never
> share responsibilities.

## Division of responsibility

```
FEXCore (vendored, deps/FEX)          PX5 (app/src/main/cpp/)
  x86-64 frontend (decode)              guest virtual memory (memory/)
  FEX IR + optimizations                guest threads + lifecycle
  ARM64 JIT backend                     TLS / FSBASE / GDT-LDT install
  CPU context registers                 ORBIS entry ABI (argc block, auxv)
                                        syscall trap routing (kernel/)
                                        NID gate for libSce* imports
```

PX5 never patches FEXCore semantics for PS5 convenience; if a guest
behaviour needs kernel-side help, the fix belongs in PX5's syscall /
memory layers, driven through FEXCore's public host interfaces.

## The execution path

```
eboot.bin (SELF) → self_extract → elf_loader → guest memory segments
  → runtime_linker (relocs, imports → NID ledger)
  → entry point: RIP=0x140800000, stack, auxv, FSBASE installed
  → FEXCore context → ExecuteThread → ARM64 JIT dispatch
```

## M1 evidence gate (deterministic, on device)

The fixtures, in order — each is a fixed byte sequence with a known
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

Since v1.15, every attempt to dispatch guest code on the device —
fork-isolated OR in-process — faults at `ExecuteThread` entry with
SIGSEGV `si_addr=0x4`, `si_code=1`, immediately after the full setup
chain succeeds (Initialize 0-6, guest window mapped prot=7, "Guest
thread created"). Evidence chain: v1.20 in-process run (pid = app
itself — fork exonerated), v1.21 evidence-first crash reports
(raw_pc first, guest-stack-safe handler, per-thread altstack), v1.22
trail prefix fix. The v1.21 session did not run the conformance, so
the symbolized PC is still outstanding.

Candidate causes under investigation: dispatcher-entry null-ish
field deref at offset 4 (function pointer/vtable through a null
frame struct), FEXCore-vs-kernel/API-36 boundary on this device
(nubia NX779, Adreno 750, ARMv9, API 36), JIT mapping
characteristics. The next diagnostic run must produce
`pc=libpx5.so+0x…` resolvable via `scripts/symbolize_px5.py` against
the CI `px5-native-symbols-arm64` artifact.

M1 is done when ALL nine fixtures pass deterministically on the
device. Nothing downstream (loader validation beyond parse, GPU IR
consumption from guest streams, HLE behaviour tests) may claim
device truth until then — with the sole exception of M5-M7 GPU
work, which is deliberately host-side and progresses independently
(see docs/gpu.md).
