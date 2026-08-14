# Architecture

## Three-layer separation

```
Probe  →  observes  →  Evidence  →  evaluated by  →  Policy  →  Verdict
```

1. **Probe** — collects structured evidence. Does NOT decide pass/fail.
2. **Evidence** — structured data (file_exists, process_mapped, logcat_line, etc.)
3. **Policy** — evaluates evidence against the contract's fail_on rules.

The AI agent is OUTSIDE this chain:
```
Agent → interprets → agent-report.json → suggests fixes
```

The agent CANNOT override a FAIL verdict by saying "the code looks correct".

## Pipeline

```
Source → Build → Install → Launch → Probes → Evidence → Policy → Report
```

Each phase is independently probeable. A failure in any phase produces
evidence that the agent can use to diagnose the root cause.
