# GPU: PM4 → GPU IR → Vulkan

> M5-M9 contract. The GPU side is built command-processor-first, not
> shader-first, and never GNM→Vulkan directly. `driverVerified=yes`
> proves a driver library MAPS — it is host infrastructure evidence
> and is never cited as renderer progress.

## Layer chain (each owns one transformation)

```
guest PM4 stream (dwords in guest memory)
  ↓  pm4_decoder.cpp        — bytes → structured packets
PacketRecord trace (bounded)  +  GnmState  ← current, existing types
  ↓  gnm_state.cpp          — packets → device state
GnmState (register banks, index type, instances, draw/dispatch records)
  ↓  GPU IR (M6, to be built — planned types: GpuCommand/GpuState-shaped
     render ops)          — state + records → render ops
SetRenderTarget / SetViewport / SetScissor / BindPipeline /
BindResource / Draw / Dispatch / CopyImage / Clear / Barrier
  ↓  vulkan_backend (M7)     — render ops → Vulkan
VkInstance → VkPhysicalDevice → VkDevice → VkQueue → images → submit
  ↓
Adreno (proprietary ICD or imported Turnip) → framebuffer
  ↓  videoout (M9)
Android Surface
```

Naming note: `GpuCommand { opcode, payload }` in earlier drafts of this
diagram is a PLANNED M6 abstraction. Today the decoder writes
`GnmState` and an optional bounded `PacketRecord` trace — those are the
only GPU types that exist; do not code against the planned names yet.

## What exists today (honest)

- `gpu/gnm/pm4_packet.h` + `pm4_decoder.cpp`: type-3 header decode,
  named opcodes, PARTIAL semantics for SET_CONFIG_REG /
  SET_CONTEXT_REG / SET_UCONFIG_REG / SET_SH_REG / SET_SH_REG_OFFSET,
  INDEX_TYPE, NUM_INSTANCES, DRAW_INDEX_AUTO, DRAW_INDEX_2,
  DISPATCH_DIRECT, NOP. Partial means: SET_SH_REG_OFFSET discards its
  address pair today, and DRAW_INDEX_2 records neither the
  index-buffer address nor the max-index field — the register and
  draw-record state IS written, the rest is deferred and must not be
  claimed as covered. Unknown opcodes are counted and body-skipped
  by length — never silently ignored. Errors land in
  `DecodeStats.streamErrors` (bounded error list with offsets).
- `gpu/gnm/gnm_state.cpp`: register banks with
  `WriteRegister/ReadRegister` (written-bit tracking), draw and
  dispatch records, `Reset`.
- `gpu/vulkan_device.cpp`: REAL host Vulkan — instance/device init on
  Adreno 750, swapchain in both orientations, self-contained
  fork-safe clear-submit proof (own render node fd, full teardown).
  This is M7 infrastructure, ahead of its milestone, with no IR
  feeding it yet (`frames=0` in every device session).
- `gpu/driver_manager.cpp` + libadrenotools: driver slot import,
  SHARED linker-namespace load, /proc/self/maps verification
  (`driverVerified=yes` on Turnip v26.3.0-R4, 2026-09-05). Host
  plumbing only.

## What does NOT exist (do not fake it)

- GPU IR: no intermediate representation between GpuState and Vulkan.
  The decoder currently stops at state records.
- IR-driven rendering: nothing turns a decoded draw into VkPipeline +
  vkCmdDraw. Any framebuffer content today is host-clear only.
- Shader recompiler: absent by design until M8 — PS5 shader binary →
  decoder → shader IR → SPIR-V. Starting shaders before the command
  path is the failure mode this plan forbids.

## M5 evidence gate (PM4 decoder) — **MET**

Host-side, deterministic: a fixed PM4 test stream with known packet
count and opcodes decodes to the exact expected structured sequence —
`N packets in, N/N records out, 0 unexpected stream errors`, register
banks hold the written values, draw records carry the right
count/indexed/instances fields.

Status: `tools/hosttests/pm4_stream_test.cpp` IS that gate and it
passes — `M5 PASS: 12 packets decoded, 12/12 expected opcodes, 0
unexpected stream errors` (wired into `tools/hosttests/run.sh`). The
gate run exposed two real defects that are now fixed: the
SET_SH_REG_OFFSET handler discarded only ONE address dword instead of
the pair, and GnmState's CONFIG bank range overlapped the entire SH
range, silently rerouting every SH write into CONFIG.

## M7 evidence gate (Vulkan backend, on device)

```
create instance → enumerate physical device → create device
→ create queue → allocate image → clear image → submit → fence
→ readback → expected pixels
```

This replaces `driverVerified=yes` as the GPU truth marker: pixels
read back from a render target are evidence; a mapped driver and a
created device are not.

## M8 evidence gate (shader recompiler)

A known PS5 shader binary decodes to IR whose SPIR-V, compiled and
executed on device, reproduces a reference triangle/test pattern in an
offscreen render target (readback-compared). Research references for
system structure: KytyPS5, shadPS4 — architecture is understood, not
copied; ownership of every layer stays in this document's terms.

## Milestone order (why command-processor first)

PM4 decode → GPU IR → Vulkan clears/draws offscreen → THEN shaders.
Once the command path exists without shaders, it is DESIGNED to yield
the first real GPU evidence gates — clear = red, triangle from a
host-built pipeline = green, test texture = known pattern, each
readback-compared (these are the planned M7/M8 evidence gates, NOT
current results — today's code clears an image and nothing more). That
de-risks the hardest integration (Adreno + Turnip behaviour) before
shader complexity stacks on top.
