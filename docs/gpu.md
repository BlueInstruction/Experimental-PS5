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
GnmState (register banks, index type, instances, draw/dispatch records,
          event-seq timeline, named-write journal)
  ↓  gnm_submit.cpp (per submission, since the submit-path wiring)      
  │      — LowerGnmStateToIR (M6) then PlanVulkanCommands accounting (M7)
  ↓
gpu/ir/gpu_ir.h ops (M6, PASS)                 shader lane (M8, GATED):
SetRenderTarget / SetViewport / SetScissor /    GnmState SH banks
BindResource / Draw / DrawIndexed / Dispatch /    ↓  shader_capture.cpp (T4)
CopyImage / Clear / Barrier                     guest RDNA dwords + declared ABI
  ↓  vulkan_backend (M7)     — render ops → Vulkan plan
                                                ↓  shader_recompiler_service.cpp (T3)
VkInstance → VkPhysicalDevice → VkDevice → VkQueue → images → submit
                                                vendored recompiler → SPIR-V
  ↓
Adreno (proprietary ICD or imported Turnip) → framebuffer
  ↓  videoout (M9)
Android Surface
```

Naming note: the M6 op list lives in `gpu/ir/gpu_ir.h` (`GpuOp`,
`OpKind`, `GpuOpList`, `LowerGnmStateToIR`). The vocabulary is COMMITTED
and the M7 backend is built against these types; since the submit-path
wiring, real submissions lower their state to this IR and run the M7
planner over it for per-kind accounting (nothing executes the plan in
the runtime path yet; `frames=0` stands). One addition to the earlier
draft list: `DrawIndexed` is its own op (GnmState records indexed draws
via DRAW_INDEX_2; a single "Draw" would erase a real distinction the
guest stream carries). A header-level guard rejects any Vulkan include
in the IR definition.

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
  `WriteRegister/ReadRegister` (written-bit tracking), draw and dispatch
  records, `Reset`. Since M6: every record carries an event-sequence
  number (`seq`) stamped at record time and shared across draws,
  dispatches and the named-write journal, so the IR lowering restores
  the guest's event order exactly; dispatches are journaled (bounded
  log, most recent last), not just last-dims-wins.
- `gpu/ir/gpu_ir.cpp` (M6): the lowering `LowerGnmStateToIR` — GnmState
  → ordered, bounded `GpuOp` list. Emits `SetScissor` (from the
  journaled PA_SC_SCREEN_SCISSOR_TL/BR writes — public PS4/PS5 RE
  consensus: context packet offsets 0xC/0xD, X[15:0] Y[31:16]; Kyty and
  RPCSX agree), `Draw`/`DrawIndexed` (draw records, payload verbatim),
  `Dispatch` (dispatch records), and exactly one submit-boundary
  `Barrier` when the list is non-empty. Register writes with no named
  semantics are COUNTED (`LowerStats.unmappedRegisterWrites`), never
  turned into guessed ops; INDEX_TYPE writes are accounted as carried
  (the value lives inside every draw payload). The remaining vocabulary
  ops (SetRenderTarget, SetViewport, BindPipeline, BindResource,
  CopyImage, Clear) are committed types with no emitter until the
  decoder deepens named-register semantics — the M5 PARTIAL note below
  is the same policy.
- `gpu/vulkan_backend.h/.cpp` (M7): the layer between the GPU IR and
  Vulkan — `PlanVulkanCommands` turns a `GpuOpList` into an ordered,
  Vulkan-free command plan (barrier → clear → submit boundary); one
  float→UNORM-8 rule (`ClearFloatToUnorm8`) fixes the exact bytes the
  readback compares; `VerifyClearReadback` is the exact-byte pixel
  checker. Ops whose materialization needs pipelines (scissor/draw/
  drawIndexed/dispatch) are DEFERRED per kind and counted — never
  guessed into commands. Locked on host by
  `tools/hosttests/vulkan_backend_test.cpp` (M7-T1). The planner TU
  never includes Vulkan headers; `gpu/vulkan_device.cpp` is the one
  sanctioned Vulkan-seeing IR consumer (PX5_GPU_BACKEND_CONSUMER).
- `gpu/gnm/gnm_submit.cpp` (submit-path wiring): after every decoded
  buffer the REAL submission path (what `sceGnmSubmitCommandBuffers`
  serves) walks the chain that ends at Vulkan — accounting only:
  `LowerGnmStateToIR` over the cumulative state, `PlanVulkanCommands`
  for per-kind deferral counts, and `ShaderCapture::CaptureAndRecompile`
  for each stage the stream programmed (memoized per program vaddr;
  bounded retries on unreadable code; outcomes in a bounded ring and in
  `GetStatsString()`). Locked by
  `tools/hosttests/gnm_submit_gpu_path_test.cpp`. Binaries without the
  shader service (x86_64 stub, HLE-only host tests) compile the capture
  leg down to a named skip (`PX5_NO_SHADER_SERVICE`).
- `gpu/vulkan_device.cpp`: REAL host Vulkan — instance/device init on
  Adreno 750, swapchain in both orientations, self-contained
  fork-safe clear-submit proof (own render node fd, full teardown),
  the M7 clear-readback proof (synthetic one-Clear IR list, step 8b)
  and the M8 compute dispatch proof (synthetic fixture shader →
  SPIR-V → `VkShaderModule` → compute pipeline → `vkCmdDispatch` →
  readback, JNI-triggered). The shader-object layer
  (`CreateShaderModuleFromWords`, `CreateComputePipelineFromReflection`,
  `CreateGraphicsPipelineFromReflection` with a real
  `vkCreateGraphicsPipelines` call) exists but has no caller yet —
  no guest SPIR-V reaches it in the runtime path.
- `gpu/driver_manager.cpp` + libadrenotools: driver slot import,
  SHARED linker-namespace load, /proc/self/maps verification
  (`driverVerified=yes` on Turnip v26.3.0-R4, 2026-09-05). Host
  plumbing only.

## What does NOT exist (do not fake it)

- IR-driven rendering: nothing turns a decoded draw into VkPipeline +
  vkCmdDraw. `vkCmdDraw`/`vkCmdDrawIndexed` appear nowhere in app/src;
  the M7 planner defers draw/drawIndexed/dispatch/scissor ops by kind
  (counted, never guessed), and the graphics-pipeline layer has no
  runtime caller. Any framebuffer content today is host-clear only.
- IR ops without evidence: SetRenderTarget / SetViewport / BindPipeline /
  BindResource / CopyImage / Clear exist as committed TYPES only. The
  lowering emits none of them, because no named register semantics back
  them yet — emitting them from raw bank dwords would be inventing
  semantics, which this layer forbids.
- Guest shaders through the pipeline: the M8 stack is host-gated and
  the submit path captures what the guest programs — but every SPIR-V
  so far comes from FIXTURE programs; no guest game shader has been
  recompiled, no guest SPIR-V has become a `VkShaderModule`, and no
  device session has logged one. The on-device dispatch proof runs a
  synthetic compute shader.
- Guest SPIR-V → VkShaderModule in the runtime path: the device layer's
  shader-object APIs exist and are CI-compiled, but nothing calls them
  with captured guest SPIR-V yet (that connection is the next link
  after a device session proves the capture leg on real guest data).

## Host GPU design constraints (external evidence, 2026-10)

- Target GPUs are TILE-BASED (Adreno GMEM/TBDR), not immediate-mode.
  PC-proven PS5 ports (AnyPS5 lineage: Dreaming Sarah, Dead Cells
  PPSA15552, Ghost of Yotei PPSA26344) were validated only on
  immediate-mode hosts (GTX 1050 Ti, RTX 5070 Ti, Intel HD 620). When
  the M7/M8 lowering adds barriers and renderpass structure, design
  for GMEM locality (subpass-local loads/stores) instead of assuming
  desktop-style full-frame barriers — otherwise Turnip falls to the
  sysmem path and LRZ stops early.
- RDNA wave64 is a real semantic, not a size hint: upstream AnyPS5
  fixes from Oct 2026 (wave64 body assembly, half-wave reductions)
  show the M8 shader IR must model wave size explicitly and lower
  reductions across both wave64 and host subgroup widths.

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

## M6 evidence gate (GPU IR) — **MET**

Host-side, deterministic (docs/testing.md: M5-M6 may pass on T1 alone —
the lowering's integration is host-side by design): a fixed PM4 stream
decodes into GnmState, then `LowerGnmStateToIR` lowers that state to the
EXACT expected op sequence — kinds, order, payloads, provenance
(seq/packetOffset), honesty counters and bounded-capacity behavior all
asserted.

Status: `tools/hosttests/gpu_ir_test.cpp` IS that gate and it passes —
it prints the mandated five-line PASS block (docs/testing.md shape):
`M6 PASS` / `GPU IR:` / `5 ops lowered` / `5/5 expected ops` /
`0 unexpected lowering drops` (wired into `tools/hosttests/run.sh`).
The gate locks, among the rest:
SetScissor box decode (X[15:0] Y[31:16] of the TL/BR pair), mid-stream
state-change interleaving between draws (the seq timeline), payload
verbatim-carry (count/instances/indexTypeRaw/initiatorRaw) for
Draw/DrawIndexed/Dispatch, one submit-boundary
Barrier only on a non-empty list (seq = last EMITTED op),
unmapped-write accounting (4/7 writes
lower to nothing and say so), journal-eviction pairing (a BR write whose
TL entry already fell out of the bounded journal still lowers the
correct box), and
drop-counting on a full list.

## M7 evidence gate (Vulkan backend, on device)

```
create instance → enumerate physical device → create device
→ create queue → allocate image → clear image → submit → fence
→ readback → expected pixels
```

This replaces `driverVerified=yes` as the GPU truth marker: pixels
read back from a render target are evidence; a mapped driver and a
created device are not.

What exists now (v1.54): the chain is WIRED end to end —
`VulkanGpuDevice::RunM7ClearReadbackProof` (self-contained, fork-safe,
fresh instance/device) builds a synthetic one-Clear `GpuOpList`
(labelled in its own detail line: the M6 lowering emits no Clear yet,
so this proves the BACKEND chain, not the decoder), plans it with the
M7-T1-locked planner, executes the plan on real Vulkan, copies the
image to a host-visible buffer, and verifies every byte against
`plan.clearRgba` (clear = red 64×64, expected 255,0,0,255 — the
docs/testing.md M7 shape) with a SHA-256 of the readback. The proof
runs as foundation-suite step 8b. What is MISSING is the device run:
until a real session logs `pixels 4096/4096 match`, the gate is unmet
and M7 stays GATED. The gate run also doubles as the Vulkan capability
record (measured device/driver/api/vendor/deviceID in every detail
line — never a requirement).

## M8 evidence gate (shader recompiler) — **GATED**

A known PS5 shader binary decodes to IR whose SPIR-V, compiled and
executed on device, reproduces a reference triangle/test pattern in an
offscreen render target (readback-compared). Research references for
system structure: KytyPS5, shadPS4 — architecture is understood, not
copied; ownership of every layer stays in this document's terms.

What exists now (post-merge + submit-path wiring): the recompiler is
vendored and compiles into the arm64 APK; T1-T7 host gates cover
fixture shaders through the full pipeline (RDNA → SPIR-V), the app
service, capture from real PM4 SET_SH streams, reflection vocabulary,
the graphics-pipeline-desc validator and the dispatch-proof contract;
the submit path lowers/plans/captures per submission (see the wiring
bullet above). What is MISSING for the gate: a device session feeding a
GUEST shader binary through capture → SPIR-V → VkShaderModule →
pipeline → executed → readback-verified output. Fixture and synthetic
proofs do not satisfy this gate.

Known pre-existing property (named, not new): the vendored recompiler
keeps thread-local IR/SSA pools whose output size varies with
allocation addresses — the T1 warm/cold window was recalibrated to
±512 after observing a +378-word delta on a g++13 host (structural
checks passed in every run); see
`tools/hosttests/shader_recompiler_test.cpp`.

## Milestone order (why command-processor first)

PM4 decode → GPU IR → Vulkan clears/draws offscreen → THEN shaders.
Once the command path exists without shaders, it is DESIGNED to yield
the first real GPU evidence gates — clear = red, triangle from a
host-built pipeline = green, test texture = known pattern, each
readback-compared (these are the planned M7/M8 evidence gates, NOT
current results — today's code clears an image and nothing more). That
de-risks the hardest integration (Adreno + Turnip behaviour) before
shader complexity stacks on top.
