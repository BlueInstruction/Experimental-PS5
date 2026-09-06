# Milestones M0-M11

> Canonical progress contract. Movement requires the evidence gate,
> never "the code exists". Status values: ABSENT / STARTED / GATED
> (code exists, gate unmet) / PASS (evidence logged).

## M0 — Architecture lock — **PASS (this commit)**

Gate: docs/architecture.md + docs/cpu.md + docs/gpu.md + docs/testing.md
committed; every layer has an owner and an honest status. Layer
ownership and the guest/host separation are frozen; changes to these
documents are milestone-visible events, not drive-by edits.

## M1 — CPU Foundation — **GATED**

FEXCore executes deterministic x86-64 fixtures on the ARM64 device.
Gate: the nine fixtures in docs/cpu.md pass deterministically with
logged register results and exit codes.
Blocker (named): SIGSEGV si_addr=0x4 at ExecuteThread entry, in-process
and isolated, since v1.15. Next step: capture the symbolized PC
(v1.21+ crash reports + `scripts/symbolize_px5.py` + CI symbols
artifact), fix the named fault.
Existing: FEXCore integration (1207-line wrapper), GDT/LDT install,
FSBASE, auxv, ORBIS entry ABI at dispatch (vc47), 9-byte conformance
blob, in-process probe.

## M2 — ELF/SELF Loader — **STARTED** (validate after M1 unblocks)

Gate: load ELF → map segments → resolve relocations → create process →
execute entry (exit 42), on OWNED test files, then the same through a
PS5-owned SELF container.
Existing: self_extract (container → inner ELF, evidence-first fact
logging), elf_loader (two-phase load, XOM handling, PT_LOAD/DYNAMIC/
DYNLIBDATA/RELRO), runtime_linker (NID registry + import trap ledger,
DT_RELA before dispatch), SHA-256 identity binding, SAF-aware target
resolution. All currently provable only to the load boundary; past it,
M1's blocker gates execution.

## M3 — Guest Memory — **STARTED**

Gate: the GuestMemory API (allocate/map/unmap/read/write/protect/
fault/alignment/translate) passes its host test AND its on-device
exercised path is logged from a real boot.
Existing: guest window model (base + size + program break with
limit/refusal), replace-on-map ranges, overflow-safe phdr and
direct-memory bounds, page-size abstraction, cross-block munmap,
lock-free exec queries, `memory_range_test.cpp`.

## M4 — Threads + ABI + HLE — **STARTED**

Gate: guest thread lifecycle (create/run/exit/join) exercised on
device; every registered HLE/NID function labelled
implemented/partial/unsupported with a per-label test.
Existing: guest thread creation through FEXCore (proven up to
dispatch), TLS/FSBASE/GDT-LDT install, ORBIS entry ABI, syscall
dispatcher with reserved NID-gate numbers, sce_kernel_hle foundation,
loud-fail unknown-NID policy.

## M5 — PM4 Decoder — **STARTED**

Gate: fixed PM4 test stream decodes to the exact expected structured
sequence (`pm4_stream_test.cpp`, T1) — counts, opcodes, register
banks, draw/dispatch records.
Existing: type-3 decode, 11 packet semantics with GnmState writes,
unknown-opcode accounting, bounded stream errors. Missing: the
committed stream fixtures + expected-sequence test.

## M6 — GPU IR — **ABSENT**

Gate: GpuState + draw records lower to a committed IR op list
(SetRenderTarget … Barrier) with a lower-to-IR host test. No Vulkan
types may appear in IR definitions (keeps GNM decoupled from Vulkan).

## M7 — Vulkan Backend — **INFRASTRUCTURE AHEAD OF GATE**

Gate: the M7 chain — instance → device → queue → image → clear →
submit → fence → READBACK with expected pixels — passes on device
(T3). Existing ahead of the gate: real instance/device/swapchain on
Adreno 750, fork-safe self-contained clear-submit proof. `frames=0`
in every session = gate unmet. `driverVerified=yes` is explicitly NOT
this milestone's evidence.

## M8 — Shader Recompiler — **ABSENT** (by design, after M5-M7)

Gate: known PS5 shader binary → decoder → shader IR → SPIR-V →
executes on device → reference pattern reproduced in readback.
Research references: KytyPS5, shadPS4 (structure study, layer
ownership per docs/architecture.md).

## M9 — VideoOut — **STARTED (host side only)**

Gate: guest framebuffer → GPU image → videoout → Android Surface,
driven by GPU IR output (first real frame with `frames>0`).
Existing: swapchain presentation path, both orientations, surface
plumbing. Nothing guest-side feeds it yet.

## M10 — First Real PS5 Software — **ABSENT**

Gate: a PS5-owned program (homebrew ELF or owned dump) runs end to
end: SELF → loader → memory → CPU → GNM/PM4 → GPU IR → Vulkan →
visible frame. Only after M1-M9 gates.

## M11 — Compatibility — **ABSENT**

Gate: per-title compatibility ledger with reproducible ratings from
logged runs; per-game workarounds carry their own evidence. Nothing
here may start before M10.

## Order and independence

```
M1 (device CPU gate) ──► M2 validate ──► M4 lifecycle ──► M10
M3 advances with M2/M4 tests
M5 ──► M6 ──► M7 gate ──► M8 ──► M9 frame        (host-side chain,
                                                  independent of M1)
M10 requires M1-M9 all PASS; M11 requires M10.
```

The GPU chain (M5-M7) is deliberately device-independent and is the
correct workstream while the M1 blocker is under investigation — the
two never block each other until M10.
