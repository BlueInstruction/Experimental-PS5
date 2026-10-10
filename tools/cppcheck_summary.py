#!/usr/bin/env python3
"""Render a compact, machine-readable cppcheck summary from an XML v2 report.

Usage: cppcheck_summary.py <cppcheck.xml> [detail_per_id]

The workflow tees stdout into GITHUB_STEP_SUMMARY (human-visible on the run
page) and the same text lands in this step's job log, so an agent with repo
read access can parse findings via the Actions API without downloading the
full artifact.

Layout: totals -> by severity -> id histogram -> full list for
error/warning -> bounded sample for style/performance/portability.
Pass/fail is decided by tools/cppcheck_gate.py, never by this summary.
"""
import sys
import xml.etree.ElementTree as ET
from collections import Counter

DETAIL_SEVERITIES = ("error", "warning")   # listed in full
SAMPLE_PER_ID = 3                          # for the remaining severities


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: cppcheck_summary.py <cppcheck.xml> [sample_per_id]",
              file=sys.stderr)
        return 2
    path = sys.argv[1]
    sample_per_id = int(sys.argv[2]) if len(sys.argv) > 2 else SAMPLE_PER_ID

    try:
        tree = ET.parse(path)
    except (OSError, ET.ParseError) as exc:
        print(f"cppcheck summary: cannot read {path}: {exc}")
        return 2

    errors = tree.findall(".//error")
    by_sev: Counter = Counter(e.get("severity", "?") for e in errors)
    by_id: Counter = Counter(e.get("id", "?") for e in errors)

    def loc(e) -> str:
        node = e.find("location")
        f = (node.get("file") if node is not None else None) or e.get("file") or "?"
        l = (node.get("line") if node is not None else None) or e.get("line") or "?"
        return f"{f.split('/app/src/main/cpp/')[-1]}:{l}"

    print("## cppcheck findings")
    print()
    print(f"Total findings: {len(errors)}")
    print()
    print("By severity:")
    for sev in ("error", "warning", "performance", "portability", "style", "information", "debug"):
        if by_sev.get(sev):
            print(f"- {sev}: {by_sev[sev]}")
    for sev, n in by_sev.most_common():
        if sev not in ("error", "warning", "performance", "portability", "style", "information", "debug"):
            print(f"- {sev}: {n}")
    print()
    print("Top finding ids:")
    for fid, n in by_id.most_common(12):
        print(f"- {fid}: {n}")
    print()

    detail = [e for e in errors if e.get("severity") in DETAIL_SEVERITIES]
    if detail:
        print(f"{'+'.join(DETAIL_SEVERITIES)} findings (full list):")
        for e in detail:
            msg = (e.get("msg") or "").replace("\n", " ")[:200]
            print(f"- [{e.get('severity')}|{e.get('id')}|{loc(e)}] {msg}")
        print()

    rest = [e for e in errors if e.get("severity") not in DETAIL_SEVERITIES]
    if rest:
        shown: Counter = Counter()
        lines = []
        for e in rest:
            fid = e.get("id", "?")
            if shown[fid] >= sample_per_id:
                continue
            shown[fid] += 1
            msg = (e.get("msg") or "").replace("\n", " ")[:140]
            lines.append(f"- [{e.get('severity')}|{fid}|{loc(e)}] {msg}")
        print(f"Sample of remaining findings ({sample_per_id} per id; "
              f"see artifact cppcheck.xml for the full set):")
        for line in lines[:48]:
            print(line)
    return 0


if __name__ == "__main__":
    sys.exit(main())
