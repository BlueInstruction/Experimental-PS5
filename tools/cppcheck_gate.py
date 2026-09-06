#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Turn a cppcheck XML report into a real pass/fail gate.

Why not just --error-exitcode=1: that flag fires on every enabled severity,
including `style` and `information`, which makes the gate unusable on a
15k-line codebase and is precisely why the previous workflow ended every
cppcheck invocation with `|| true` and shipped a permanently green badge.

This gate is explicit instead: severity=error fails the build, everything
else is counted and printed. Raise the bar by moving severities from
INFORMATIONAL into BLOCKING once the existing findings are cleared.

Usage:
    cppcheck_gate.py build-reports/cppcheck.xml
"""
import sys
import xml.etree.ElementTree as ET
from collections import Counter

BLOCKING = {"error"}
INFORMATIONAL = {"warning", "style", "performance", "portability", "information"}


def main(argv):
    if len(argv) != 2:
        print("usage: cppcheck_gate.py <cppcheck.xml>")
        return 2
    path = argv[1]

    try:
        root = ET.parse(path).getroot()
    except FileNotFoundError:
        print(f"[FAIL] {path} not found - cppcheck did not produce a report")
        return 1
    except ET.ParseError as e:
        print(f"[FAIL] {path} is not valid XML: {e}")
        return 1

    counts = Counter()
    blocking = []
    for err in root.iter("error"):
        sev = err.get("severity", "unknown")
        counts[sev] += 1
        if sev in BLOCKING:
            loc = err.find("location")
            where = "unknown location"
            if loc is not None:
                where = f"{loc.get('file')}:{loc.get('line')}"
            blocking.append(f"  {where}: [{err.get('id')}] {err.get('msg')}")

    total = sum(counts.values())
    print(f"cppcheck findings: {total}")
    for sev in sorted(counts):
        mark = "BLOCKING" if sev in BLOCKING else "informational"
        print(f"  {sev:<12} {counts[sev]:>5}   ({mark})")

    if blocking:
        print(f"\n[FAIL] {len(blocking)} blocking finding(s) at severity=error:")
        for line in blocking:
            print(line)
        return 1

    print("\n[OK] no severity=error findings")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
