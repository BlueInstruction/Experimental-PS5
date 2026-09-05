// Host regression test for the v1.45 import traps: the vc45 device session
// died on the null-import wall (PLT -> GOT slot 0 -> jmp [0] -> SIGSEGV).
// This drives the REAL loader (ElfLoader::LoadElfFromMemory) over a crafted
// ET_DYN image whose dynamic tables contain exactly the vc45 classes —
// one R_X86_64_RELATIVE, one UNDEF-STRONG R_X86_64_64, one UNDEF-WEAK
// GLOB_DAT, one UNDEF-STRONG JUMP_SLOT — and asserts the whole trap chain:
// slot redirection, stub bytes, weak-to-zero semantics, ledger events and
// the registry counters, without a device or a JIT.
#include "loader/elf_loader.h"
#include "loader/runtime_linker.h"
#include "memory/memory.h"
#include "utils/evidence.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace PX5;

static int fails = 0;
static void chk(bool ok, const char* what) {
    printf("  [%s] %s\n", ok ? "OK  " : "FAIL", what);
    if (!ok) ++fails;
}

namespace {

#pragma pack(push, 1)
struct EhdrRaw {
    uint8_t  ident[16];
    uint16_t type, machine;
    uint32_t version;
    uint64_t entry, phoff, shoff;
    uint32_t flags;
    uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
};
struct PhdrRaw {
    uint32_t type, flags;
    uint64_t offset, vaddr, paddr, filesz, memsz, align;
};
struct DynRaw  { int64_t tag; uint64_t val; };
struct SymRaw  { uint32_t name; uint8_t info, other; uint16_t shndx;
                 uint64_t value, size; };
struct RelaRaw { uint64_t offset, info; int64_t addend; };
#pragma pack(pop)

constexpr uint64_t kStrVa  = 0x1B0;
constexpr uint64_t kSymVa  = 0x150;
constexpr uint64_t kRelaVa = 0x1E0;
constexpr uint64_t kJmpVa  = 0x228;

std::vector<uint8_t> BuildImage() {
    std::vector<uint8_t> img(0x240, 0);
    EhdrRaw e{};
    const uint8_t magic[16] = {0x7f,'E','L','F',2,1,1,0,0,0,0,0,0,0,0,0};
    memcpy(e.ident, magic, 16);
    e.type = 3;                 // ET_DYN -> loader maps at the window base
    e.machine = 62;             // EM_X86_64
    e.version = 1;
    e.entry = 0x70;
    e.phoff = 0x40;
    e.phentsize = 56;
    e.phnum = 2;
    e.ehsize = 64;
    memcpy(img.data(), &e, sizeof e);

    PhdrRaw load{};
    load.type = 1;              // PT_LOAD
    load.flags = 7;             // R W X
    load.offset = 0;
    load.vaddr = 0;
    load.filesz = 0x240;        // headers + tables; the GOT tail is BSS
    load.memsz = 0x3000;        // slots at 0x2000.. live in the zero tail
    load.align = 0x1000;
    memcpy(img.data() + 0x40, &load, sizeof load);

    PhdrRaw dynp{};
    dynp.type = 2;              // PT_DYNAMIC
    dynp.flags = 6;
    dynp.offset = 0xB0;
    dynp.vaddr = 0xB0;
    dynp.filesz = 10 * sizeof(DynRaw);
    dynp.memsz = dynp.filesz;
    dynp.align = 8;
    memcpy(img.data() + 0x78, &dynp, sizeof dynp);

    DynRaw dyn[10] = {};
    dyn[0] = {5, kStrVa};                       // DT_STRTAB
    dyn[1] = {10, 37};                          // DT_STRSZ
    dyn[2] = {6, kSymVa};                       // DT_SYMTAB
    dyn[3] = {7, kRelaVa};                      // DT_RELA
    dyn[4] = {8, 3 * sizeof(RelaRaw)};          // DT_RELASZ
    dyn[5] = {9, sizeof(RelaRaw)};              // DT_RELAENT
    dyn[6] = {0x6ffffff9, 1};                   // DT_RELACOUNT
    dyn[7] = {23, kJmpVa};                      // DT_JMPREL
    dyn[8] = {2, sizeof(RelaRaw)};              // DT_PLTRELSZ
    dyn[9] = {0, 0};                            // DT_NULL
    memcpy(img.data() + 0xB0, dyn, sizeof dyn);

    SymRaw syms[4] = {};
    syms[1] = {1,  0x12, 0, 0, 0, 0};           // strong undef "scePadOpen"
    syms[2] = {12, 0x22, 0, 0, 0, 0};           // weak   undef "opt_weak_ptr"
    syms[3] = {25, 0x12, 0, 0, 0, 0};           // strong undef "sceFiboCalc"
    memcpy(img.data() + kSymVa, syms, sizeof syms);

    const char* strtab = "\0scePadOpen\0opt_weak_ptr\0sceFiboCalc\0";
    memcpy(img.data() + kStrVa, strtab, 37);

    RelaRaw rela[3] = {};
    rela[0] = {0x2000, 8, 0x11111};             // RELATIVE -> applied
    rela[1] = {0x2008, (1ull << 32) | 1, 0};    // R_64, strong undef -> trap
    rela[2] = {0x2010, (2ull << 32) | 6, 0};    // GLOB_DAT, weak undef -> 0
    memcpy(img.data() + kRelaVa, rela, sizeof rela);

    RelaRaw jmp[1] = {};
    jmp[0] = {0x2018, (3ull << 32) | 7, 0};     // JUMP_SLOT, strong -> trap
    memcpy(img.data() + kJmpVa, jmp, sizeof jmp);
    return img;
}

uint64_t SlotValue(uint64_t va) {
    uint64_t v = 0xAAAAAAAAAAAAAAAAull;
    if (!MemoryManager::GetInstance().ReadGuestMemory(va, &v, 8))
        printf("  [FAIL] slot read 0x%llx (unmapped?)\n",
               (unsigned long long)va);
    return v;
}

} // namespace

