# agent-evidence-engine

A deterministic runtime evidence collection engine that prevents
"blind development" — the situation where an AI agent writes code,
sees `BUILD SUCCESSFUL`, and assumes the app works, while the user
sees a crash on their phone.

## Core principle

```
Probe  →  observes  →  Evidence  →  evaluated by  →  Policy  →  Verdict
                                                                    |
                                                                    +-- PASS
                                                                    +-- FAIL
                                                                    +-- INCONCLUSIVE
```

The AI agent **interprets** evidence and suggests fixes.
The engine **decides** the verdict.

## Pipeline

```
Source Code → Build → Install → Launch → Runtime Probes → Evidence → Policy
                                                                          |
                                                                          v
                                                                  agent-report.json
```

## Project contract

Each consumer project defines a YAML contract specifying:
- What the app is (package, activity, components)
- How to build it (gradle command, expected APK path)
- What probes to run (build, native-loader, vulkan, etc.)
- What constitutes failure (P0, P1, P2, P3 severity)
- What evidence to collect (logcat, native logs, probe results)

Example:
```yaml
version: 1
project:
  name: px5
  type: android-native-emulator
runtime:
  package: com.px5.emulator
  activity: com.px5.emulator.MainActivity
components: [android, native, fex, vulkan, ps5-loader]
probes: [build, native-loader, vulkan-dummy]
policy:
  fail_on: [P0, P1]
artifacts:
  logcat: true
  native_logs: true
  probe_results: true
```

## Probes

| Probe | What it validates |
|-------|-------------------|
| `build` | APK exists, correct ABI, native libraries present |
| `native-loader` | Native .so files exist in APK AND load at runtime |
| `vulkan-dummy` | vkCreateInstance + physical device enumeration |

Probes are separated from policy. A probe only collects evidence;
it does not decide pass/fail.

## agent-report.json

The primary output artifact. Compact, structured, machine-readable:

```json
{
  "schema_version": 1,
  "engine_version": "0.1.0",
  "status": "FAIL",
  "severity": "P1",
  "phase": "RUNTIME",
  "summary": "Native runtime initialization failed.",
  "findings": [
    {
      "id": "NATIVE-LOADER-001",
      "severity": "P1",
      "component": "native",
      "title": "Required native library was not loaded",
      "evidence": [
        "libfexcore.so exists in APK",
        "libfexcore.so is absent from process mappings"
      ]
    }
  ],
  "probes": {
    "build": "PASS",
    "native-loader": "FAIL",
    "vulkan-dummy": "PASS"
  },
  "agent_action": {
    "required": true,
    "instruction": "Investigate native library loading and Android linker dependencies."
  }
}
```

## Roadmap

- **v1** — Evidence collection (build + native + vulkan probes, agent-report.json)
- **v2** — Regression history (SQLite, fingerprinting, "last known good" detection)
- **v3** — Governance (PR policy, severity enforcement, request-more-evidence)

## License

MIT
