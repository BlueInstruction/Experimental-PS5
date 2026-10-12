#!/usr/bin/env python3
"""Decode the import list of the real game inner ELF (saf_eboot.bin.inner.elf).

Section headers are stripped, so walk PT_DYNAMIC like the real loader.
Classify UNDEF imports, cross-check the v1.62 counts, and write the full
list for the tranche work.
"""
import struct, collections, sys

ELF = sys.argv[1] if len(sys.argv) > 1 else "saf_eboot.bin.inner.elf"
data = open(ELF, "rb").read()

phoff = struct.unpack_from("<Q", data, 0x20)[0]
phentsize = struct.unpack_from("<H", data, 0x36)[0]
phnum = struct.unpack_from("<H", data, 0x38)[0]

loads, dyn_off, dyn_sz = [], 0, 0
for i in range(phnum):
    off = phoff + i * phentsize
    ptype = struct.unpack_from("<I", data, off)[0]
    p_offset, p_vaddr = struct.unpack_from("<QQ", data, off + 8)
    p_filesz = struct.unpack_from("<Q", data, off + 32)[0]
    if ptype == 1:
        loads.append((p_vaddr, p_offset, p_filesz))
    elif ptype == 2:
        dyn_off, dyn_sz = p_offset, p_filesz

entries = {}
for k in range(dyn_sz // 16):
    tag, val = struct.unpack_from("<QQ", data, dyn_off + k * 16)
    if tag == 0:
        break
    entries.setdefault(tag, val)

DT = {5: "STRTAB", 6: "SYMTAB", 7: "RELA", 8: "RELASZ", 11: "SYMENT",
      23: "JMPREL", 2: "PLTRELSZ"}
strtab, symtab = entries[5], entries[6]
rela, relasz = entries.get(7, 0), entries.get(8, 0)
jmprel, pltrelsz = entries.get(23, 0), entries.get(2, 0)
syment = entries.get(11, 24)

def v2f(v):
    for va, fo, sz in loads:
        if va <= v < va + sz:
            return fo + (v - va)
    return None

def sym(i):
    off = v2f(symtab) + i * syment
    st_name, st_info, st_other, st_shndx, st_value, st_size = \
        struct.unpack_from("<IBBHQQ", data, off)
    base = v2f(strtab) + st_name
    end = data.index(b"\0", base)
    return data[base:end].decode(errors="replace"), st_info

seen, undef = set(), []
for rbase, rsz, kind in ((rela, relasz, "RELA"), (jmprel, pltrelsz, "JMPREL")):
    if not rbase or not rsz:
        continue
    f = v2f(rbase)
    for k in range(rsz // 24):
        r_offset, r_info, r_addend = struct.unpack_from("<QQq", data, f + k * 24)
        si = r_info >> 32
        rtype = r_info & 0xFFFFFFFF
        if si == 0 or si in seen:
            continue
        seen.add(si)
        name, info = sym(si)
        undef.append((si, name, kind, rtype, r_offset, r_addend))

print(f"unique imported symbols: {len(undef)}")
print("symbol table size ~", (v2f(strtab) - v2f(symtab)) if v2f(strtab) and v2f(symtab) else "?")

def classify(name):
    if name.startswith("_Z"):
        return "C++ mangled"
    if name.startswith("__cxa"):
        return "__cxa"
    if name.startswith("_Unwind"):
        return "_Unwind"
    if name.startswith("sce"):
        return "sce*"
    if name.startswith("_ZN") or name.startswith("_ZS"):
        return "C++ mangled"
    return "libc/other"

groups = collections.Counter(classify(n) for _, n, _, _, _, _ in undef)
print("groups:", dict(groups))

kinds = collections.Counter(k for _, _, k, _, _, _ in undef)
print("reloc kinds:", dict(kinds))

print("\nfirst 25 non-mangled imports:")
for si, name, kind, rtype, off, add in undef:
    if not name.startswith("_Z"):
        print(f"  [{si:4d}] {name}")
    if si > 4000:
        break

out_path = sys.argv[2] if len(sys.argv) > 2 else "game_import_list.txt"
with open(out_path, "w") as f:
    for si, name, kind, rtype, off, add in sorted(undef):
        f.write(f"{name}\t[{kind} rtype={rtype}]\n")
print("\nwrote /home/z/my-project/scripts/import_list.txt:", len(undef))
