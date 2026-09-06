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
and isolated, since v1.15. v1.24 symbolized that crash
(`pc=libpx5.so+0x3809e4`, null GDT/LDT segment arrays) and fixed it;
v1.25 (IS64BIT_MODE), v1.40 (FSBASE before entry) and v1.46 (ORBIS
entry ABI) each exposed the next stage. Next step: re-run the fixture
suite on device (vc48+) and resolve any new fault with
`tools/symbolize_px5.py` + CI symbols artifact.
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

## M5 — PM4 Decoder — **PASS** (pm4_stream_test.cpp, T1)

Gate: fixed PM4 test stream decodes to the exact expected structured
sequence (`tools/hosttests/pm4_stream_test.cpp`) — counts, opcodes,
register banks, draw/dispatch records. Evidence: `M5 PASS — 12 packets
decoded, 12/12 expected opcodes, 0 unexpected stream errors`
(deterministic, host-side; wired into `tools/hosttests/run.sh`).
Existing: type-3 decode, 11 packet semantics with GnmState writes
(SET_SH_REG_OFFSET discards its address pair; DRAW_INDEX_2 records
count/initiator only — both partial by design), unknown-opcode
accounting, bounded stream errors. The gate run also caught and fixed
a real GnmState defect: the CONFIG bank size overlapped the SH range,
so every SH write was rerouted into the CONFIG bank.

## M6 — GPU IR — **PASS** (gpu_ir_test.cpp, T1)

Gate: GnmState + draw records lower to a committed IR op list
(SetRenderTarget … Barrier) with a lower-to-IR host test. No Vulkan
types may appear in IR definitions (keeps GNM decoupled from Vulkan).
Status: `gpu/ir/gpu_ir.h` commits the full op vocabulary (SetRenderTarget,
SetViewport, SetScissor, BindPipeline, BindResource, Draw, DrawIndexed,
Dispatch, CopyImage, Clear, Barrier — plain POD payloads, header guard
rejects Vulkan includes) and `gpu/ir/gpu_ir.cpp` lowers
`LowerGnmStateToIR` over GnmState's event-seq timeline. Evidence:
`M6 PASS — 5 ops lowered, 5/5 expected ops, 0 unexpected lowering drops`
(deterministic, host-side; wired into `tools/hosttests/run.sh`). Honest
scope: the lowering emits SetScissor / Draw / DrawIndexed / Dispatch /
Barrier — the ops the state model can back with named semantics; the
remaining vocabulary ops are committed types with no emitter until the
decoder deepens named-register semantics (same policy as M5's PARTIAL
notes), and unmapped register writes are counted, never guessed into
ops. The M6 gate run also deepened the state model it lowers from:
draws/dispatches/named writes now share an event-seq stamp, dispatches
are journaled (not last-wins), and the scissor pair (PA_SC_SCREEN_
SCISSOR_TL/BR, context offsets 0xC/0xD per Kyty + RPCSX) is journaled
by name with eviction-safe pairing (a BR record carries its write-time
TL value, so TL eviction from the bounded journal cannot corrupt the
lowered box for as long as the BR record itself remains retained; a BR
record that has itself fallen out of the bounded journal lowers
nothing, like any record past the window) plus eviction-proof
cumulative named/carried counters.

## M7 — Vulkan Backend — **GATED** (T1 core locked, device gate run pending)

Gate: the M7 chain — instance → device → queue → image → clear →
submit → fence → READBACK with expected pixels — passes on device
(T3). Ahead of the gate (NOT the gate): real instance/device/swapchain
on Adreno 750, fork-safe self-contained clear-submit proof, and now
(v1.53) the backend layer itself: `gpu/vulkan_backend.h/.cpp` plans a
`GpuOpList` into an ordered Vulkan command sequence (barrier → clear →
submit boundary; pipeline-needing ops deferred by kind, counted, never
guessed), locked on host by `tools/hosttests/vulkan_backend_test.cpp`
(M7-T1 — includes the float→UNORM byte rule the readback compares
against), and `VulkanGpuDevice::RunM7ClearReadbackProof` wires the full
chain — synthetic one-Clear IR list (labelled: proves the BACKEND, not
the decoder) → planner → submit → fence → `vkCmdCopyImageToBuffer` →
exact-pixel verify + SHA-256 — into the foundation suite (step 8b).
The gate run itself needs the device: until a real session logs
`pixels 4096/4096 match` from that proof, M7 stays GATED. `frames=0`
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
M5 ──► M6 (host-side) ──► M7 gate (ON DEVICE, T3) ──► M8 ──► M9 frame
M10 requires M1-M9 all PASS; M11 requires M10.
```

The GPU chain (M5-M7) is deliberately independent of the M1 CPU
blocker — M5 and M6 run entirely host-side, while M7's readback gate
still requires the device. This is the correct workstream while M1 is
under investigation; the two never block each other until M10.
