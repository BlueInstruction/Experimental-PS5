#!/usr/bin/env python3
"""
PX5 Foundation Test Generator  (v2 — advanced guest added)
==========================================================
Generates `test_guest.h` containing three embedded x86-64 guest programs:

1. TEST_GUEST_RAW_CODE  - raw blob: write("PX5-OK!") -> exit_group(42) -> hlt

2. TEST_GUEST_ELF       - same program inside a REAL minimal ELF64 ET_EXEC
      exercising: file IO -> ELF parse -> PT_LOAD map -> FEXCore bridge ->
      Linux syscall interception -> stdout capture + exit code.

3. TEST_GUEST_ELF_V2    - advanced ELF guest proving REAL MEMORY WORK:
        page = mmap(fixed VA 0x149000000, 0x1000, RW,
                    MAP_PRIVATE|ANON|FIXED)
        assert RAX == returned address == requested VA
        *(uint32_t*)page = MARKER
        reload & compare against MARKER
        write("MMAP-PASS!") -> exit_group(7) / on failure exit_group(19)

Identity-mapping contract: MemoryManager reserves the canonical window with
MAP_FIXED at exactly 0x140000000, therefore guest numeric VAs == host
addresses inside the window; direct stores through registers hit the pages
the bridge maps (see memory.h "windowed mapping" notes).

Regenerate after editing:  python3 tools/gen_test_guest.py
"""
import struct

TEST_GUEST_LOAD_VADDR = 0x140000000
PAYLOAD_FILE_OFF      = 0x80
MSG                   = b"PX5-OK!\n"

V2_MMAP_TARGET  = 0x149000000     # free page well above stack/dm areas
V2_MMAP_LEN     = 0x1000
V2_MARKER       = 0x50414532      # '2EAP' little-endian read ("PEA2")
V2_MSG_PASS     = b"MMAP-PASS!\n"
V2_EXIT_OK      = 7
V2_EXIT_FAIL    = 19


# ---------------------------------------------------------------------------
# Small hand-assembler helpers (verify-by-construction style, like v1)
# ---------------------------------------------------------------------------
def imm32(v):
    return struct.pack("<I", v & 0xFFFFFFFF)

def imm64(v):
    return struct.pack("<Q", v)


