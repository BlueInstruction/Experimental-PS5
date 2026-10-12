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
    // guard: REAL contention — three losers race one in-use guard; the
    // outcome (no double-init) is asserted deterministically.
    // ------------------------------------------------------------------
    std::printf("[cxxabi_hle_test] guard contention (outcome)\n");
    const uint64_t guard2 = guardVa + 0x40;
    CHECK(Call("__cxa_guard_acquire", {guard2}) == 1, "holder acquires first");
    CHECK(guardBytes[0x40] == 0 && guardBytes[0x41] == 1,
          "guest bytes: done=0 in-use=1 while the holder initializes");
    {
        std::atomic<int> entered{0};
        std::atomic<int> loserZeros{0};
        std::atomic<bool> go{false};
        auto loser = [&] {
            while (!go.load()) std::this_thread::yield();
            entered.fetch_add(1);
            if (Call("__cxa_guard_acquire", {guard2}) == 0)
                loserZeros.fetch_add(1);   // blocked, then saw done=1
        };
        std::vector<std::thread> losers;
        for (int i = 0; i < 3; ++i) losers.emplace_back(loser);
        go.store(true);
        const auto enterDeadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (entered.load() < 3 &&
               std::chrono::steady_clock::now() < enterDeadline)
            std::this_thread::yield();
        CHECK(entered.load() == 3, "all three losers reached the acquire");
        CHECK(Call("__cxa_guard_release", {guard2}) == 0, "holder releases");
        for (auto& t : losers) t.join();
        CHECK(loserZeros.load() == 3,
              "all three losers observed done (none initialized)");
        CHECK(guardBytes[0x40] == 1 && guardBytes[0x41] == 0,
              "guard2 committed done=1 in-use=0");
    }
    // second acquire after commit -> 0 (double-init impossible)
    CHECK(Call("__cxa_guard_acquire", {guard2}) == 0,
          "post-commit acquire returns 0");

    // ------------------------------------------------------------------
    // v1.69 — the BLOCKED WAIT is proven deterministically: a worker holds
    // the guard, MAIN blocks on it, and a gate thread opens the release
    // only after the parked-waiter counter shows main is blocked (the
    // counter increments the moment the wait begins). No sleep-based
    // window: the release cannot happen before the wait is observable.
    // ------------------------------------------------------------------
    std::printf("[cxxabi_hle_test] guard blocking (deterministic)\n");
    {
        const uint64_t guardB = guardVa + 0xC0;
        std::memset(guardBytes + 0xC0, 0, 16);
        std::atomic<bool> holderHolds{false};
        std::atomic<bool> mayRelease{false};
        std::atomic<bool> parked{false};
        const auto contendedBefore =
            CxxAbiHle::Instance().GetStats().guardContended;
        std::thread holder([&] {
            CHECK(Call("__cxa_guard_acquire", {guardB}) == 1,
                  "holder worker acquires");
            holderHolds.store(true);
            while (!mayRelease.load()) std::this_thread::yield();
            CHECK(Call("__cxa_guard_release", {guardB}) == 0,
                  "holder worker releases");
        });
        for (int i = 0;
             i < 4000 && !holderHolds.load(); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        CHECK(holderHolds.load(), "holder owns the guard");
        std::thread gate([&] {
            // opens the release gate ONLY once main's park is observable —
            // bounded, so a semantics bug fails the check instead of hanging
            const auto dl =
                std::chrono::steady_clock::now() + std::chrono::seconds(2);
            while (CxxAbiHle::Instance().GetStats().guardContended <=
                       contendedBefore &&
                   std::chrono::steady_clock::now() < dl)
                std::this_thread::yield();
            parked.store(CxxAbiHle::Instance().GetStats().guardContended >
                         contendedBefore);
            mayRelease.store(true);
        });
        const auto rc = Call("__cxa_guard_acquire", {guardB});   // BLOCKS
        gate.join();
        holder.join();
        CHECK(parked.load(),
              "main parked on the in-use guard (observed pre-release)");
        CHECK(rc == 0, "blocked acquire returns 0 after the holder commits");
        CHECK(guardBytes[0xC0] == 1 && guardBytes[0xC1] == 0,
              "guardB committed done=1 in-use=0");
    }

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
    // v1.69 — the allocation goes through memalign(alignment, size): a
    // non-power-of-two object size must yield a real 16-aligned window
    // address (the reversed-argument bug returned the EINVAL code itself
    // as the "object address").
    // ------------------------------------------------------------------
    std::printf("[cxxabi_hle_test] exception registry\n");
    const uint64_t excVa = Call("__cxa_allocate_exception", {64});
    CHECK(excVa != 0, "exception object allocated from the guest heap");
    CHECK((excVa & 15) == 0, "exception object is 16-aligned");
    CHECK(mm.IsValidAddress(excVa, 64), "exception object is window memory");
    {
        const uint64_t excOdd = Call("__cxa_allocate_exception", {120});
        CHECK(excOdd != 0 && (excOdd & 15) == 0,
              "120-byte object allocates aligned (memalign arg order fixed)");
        CHECK(mm.IsValidAddress(excOdd, 120),
              "120-byte object is window memory (not an error code)");
    }
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

    // ------------------------------------------------------------------
    // v1.69 — session reset voids stale pending exceptions: the generation
    // bump makes the old thread_local record read as "none" instead of
    // pointing into a dead guest window.
    // ------------------------------------------------------------------
    std::printf("[cxxabi_hle_test] reset voids stale pending exception\n");
    CHECK(Call("__cxa_throw", {excVa, 0x140123000ull, 0}) == 0, "throw for reset");
    CxxAbiHle::Instance().ResetForSession();
    CHECK(Call("__cxa_get_exception_ptr", {0}) == 0,
          "pending exception void after reset (generation bump)");
    CHECK(Call("__cxa_begin_catch", {excVa}) == 0,
          "begin_catch on a stale record returns none");
    // a throw/catch round-trip works again in the new session
    CHECK(Call("__cxa_throw", {excVa, 0x140123000ull, 0}) == 0, "fresh throw");
    CHECK(Call("__cxa_begin_catch", {excVa}) == excVa,
          "fresh session registry is live");
    CHECK(Call("__cxa_end_catch", {0}) == 0, "fresh end_catch");

    if (g_failures == 0) {
        std::printf("[cxxabi_hle_test] ALL PASS\n");
        return 0;
    }
    std::printf("[cxxabi_hle_test] FAILURES: %d\n", g_failures);
    return 1;
}
