# Changelog

## [0.1.0] — 2026-08-14

### Added
- Project contract schema (YAML)
- Build probe (APK validation)
- Native loader probe (.so file existence + runtime loading)
- Vulkan dummy probe (instance creation + device enumeration)
- Evidence model (structured probe results)
- Policy engine (PASS/FAIL/INCONCLUSIVE based on severity)
- Agent report generator (compact JSON output)
- CLI entry point (`aee` command)

### Design principles
- Probes collect evidence; they do NOT decide pass/fail
- Policy evaluates evidence; it does NOT collect evidence
- The AI agent interprets reports; it does NOT override verdicts
