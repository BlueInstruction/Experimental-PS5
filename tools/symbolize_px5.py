#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Resolve PX5 crash-report addresses against the unstripped native library.

PX5 crash reports (and the FEX fault logs) carry module-relative PCs in
the form::

    SIGSEGV si_addr=0x4 pc=libpx5.so+0x3809e4

Those offsets mean nothing without the matching UNSTRIPPED binary. This
tool resolves them with llvm-symbolizer (NDK) or binutils addr2line
against the `px5-native-symbols-arm64` CI artifact (unstripped
libpx5.so), so a device fault becomes a named function and line instead
of a hex rumour.

Usage:
  tools/symbolize_px5.py --bin <libpx5.so> pc=libpx5.so+0x3809e4
  tools/symbolize_px5.py --bin <libpx5.so> libpx5.so+0x3809e4 0x380a00
  tools/symbolize_px5.py --bin <libpx5.so> --file px5_main.log
  px5_main.log | tools/symbolize_px5.py --bin <libpx5.so> --stdin

Every input token must name the module the binary belongs to (checked by
basename, override with --force). Missing tools or binary are hard
errors — this script never prints a guess.
"""

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

# module+0xoffset | module!0xoffset | bare 0xoffset (requires --force)
TOKEN_RE = re.compile(
    r"\b([A-Za-z0-9_.+-]+)?(?:\+|!)(0x[0-9a-fA-F]+)\b|\b(0x[0-9a-fA-F]+)\b"
)


def find_symbolizer(explicit: str | None) -> list[str]:
    """Return the command prefix for llvm-symbolizer or addr2line."""
    if explicit:
        p = Path(explicit)
        if not p.is_file():
            sys.exit(f"[FAIL] --symbolizer not a file: {p}")
        return [str(p)]
    for name, args in (("llvm-symbolizer", ["--obj={bin}", "--functions=linkage",
                                            "--inlines", "--demangle"]),
                       ("addr2line", ["-f", "-C", "-e", "{bin}"])):
        path = shutil.which(name)
        if path:
            return [path] + args
    # NDK layout is common enough to search directly when on PATH-less hosts
    ndk = Path.home() / "Android/Sdk/ndk"
    if ndk.is_dir():
        for cand in sorted(ndk.glob("*/toolchains/llvm/prebuilt/*/bin/llvm-symbolizer")):
            return [str(cand), "--obj={bin}", "--functions=linkage",
                    "--inlines", "--demangle"]
    sys.exit("[FAIL] no llvm-symbolizer or addr2line on PATH and no NDK found — "
             "install binutils or pass --symbolizer")


def resolve(cmd: list[str], bin_path: Path, offset: int) -> str:
    """Run the symbolizer for one offset; return multi-line text."""
    argv = [c.replace("{bin}", str(bin_path)) if "{bin}" in c else c
            for c in cmd]
    argv.append(hex(offset))
    r = subprocess.run(argv, capture_output=True, text=True, timeout=30)
    if r.returncode != 0:
        return f"  <symbolizer exit {r.returncode}: {r.stderr.strip()}>"
    lines = [ln.strip() for ln in r.stdout.splitlines() if ln.strip()]
    # llvm-symbolizer prints function/loc pairs; addr2line prints fn then file:line
    frames: list[str] = []
    for i in range(0, len(lines) - 1, 2):
        frames.append(f"  {lines[i]} @ {lines[i + 1]}")
    if len(lines) % 2 == 1:
        frames.append(f"  {lines[-1]}")
    return "\n".join(frames) if frames else "  <no output>"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--bin", required=True,
                    help="unstripped module binary (CI artifact px5-native-symbols-arm64)")
    ap.add_argument("--symbolizer", default=None,
                    help="explicit llvm-symbolizer/addr2line path")
    ap.add_argument("--force", action="store_true",
                    help="accept bare 0x… offsets and any module name")
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument("tokens", nargs="*", help="pc=lib.so+0x… / lib.so+0x… / 0x…")
    src.add_argument("--file", help="scan a log file for pc= tokens")
    src.add_argument("--stdin", action="store_true", help="scan stdin for pc= tokens")
    args = ap.parse_args()

    bin_path = Path(args.bin)
    if not bin_path.is_file():
        sys.exit(f"[FAIL] binary not found: {bin_path} "
                 f"(download the px5-native-symbols-arm64 CI artifact)")
    module = bin_path.name
    cmd = find_symbolizer(args.symbolizer)

    if args.file:
        text = Path(args.file).read_text(errors="replace")
    elif args.stdin:
        text = sys.stdin.read()
    else:
        text = "\n".join(args.tokens)

    found = False
    for m in TOKEN_RE.finditer(text):
        mod, off_hex, bare_hex = m.groups()
        if bare_hex and not args.force:
            continue
        offset = int(off_hex or bare_hex, 16)
        if mod and not args.force and mod != module:
            continue
        found = True
        print(f"{mod or module}+{hex(offset)}:")
        print(resolve(cmd, bin_path, offset))
    if not found:
        print("[FAIL] no matching module+offset token in input "
              "(use --force for bare offsets)")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