def build_v2_code():
    """Two-phase assembly: first compute total code length (labels depend on
    it because messages are trailing payloads addressed RIP-relative),
    then emit with resolved displacements."""
    # ---------------- phase sizing ------------------------------------
    # NOTE lea length 7, every other instruction below fixed-width.
    mmap_block_len = (
        5 +           # mov eax,9
        5 +           # mov edi,target
        5 +           # mov esi,len
        5 +           # mov edx,prot
        6 +           # mov r10d,flags (REX.B)
        6 +           # mov r8d,-1
        3 +           # xor r9d,r9d
        2             # syscall
    )
    check_block_len = (
        3 +           # test rax,rax   (48 85 C0)
        2 +           # js .fail       (rel8)
        10 +          # movabs rcx,imm64 (48 B9 imm64)
        3 +           # cmp rax,rcx    (48 39 C8)
        2             # jne .fail      (rel8)
    )
    store_load_len = (
        6 +           # mov dword [rax],MARKER
        2 +           # mov edx,[rax]
        5 +           # mov ecx,MARKER
        2 +           # cmp edx,ecx
        2             # je .pass (rel8)
    )
    fail_block_len = 5 + 5 + 2            # exit(FAIL) mov/mov/syscall
    pass_write_len = 5 + 5 + 7 + 5 + 2    # write(msg)
    pass_exit_len  = 5 + 5 + 2 + 1        # exit(OK)+hlt

    # layout after all instructions:
    #   [code total][msg_pass][NUL]
    code_total = (mmap_block_len + check_block_len + store_load_len +
                  fail_block_len + pass_write_len + pass_exit_len)

    def emit_with_layout():
        out = bytearray()
        pc = 0
        fail_pos = None
        pass_pos = None

        def put(b):
            nonlocal pc
            out.extend(b); pc += len(b)

        # ---- mmap(fd-fixed anon page) ---------------------------------
        put(b"\xB8" + imm32(9))                       # mov eax, __NR_mmap
        put(b"\xBF" + imm32(V2_MMAP_TARGET))          # mov edi, addr
        put(b"\xBE" + imm32(V2_MMAP_LEN))             # mov esi, len
        put(b"\xBA" + imm32(0x3))                     # mov edx, PROT_RW
        put(b"\x41\xBA" + imm32(0x32))                # mov r10d, P|A|FIXED
        put(b"\x41\xB8\xFF\xFF\xFF\xFF")              # mov r8d, -1
        put(b"\x45\x31\xC9")                          # xor r9d,r9d
        put(b"\x0F\x05")                              # syscall
        assert pc == mmap_block_len

        # ---- success sanity: RAX == requested address ------------------
        put(b"\x48\x85\xC0")                          # test rax,rax
        j_fail_1 = pc                                 # js rel8 -> fail
        put(b"\x78\x00")                              # placeholder js
        put(b"\x48\xB9" + imm64(V2_MMAP_TARGET))      # movabs rcx,target
        put(b"\x48\x39\xC8")                          # cmp rax,rcx
        j_fail_2 = pc                                 # jne rel8 -> fail
        put(b"\x75\x00")
        assert pc == mmap_block_len + check_block_len

        # ---- store/load/compare marker --------------------------------
        put(b"\xC7\x00" + imm32(V2_MARKER))           # mov [rax],MARKER
        put(b"\x8B\x10")                              # mov edx,[rax]
        put(b"\xB9" + imm32(V2_MARKER))               # mov ecx,MARKER
        put(b"\x39\xCA")                              # cmp edx,ecx
        j_pass = pc                                   # je rel8 -> pass
        put(b"\x74\x00")
        assert pc == mmap_block_len + check_block_len + store_load_len

        # ---- FAIL block ------------------------------------------------
        fail_pos = pc
        put(b"\xB8" + imm32(231))                     # mov eax,exit_group
        put(b"\xBF" + imm32(V2_EXIT_FAIL))
        put(b"\x0F\x05")

        # ---- PASS write -------------------------------------------------
        pass_pos = pc
        put(b"\xB8" + imm32(1))                       # mov eax,__NR_write
        put(b"\xBF" + imm32(1))                       # fd = stdout
        msg_off = code_total                          # payload location
        disp = msg_off - pc                           # rip after lea == pc+len(lea)... careful:
        # lea rsi,[rip+disp] where rip = byte after instruction.
        # We patch displacement next line knowing its own size (7):
        disp = msg_off - (pc + 7)
        put(b"\x48\x8D\x35" + struct.pack("<i", disp))
        put(b"\xBA" + imm32(len(V2_MSG_PASS)))        # edx = len
        put(b"\x0F\x05")
        assert pc == code_total - pass_exit_len

        # ---- PASS exit ---------------------------------------------------
        put(b"\xB8" + imm32(231))
        put(b"\xBF" + imm32(V2_EXIT_OK))
        put(b"\x0F\x05")
        put(b"\xF4")                                  # hlt safety stop

        # resolve short branches
        out[j_fail_1 + 1] = (fail_pos - (j_fail_1 + 2)) & 0xFF
        out[j_fail_2 + 1] = (fail_pos - (j_fail_2 + 2)) & 0xFF
        out[j_pass + 1]   = (pass_pos - (j_pass + 2)) & 0xFF
        return bytes(out)

    code = emit_with_layout()
    assert len(code) == code_total, f"{len(code)} != {code_total}"
    return code + V2_MSG_PASS + b"\x00"


