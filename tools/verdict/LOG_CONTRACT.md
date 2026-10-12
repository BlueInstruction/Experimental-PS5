# PX5 log contract — what `px5_verdict.py` parses, and who emits it

`tools/verdict/px5_verdict.py` is the one-screen post-run verdict: it reads only
`px5_main.log`, `px5_evidence.log`, `px5_crash_latest.log` and decides whether a
REAL game is running (gates 1–5 must all pass; gates 6–7 are informational).
Exit code `0` = real game, `1` = not.

Division of labor with `tools/verify_evidence.py`:

| tool | question it answers | trust model |
|------|--------------------|-------------|
| `verify_evidence.py` | "is this ELF really the game?" | recomputes hashes from the user's own file — anti-fabrication |
| `verdict/px5_verdict.py` | "is a real game RUNNING right now?" | parses emitted telemetry lines; every number comes from a log line, nothing is guessed |

**The regexes below are a stable API between the emitter (C++ side) and the
analyzer (this script).** Changing an emitter line format is a contract break:
update both sides in the same commit, and bump the gate expectation in CI.

Verified against real device logs: v1.60 (`logs_v160`) and v1.61/v1.69 — the
verdicts matched the independent manual forensics (815 trap stubs, `hlt` stop,
ud2 SIGILL correctly attributed to the synthetic self-test, HLE symbol counts).

## Emitters ↔ gates

### Gate 1 — Image loaded
Source: `px5_main.log`, loader.
```
[GUEST] SELF load: OK
REL: applied=119961 unresolvedImports=815
```
Detail line prints `applied`.

### Gate 2 — Imports resolved
Source: same `REL:` line; detail appends `import-trap map FAILED` when the
trap table build failed (observed in v1.60).

⚠️ **Known semantic gap (to fix emitter-side):** in v1.61+ all 815 imports were
*trap-routed* (evidence line `import traps stubs=815 region=0x160000000`),
not raw-unresolved — the `REL:` line conflates the two. Until the emitter
splits them, gate 2 stays red even when every import is safely trapped.
v1.69 already added `trapped=815 abiResolved=370` to the REL line.
**Planned v1.71+ emitter:**
```
REL: applied=N unresolvedImports=U trapRouted=T dataMaterialized=D
```
Gate 2 then becomes `U == 0 && D == expected data-import count` (the v1.61
profile found 372 data imports: vtables/RTTI that must be real C++ ABI
objects, not code stubs).

### Gate 3 — Guest code runs
Source: the block **within 12 lines above the last `[GUEST] dispatch result`**
line of `px5_main.log`. The emitter must keep these three lines adjacent:
```
Guest execution finished in 7.54 ms | RAX=... | ...
exec: syscallRipRing seq=3 last=0x160001758 recent=[...] | syscalls total=3 handled=3
[GUEST] dispatch result: started=1 stopped_by=hlt guest_rip=0x1402e3305 ...
```
Pass = `ms >= 1000 && syscalls >= 100` (boot bar for a real guest; the
synthetic `exitCode=42` self-test runs elsewhere in the log and never lands in
this block — that is the point of anchoring on the *last* dispatch result).
`stopped_by` is reported verbatim (`hlt` today).

### Gate 4 — GPU draws
Source: `px5_evidence.log`, emitted by the Gnm/PM4 command-processor path once
a real submit carries draw packets:
```
REAL-GUEST 0e95ecd2 gpu-stats submits=2 dwords=1184 draws=3
```
v1.69 already emits the line with zeros (`submits=0 dwords=0 draws=0
dispatches=0 regWrites=0 …`) — the gate reads it, sees zeros, stays red
until real submits flow.

### Gate 5 — Frames presented
Source: `px5_main.log`, videoout:
```
Render loop stopped (frames=240)
```
Max across the run wins. `frames=None` (never presented) reads red.

### Gate 6 — Engine audio (informational)
Counts `px5_main.log` lines matching `^\[<ts>\] Audio…` or `cat=Audio`.
Note the script's own caveat: audible sound during a stuck boot is UI music,
not engine audio — this gate never counts toward the verdict.

### Gate 7 — Pad reads (informational, currently unprovable)
Needs a `pad_reads=N` counter from the pad HLE path. **No build emits it
yet** → always ⚠️. Planned v1.71+ emitter (PadState poll counter, same line
family as audio).

### Crash attribution footer
Source: `px5_crash_latest.log`:
```
signal=SIGILL(4) si_addr=0x7b80d81370 si_code=1 pid=... tid=...
guest bytes @rip: 0f 0b 00 00 ...
guest_state: rip=0x140400000 rsp=0x148000000 ...
```
The script classifies the crash as *synthetic self-test* when the guest bytes
at RIP are `0f 0b` (ud2) or RIP is the known self-test page `0x140400000`;
anything else prints "needs inspection". The `0x140400000` page constant is
part of the contract: move the ud2 self-test page and this heuristic must
move with it.

## Analyzer-side hygiene rules (already implemented)

- `[SYNTH]` lines are counted and displayed but never used as evidence.
- Only the three named log files are read; inside a zip, `/vfs/` paths and
  directories are skipped.
- `errors="replace"` everywhere: a torn UTF-8 tail must not kill the verdict.
- Exit code is CI-usable: `0` only when gates 1–5 are all True.

## Planned extensions (next tranche, do not improvise per-side)

1. `--json` output mode (machine-readable per-gate facts + matched line
   numbers) for the CI job that boots the synthetic guest.
2. Per-gate citation: print the exact source line (with line number) each
   number was parsed from, so a verdict is auditable without opening the log.
3. Emitter counters above (`dataMaterialized=`, `pad_reads=`).
4. CI wiring: build → hosttests → boot synthetic guest → `px5_verdict.py`
   must keep gate 1 + 3 green (loader/trap plumbing regression gate).
5. v1.71+ (per the SharpDroid baseline work): report the last real-guest
   `guest_rip` as a first-class fact so the T1 protocol (see BASELINE.md)
   is machine-checkable.
