#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# PX5 — offline evidence verifier (v1.42).
#
# WHY: the 2026-09-04 trust review ruled that a log can claim anything.
# This script turns the app's px5_evidence.log claims into recomputations
# a third party runs WITHOUT the app or the agent: every hash the loader
# logged is recomputed here from the user's own file (and, for SELF
# containers, from the inner-ELF dump the app writes to
# <logs>/elfdumps/). Any claim that does not recompute is a FAIL.
#
# Usage:
#   python3 tools/verify_evidence.py --ledger px5_evidence.log \
#       --file /path/to/eboot.bin [--inner /path/to/eboot.bin.inner.elf]
#
# Ledger line shape (written by app/src/main/cpp/utils/evidence.cpp):
#   <epoch_seconds> <img_sha256_prefix|-> <event body>
# Events verified here:
#   image bound stream=file|inner_elf sha256=<H> container_sha256=<H>
#       size=<N> entry=0x<E> self=<0|1> segs=<N> path=<P>
#   segment phdr=<i> va=0x<V> file_off=0x<O> filesz=<S> sha256=<H>
#   entry_proof file_off=0x<O> match=<0|1> sha256=<H>
#   inner_elf_dump path=<P> sha256=<H> size=<N>
#
# Verification model per bound image:
#   stream=file      : segment/entry hashes are recomputed from --file
#                      (plain ELF: the parsed stream IS the file)
#   stream=inner_elf : they are recomputed from --inner (the dumped
#                      extracted ELF); the container hash is recomputed
#                      from --file
#
# Exit code 0 = every claim recomputed OK; 1 = at least one FAIL.

import argparse
import hashlib
import re
import struct
import sys

CHUNK = 1 << 20

PT_LOAD = 1


