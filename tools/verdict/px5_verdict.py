#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# One-screen post-run verdict: parses the three px5_*.log files and decides,
# from emitted evidence only, whether a REAL game is running (gates 1-5).
# Contract: see LOG_CONTRACT.md next to this file.
"""px5_verdict.py - one-screen verdict for PX5 logs: is a REAL game running?

Usage:  python3 px5_verdict.py <logs-folder | logs.zip> [--lang ar|en]
Exit code: 0 = gates 1-5 pass (a real game is running), 1 = it is not.

Reads only px5_main.log, px5_evidence.log, px5_crash_latest.log.
Every number printed is parsed from a log line; nothing is guessed.
Lines tagged [SYNTH] are built-in self-tests: they never count as evidence.
"""
import argparse, os, re, sys, zipfile

NAMES = ("px5_main.log", "px5_evidence.log", "px5_crash_latest.log")


def read_sources(path):
    out = {n: "" for n in NAMES}
    if os.path.isdir(path):
        for n in NAMES:
            p = os.path.join(path, n)
            if os.path.isfile(p):
                out[n] = open(p, encoding="utf-8", errors="replace").read()
    else:
        with zipfile.ZipFile(path) as z:
            for i in z.infolist():
                b = os.path.basename(i.filename)
                if b in out and not i.is_dir() and "/vfs/" not in i.filename:
                    out[b] = z.read(i).decode("utf-8", "replace")
    return out


def last(rx, text, flags=0):
    m = re.findall(rx, text, flags)
    return m[-1] if m else None


def facts(src):
    text = src["px5_main.log"]
    lines = text.splitlines()
    ev = src["px5_evidence.log"]
    crash = src["px5_crash_latest.log"]
    f = {}
    f["build"] = last(r"build identity: (v[\d.]+ vc\d+ sha=\w+)", text) or "?"
    g = last(r"screen_entered game=(.+?) titleId=(\S+)", text)
    f["game"] = f"{g[0]} ({g[1]})" if g else "?"
    f["loaded"] = "[GUEST] SELF load: OK" in text or "ELF loaded honestly" in text
    rel = last(r"REL: applied=(\d+) unresolvedImports=(\d+)", text)
    f["applied"], f["unresolved"] = (int(rel[0]), int(rel[1])) if rel else (None, None)
    f["trap_failed"] = "import-trap map FAILED" in text
    f["hle_symbols"] = last(r"libkernel HLE v1 registered: (\d+) symbols", text)
    idx = [i for i, l in enumerate(lines) if "[GUEST] dispatch result" in l]
    f["stopped_by"] = f["ms"] = f["syscalls"] = None
    if idx:
        i = idx[-1]
        m = re.search(r"stopped_by=(\w+)", lines[i])
        f["stopped_by"] = m.group(1) if m else None
        for l in lines[max(0, i - 12):i]:
            m = re.search(r"Guest execution finished in ([\d.]+) ms", l)
            if m:
                f["ms"] = float(m.group(1))
            m = re.search(r"syscalls total=(\d+)", l)
            if m:
                f["syscalls"] = int(m.group(1))
    gp = last(r"REAL-GUEST\s+\S+\s+gpu-stats submits=(\d+) dwords=\d+ draws=(\d+)", ev)
    f["submits"], f["draws"] = (int(gp[0]), int(gp[1])) if gp else (None, None)
    fr = [int(x) for x in re.findall(r"Render loop stopped \(frames=(\d+)\)", text)]
    f["frames"] = max(fr) if fr else None
    f["audio"] = sum(1 for l in lines if re.search(r"^\[\s*[\d.]+\]\s+Audio\b|cat=Audio\b", l))
    f["input"] = sum(1 for l in lines if re.search(r"^\[\s*[\d.]+\]\s+Input\b|cat=Input\b|pad_reads", l))
    f["synth"] = sum(1 for l in lines if "[SYNTH]" in l)
    f["crash"] = None
    if crash.strip():
        sig = last(r"signal=(\w+)\(\d+\)", crash) or "?"
        synth = "guest bytes @rip: 0f 0b" in crash or "rip=0x140400000" in crash
        f["crash"] = (sig, synth)
    return f