def build_v1():
    """Original single-write guest (kept verbatim behaviour)."""
    out = bytearray()
    out += b"\xB8\x01\x00\x00\x00"                # mov eax,1
    out += b"\xBF\x01\x00\x00\x00"                # mov edi,1
    total = 5 + 5 + 7 + 5 + 2 + 5 + 5 + 2 + 1
    disp = total - (5 + 5 + 7)
    out += b"\x48\x8D\x35" + struct.pack("<i", disp)
    out += b"\xBA" + imm32(len(MSG))
    out += b"\x0F\x05"
    out += b"\xB8\xE7\x00\x00\x00"
    out += b"\xBF\x2A\x00\x00\x00"
    out += b"\x0F\x05"
    out += b"\xF4"
    assert total == len(out)
    return bytes(out) + MSG + b"\x00"


CODE1 = build_v1()

# ---- v3 contract: guest synchronous trap (ud2) -> routed, app survives ---
# FEXCore compiles guest ud2 to: SynchronousFaultData{FaultToTopAndGenerated
# Exception=1, Signal=SIGILL(4), TrapNo=X86_TRAPNO_UD(6), si_code=2(ILL_ILLOPN)}
# then branch to the dispatcher's GuestSignal_SIGILL block, which faults with
# hlt(0) -> host SIGILL. PX5's fault router unwinds via ThreadStopHandler.
TEST_GUEST_UD2        = b"\x0F\x0B"           # ud2
TEST_GUEST_UD2_SIGNAL = 4                       # SIGILL
TEST_GUEST_UD2_TRAPNO = 6                       # X86_TRAPNO_UD
TEST_GUEST_UD2_SICODE = 2                       # ILL_ILLOPN

def wrap_elf(code):
    # v1.29 FIX (the vc29 app-killer): e_entry is a VIRTUAL ADDRESS. The
    # code sits at file offset PAYLOAD_FILE_OFF inside a segment whose
    # p_vaddr is TEST_GUEST_LOAD_VADDR, so the code lands at guest VA
    # TEST_GUEST_LOAD_VADDR — NOT at +PAYLOAD_FILE_OFF. The old formula
    # (vaddr + file offset) double-counted p_offset: the vc29 device
    # session dispatched at 0x140000080, 82 bytes past the 46-byte image,
    # executed zero-filled memory (x86 00 00 = add byte [rax],al with
    # rax=0) and the guest null-store SIGSEGV killed the whole app.
    ehdr = struct.pack(
        "<16sHHIQQQIHHHHHH",
        b"\x7fELF\x02\x01\x01\x00" + b"\x00"*8,
        2, 62, 1,
        TEST_GUEST_LOAD_VADDR,
        64, 0, 0, 64, 56, 1, 0, 0, 0)
    phdr = struct.pack(
        "<IIQQQQQQ", 1, 7, PAYLOAD_FILE_OFF,
        TEST_GUEST_LOAD_VADDR, TEST_GUEST_LOAD_VADDR,
        len(code), len(code), 0x1000)
    img = bytearray(ehdr + phdr)
    img += b"\x00" * (PAYLOAD_FILE_OFF - len(img))
    img += code
    return bytes(img)

ELF1 = wrap_elf(CODE1)

CODE2 = build_v2_code()
ELF2 = wrap_elf(CODE2)

# Sanity: marker immediate must appear exactly twice in v2 code stream.
def _count_marker(code):
    import re
    m = imm32(V2_MARKER)
    return code.count(m)
cnt = _count_marker(CODE2)
assert cnt == 2, f"expected 2 marker immediates (store+reload), got {cnt}"

# ---------------------------------------------------------------------------
# Emit C++ header
# ---------------------------------------------------------------------------
def c_array(name, data, width=16):
    lines = [f"static constexpr uint8_t {name}[] = {{"]
    for i in range(0, len(data), width):
        chunk = ", ".join(f"0x{b:02X}" for b in data[i:i+width])
        lines.append("    " + chunk + ",")
    lines[-1] = lines[-1].rstrip(",")
    lines.append("};")
    return "\n".join(lines)