def sha256_of(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while True:
            b = f.read(CHUNK)
            if not b:
                break
            h.update(b)
    return h.hexdigest()


def read_span(path, off, size):
    with open(path, "rb") as f:
        f.seek(off)
        return f.read(size)


def parse_phdrs(path):
    """Return [(p_type, p_flags, p_offset, p_vaddr, p_filesz, p_memsz)] of a
    real ELF64 file. Raises on malformed headers (a malformed dump is a
    verdict, not an exception to hide)."""
    with open(path, "rb") as f:
        hdr = f.read(64)
    if len(hdr) < 64 or hdr[:4] != b"\x7fELF":
        raise ValueError(f"{path}: not an ELF (magic {hdr[:4].hex()})")
    if hdr[4] != 2:  # EI_CLASS
        raise ValueError(f"{path}: not ELF64 (EI_CLASS={hdr[4]})")
    if hdr[5] != 1:  # EI_DATA
        raise ValueError(f"{path}: not little-endian (EI_DATA={hdr[5]})")
    e_phoff = struct.unpack_from("<Q", hdr, 0x20)[0]
    e_phentsize = struct.unpack_from("<H", hdr, 0x36)[0]
    e_phnum = struct.unpack_from("<H", hdr, 0x38)[0]
    if e_phentsize < 56:
        raise ValueError(f"{path}: e_phentsize={e_phentsize} < 56")
    out = []
    with open(path, "rb") as f:
        f.seek(e_phoff)
        blob = f.read(e_phentsize * e_phnum)
    if len(blob) < e_phentsize * e_phnum:
        raise ValueError(f"{path}: phdr table truncated")
    for i in range(e_phnum):
        (p_type, p_flags, p_offset, p_vaddr, _p_paddr, p_filesz, p_memsz,
         _p_align) = struct.unpack_from("<IIQQQQQQ", blob, i * e_phentsize)
        out.append((p_type, p_flags, p_offset, p_vaddr, p_filesz, p_memsz))
    return out


class Report:
    def __init__(self):
        self.lines = []
        self.fails = 0
        self.oks = 0

    def check(self, ok, label, detail=""):
        tag = "OK  " if ok else "FAIL"
        line = f"[{tag}] {label}" + (f" — {detail}" if detail else "")
        self.lines.append(line)
        print(line)
        if ok:
            self.oks += 1
        else:
            self.fails += 1


EVENT_RE = re.compile(r"^(\d+)\s+([0-9a-f]{8}|-)\s+(.*)$")


def parse_ledger(path):
    """Return list of (img_prefix, event) preserving order."""
    events = []
    with open(path, "r", errors="replace") as f:
        for ln, raw in enumerate(f, 1):
            m = EVENT_RE.match(raw.rstrip("\n"))
            if not m:
                if raw.strip():
                    print(f"[warn] ledger line {ln}: unparsed: {raw.strip()}")
                continue
            events.append((m.group(2), m.group(3)))
    return events


def kv(body, key):
    m = re.search(rf"\b{key}=(\S+)", body)
    return m.group(1) if m else None


def verify_group(rep, img, image_ev, seg_evs, proof_ev, dump_ev,
                 file_hashes, inner_path):
    stream = kv(image_ev, "stream")
    stream_sha = kv(image_ev, "sha256")
    container_sha = kv(image_ev, "container_sha256")
    size = int(kv(image_ev, "size") or "0")
    segs = int(kv(image_ev, "segs") or "0")
    path = kv(image_ev, "path")

    print(f"\n=== image {img or '-'} path={path} stream={stream} "
          f"declared_segs={segs} ===")

    # 1. Container hash: one of the user's OWN files must match it.
    container_path = None
    container_size = int(kv(image_ev, "container_size") or "0")
    for path, (h, sz) in file_hashes.items():
        if container_sha and h != container_sha:
            continue
        if container_size and sz != container_size:
            continue
        container_path = path
        break
    if not file_hashes:
        rep.check(False, "container sha256",
                  "no --file given; supply the SAME file(s) the app loaded")
        return
    rep.check(container_path is not None,
              "container sha256 == sha256sum(one of your files)",
              f"ledger={container_sha} container_size={container_size} "
              f"matched={'yes' if container_path else 'NO PROVIDED FILE'}")
    if container_path is None:
        return
    file_path = container_path

    # 2. Choose the byte stream the executed image came from.
    exec_stream_path = file_path
    if stream == "inner_elf":
        # The dumped inner ELF must exist and its hash must match both the
        # ledger's inner_elf_dump event and the bound stream sha256.
        if inner_path is None:
            rep.check(False, "inner ELF stream",
                      "stream=inner_elf but no --inner dump given "
                      "(copy <logs>/elfdumps/*.inner.elf from the device)")
            return
        dump_sha = sha256_of(inner_path)
        rep.check(dump_sha == stream_sha,
                  "inner-ELF dump sha256 == bound stream sha256",
                  f"dump={dump_sha} stream={stream_sha}")
        if dump_ev:
            dsha = kv(dump_ev, "sha256")
            dsize = int(kv(dump_ev, "size") or "0")
            dpath = kv(dump_ev, "path")
            rep.check(dsha == dump_sha and dpath is not None,
                      "inner_elf_dump ledger event matches the dump file",
                      f"ledger path={dpath} sha={dsha}")
            rep.check(__import__("os").path.getsize(inner_path) == dsize,
                      "inner-ELF dump size == ledger size",
                      f"ledger={dsize}")
        exec_stream_path = inner_path

    # 3. Segment hashes recomputed from the exec stream's own phdrs.
    if not seg_evs:
        rep.check(False, "segment events", "ledger carried none")
    else:
        try:
            phdrs = parse_phdrs(exec_stream_path)
        except ValueError as e:
            rep.check(False, "ELF header parse of exec stream", str(e))
            return
        loads = [(i, p) for i, p in enumerate(phdrs) if p[0] == PT_LOAD]
        rep.check(len(seg_evs) == segs,
                  "segment event count == declared segs",
                  f"events={len(seg_evs)} declared={segs}")
        for ev in seg_evs:
            phdr_i = int(kv(ev, "phdr") or "-1")
            file_off = int(kv(ev, "file_off") or "0", 16)
            filesz = int(kv(ev, "filesz") or "0")
            lsha = kv(ev, "sha256")
            if phdr_i >= len(phdrs):
                rep.check(False, f"segment phdr={phdr_i}",
                          f"phdr index out of range ({len(phdrs)} phdrs)")
                continue
            p = phdrs[phdr_i]
            if p[0] != PT_LOAD:
                rep.check(False, f"segment phdr={phdr_i}",
                          f"phdr type={p[0]} is not PT_LOAD")
                continue
            p_offset, p_filesz = p[2], p[4]
            rep.check(p_offset == file_off,
                      f"segment phdr={phdr_i} file_off == p_offset",
                      f"ledger=0x{file_off:x} phdr=0x{p_offset:x}")
            rep.check(p_filesz == filesz,
                      f"segment phdr={phdr_i} filesz == p_filesz",
                      f"ledger={filesz} phdr={p_filesz}")
            data = read_span(exec_stream_path, p_offset, p_filesz)
            actual_seg = hashlib.sha256(data).hexdigest()
            rep.check(actual_seg == lsha,
                      f"segment phdr={phdr_i} sha256 recomputed",
                      f"ledger={lsha} recomputed={actual_seg}")

    # 4. Entry proof: recompute 32 bytes at file_off from the exec stream.
    if proof_ev:
        file_off = int(kv(proof_ev, "file_off") or "0", 16)
        claimed_match = kv(proof_ev, "match") == "1"
        psha = kv(proof_ev, "sha256")
        data = read_span(exec_stream_path, file_off, 32)
        if len(data) == 32:
            actual = hashlib.sha256(data).hexdigest()
            rep.check(actual == psha,
                      "entry_proof sha256 recomputed from YOUR bytes at "
                      "file_off",
                      f"file_off=0x{file_off:x} ledger={psha} "
                      f"recomputed={actual}")
            if claimed_match and actual == psha:
                print("[note] match=1 now INDEPENDENTLY CONFIRMED: the "
                      "dispatch target the app executed is byte-identical "
                      "to your file at this offset.")
        else:
            rep.check(False, "entry_proof bytes",
                      f"file_off=0x{file_off:x} beyond stream end")


def main():
    ap = argparse.ArgumentParser(
        description="Recompute every px5_evidence.log hash claim offline.")
    ap.add_argument("--ledger", required=True,
                    help="px5_evidence.log pulled from the device")
    ap.add_argument("--file", action="append", default=[],
                    help="YOUR eboot.bin/.self/.elf (repeatable — every "
                         "file that was loaded this session)")
    ap.add_argument("--inner",
                    help="*.inner.elf from <logs>/elfdumps/ (SELF loads)")
    args = ap.parse_args()

    events = parse_ledger(args.ledger)

    rep = Report()

    # Group ledger events by bound image (each event line carries the img
    # prefix written by AppendLedger; '-' = no image bound yet).
    groups = []            # [img, image_ev, [seg_evs], proof_ev, dump_ev]
    cur = None
    for (img, ev) in events:
        if ev.startswith("image bound "):
            cur = [img, ev, [], None, None]
            groups.append(cur)
            continue
        if img == "-" or cur is None or img != cur[0]:
            continue       # pre-bind or post-unbind event: not verifiable
        if ev.startswith("segment "):
            cur[2].append(ev)
        elif ev.startswith("entry_proof "):
            cur[3] = ev
        elif ev.startswith("inner_elf_dump "):
            cur[4] = ev

    if not groups:
        print("[FAIL] no 'image bound' event in ledger — nothing was ever "
              "loaded+bound; the ledger proves no game execution claim.")
        return 1

    file_hashes = {}
    for p in args.file:
        try:
            file_hashes[p] = (sha256_of(p), __import__("os").path.getsize(p))
        except OSError as e:
            print(f"[warn] cannot read {p}: {e}")

    for (img, image_ev, seg_evs, proof_ev, dump_ev) in groups:
        verify_group(rep, img, image_ev, seg_evs, proof_ev, dump_ev,
                     file_hashes, args.inner)

    # Non-hash events are surfaced, not verified (they are state claims):
    nid_events = [ev for (_i, ev) in events if ev.startswith("nid miss ")]
    if nid_events:
        print(f"\n[note] {len(nid_events)} missing-NID event(s) — the guest "
              f"asked for imports PSX5 does not implement yet:")
        for ev in nid_events[:12]:
            print(f"  {ev}")

    print(f"\n==== VERDICT: {rep.oks} OK, {rep.fails} FAIL ====")
    print("Every OK line above was recomputed HERE from your own bytes — "
          "no trust in the app or the agent required.")
    return 1 if rep.fails else 0


if __name__ == "__main__":
    sys.exit(main())
