// Host regression test for the v1.47 transparent import dispatch: the
// bridge that turns a guest PLT call into a real HLE invocation without
// touching a single argument register.
//
// Three layers are locked here, because each one failed a real guest in
// some earlier emulator and each is independently testable without a JIT:
//   1. the NID layer — AnyPS5's PS5 function-NID algorithm produces the
//      exact strings real images use as dynstr import names (golden
//      vectors computed independently of the implementation);
//   2. the registry — HLE symbols seeded by name become NID-resolvable;
//   3. the dispatch — DispatchImportCall resolves readable names and NID
//      strings to the HLE surface, keeps unimplemented imports on the
//      named ledger (RAX=0), refuses out-of-range indices, caches the
//      route after the first call, and reads 7th+ arguments off the
//      preserved guest stack.
// The stub BYTES themselves (mov r10,rcx; mov eax,base+idx; syscall; ret)
// are locked in import_trap_test through the real loader.
#include "kernel/sce_kernel_hle.h"
#include "loader/nid.h"
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

int main() {
    // The dispatch ledger writes to this run's own file (same discipline
    // as import_trap_test: every run owns its events).
    Evidence::SetLedgerPath("/tmp/px5_nid_dispatch_test_ledger.log");
    if (FILE* trunc = fopen("/tmp/px5_nid_dispatch_test_ledger.log", "w")) {
        fclose(trunc);
    }

    // ------------------------------------------------------------------
    // 1. NID algorithm — golden vectors from an independent reference
    //    (python hashlib, NOT this translation unit).
    // ------------------------------------------------------------------
    printf("\nNID algorithm (golden vectors):\n");
    chk(Nid::ComputeNidString("scePadOpen") == "xk0AcarP3V4",
        "scePadOpen -> xk0AcarP3V4");
    chk(Nid::ComputeNidString("sceAudioOutOpen") == "ekNvsT22rsY",
        "sceAudioOutOpen -> ekNvsT22rsY");
    chk(Nid::ComputeNidString("sceKernelIsNeoMode") == "WslcK1FQcGI",
        "sceKernelIsNeoMode -> WslcK1FQcGI");
    chk(Nid::ComputeNidString("") == "",
        "empty symbol -> empty NID (no throw)");
    chk(Nid::ComputeNidString("scePadOpen").size() == 11,
        "NID strings are exactly 11 characters");

    chk(Nid::IsNidString("xk0AcarP3V4"), "IsNidString accepts a real NID");
    chk(!Nid::IsNidString("scePadOpen"),
        "IsNidString rejects a readable symbol name");
    chk(!Nid::IsNidString("xk0AcarP3V"), "rejects 10 characters");
    chk(!Nid::IsNidString("xk0AcarP3V4~"), "rejects a non-alphabet byte");

    // ------------------------------------------------------------------
    // 2. Registry seeding through the HLE surface.
    // ------------------------------------------------------------------
    printf("\nNID registry (HLE symbols seeded):\n");
    auto& hle = SceKernelHle::KernelHle::GetInstance();
    hle.RegisterAll();
    const size_t symbols = hle.SymbolCount();
    chk(symbols > 20, "HLE surface has its symbol table");
    chk(Nid::NidRegistry::Instance().Size() == symbols,
        "every HLE symbol has a NID entry");
    chk(Nid::NidRegistry::Instance().Resolve("xk0AcarP3V4") == "scePadOpen",
        "NID resolves back to the symbol");
    chk(Nid::NidRegistry::Instance().Resolve("WslcK1FQcGI") ==
            "sceKernelIsNeoMode",
        "kernel symbol resolvable too");
    chk(Nid::NidRegistry::Instance().Resolve("AAAAAAAAAAA").empty(),
        "unknown NID resolves to empty (not a guess)");
    chk(Nid::NidRegistry::Instance().Resolve("scePadOpen").empty(),
        "readable name is NOT a NID lookup input");

    // ------------------------------------------------------------------
    // 3. DispatchImportCall — the transparent bridge.
    // ------------------------------------------------------------------
    printf("\nTransparent import dispatch:\n");
    auto& rl = RuntimeLinker::GetInstance();
    ImportTrapEntry readable;
    readable.name = "sceKernelIsNeoMode";
    ImportTrapEntry byNid;
    byNid.name = Nid::ComputeNidString("sceAudioOutInit");  // 11-char NID str
    ImportTrapEntry missing;
    missing.name = "sceDefinitelyNotImplemented";
    rl.SetImportTraps(0x5C000000ull, 0x5C000000ull + 3 * 16,
                      {readable, byNid, missing});

    ImportCall call;
    call.args[0] = 0;   // a1
    call.args[1] = 0;   // a2
    call.args[2] = 0;   // a3
    call.args[3] = 0;   // a4 (what the v1.45 stub would have LOST)
    call.args[4] = 0;   // a5
    call.args[5] = 0;   // a6

    const uint64_t directNeo =
        hle.InvokeByName("sceKernelIsNeoMode");
    const uint64_t viaImport = rl.DispatchImportCall(0, call);
    chk(viaImport == directNeo,
        "readable name dispatch == direct InvokeByName result");

    const uint64_t directInit =
        hle.InvokeByName("sceAudioOutInit");
    const uint64_t viaNid = rl.DispatchImportCall(1, call);
    chk(viaNid == directInit,
        "NID-string dispatch reaches the real audio HLE");

    chk(rl.DispatchImportCall(2, call) == 0,
        "unimplemented import keeps the ledger path (RAX=0)");
    chk(rl.DispatchImportCall(9999, call) == 0,
        "out-of-range index refused (returns 0, counted)");

    // Route caching: the second call must still dispatch (not degrade to
    // the ledger after the first resolution).
    chk(rl.DispatchImportCall(0, call) == directNeo,
        "repeat dispatch uses the cached route");

    const std::string summary = rl.GetImportTrapSummary();
    printf("  summary: %s\n", summary.c_str());
    // Dispatches were: idx0 (hle), idx1 (hle), idx2 (unresolved), OOB 9999
    // (never reaches hit accounting), idx0 repeat (hle). So 4 hits over 3
    // distinct imports, 3 HLE dispatches, 1 unresolved.
    chk(summary.find("hits=4") != std::string::npos &&
        summary.find("distinct=3") != std::string::npos,
        "hit accounting shared with the trap ledger (OOB not counted)");
    chk(summary.find("hle-calls=3") != std::string::npos &&
        summary.find("unresolved=1") != std::string::npos,
        "dispatch counters: 3 HLE calls, 1 unresolved");

    // ------------------------------------------------------------------
    // 4. Stack arguments — 7th+ arguments ride the preserved guest rsp.
    // ------------------------------------------------------------------
    printf("\nStack arguments (ImportCall::ReadStackArg):\n");
    auto& mm = MemoryManager::GetInstance();
    chk(mm.Initialize(4096), "memory window initializes");
    const uint64_t base = mm.GetGuestBase();
    const uint64_t stackVa = base + 0x30000000ull;
    // A synthetic call frame: [rsp] = return address, [rsp+8] = arg7,
    // [rsp+16] = arg8.
    const uint64_t frame[3] = {0xDEAD0000BEEF0000ull, 0x1111111111111111ull,
                               0x2222222222222222ull};
    chk(mm.MapMemory(stackVa, 64,
                     MemoryFlags::PAGE_READ | MemoryFlags::PAGE_WRITE,
                     "nid_dispatch_stack"),
        "guest stack page maps");
    chk(mm.WriteGuestMemory(stackVa, frame, sizeof frame),
        "frame written into the window");
    ImportCall stackCall;
    stackCall.guestRsp = stackVa;
    uint64_t arg7 = 0, arg8 = 0;
    chk(stackCall.ReadStackArg(0, &arg7) && arg7 == 0x1111111111111111ull,
        "arg7 read from [rsp+8]");
    chk(stackCall.ReadStackArg(1, &arg8) && arg8 == 0x2222222222222222ull,
        "arg8 read from [rsp+16]");
    uint64_t scratch = 0;
    ImportCall badStack;
    badStack.guestRsp = base + 0x3F000000ull;   // never mapped
    chk(!badStack.ReadStackArg(0, &scratch),
        "untranslated stack refuses (no guess)");
    chk(!stackCall.ReadStackArg(0, nullptr),
        "null out-pointer refuses");

    // ------------------------------------------------------------------
    printf("\n%s\n", fails == 0
                         ? "nid_dispatch_test: ALL PASS"
                         : "nid_dispatch_test: FAILURES PRESENT");
    return fails == 0 ? 0 : 1;
}
