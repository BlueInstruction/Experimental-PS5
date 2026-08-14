# Evidence Model

Evidence is structured data collected by probes. Each piece of evidence has:

- `type` — what kind of evidence (file_exists, process_mapped, logcat_line, etc.)
- `key` — what the evidence is about (e.g., "libfexcore.so")
- `value` — the observed value (true/false, string, number)
- `source` — where the evidence came from (file path, /proc/pid/maps, logcat)
- `timestamp` — when it was collected

Evidence is NOT pass/fail. It's raw observation. The Policy engine decides
what the evidence means.
