# PX5 Architecture

> M0 contract. This document fixes the layer boundaries and the ownership
> of every component. Nothing in this repository may claim a layer is
> "implemented" unless the milestone evidence in `docs/milestones.md`
> says so. Guest architecture and host implementation are different
> axes and are never mixed.

## The two axes

```
GUEST (what we emulate)              HOST (what we run on)
  PS5 console                          Android ARM64
  x86-64 AMD Jaguar-derived CPU        Bionic libc, Linux kernel
  RDNA2-derived GPU (GCN packets)      Vulkan (Adreno proprietary / Turnip)
  GNM API + PM4 command stream         libadrenotools driver loading
  PS5 kernel (libkernel syscalls)      APK process, Compose UI shell
  PS5 ELF/SELF executables             APK assets, SAF storage
```

The cardinal rule: a guest concept (PM4 packet, SELF segment, PS5
syscall) must always cross a named PX5 layer before it becomes a host
concept (Vulkan command, host mmap, host function call). Code that
jumps from guest bytes straight to a host API call is a bug in the
architecture, even when it works.

## Stack and ownership

```
┌────────────────────────────────────────────────────────────┐
│ frontend / Android (Compose UI, settings, driver manager,  │
│ game library, diagnostics) — shell only, owns NO emulation │
├────────────────────────────────────────────────────────────┤
│ core/emulator — session orchestration, boot pipeline       │
├──────────┬──────────┬──────────────┬───────────────────────┤
│ loader   │ memory   │ cpu          │ kernel + syscalls     │
│ SELF →   │ guest    │ FEXCore IR/  │ syscall dispatcher,   │
│ ELF →    │ VA model,│ JIT, guest   │ NID gate, HLE:        │
│ segments,│ ranges,  │ threads,     │ implemented / partial │
│ relocs,  │ brk,     │ TLS/FSBASE,  │ / unsupported — each  │
│ imports  │ protect  │ ORBIS ABI    │ with its own test     │
├──────────┴──────────┴──────────────┴───────────────────────┤
│ gpu: PM4 decoder → GnmState → GPU IR → Vulkan backend      │
│      (driver_manager + libadrenotools = host DRIVER        │
│       infrastructure only — it is not the renderer)        │
├────────────────────────────────────────────────────────────┤
│ audio (guest model → Android track) · input (DualSense →   │
│ guest pad model) · videoout (guest framebuffer → Surface)  │
└────────────────────────────────────────────────────────────┘
```

### Ownership rules

| Concern | Owner | Explicitly NOT owned by |
|---|---|---|
| x86-64 → ARM64 translation | FEXCore (fetched, `.deps/FEX`) | PX5 kernel logic must never leak into FEXCore |
| PS5 execution environment | PX5 (`app/src/main/cpp/`) | FEXCore knows nothing about PS5 |
| Driver import / linker namespace | `gpu/driver_manager.cpp`, libadrenotools | It proves a driver MAPS; it does not render |
| GPU command semantics | `gpu/gnm/` decoder + GnmState | Not Vulkan code |
| Host GPU submission | `gpu/vulkan_device.cpp` | No PM4 knowledge allowed there |

## Data paths (the only legal flows)

```
CPU:  SELF file → loader → guest memory → FEXCore → ARM64 host
GPU:  guest PM4 stream → PM4 decoder → GnmState → PX5 GPU IR
        → Vulkan backend → Adreno → framebuffer → videoout → Surface
HLE:  guest libSce* call → NID gate (syscall trap) → HLE function
        → structured result → guest registers
```

## Current honest layer status (2026-09-06, vc51)

| Layer | Status | Evidence |
|---|---|---|
| Android frontend | working shell | daily device use |
| Driver import + namespace load | working on device | `driverVerified=yes`, Turnip v26.3.0-R4, 2026-09-05 screenshots |
| Host Vulkan init + swapchain + clear-submit proof | works on device | self-contained GPU proof PASS |
| PM4 decoder → GnmState | **M5 PASS** (12/12 packets, T1) | `tools/hosttests/pm4_stream_test.cpp` in `run.sh`; SET_SH_REG_OFFSET/DRAW_INDEX_2 semantics partial by design (see docs/gpu.md) |
| GPU IR | **M6 PASS** (5/5 ops, T1) | `tools/hosttests/gpu_ir_test.cpp` in `run.sh`; vocabulary committed in `gpu/ir/gpu_ir.h`, Vulkan-free by header guard |
| IR-driven Vulkan rendering | absent | `frames=0` in every session |
| SELF/ELF loader, runtime linker | real code, unvalidated past load | load path blocked behind CPU dispatch fault |
| Guest memory model | real code | `tools/hosttests/memory_range_test.cpp` |
| CPU execution (M1 gate) | **BLOCKED on device** | SIGSEGV si_addr=0x4 at `ExecuteThread`, in-process and isolated, since v1.15 (see `docs/cpu.md`) |
| Kernel HLE | foundation only | `sce_kernel_hle.cpp`, NID gate loud-fail policy |
| Audio / Shader recompiler / Compatibility | absent | honest "not implemented" labels in UI |

A user-visible feature that depends on an absent layer must say so in
its UI string. Diagnostics must never present host-infrastructure
success (driver verified, Vulkan device created) as guest-layer
progress.
