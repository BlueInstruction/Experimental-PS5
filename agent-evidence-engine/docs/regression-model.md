# Regression Model (v2)

v2 adds a SQLite database that stores probe results per commit:

```
evidence-history/
└── evidence.db
```

When a new run completes, the engine compares results to the previous run:

```
REGRESSION DETECTED

Component: native
Last known good: commit A (2026-08-13 10:00)
First known bad: commit B (2026-08-13 11:00)

Changed probes:
  native-loader: PASS → FAIL
```

This transforms debugging from "something is broken" to "native-loader was
PASS in commit A and FAIL in commit B".