int main() {
    Evidence::SetLedgerPath("/tmp/px5_import_trap_test_ledger.log");
    // The ledger path is a fixed HOST file shared across runs — on a
    // persistent runner it accumulates stale lines and the flood-
    // discipline check (count occurrences) fails on yesterday's events.
    // CI never saw this (fresh VM per run); every run owns its file now.
    if (FILE* trunc = fopen("/tmp/px5_import_trap_test_ledger.log", "w")) {
        fclose(trunc);
    }
    auto& mm = MemoryManager::GetInstance();
    if (!mm.Initialize(4096)) {                 // device-sized window
        printf("window init failed\n");
        return 2;
    }
    const uint64_t base = mm.GetGuestBase();

    const std::vector<uint8_t> img = BuildImage();
    LoadedElfImage out;
    const bool ok = ElfLoader::LoadElfFromMemory(
        img.data(), img.size(), "import-trap hosttest", out);
    chk(ok, "crafted ET_DYN image loads");
    if (!ok) {
        printf("  loader error: %s\n", out.error.c_str());
        return 1;
    }

    printf("\nRelocation processing:\n");
    chk(out.relocApplied == 1, "one RELATIVE applied");
    chk(out.relocUnresolvedImports == 3, "three UNDEF imports counted");
    chk(out.relocWeakZero == 1, "one weak import resolved to 0");
    chk(out.relocImportTraps == 2, "two strong imports trapped");

    auto& rl = RuntimeLinker::GetInstance();
    chk(rl.ImportTrapCount() == 2, "registry holds both trap entries");

    printf("\nSlot contents (the wall becomes a named ledger):\n");
    const uint64_t stubBase = base + 0x20000000ull;
    chk(SlotValue(base + 0x2000) == base + 0x11111,
        "RELATIVE slot = base + addend");
    chk(SlotValue(base + 0x2008) == stubBase,
        "strong R_64 slot -> trap stub #0");
    chk(SlotValue(base + 0x2010) == 0,
        "weak GLOB_DAT slot stays 0 (ELF semantics)");
    chk(SlotValue(base + 0x2018) == stubBase + 16,
        "strong JUMP_SLOT -> trap stub #1");

    printf("\nStub bytes (mov eax,nr; mov edi,idx; syscall; ret):\n");
    const uint8_t want0[16] = {0xB8, 0x02, 0x00, 0x50, 0x5C,
                               0xBF, 0x00, 0x00, 0x00, 0x00,
                               0x0F, 0x05, 0xC3, 0x90, 0x90, 0x90};
    const uint8_t want1[16] = {0xB8, 0x02, 0x00, 0x50, 0x5C,
                               0xBF, 0x01, 0x00, 0x00, 0x00,
                               0x0F, 0x05, 0xC3, 0x90, 0x90, 0x90};
    uint8_t got[16] = {};
    chk(mm.ReadGuestMemory(stubBase, got, 16) &&
        memcmp(got, want0, 16) == 0, "stub #0 bytes + index 0");
    chk(mm.ReadGuestMemory(stubBase + 16, got, 16) &&
        memcmp(got, want1, 16) == 0, "stub #1 bytes + index 1");

    printf("\nDispatch path (what GuestSyscalls::Dispatch forwards to):\n");
    chk(rl.DispatchImportTrap(0) == 0, "trap #0 dispatch returns 0");
    chk(rl.DispatchImportTrap(0) == 0, "trap #0 repeat returns 0");
    chk(rl.DispatchImportTrap(1) == 0, "trap #1 dispatch returns 0");
    chk(rl.DispatchImportTrap(9999) == 0,
        "out-of-range index refused (returns 0, counted)");

    const std::string summary = rl.GetImportTrapSummary();
    printf("  summary: %s\n", summary.c_str());
    chk(summary.find("stubs=2") != std::string::npos &&
        summary.find("hits=3") != std::string::npos &&
        summary.find("distinct=2") != std::string::npos,
        "summary counts hits and distinct");
    chk(summary.find("scePadOpen") != std::string::npos &&
        summary.find("sceFiboCalc") != std::string::npos,
        "summary names both imports (hottest first order)");
    chk(rl.GetImportTrapSummary().find("'scePadOpen'(x2)") !=
        std::string::npos, "hit counts attach to the right name");

    printf("\nEvidence ledger:\n");
    FILE* f = fopen("/tmp/px5_import_trap_test_ledger.log", "r");
    chk(f != nullptr, "ledger file exists");
    if (f) {
        char buf[16384] = {};
        const size_t n = fread(buf, 1, sizeof buf - 1, f);
        fclose(f);
        const std::string ledger(buf, n);
        chk(ledger.find("import traps stubs=2") != std::string::npos,
            "install event ledgered");
        chk(ledger.find("import miss idx=0 name='scePadOpen'") !=
            std::string::npos, "first miss ledgered by name");
        chk(ledger.find("import miss idx=1 name='sceFiboCalc'") !=
            std::string::npos, "second miss ledgered by name");
        chk(ledger.find("idx=0 name='scePadOpen'") ==
                ledger.rfind("idx=0 name='scePadOpen'"),
            "repeat hit NOT re-ledgered (flood discipline)");
    }

    printf("\nReset contract:\n");
    rl.Reset();
    chk(rl.ImportTrapCount() == 0, "Reset clears the trap table");
    chk(rl.GetImportTrapSummary() == "import traps: none installed",
        "Reset clears the summary");

    printf("\n%s (%d failure%s)\n", fails ? "FAILED" : "ALL PASS",
           fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
