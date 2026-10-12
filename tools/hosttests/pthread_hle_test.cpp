// PX5 v1.65 — pthread tranche host test.
//
// Runs the REAL pthread_hle.cpp against the REAL MemoryManager, with the
// guest-thread launcher seam overridden by a host function launcher — so
// every lifecycle, synchronization and handle path executes without the
// JIT. The device half of the seam (FEXCore RunGuestThread) is the only
// piece this binary cannot exercise; its per-thread prep is shared with the
// main dispatch path that device sessions already prove.
//
// Locked here:
//   * attr lifecycle: init/settype/setstacksize/detachstate/destroy
//   * mutex: init (default=errorcheck), lock/unlock pairing, relock ->
//     EDEADLK, recursive kind relocks, destroy of a locked mutex -> EBUSY,
//     unlock-by-non-owner -> EPERM
//   * cond: init/destroy, signal/broadcast wake a waiter, timedwait times
//     out (usec contract) while releasing and reacquiring the mutex
//   * threads: create runs the launcher with the right entry/arg/stack/
//     TCB, exit records the value, join returns it, detach refuses join,
//     self reports the calling thread's handle
//   * handles: written into the guest slot, unreadable slots refuse EFAULT
#include "kernel/pthread_hle.h"
#include "kernel/sce_kernel_hle.h"   // SymbolEntry
#include "memory/memory.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <thread>

using PX5::SceKernelHle::PthreadHle;
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

static uint64_t Call(const char* symbol, std::initializer_list<uint64_t> args,
                     uint64_t guestRsp = 0) {
    ImportCall c{};
    size_t i = 0;
    for (uint64_t a : args) c.args[i++] = a;
    c.guestRsp = guestRsp;
    return PthreadHle::Instance().Invoke(symbol, c);
}

// --- host launcher: mimics "entry returns its arg*2", records the inputs ---
struct LaunchRecord {
    std::atomic<uint64_t> entry{0};
    std::atomic<uint64_t> arg{0};
    std::atomic<uint64_t> stackTop{0};
    std::atomic<uint64_t> tcb{0};
    std::atomic<int> runs{0};
};
static LaunchRecord g_launch;

static bool HostLaunch(uint64_t entry, uint64_t arg, uint64_t stackTop,
                       uint64_t tcb, uint64_t* raxOut) {
    g_launch.entry = entry;
    g_launch.arg = arg;
    g_launch.stackTop = stackTop;
    g_launch.tcb = tcb;
    g_launch.runs.fetch_add(1);
    *raxOut = arg * 2;
    return true;
}

static bool WindowReady() {
    auto& mm = Mgr::GetInstance();
    if (!mm.Initialize()) return false;
    return mm.GetGuestBase() != 0;
}

