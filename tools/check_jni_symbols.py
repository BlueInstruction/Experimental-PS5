#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Verify every native method declared in Java has a definition in BOTH ABIs.

FexCoreWrapper.java declares native methods; the arm64-v8a engine defines them
in cpp/fexcore_wrapper.cpp and the x86_64 UI-smoke library in
cpp/stub/ui_smoke_stub.cpp. The JVM resolves native methods when the class is
initialised, so a single missing symbol is an UnsatisfiedLinkError at runtime,
not a build error -- five of them shipped undetected.

Run offline, no toolchain needed. Non-zero exit on any mismatch.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
JAVA = ROOT / "app/src/main/java/com/px5/emulator/core/FexCoreWrapper.java"
NATIVE = ROOT / "app/src/main/cpp/fexcore_wrapper.cpp"
STUB = ROOT / "app/src/main/cpp/stub/ui_smoke_stub.cpp"

PREFIX = "Java_com_px5_emulator_core_FexCoreWrapper_"
DECL_RE = re.compile(r"\bpublic\s+(?:static\s+)?native\s+[\w<>\[\]., ]+?\s+(\w+)\s*\(")
DEF_RE = re.compile(re.escape(PREFIX) + r"(\w+)")


def declared():
    return set(DECL_RE.findall(JAVA.read_text()))


def defined(path):
    return set(DEF_RE.findall(path.read_text()))


def main():
    for p in (JAVA, NATIVE, STUB):
        if not p.exists():
            print(f"[FAIL] missing source file: {p}")
            return 1

    want = declared()
    if not want:
        print("[FAIL] parsed zero native declarations - regex out of date?")
        return 1

    failures = 0
    for label, path in (("arm64-v8a engine", NATIVE), ("x86_64 UI-smoke", STUB)):
        have = defined(path)
        missing = sorted(want - have)
        if missing:
            failures += 1
            print(f"[FAIL] {label} ({path.relative_to(ROOT)}) is missing "
                  f"{len(missing)} symbol(s) declared in FexCoreWrapper.java:")
            for m in missing:
                print(f"         {PREFIX}{m}")
        else:
            print(f"[OK  ] {label}: all {len(want)} declared methods defined")

    arm, stub = defined(NATIVE), defined(STUB)
    drift = sorted(arm ^ stub)
    if drift:
        failures += 1
        print(f"[FAIL] ABI symbol drift ({len(drift)} symbol(s) in one library "
              f"but not the other):")
        for d in drift:
            where = "arm64 only" if d in arm else "stub only"
            print(f"         {PREFIX}{d}  [{where}]")
    else:
        print(f"[OK  ] both ABIs export the same {len(arm)} JNI symbols")

    print(f"\n==== {'FAIL' if failures else 'PASS'}: "
          f"{len(want)} declared native methods checked ====")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
