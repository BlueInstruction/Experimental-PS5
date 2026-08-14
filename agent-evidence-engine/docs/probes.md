# Probes

A probe is a Python class that inherits from `BaseProbe` and implements `run()`.

## Available probes

| Probe | Phase | What it validates |
|-------|-------|-------------------|
| `build` | BUILD | APK exists, correct ABI, native libraries in APK |
| `native-loader` | RUNTIME | .so files load at runtime (verified via /proc/pid/maps) |
| `vulkan-dummy` | RUNTIME | Vulkan instance + physical device enumeration |

## Writing a custom probe

```python
from probes import BaseProbe

class MyProbe(BaseProbe):
    PROBE_ID = "my-probe"
    COMPONENT = "my-component"

    def run(self, contract):
        evidence = []
        findings = []

        # Collect evidence
        evidence.append({
            "type": "file_exists",
            "key": "my_file",
            "value": True,
        })

        # Report findings
        if something_wrong:
            findings.append({
                "id": "MY-001",
                "severity": "P1",
                "title": "Something is wrong",
                "detail": "...",
            })

        status = "FAIL" if findings else "PASS"
        return {
            "probe_id": self.PROBE_ID,
            "status": status,
            "evidence": evidence,
            "findings": findings,
        }
```

## Severity levels

| Level | Meaning |
|-------|---------|
| P0 | Critical — blocks release |
| P1 | High — blocks PR merge |
| P2 | Medium — warning |
| P3 | Low — informational |