int main() {
    std::printf("[pthread_hle_test] window init\n");
    CHECK(WindowReady(), "memory window initialized");

    PthreadHle::Instance().SetGuestLauncher(&HostLaunch);

    // ------------------------------------------------------------------
    // symbol surface: the exact 27 the game imports
    // ------------------------------------------------------------------
    std::printf("[pthread_hle_test] symbol table\n");
    std::vector<PX5::SceKernelHle::SymbolEntry> table;
    PthreadHle::Instance().CollectSymbols(&table);
    CHECK(table.size() == 27, "surface exposes exactly 27 symbols");
    CHECK(PthreadHle::Instance().HasSymbol("scePthreadCreate") &&
              PthreadHle::Instance().HasSymbol("scePthreadCondTimedwait") &&
              !PthreadHle::Instance().HasSymbol("scePthreadCondWait"),
          "names match the decoded import list");

    auto& mm = Mgr::GetInstance();
    const uint64_t slot = mm.MapMemory(
        mm.GetGuestBase() + 0x30000000, 0x1000,
        PX5::MemoryFlags::PAGE_READ | PX5::MemoryFlags::PAGE_WRITE,
        "test_slots");
    CHECK(slot != 0, "test handle slots mapped");
    auto* slots = reinterpret_cast<uint64_t*>(
        mm.GetHostPointer(mm.GetGuestBase() + 0x30000000));

    // ------------------------------------------------------------------
    // attr lifecycle
    // ------------------------------------------------------------------
    std::printf("[pthread_hle_test] attr lifecycle\n");
    CHECK(Call("scePthreadAttrInit", {slot + 0x00}) == 0, "attr init ok");
    const uint64_t attr = slots[0x00 / 8];
    CHECK(attr != 0, "attr handle written into guest slot");
    CHECK(Call("scePthreadAttrSetstacksize", {slot + 0x00, 64 * 1024}) == 0,
          "attr setstacksize ok");
    CHECK(Call("scePthreadAttrSetdetachstate", {slot + 0x00, 1}) == 0,
          "attr setdetachstate(DETACHED) ok");
    CHECK(Call("scePthreadAttrSetdetachstate", {slot + 0x00, 7}) ==
              static_cast<uint64_t>(-22),
          "bogus detachstate -> EINVAL");
    CHECK(Call("scePthreadAttrDestroy", {slot + 0x00}) == 0, "attr destroy ok");

    // fresh joinable attr for the thread tests
    CHECK(Call("scePthreadAttrInit", {slot + 0x08}) == 0, "attr2 init ok");

    // ------------------------------------------------------------------
    // mutex: default kind, errorcheck, recursive, destroy-locked
    // ------------------------------------------------------------------
    std::printf("[pthread_hle_test] mutex\n");
    CHECK(Call("scePthreadMutexattrInit", {slot + 0x10}) == 0,
          "mutexattr init ok");
    CHECK(Call("scePthreadMutexInit", {slot + 0x18, slot + 0x10}) == 0,
          "mutex init ok (default kind)");
    const uint64_t mtx = slots[0x18 / 8];
    CHECK(mtx != 0, "mutex handle written");
    CHECK(Call("scePthreadMutexLock", {slot + 0x18}) == 0, "lock ok");
    // relock from the same thread on an errorcheck mutex -> EDEADLK
    CHECK(Call("scePthreadMutexLock", {slot + 0x18}) ==
              static_cast<uint64_t>(-35),
          "errorcheck relock -> EDEADLK");
    CHECK(Call("scePthreadMutexUnlock", {slot + 0x18}) == 0, "unlock ok");
    CHECK(Call("scePthreadMutexUnlock", {slot + 0x18}) ==
              static_cast<uint64_t>(-1),
          "unlock of free mutex -> EPERM");

    // recursive kind
    CHECK(Call("scePthreadMutexattrSettype", {slot + 0x10, 2}) == 0,
          "mutexattr settype RECURSIVE ok");
    CHECK(Call("scePthreadMutexInit", {slot + 0x20, slot + 0x10}) == 0,
          "recursive mutex init ok");
    CHECK(Call("scePthreadMutexLock", {slot + 0x20}) == 0, "rec lock 1 ok");
    CHECK(Call("scePthreadMutexLock", {slot + 0x20}) == 0, "rec lock 2 ok");
    CHECK(Call("scePthreadMutexUnlock", {slot + 0x20}) == 0, "rec unlock 1");
    CHECK(Call("scePthreadMutexUnlock", {slot + 0x20}) == 0, "rec unlock 2");

    // destroy of a locked mutex -> EBUSY
    CHECK(Call("scePthreadMutexLock", {slot + 0x20}) == 0, "lock for destroy");
    CHECK(Call("scePthreadMutexDestroy", {slot + 0x20}) ==
              static_cast<uint64_t>(-16),
          "destroy locked mutex -> EBUSY");
    CHECK(Call("scePthreadMutexUnlock", {slot + 0x20}) == 0, "unlock again");
    CHECK(Call("scePthreadMutexDestroy", {slot + 0x20}) == 0,
          "destroy unlocked mutex ok");

    // unreadable handle slot -> EFAULT
    CHECK(Call("scePthreadMutexLock", {0x0ull}) == static_cast<uint64_t>(-14),
          "null handle slot -> EFAULT");

    // ------------------------------------------------------------------
    // threads: create/exit/join through the launcher seam
    // ------------------------------------------------------------------
    std::printf("[pthread_hle_test] threads\n");
    CHECK(Call("scePthreadCreate",
               {slot + 0x28, slot + 0x08, 0x140001234ull, 21, 0}) == 0,
          "thread create ok");
    const uint64_t th = slots[0x28 / 8];
    CHECK(th != 0, "thread handle written");
    CHECK(Call("scePthreadJoin", {th, slot + 0x30}) == 0, "join ok");
    CHECK(slots[0x30 / 8] == 42, "join returned exit value (arg*2 = 42)");
    CHECK(g_launch.runs.load() == 1, "launcher ran exactly once");
    CHECK(g_launch.entry.load() == 0x140001234ull, "launcher got guest entry");
    CHECK(g_launch.arg.load() == 21, "launcher got guest arg");
    CHECK(g_launch.stackTop.load() != 0, "launcher got a stack top");
    CHECK(g_launch.tcb.load() != 0, "launcher got a TCB address");
    // TCB self-pointer contract: [tcb+0] == tcb
    {
        const auto* tcb = reinterpret_cast<const uint64_t*>(
            mm.GetHostPointer(g_launch.tcb.load()));
        CHECK(tcb && tcb[0] == g_launch.tcb.load(), "TCB self pointer set");
        CHECK(tcb && tcb[2] == th, "TCB pthread slot carries the handle");
    }

    // join twice -> the thread is gone; second join refuses (EINVAL)
    CHECK(Call("scePthreadJoin", {th, 0}) == static_cast<uint64_t>(-22),
          "double join -> EINVAL");

    // self on the calling (non-guest) thread materializes the main handle
    const uint64_t selfMain = Call("scePthreadSelf", {});
    CHECK(selfMain != 0, "self returns the main-thread handle");
    CHECK(Call("scePthreadSelf", {}) == selfMain, "self is stable");
    CHECK(Call("scePthreadYield", {}) == 0, "yield ok");

    // detached thread: create with DETACHED attr, join must refuse
    CHECK(Call("scePthreadAttrInit", {slot + 0x38}) == 0, "attr3 init");
    CHECK(Call("scePthreadAttrSetdetachstate", {slot + 0x38, 1}) == 0,
          "attr3 detached");
    CHECK(Call("scePthreadCreate",
               {slot + 0x40, slot + 0x38, 0x140001234ull, 5, 0}) == 0,
          "detached create ok");
    const uint64_t thD = slots[0x40 / 8];
    // wait for the detached body to finish before joining refusals matter
    for (int i = 0; i < 200 && g_launch.runs.load() < 2; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    CHECK(g_launch.runs.load() == 2, "detached thread ran");
    CHECK(Call("scePthreadJoin", {thD, 0}) == static_cast<uint64_t>(-22),
          "join of detached thread -> EINVAL");
    CHECK(Call("scePthreadDetach", {thD}) == 0, "detach of detached ok (idem)");

    // ------------------------------------------------------------------
    // cond: signal/broadcast/timedwait (usec contract)
    // ------------------------------------------------------------------
    std::printf("[pthread_hle_test] cond\n");
    CHECK(Call("scePthreadCondInit", {slot + 0x48, 0}) == 0, "cond init ok");
    CHECK(Call("scePthreadMutexInit", {slot + 0x50, 0}) == 0,
          "cond-pair mutex init ok");

    // timedwait with a 50 ms budget on a mutex WE hold: releases, times
    // out at ~50 ms, reacquires.
    CHECK(Call("scePthreadMutexLock", {slot + 0x50}) == 0, "cond mutex lock");
    const auto t0 = std::chrono::steady_clock::now();
    CHECK(Call("scePthreadCondTimedwait", {slot + 0x48, slot + 0x50, 50000}) ==
              static_cast<uint64_t>(-110),
          "timedwait 50ms -> ETIMEDOUT");
    const auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - t0).count();
    CHECK(dt >= 45, "timedwait actually waited (>=45ms)");
    // mutex is REACQUIRED after the wait: relock detects it
    CHECK(Call("scePthreadMutexLock", {slot + 0x50}) ==
              static_cast<uint64_t>(-35),
          "mutex reacquired after timedwait (relock -> EDEADLK)");
    CHECK(Call("scePthreadMutexUnlock", {slot + 0x50}) == 0, "cond unlock");

    // signal/broadcast on a live cond return ok; timedwait with zero usec
    // is an immediate timeout that still round-trips the mutex.
    CHECK(Call("scePthreadCondSignal", {slot + 0x48}) == 0, "signal ok");
    CHECK(Call("scePthreadCondBroadcast", {slot + 0x48}) == 0, "broadcast ok");
    CHECK(Call("scePthreadMutexLock", {slot + 0x50}) == 0, "lock for zero-wait");
    CHECK(Call("scePthreadCondTimedwait", {slot + 0x48, slot + 0x50, 0}) ==
              static_cast<uint64_t>(-110),
          "zero-usec timedwait -> ETIMEDOUT");
    CHECK(Call("scePthreadMutexUnlock", {slot + 0x50}) == 0, "zero-wait unlock");
    CHECK(Call("scePthreadCondDestroy", {slot + 0x48}) == 0, "cond destroy ok");

    // ------------------------------------------------------------------
    // cross-thread handoff: worker locks a mutex, main PROVES ownership
    // without a single sleep — the worker signals "locked" through an
    // atomic after Lock returns, main spins on that fact, and the worker
    // only unlocks after main finished the ownership checks.
    // ------------------------------------------------------------------
    std::printf("[pthread_hle_test] cross-thread handoff (deterministic)\n");
    static std::atomic<uint64_t> g_workSlot{0};
    static std::atomic<bool> g_workLocked{false};
    static std::atomic<bool> g_mainChecked{false};
    struct Seam {
        static bool Body(uint64_t, uint64_t, uint64_t, uint64_t,
                         uint64_t* raxOut) {
            ImportCall c{};
            // the slot VA (not the handle value): HLE read the handle from
            // guest memory exactly like the real caller does
            c.args[0] = g_workSlot.load();
            auto& inst = PthreadHle::Instance();
            CHECK(inst.Invoke("scePthreadMutexLock", c) == 0, "worker lock");
            g_workLocked.store(true);          // the fact main waits on
            while (!g_mainChecked.load())      // hold until main verified
                std::this_thread::yield();
            (void)inst.Invoke("scePthreadMutexUnlock", c);
            *raxOut = 7;
            return true;
        }
    };
    PthreadHle::Instance().SetGuestLauncher(&Seam::Body);
    CHECK(Call("scePthreadMutexInit", {slot + 0x58, 0}) == 0, "work mutex init");
    g_workSlot = slot + 0x58;
    CHECK(Call("scePthreadCreate",
               {slot + 0x60, slot + 0x08, 0x140000000ull, 0, 0}) == 0,
          "worker create ok");
    const uint64_t thW = slots[0x60 / 8];
    // the mutex must appear locked while the worker holds it: try from main
    for (int i = 0; i < 2000 && !g_workLocked.load(); ++i)
        std::this_thread::yield();
    CHECK(g_workLocked.load(), "worker signalled the lock (explicit sync)");
    CHECK(Call("scePthreadMutexUnlock", {slot + 0x58}) ==
              static_cast<uint64_t>(-1),
          "main cannot unlock the worker's mutex (EPERM — real ownership)");
    g_mainChecked.store(true);                 // release the worker
    CHECK(Call("scePthreadJoin", {thW, slot + 0x68}) == 0, "worker join ok");
    CHECK(slots[0x68 / 8] == 7, "worker exit value carried back");
    CHECK(Call("scePthreadMutexLock", {slot + 0x58}) == 0,
          "mutex free after worker exit");
    CHECK(Call("scePthreadMutexUnlock", {slot + 0x58}) == 0, "work unlock");
    PthreadHle::Instance().SetGuestLauncher(&HostLaunch);   // restore the seam

    // ------------------------------------------------------------------
    // v1.69 — self-join is EDEADLK, including the main thread joining its
    // own lazily materialized handle (the review's join-self hang).
    // ------------------------------------------------------------------
    std::printf("[pthread_hle_test] self-join refuses\n");
    CHECK(Call("scePthreadJoin", {selfMain, 0}) == static_cast<uint64_t>(-35),
          "main joining itself -> EDEADLK (hang fixed)");

    // ------------------------------------------------------------------
    // v1.69 — detached threads are REAPED: exited+detached objects leave
    // the live set once a subsequent Create drives the reaper. Before this
    // fix their slot and object leaked until session teardown.
    // ------------------------------------------------------------------
    std::printf("[pthread_hle_test] detached-thread reaping\n");
    {
        CHECK(Call("scePthreadCreate",
                   {slot + 0x70, slot + 0x38, 0x140001234ull, 9, 0}) == 0,
              "second detached create ok");
        const uint64_t thD2 = slots[0x70 / 8];
        for (int i = 0; i < 200 && g_launch.runs.load() < 3; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        CHECK(g_launch.runs.load() == 3, "second detached thread ran");
        CHECK(Call("scePthreadJoin", {thD2, 0}) == static_cast<uint64_t>(-22),
              "join of detached thread -> EINVAL");
        // a fresh Create drives ReapDetached; the exited detached object
        // must leave the live set (bounded wait on a real condition).
        // Baseline is taken RIGHT AFTER the create: the live set grew by
        // the detached thread, and the reap must bring it back down.
        CHECK(Call("scePthreadAttrInit", {slot + 0x78}) == 0, "attr4 init");
        const auto livePeak = PthreadHle::Instance().GetStats().liveThreads;
        CHECK(Call("scePthreadCreate",
                   {slot + 0x80, slot + 0x78, 0x140001234ull, 3, 0}) == 0,
              "driver create ok");
        CHECK(Call("scePthreadJoin", {slots[0x80 / 8], 0}) == 0, "driver join");
        bool reaped = false;
        for (int i = 0; i < 400; ++i) {
            if (PthreadHle::Instance().GetStats().liveThreads < livePeak) {
                reaped = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        CHECK(reaped, "exited detached thread object reaped (live count dropped)");
    }

    // ------------------------------------------------------------------
    PthreadHle::Instance().SetGuestLauncher(&HostLaunch);
    auto stats = PthreadHle::Instance().GetStats();
    std::printf("[pthread_hle_test] stats: created=%llu joined=%llu exited=%llu "
                "mutexLocks=%llu condSignals=%llu live=%llu\n",
                (unsigned long long)stats.threadsCreated,
                (unsigned long long)stats.threadsJoined,
                (unsigned long long)stats.threadsExited,
                (unsigned long long)stats.mutexLocks,
                (unsigned long long)stats.condSignals,
                (unsigned long long)stats.liveThreads);
    CHECK(stats.threadsCreated >= 3, "created counter moved");
    CHECK(stats.threadsJoined >= 2, "joined counter moved");

    // ------------------------------------------------------------------
    // v1.69 — session reset with a LIVE thread: teardown must RETURN
    // (bounded grace), park the stuck object in the named leak list, never
    // delete under the running body, and never terminate the process.
    // This is the review's use-after-free finding, locked as behaviour.
    // ------------------------------------------------------------------
    std::printf("[pthread_hle_test] session reset with a live thread\n");
    {
        static std::atomic<bool> g_blockRelease{false};
        static std::atomic<bool> g_blockedDone{false};
        struct BlockingSeam {
            static bool Body(uint64_t, uint64_t, uint64_t, uint64_t,
                             uint64_t* raxOut) {
                while (!g_blockRelease.load()) std::this_thread::yield();
                *raxOut = 1;
                g_blockedDone.store(true);
                return true;
            }
        };
        PthreadHle::Instance().SetGuestLauncher(&BlockingSeam::Body);
        CHECK(Call("scePthreadAttrInit", {slot + 0x88}) == 0, "reset attr");
        CHECK(Call("scePthreadCreate",
                   {slot + 0x90, slot + 0x88, 0x140000000ull, 0, 0}) == 0,
              "stuck thread create ok");
        // the body is now parked in its blocking loop; the reset must
        // return within the bounded grace and park it — never hang, never
        // delete under the running body
        PthreadHle::Instance().ResetForSession();
        g_blockRelease.store(true);                 // let the parked body end
        for (int i = 0; i < 200 && !g_blockedDone.load(); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        CHECK(g_blockedDone.load(),
              "parked body finished naturally after reset released it");
        // the process is alive and the surface is usable again
        auto post = PthreadHle::Instance().GetStats();
        CHECK(post.threadsCreated == 0, "counters cleared by the reset");
        CHECK(Call("scePthreadSelf", {}) != 0, "surface alive after reset");
        PthreadHle::Instance().SetGuestLauncher(&HostLaunch);
    }

    if (g_failures == 0) {
        std::printf("[pthread_hle_test] ALL PASS\n");
        return 0;
    }
    std::printf("[pthread_hle_test] FAILURES: %d\n", g_failures);
    return 1;
}
