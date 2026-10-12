// PX5 v1.67 — C++ ABI runtime host test.
//
// Locks the guard machinery with REAL contention (two host threads racing
// one static-local guard: exactly one initializer runs, the loser blocks
// then observes done), the exception registry round-trip, and the named
// refusals. The guest guard bytes live in the real window.
#include "kernel/cxxabi_hle.h"
#include "kernel/libc_hle.h"
#include "kernel/sce_kernel_hle.h"   // SymbolEntry
#include "memory/memory.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

using PX5::SceKernelHle::CxxAbiHle;
using PX5::SceKernelHle::LibcHle;
using PX5::ImportCall;
using Mgr = PX5::MemoryManager;

static int g_failures = 0;
#define CHECK(cond, msg)                                                    \
    do {                                                                    \
        if (!(cond)) {                                                      \
            ++g_failures;                                                   \
            std::printf("  FAIL: %s (line %d)\n", msg, __LINE__);           \
        }                                                                   \
    } while (0)

static uint64_t Call(const char* symbol, std::initializer_list<uint64_t> args) {
    ImportCall c{};
    size_t i = 0;
    for (uint64_t a : args) c.args[i++] = a;
    return CxxAbiHle::Instance().Invoke(symbol, c, nullptr);
}

int main() {
    std::printf("[cxxabi_hle_test] window init\n");
    auto& mm = Mgr::GetInstance();
    CHECK(mm.Initialize() && mm.GetGuestBase() != 0, "window initialized");

    const uint64_t guardVa = mm.GetGuestBase() + 0x30002000;
    CHECK(mm.MapMemory(guardVa & ~0xFFFull, 0x1000,
                       PX5::MemoryFlags::PAGE_READ |
                           PX5::MemoryFlags::PAGE_WRITE,
                       "cxxabi_test") != 0,
          "guard page mapped");
    auto* guardBytes = static_cast<uint8_t*>(mm.GetHostPointer(guardVa));
    std::memset(guardBytes, 0, 16);

    // ------------------------------------------------------------------
    // guard: acquire -> release marks done; second acquire returns 0
    // ------------------------------------------------------------------
    std::printf("[cxxabi_hle_test] guard lifecycle\n");
    CHECK(Call("__cxa_guard_acquire", {guardVa}) == 1,
          "first acquire returns 1 (caller initializes)");
    CHECK(guardBytes[0] == 0 && guardBytes[1] == 1,
          "guest bytes: done=0 in-use=1 while initializing");
    CHECK(Call("__cxa_guard_release", {guardVa}) == 0, "release ok");
    CHECK(guardBytes[0] == 1 && guardBytes[1] == 0,
          "guest bytes: done=1 in-use=0 after release");
    CHECK(Call("__cxa_guard_acquire", {guardVa}) == 0,
          "second acquire returns 0 (already done)");

    // ------------------------------------------------------------------
    // guard: real contention — loser blocks, both threads survive, one init
    // ------------------------------------------------------------------
    std::printf("[cxxabi_hle_test] guard contention\n");
    const uint64_t guard2 = guardVa + 0x40;
    std::atomic<int> initRuns{0};
    std::atomic<bool> startRace{false};
    auto worker = [&](int id) {
        while (!startRace.load()) std::this_thread::yield();
        if (Call("__cxa_guard_acquire", {guard2}) == 1) {
            initRuns.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            Call("__cxa_guard_release", {guard2});
        }
        (void)id;
    };
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) threads.emplace_back(worker, i);
    startRace = true;
    for (auto& t : threads) t.join();
    CHECK(initRuns.load() == 1, "exactly one initializer ran under contention");
    CHECK(guardBytes[0x40] == 1, "guard2 committed done");

    // abort path: acquire (in-use) then abort reopens for a retry
    CHECK(Call("__cxa_guard_acquire", {guard2 + 0x40}) == 1, "guard3 acquire");
    CHECK(guardBytes[0x80] == 0 && guardBytes[0x81] == 1, "guard3 in-use");
    CHECK(Call("__cxa_guard_abort", {guard2 + 0x40}) == 0, "guard3 abort");
    CHECK(guardBytes[0x80] == 0 && guardBytes[0x81] == 0,
          "guard3 reopened (retryable)");
    CHECK(Call("__cxa_guard_acquire", {guard2 + 0x40}) == 1,
          "guard3 retry acquires");

    // ------------------------------------------------------------------
    // exception registry: allocate -> throw -> begin_catch -> end_catch
    // ------------------------------------------------------------------
    std::printf("[cxxabi_hle_test] exception registry\n");
    const uint64_t excVa = Call("__cxa_allocate_exception", {64});
    CHECK(excVa != 0, "exception object allocated from the guest heap");
    {
        // heap truth: the allocated VA is writable window memory
        auto* p = mm.GetHostPointer(excVa);
        CHECK(p != nullptr, "exception object is window memory");
        if (p) std::memset(p, 0xAB, 64);
    }
    CHECK(Call("__cxa_throw", {excVa, 0x140123000ull, 0}) == 0, "throw records");
    CHECK(Call("__cxa_get_exception_ptr", {0}) == excVa,
          "pending exception visible");
    CHECK(Call("__cxa_begin_catch", {excVa}) == excVa,
          "begin_catch hands back the object");
    CHECK(Call("__cxa_end_catch", {0}) == 0, "end_catch clears");
    CHECK(Call("__cxa_get_exception_ptr", {0}) == 0,
          "pending slot empty after end_catch");
    CHECK(Call("__cxa_free_exception", {excVa}) == 0, "free is a named no-op");

    // rethrow keeps the object; begin_catch still sees it
    CHECK(Call("__cxa_throw", {excVa, 0x140123000ull, 0}) == 0,
          "second throw");
    CHECK(Call("__cxa_rethrow", {0}) == 0, "rethrow ok");
    CHECK(Call("__cxa_begin_catch", {excVa}) == excVa,
          "rethrown object still pending");
    CHECK(Call("__cxa_end_catch", {0}) == 0, "final end_catch");

    // _Unwind_Resume: fatal path returns 0 and is ledgered (loud)
    CHECK(Call("_Unwind_Resume", {excVa, 0}) == 0, "unwind_resume returns");

    auto stats = CxxAbiHle::Instance().GetStats();
    std::printf("[cxxabi_hle_test] stats: guards=%llu contended=%llu "
                "thrown=%llu caught=%llu\n",
                (unsigned long long)stats.guardAcquires,
                (unsigned long long)stats.guardContended,
                (unsigned long long)stats.exceptionsThrown,
                (unsigned long long)stats.exceptionsCaught);
    CHECK(stats.guardAcquires >= 2, "guard counter moved");
    CHECK(stats.exceptionsThrown >= 2, "throw counter moved");

    if (g_failures == 0) {
        std::printf("[cxxabi_hle_test] ALL PASS\n");
        return 0;
    }
    std::printf("[cxxabi_hle_test] FAILURES: %d\n", g_failures);
    return 1;
}
