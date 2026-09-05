// Link the REAL MemoryManager and exercise the exact scenarios that broke.
#include "memory/memory.h"
#include <atomic>
#include <cstdio>
#include <thread>
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

    printf("\nbrk contract (a refusal must return the unchanged break):\n");
    {
        const uint64_t brkBase = base + 0x1000000;   // 16 MiB into the window
        mm.SetProgramBreak(brkBase);
        uint64_t out = 0;
        chk(mm.SetBrk(0, out) && out == brkBase, "query form returns the break");
        const uint64_t grown = brkBase + 0x20000;
        chk(mm.SetBrk(grown, out) && out == grown,
            "grow succeeds and reports the new break");
        uint64_t refused = 0xdeadbeefULL;
        chk(!mm.SetBrk(base + 0xF0000000ULL, refused) && refused == grown,
            "out-of-limit refusal reports the unchanged break");
    }

    printf("\nCross-block munmap (NR_munmap spanning two allocations):\n");
    {
        const size_t MiB = 1024 * 1024;
        const uint64_t a = base + 0x4000000;         // 64 MiB into the window
        mm.MapMemory(a, 2 * MiB,
                     MemoryFlags::PAGE_READ | MemoryFlags::PAGE_WRITE, "blkA");
        mm.MapMemory(a + 2 * MiB, 2 * MiB,
                     MemoryFlags::PAGE_READ | MemoryFlags::PAGE_WRITE, "blkB");
        const size_t before2 = mm.GetTotalAllocatedMB();
        chk(mm.UnmapMemory(a + MiB, 2 * MiB), "range spanning A-tail+B-head succeeds");
        chk(!mm.IsValidAddress(a + MiB, 1),        "A tail is unmapped");
        chk(!mm.IsValidAddress(a + 2 * MiB, 1),    "B head is unmapped");
        chk(mm.IsValidAddress(a, 1),               "A head survives");
        chk(mm.IsValidAddress(a + 3 * MiB, 1),     "B tail survives");
        chk(mm.GetTotalAllocatedMB() == before2 - 2,
            "accounting dropped by exactly the unmapped 2 MiB");

        printf("Linux hole semantics:\n");
        chk(mm.UnmapMemory(a + MiB + 0x10000, 0x1000),
            "munmap entirely inside a hole succeeds (Linux returns 0)");
        // Start mid-hole (hole is [a+1MiB, a+3MiB)); the range crosses into
        // the surviving B-tail at a+3MiB.
        chk(mm.UnmapMemory(a + 2 * MiB + 0x80000, MiB),
            "munmap from a hole across B-tail succeeds");
        chk(!mm.IsValidAddress(a + 3 * MiB, 1),    "B tail now unmapped");
        chk(mm.IsValidAddress(a + 3 * MiB + 0x80000, 1),
            "B tail beyond the munmap end survives");
        chk(!mm.UnmapMemory(a, 0), "zero-length munmap refused");
        chk(mm.UnmapMemory(base + 0x9000000, 0x1000),
            "munmap of a never-mapped range succeeds (Linux returns 0)");
    }

    printf("\nExecutable-range snapshot coherence (lock-free query path):\n");
    {
        const uint64_t e = base + 0x6000000;         // 96 MiB into the window
        MemoryManager::ExecMapInfo info{};
        mm.MapMemory(e, 0x10000,
                     MemoryFlags::PAGE_READ | MemoryFlags::PAGE_WRITE |
                         MemoryFlags::PAGE_EXEC, "snap-test");
        chk(mm.FindExecutableMapping(e + 0x8000, info) && info.exec && info.writable,
            "fresh exec block visible through the snapshot");
        chk(info.base == e && info.size >= 0x10000, "snapshot reports the block span");
        mm.ProtectMemory(e, 0x10000,
                         MemoryFlags::PAGE_READ | MemoryFlags::PAGE_EXEC);
        chk(mm.FindExecutableMapping(e + 0x8000, info) && info.exec && !info.writable,
            "protect dropping W is reflected in the snapshot");
        mm.UnmapMemory(e, 0x10000);
        chk(!mm.FindExecutableMapping(e + 0x8000, info),
            "unmapped exec block is gone from the snapshot");
        mm.MapMemory(e, 0x3000,
                     MemoryFlags::PAGE_READ | MemoryFlags::PAGE_EXEC, "snap-split");
        mm.UnmapMemory(e + 0x1000, 0x1000);
        chk(mm.FindExecutableMapping(e, info) && info.size == 0x1000,
            "split head visible with its exact size");
        chk(mm.FindExecutableMapping(e + 0x2000, info) && info.size == 0x1000,
            "split tail visible with its exact size");
        chk(!mm.FindExecutableMapping(e + 0x1000, info),
            "hole inside a split block is not executable");
    }

    printf("\nConcurrent map/unmap vs lock-free exec queries (smoke):\n");
    {
        // Smoke only: asserts no deadlock/crash and reader progress under a
        // hammering writer. Data-race freedom is by construction (atomics),
        // not by this test; TSAN would be the tool for deeper checking.
        std::atomic<bool> stop{false};
        std::atomic<long> queries{0};
        std::thread writer([&] {
            const uint64_t va = base + 0xA000000;    // 160 MiB into the window
            for (int i = 0; i < 400; ++i) {
                mm.MapMemory(va, 0x10000,
                             MemoryFlags::PAGE_READ | MemoryFlags::PAGE_EXEC,
                             "stress");
                mm.ProtectMemory(va, 0x10000, MemoryFlags::PAGE_READ);
                mm.UnmapMemory(va, 0x10000);
            }
            stop.store(true, std::memory_order_relaxed);
        });
        std::thread reader([&] {
            MemoryManager::ExecMapInfo info{};
            while (!stop.load(std::memory_order_relaxed)) {
                mm.FindExecutableMapping(base + 0xA008000, info);
                mm.FindExecutableMapping(base + 0x1000, info);
                queries.fetch_add(1, std::memory_order_relaxed);
            }
        });
        writer.join();
        reader.join();
        chk(queries.load() > 0, "reader kept progressing during mutations");
    }

    printf("\n==== %s ====\n", fails ? "FAILURES PRESENT" : "ALL CHECKS PASSED");
    return fails ? 1 : 0;
}
