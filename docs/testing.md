# Testing & Evidence Contracts

> M0 contract. A milestone moves when its evidence exists, not when
> its code exists. Every claim in this repository must be
> reproducible from a committed test or a logged device run.

## The three evidence tiers

| Tier | Where | Proves | Examples |
|---|---|---|---|
| T1 host tests | `tools/hosttests/run.sh` (no device needed) | logic correctness, ABI parity, deterministic decode | `import_trap_test`, `memory_range_test`, JNI parity (45/45 symbols), pm4 stream test (M5) |
| T2 device evidence | `px5_main.log` + event stream from a real run | behaviour on ARM64 hardware | driver preload via SHARED namespace, GPU proof PASS, crash reports with `pc=` |
| T3 readback/pixel proof | offscreen render + readback | the render path produces specific bytes | M7 gate, M8 gate |

T1 without T2 is "logic is right". T2 without T3 is "it runs" — never
"it renders". Only T3 turns GPU claims into facts.

## Milestone PASS format (mandatory shape)

Every milestone completion is reported in this exact form, so the
repository stays auditable instead of becoming a set of claims. The
block below is an ILLUSTRATIVE example of the format only — none of
its lines is current status (M1, M5 and M7 are all unmet today; see
`docs/milestones.md`):

```
M1 PASS
FEXCore:
    initialized
    x86-64 fixture executed (test_x86_basic)
    result = 42
    exit = 42

M5 PASS
PM4:
    37 packets decoded
    37/37 expected opcodes
    0 unexpected stream errors

M7 PASS
Vulkan:
    device created (Adreno 750 / Turnip v26.3.0-R4)
    queue submitted
    framebuffer readback PASS (clear=red 1116x2480)
```

Numbers or it did not happen. `works`, `improved`, `fixed` are not
evidence.

## Commit convention

Every commit = implementation + test + verification, titled by
milestone-verifiable scope:

```
cpu: execute deterministic x86-64 fixture through FEXCore
loader: map ELF PT_LOAD segments into guest memory
memory: add guest virtual memory protection model
kernel: add guest thread lifecycle
gpu: decode PM4 type-3 packets
gpu: introduce command IR
vulkan: execute GPU IR clear command
shader: add PS5 shader instruction decoder
```

Forbidden: `fix emulator`, `update gpu`, `improve cpu`, or any commit
that changes behaviour without touching a test or producing a logged
verification.

## Component test registry

| Component | Test | Tier | Status |
|---|---|---|---|
| SELF container extract | `runtime_linker_selftest` / `self_extract_selftest` | T1 | exists |
| Runtime linker + NID gate | `import_trap_test.cpp` | T1 | exists, PASS |
| Guest memory ranges | `memory_range_test.cpp` | T1 | exists, PASS |
| JNI symbol-name parity | source-level name check, 45 symbols × 2 ABIs (`tools/check_jni_symbols.py`) | T1 | exists, PASS — names only; signatures and built-library exports are NOT checked here |
| PM4 stream decode | `pm4_stream_test.cpp` | T1 | exists, PASS (12/12 packets, 0 stream errors) |
| CPU fixtures (9) | `docs/cpu.md` table | T1+T2 | M1 deliverable — BLOCKED on device (see cpu.md) |
| Vulkan readback | self-contained proof → readback | T2→T3 | proof exists; readback = M7 deliverable |
| Shader compile | reference pattern compare | T3 | M8 deliverable |

## Rules

1. No component appears "implemented" in UI or docs before its gate
   passes with logged evidence.
2. HLE functions carry one of three labels — implemented / partial /
   unsupported — each with its own test. No mass stubs; unknown NIDs
   fail loudly (existing policy in `runtime_linker.cpp`).
3. Device sessions produce evidence, not anecdotes: every diagnostic
   line must name its mechanism (per-mechanism driver resolution,
   per-stage heartbeat, evidence-first crash dumps — all existing
   patterns to keep).
4. A layer may advance on T1 alone only when its integration is
   host-side by design AND its gate does not demand more: M5-M6 only.
   M7 requires on-device T2 evidence and its T3 readback gate; M8's
   decoder is host-side but its gate requires T3 readback too. Each
   milestone entry must say which tiers its own gate names.