header = f"""// AUTO-GENERATED by tools/gen_test_guest.py -- DO NOT EDIT BY HAND.
// Regenerate: python3 tools/gen_test_guest.py
#ifndef PX5_TESTS_TEST_GUEST_H
#define PX5_TESTS_TEST_GUEST_H

#include <cstdint>

// ---- shared contract (v1) ----------------------------------------------
static constexpr uint64_t TEST_GUEST_LOAD_VADDR = 0x{TEST_GUEST_LOAD_VADDR:X}ULL;
static constexpr const char* TEST_GUEST_EXPECTED_OUTPUT = "{MSG.decode().rstrip()}";
static constexpr uint64_t TEST_GUEST_EXPECTED_EXIT_CODE = 42;

// Raw x86-64 machine code: write(1,"PX5-OK!\\n",8); exit_group(42); hlt;
{c_array("TEST_GUEST_RAW_CODE", CODE1)}

// Same program wrapped in a real minimal ELF64 ET_EXEC (PT_LOAD RWX):
static constexpr unsigned int TEST_GUEST_ELF_SIZE = {len(ELF1)};
{c_array("TEST_GUEST_ELF", ELF1)}

// ---- v2 contract: real mmap + memory round-trip verification ------------
static constexpr uint64_t TEST_GUEST_V2_MMAP_TARGET = 0x{V2_MMAP_TARGET:X}ULL;
static constexpr uint64_t TEST_GUEST_V2_MMAP_LEN    = {V2_MMAP_LEN}ULL;
static constexpr uint32_t TEST_GUEST_V2_MARKER      = 0x{V2_MARKER:X}U;
static constexpr const char* TEST_GUEST_V2_EXPECTED_OUTPUT = "{V2_MSG_PASS.decode().strip()}";
static constexpr uint64_t TEST_GUEST_V2_EXIT_OK    = {V2_EXIT_OK};
static constexpr uint64_t TEST_GUEST_V2_EXIT_FAIL  = {V2_EXIT_FAIL};

// Advanced ELF guest: mmap(MAP_FIXED) -> store/load marker -> write ->
// exit_group(7). Failure paths exit(19).
static constexpr unsigned int TEST_GUEST_ELF_V2_SIZE = {len(ELF2)};
{c_array("TEST_GUEST_ELF_V2", ELF2)}

// ---- v3 contract: guest synchronous trap routing (ud2) -------------------
static constexpr uint8_t TEST_GUEST_UD2_CODE[]      = {{ 0x{TEST_GUEST_UD2[0]:02X}, 0x{TEST_GUEST_UD2[1]:02X} }};
static constexpr unsigned int TEST_GUEST_UD2_SIZE   = {len(TEST_GUEST_UD2)};
static constexpr unsigned int TEST_GUEST_UD2_SIGNAL = {TEST_GUEST_UD2_SIGNAL};  // SIGILL
static constexpr unsigned int TEST_GUEST_UD2_TRAPNO = {TEST_GUEST_UD2_TRAPNO};  // x86 #UD
static constexpr unsigned int TEST_GUEST_UD2_SICODE = {TEST_GUEST_UD2_SICODE};  // ILL_ILLOPN

#endif // PX5_TESTS_TEST_GUEST_H
"""

with open("app/src/main/cpp/tests/test_guest.h", "w") as f:
    f.write(header)

print(f"v1 code : {len(CODE1)} B | elf1: {len(ELF1)} B")
print(f"v2 code : {len(CODE2)} B | elf2: {len(ELF2)} B "
      f"@ entry {TEST_GUEST_LOAD_VADDR:#x}")
print(f"ud2 trap fixture: {TEST_GUEST_UD2.hex(' ')} "
      f"-> signal={TEST_GUEST_UD2_SIGNAL} trapno={TEST_GUEST_UD2_TRAPNO} "
      f"si_code={TEST_GUEST_UD2_SICODE}")
print("Wrote app/src/main/cpp/tests/test_guest.h")