def gates(f):
    """(ok, ar_title, en_title, ar_detail, en_detail); ok = True / False / None(unknown)"""
    u = f["unresolved"]
    g = []
    g.append((f["loaded"], "تحميل eboot", "Image loaded",
              f"تم تطبيق {f['applied']} إزاحة", f"{f['applied']} relocations applied"))
    g.append((None if u is None else u == 0, "ربط دوال النظام", "Imports resolved",
              f"{u} دالة بلا تنفيذ" + (" + جدول الاستدعاء فشل (import-trap)" if f["trap_failed"] else ""),
              f"{u} unresolved" + (" + import-trap map FAILED" if f["trap_failed"] else "")))
    ran = None if f["ms"] is None else (f["ms"] >= 1000 and (f["syscalls"] or 0) >= 100)
    g.append((ran, "تشغيل كود اللعبة", "Guest code runs",
              f"اشتغل {f['ms']} ms و{f['syscalls']} syscall ثم توقف ({f['stopped_by']})",
              f"ran {f['ms']} ms, {f['syscalls']} syscalls, stopped by {f['stopped_by']}"))
    g.append((None if f["draws"] is None else f["draws"] > 0, "GPU يرسم", "GPU draws",
              f"draws={f['draws']} submits={f['submits']}", f"draws={f['draws']} submits={f['submits']}"))
    g.append((None if f["frames"] is None else f["frames"] > 0, "إطارات معروضة", "Frames presented",
              f"frames={f['frames']}", f"frames={f['frames']}"))
    g.append((f["audio"] > 0, "صوت من المحرك", "Engine audio",
              f"{f['audio']} سطر صوت (الصوت المسموع غالبًا موسيقى الواجهة)",
              f"{f['audio']} audio lines (audible sound is likely UI music)"))
    g.append((None if f["input"] == 0 else True, "قراءة اللمس/اليد", "Pad reads",
              "لا يوجد عدّاد في اللوج، لا يمكن الإثبات", "no counter in the log, cannot prove"))
    return g


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("path")
    ap.add_argument("--lang", default="ar", choices=["ar", "en"])
    a = ap.parse_args()
    ar = a.lang == "ar"
    f = facts(read_sources(a.path))
    gs = gates(f)
    mark = {True: "✅", False: "❌", None: "⚠️"}
    print(("PX5 — حكم اللوج" if ar else "PX5 log verdict"))
    print(f"{f['build']} | {f['game']}\n")
    for n, (ok, t_ar, t_en, d_ar, d_en) in enumerate(gs, 1):
        print(f"{mark[ok]} {n}. {t_ar if ar else t_en}: {d_ar if ar else d_en}")
    if f["crash"]:
        sig, synth = f["crash"]
        if ar:
            print(f"⚠️ انهيار {sig}: " + ("داخل اختبار ud2 الاصطناعي، لا داخل اللعبة" if synth else "خارج الاختبارات الاصطناعية — يحتاج فحصًا"))
        else:
            print(f"⚠️ crash {sig}: " + ("inside the synthetic ud2 self-test, not the game" if synth else "outside the synthetic tests - needs inspection"))
    if f["hle_symbols"]:
        print(("ℹ️ دوال نظام منفّذة: " if ar else "ℹ️ HLE symbols implemented: ") + f["hle_symbols"])
    print(("ℹ️ أسطر [SYNTH] (اختبارات داخلية لا تثبت شيئًا عن اللعبة): " if ar else "ℹ️ [SYNTH] lines (built-in tests, not evidence): ") + str(f["synth"]))
    real = all(ok is True for ok, *_ in gs[:5])
    first = next((n for n, g in enumerate(gs[:5], 1) if g[0] is not True), None)
    print()
    if real:
        print("✅ " + ("لعبة حقيقية تعمل" if ar else "A real game is running"))
    else:
        print("❌ " + ("الحكم: اللعبة لا تعمل فعليًا" if ar else "Verdict: no real game is running"))
        t = gs[first - 1]
        print(("أول عائق: #%d %s" % (first, t[1])) if ar else ("First blocker: #%d %s" % (first, t[2])))
        print("بعد أي إصلاح يجب أن يتغير: unresolved ↓، syscalls ↑، draws>0، frames>0" if ar
              else "After any fix these must move: unresolved down, syscalls up, draws>0, frames>0")
    return 0 if real else 1


if __name__ == "__main__":
    sys.exit(main())
