// Link the REAL MemoryManager and exercise the exact scenario that was broken.
#include "memory/memory.h"
#include <cstdio>
using namespace PX5;

static int fails = 0;
static void chk(bool ok, const char* what) {
    printf("  [%s] %s\n", ok ? "OK  " : "FAIL", what);
    if (!ok) ++fails;
}

int main() {
    auto& mm = MemoryManager::GetInstance();
    if (!mm.Initialize(256)) { printf("window init failed\n"); return 2; }
    printf("%s\n\n", mm.GetWindowInfoString().c_str());

    const uint64_t base = mm.GetGuestBase();
    const size_t   sz   = 2900000;   // 2.9 MB text segment, like a real PT_LOAD
    const size_t   szPg = (sz + 4095) & ~size_t{4095};  // manager page-rounds

    printf("Mapping one 2.9 MB executable segment at 0x%llx\n",
           (unsigned long long)base);
    mm.MapMemory(base, sz, MemoryFlags::PAGE_READ | MemoryFlags::PAGE_EXEC,
                 "PT_LOAD#0");

    printf("\nIsValidAddress across the segment:\n");
    chk(mm.IsValidAddress(base, 1),               "first byte");
    chk(mm.IsValidAddress(base + 0x70, 1),        "entry+0x70 (page 0)");
    chk(mm.IsValidAddress(base + 0x1000, 1),      "page 1  (was FALSE before)");
    chk(mm.IsValidAddress(base + 0x10000, 1),     "page 16 (was FALSE before)");
    chk(mm.IsValidAddress(base + 0x200000, 1),    "+2 MiB  (was FALSE before)");
    chk(mm.IsValidAddress(base + sz - 1, 1),      "last byte");
    (void)szPg;
    chk(!mm.IsValidAddress(base + sz + 0x10000, 1), "past the end rejected");

    printf("\nFindExecutableMapping (what FEXCore asks before translating):\n");
    for (uint64_t off : {0x0ull, 0x70ull, 0x1000ull, 0x100000ull, 0x2C0000ull}) {
        MemoryManager::ExecMapInfo info{};
        const bool found = mm.FindExecutableMapping(base + off, info);
        char msg[128];
        snprintf(msg, sizeof msg,
                 "+0x%-8llx -> exec=%d base=0x%llx size=%zu",
                 (unsigned long long)off, found && info.exec ? 1 : 0,
                 (unsigned long long)info.base, info.size);
        chk(found && info.exec && info.size >= sz, msg);
    }

    printf("\nSpan checks (whole range must be covered):\n");
    chk(mm.IsValidAddress(base, sz),        "full-segment span accepted");
    chk(!mm.IsValidAddress(base + sz - 8, 4096), "span crossing the end rejected");

    printf("\nPartial unmap accounting:\n");
    const size_t before = mm.GetTotalAllocatedMB();
    mm.UnmapMemory(base + 0x100000, 0x1000);   // punch a hole mid-segment
    chk(!mm.IsValidAddress(base + 0x100000, 1), "hole is unmapped");
    chk(mm.IsValidAddress(base + 0x50000, 1),   "head survives the split");
    chk(mm.IsValidAddress(base + 0x200000, 1),  "tail survives the split");
    printf("  allocatedMB before=%zu after=%zu\n", before, mm.GetTotalAllocatedMB());

    printf("\n==== %s ====\n", fails ? "FAILURES PRESENT" : "ALL CHECKS PASSED");
    return fails ? 1 : 0;
}
