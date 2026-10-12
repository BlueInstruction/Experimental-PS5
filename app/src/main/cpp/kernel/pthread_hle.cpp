// PX5 v1.65 — the pthread tranche implementation.
//
// See pthread_hle.h for the design contract. This file implements:
//   * the 27 imported scePthread* symbols with real POSIX semantics,
//   * a hand-rolled mutex (errorcheck/recursive/normal) because std::mutex
//     cannot express relock-detection, and a hand-rolled condvar wait that
//     releases that mutex while blocked,
//   * guest threads running REAL guest code through the launcher seam
//     (device: FEXCore dispatcher; host tests: direct function launcher),
//   * per-thread TCBs built from the published image PT_TLS geometry
//     (FreeBSD variant II, same recipe as the main thread — v1.40).
#include "pthread_hle.h"

#include "sce_kernel_hle.h"   // SymbolEntry

#include "../memory/memory.h"
#include "../utils/logger.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <ctime>
#include <functional>
#include <map>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>

namespace PX5::SceKernelHle {

namespace {

using MemoryManager = ::PX5::MemoryManager;

// ---------------------------------------------------------------------------
// error codes: this floor standardizes on signed errno (sce_kernel_hle.h
// convention). The real console returns ORBIS 0x8002xxxx codes — named in
// the header; a game that compares those will say so on the ledger.
// ---------------------------------------------------------------------------
constexpr int64_t kErrPerm     = -1;     // EPERM
constexpr int64_t kErrFault    = -14;    // EFAULT
constexpr int64_t kErrBusy     = -16;    // EBUSY
constexpr int64_t kErrInval    = -22;    // EINVAL
constexpr int64_t kErrDeadlk   = -35;    // EDEADLK
constexpr int64_t kErrTimedout = -110;   // ETIMEDOUT

constexpr uint64_t Retc(int64_t v) { return static_cast<uint64_t>(v); }

// ---------------------------------------------------------------------------
// guest-thread zone geometry (inside the window, past the libc heap and the
// import-trap region: heap ends at base+0x20000000, traps sit at 0x160000000
// absolute = base+0x20000000; this zone starts at base+0x22000000).
// ---------------------------------------------------------------------------
constexpr uint64_t kZoneOffsetInWindow = 0x22000000ull;
constexpr uint64_t kSlotBytes          = (1ull << 20) + ScePthread::kTlsRegionBytes;
constexpr size_t   kMaxSlots           = 87;   // 96 MiB zone

// current-thread identity: the launcher body registers the GuestThread of
// every host thread that runs guest code (device + host tests alike).
thread_local void* t_currentGuest = nullptr;

uint64_t GuestTid() {
    return static_cast<uint64_t>(
        std::hash<std::thread::id>{}(std::this_thread::get_id()));
}

// ---------------------------------------------------------------------------
// window-slot pointer: guest VA -> host pointer, refusing non-window VAs.
// ---------------------------------------------------------------------------
void* WindowPtr(uint64_t va, size_t len) {
    auto& mm = MemoryManager::GetInstance();
    if (!mm.IsValidAddress(va, len ? len : 1)) return nullptr;
    return mm.GetHostPointer(va);
}


// Bounded guest C-string read: walks at most kMaxNameBytes looking for NUL;
// an unterminated string truncates instead of reading the window edge.
constexpr size_t kMaxNameBytes = 256;
std::string ReadGuestName(uint64_t va) {
    if (!va) return {};
    const auto* p = static_cast<const char*>(WindowPtr(va, 1));
    if (!p) return {};
    return std::string(p, strnlen(p, kMaxNameBytes));
}

bool ReadSlotHandle(uint64_t slotVa, uint64_t* handleOut) {
    auto* p = static_cast<uint64_t*>(WindowPtr(slotVa, 8));
    if (!p) return false;
    *handleOut = *p;
    return true;
}

bool WriteSlotHandle(uint64_t slotVa, uint64_t handle) {
    auto* p = static_cast<uint64_t*>(WindowPtr(slotVa, 8));
    if (!p) return false;
    *p = handle;
    return true;
}

// ---------------------------------------------------------------------------
// private objects (handles are the host pointers of these structs)
// ---------------------------------------------------------------------------
struct MutexPrivate {
    int kind = ScePthread::kMutexErrorcheck;
    std::string name;
    std::mutex meta;                     // guards owner/count, cv waits on it
    std::condition_variable cv;
    uint64_t owner = 0;
    uint32_t count = 0;
    bool dead = false;

    int Lock(uint64_t tid) {
        std::unique_lock<std::mutex> lk(meta);
        while (count != 0 && owner != tid) cv.wait(lk);
        if (count != 0) {                // owner == tid here
            if (kind != ScePthread::kMutexRecursive) return static_cast<int>(kErrDeadlk);
            ++count;
            return 0;
        }
        owner = tid;
        count = 1;
        return 0;
    }

    int Unlock(uint64_t tid) {
        std::lock_guard<std::mutex> lk(meta);
        if (count == 0 || owner != tid) return static_cast<int>(kErrPerm);
        if (--count == 0) {
            owner = 0;
            cv.notify_one();
        }
        return 0;
    }
};

struct CondPrivate {
    std::string name;
    // v1.69 — per-cond wait mutex. The old process-wide static serialized
    // EVERY waiter in the process and let a signal on cond A spuriously
    // wake a waiter on cond B; notify under the same mutex the waiter
    // holds also closes the lost-wakeup window between check and wait.
    std::mutex waitM;
    std::condition_variable cv;
    bool dead = false;
};

struct AttrPrivate {
    int kind = ScePthread::kMutexErrorcheck;   // mutex kind for MutexInit
    int detachstate = ScePthread::kCreateJoinable;
    uint64_t stacksize = 0;              // 0 = engine default
    int policy = ScePthread::kSchedOther;
    int32_t priority = 0;
    bool dead = false;
};

struct GuestThread {
    uint64_t id = 0;
    std::string name;
    uint64_t entry = 0;
    uint64_t arg = 0;
    uint64_t exitValue = 0;
    bool exited = false;                 // Exit() recorded or host body done
    bool detached = false;
    bool joined = false;
    bool sessionDead = false;            // reset woke abandoned joiners
    std::mutex m;
    std::condition_variable cv;
    std::thread host;

    // v1.69 lifetime: the handle-validity registry holds the base reference;
    // transient handle users (Join/Detach/AttrGet) hold a "use" for the
    // duration of their access. Deletion happens exactly once (dead flag),
    // only when the object is OUT of the registry, its body finished, and
    // the last use released it. Nothing is ever deleted under a running
    // body — the review's use-after-free class is closed by construction.
    std::atomic<int> uses{0};
    std::atomic<bool> bodyDone{false};   // the very last thing ThreadBody does
    std::atomic<bool> zombieQueued{false}; // exited+detached; reaper owns it
    std::atomic<bool> dead{false};       // deletion already claimed

    ~GuestThread() {
        // Last-resort for leaked-at-reset objects only: a joinable
        // std::thread's destructor would terminate the process.
        if (host.joinable()) host.detach();
    }

    uint64_t slotVa = 0;
    uint64_t stackTop = 0;
    uint64_t tcbVa = 0;
    AttrPrivate attr;                    // snapshot for AttrGet
};

// ---------------------------------------------------------------------------
// published image TLS geometry (set by the load path — PublishImageTls)
// ---------------------------------------------------------------------------
std::mutex g_tlsInfoMutex;
uint64_t g_tlsVa = 0;
size_t g_tlsFilesz = 0;
size_t g_tlsMemsz = 0;
uint64_t g_tlsAlign = 16;

// ---------------------------------------------------------------------------
// typed object registry: every private object lands here with its deleter,
// so session teardown reclaims exactly what the surface allocated.
// ---------------------------------------------------------------------------
std::mutex g_regMutex;
std::vector<std::pair<void*, void (*)(void*)>> g_typed;
// v1.69 — one-session graveyard: mutex/cond/attr objects of the PREVIOUS
// session stay alive until the NEXT teardown. Stuck bodies from the old
// session keep their objects valid through the boundary instead of reading
// freed memory; new-session code can only hold new handles.
std::vector<std::pair<void*, void (*)(void*)>> g_graveyard;

template <typename T>
void DeleteAs(void* p) { delete static_cast<T*>(p); }

template <typename T>
T* NewTracked() {
    T* p = new T();
    std::lock_guard<std::mutex> lk(g_regMutex);
    g_typed.emplace_back(p, &DeleteAs<T>);
    return p;
}

std::map<uint64_t, uint64_t> g_slotInUse;   // slot index -> host tid
size_t g_slotHint = 0;
GuestThread* g_mainThread = nullptr;        // lazy main-thread self
GuestThreadLauncher g_launcher = nullptr;   // the seam

uint64_t g_nextThreadId = 1;

// ---------------------------------------------------------------------------
// v1.69 thread lifetime: g_live is the handle-validity authority; g_zombies
// holds exited threads whose objects the reaper joins and deletes; g_leaked
// holds bodies that never stopped at session reset (named, never touched
// again). Lock order: g_regMutex and g_zombieMutex are never held together;
// a GuestThread's own mutex is never held across either registry mutex.
// ---------------------------------------------------------------------------
std::unordered_set<GuestThread*> g_live;
std::mutex g_zombieMutex;
std::vector<GuestThread*> g_zombies;
std::vector<GuestThread*> g_leaked;
std::atomic<bool> g_resetting{false};

void FreeSlotLocked(uint64_t slotVa);   // defined with the slot machinery

// Begins a validated handle use: nullptr when the handle is stale, reaped,
// or garbage — the guest never dereferences handles, so a bad one is
// refused (EINVAL) instead of dereferenced.
GuestThread* UseBegin(uint64_t handle) {
    auto* t = reinterpret_cast<GuestThread*>(handle);
    if (!t) return nullptr;
    std::lock_guard<std::mutex> lk(g_regMutex);
    if (!g_live.count(t)) return nullptr;
    t->uses.fetch_add(1, std::memory_order_relaxed);
    return t;
}

void FinishDelete(GuestThread* t) {
    // the body is done; join the (already finished) host thread, release the
    // slot, delete. Never called for an object any thread can still reach.
    if (t->host.joinable()) t->host.join();
    if (t->slotVa) {
        const uint64_t slotVa = t->slotVa;
        t->slotVa = 0;
        FreeSlotLocked(slotVa);
    }
    delete t;
}

// Ends a handle use. The LAST releaser deletes the object when it is out of
// the registry, its body finished, and the reaper does not own it.
void UseEnd(GuestThread* t) {
    if (t->uses.fetch_sub(1, std::memory_order_acq_rel) != 1) return;
    if (t->zombieQueued.load(std::memory_order_acquire)) return;  // reaper owns
    if (!t->bodyDone.load(std::memory_order_acquire)) return;
    {
        std::lock_guard<std::mutex> lk(g_regMutex);
        if (g_live.count(t)) return;   // still a valid handle (e.g. main)
    }
    if (t->dead.exchange(true, std::memory_order_acq_rel)) return;
    FinishDelete(t);
}

// Queue for the reaper. The g_resetting check under the queue mutex closes
// the race with session teardown: either the push lands before the reset's
// collection (the reset sees it), or the reset is already collecting and
// owns the object — a late push must never leave a dangling entry behind.
void QueueZombie(GuestThread* t) {
    if (t->zombieQueued.exchange(true)) return;
    std::lock_guard<std::mutex> lk(g_zombieMutex);
    if (g_resetting.load(std::memory_order_acquire)) return;
    g_zombies.push_back(t);
}

// Reclaims exited+detached threads: validate-out, join the finished body,
// free the slot, delete. Called opportunistically from Create and at reset.
void ReapDetached() {
    std::vector<GuestThread*> batch;
    {
        std::lock_guard<std::mutex> lk(g_zombieMutex);
        batch.swap(g_zombies);
    }
    if (batch.empty()) return;
    std::vector<GuestThread*> keepBack;
    for (auto* t : batch) {
        bool deletable = false;
        {
            std::lock_guard<std::mutex> lk(g_regMutex);
            g_live.erase(t);   // the handle becomes invalid (POSIX: detached
                               // threads' handles are dead once reaped)
            deletable = t->uses.load(std::memory_order_acquire) == 0 &&
                        t->bodyDone.load(std::memory_order_acquire) &&
                        !t->dead.exchange(true, std::memory_order_acq_rel);
        }
        if (deletable) {
            FinishDelete(t);
        } else {
            keepBack.push_back(t);   // a user still holds it, or the body is
                                     // microseconds from done — next reap
        }
    }
    if (!keepBack.empty()) {
        std::lock_guard<std::mutex> lk(g_zombieMutex);
        g_zombies.insert(g_zombies.end(), keepBack.begin(), keepBack.end());
    }
}

struct StatsCounters {
    std::atomic<uint64_t> threadsCreated{0};
    std::atomic<uint64_t> threadsJoined{0};
    std::atomic<uint64_t> threadsExited{0};
    std::atomic<uint64_t> mutexLocks{0};
    std::atomic<uint64_t> condSignals{0};
};
StatsCounters g_stats;

bool AllocSlot(uint64_t* slotVaOut) {
    auto& mm = MemoryManager::GetInstance();
    const uint64_t base = mm.GetGuestBase();
    const uint64_t end = mm.GetGuestEnd();
    if (base == 0 || end == 0) return false;
    std::lock_guard<std::mutex> lk(g_regMutex);
    if (g_slotInUse.size() >= kMaxSlots) return false;
    for (size_t tries = 0; tries < kMaxSlots; ++tries) {
        const size_t idx = (g_slotHint + tries) % kMaxSlots;
        if (g_slotInUse.count(idx)) continue;
        const uint64_t va = base + kZoneOffsetInWindow + idx * kSlotBytes;
        if (va + kSlotBytes > end) continue;
        if (!g_slotInUse.emplace(idx, GuestTid()).second) continue;
        if (!mm.MapMemory(va, kSlotBytes,
                          MemoryFlags::PAGE_READ | MemoryFlags::PAGE_WRITE,
                          "pthread_slot")) {
            g_slotInUse.erase(idx);
            continue;
        }
        // v1.69 — zero the slot on (re)use: a recycled slot must never hand
        // the next thread a stack/TLS block carrying the previous tenant's
        // bytes (the review's memory-reuse-without-clearing finding).
        if (auto* p = WindowPtr(va, kSlotBytes)) {
            std::memset(p, 0, kSlotBytes);
        }
        g_slotHint = (idx + 1) % kMaxSlots;
        *slotVaOut = va;
        return true;
    }
    return false;
}

void FreeSlotLocked(uint64_t slotVa) {
    auto& mm = MemoryManager::GetInstance();
    const uint64_t base = mm.GetGuestBase();
    if (base == 0 || slotVa < base + kZoneOffsetInWindow) return;
    const uint64_t rel = slotVa - (base + kZoneOffsetInWindow);
    const uint64_t idx = rel / kSlotBytes;
    if (rel % kSlotBytes != 0 || idx >= kMaxSlots) return;
    g_slotInUse.erase(idx);
}

void FreeSlot(uint64_t slotVa) {
    std::lock_guard<std::mutex> lk(g_regMutex);
    FreeSlotLocked(slotVa);
}

/**
 * Builds a per-thread TCB at the slot's TLS block and returns its VA.
 * FreeBSD variant II, the exact recipe the main thread uses (v1.40):
 *   [TLS image][pad 16][TCB: self, dtv, pthread][dtv: gen, block, term]
 * The +16 slot carries the GuestThread handle (tcb_pthread) so guest code
 * reading fs:[16] sees the object scePthreadSelf returns.
 */
uint64_t BuildThreadTcb(uint64_t slotBase, uint64_t threadHandle) {
    auto& mm = MemoryManager::GetInstance();
    size_t imageBytes = 0;
    {
        std::lock_guard<std::mutex> lk(g_tlsInfoMutex);
        if (g_tlsMemsz) {
            imageBytes = g_tlsMemsz < ScePthread::kMaxTlsImageBytes
                             ? g_tlsMemsz : ScePthread::kMaxTlsImageBytes;
        }
    }
    const size_t imageAligned =
        imageBytes ? (imageBytes + 15) & ~size_t{15} : 0;
    auto* tlsHost = static_cast<uint8_t*>(
        WindowPtr(slotBase, ScePthread::kTlsRegionBytes));
    if (!tlsHost) return 0;
    if (imageBytes) {
        size_t copy = 0;
        uint64_t src = 0;
        {
            std::lock_guard<std::mutex> lk(g_tlsInfoMutex);
            copy = g_tlsFilesz < imageBytes ? g_tlsFilesz : imageBytes;
            src = g_tlsVa;
        }
        if (copy && !mm.ReadGuestMemory(src, tlsHost, copy)) {
            PX5_LOGW(LogCategory::KERNEL,
                     "PTHREAD: TLS init image read failed at 0x%llx — "
                     "thread TCB built with zeroed TLS block",
                     (unsigned long long)src);
        }
    }
    const uint64_t tcbVa = slotBase + imageAligned;
    const uint64_t dtvVa = tcbVa + 32;
    auto write = [tlsHost](size_t off, uint64_t v) {
        memcpy(tlsHost + off, &v, 8);
    };
    write(imageAligned + 0,  tcbVa);           // tcb_self
    write(imageAligned + 8,  dtvVa);           // tcb_dtv
    write(imageAligned + 16, threadHandle);    // tcb_pthread
    write(imageAligned + 32, 1);               // dtv[0] = gen/count
    write(imageAligned + 40, slotBase);        // dtv[1] = static block
    write(imageAligned + 48, 0);               // dtv[2] = terminator
    return tcbVa;
}

// ---------------------------------------------------------------------------
// thread body: registers identity, runs the launcher, records the exit
// ---------------------------------------------------------------------------
void ThreadBody(GuestThread* self) {
    t_currentGuest = self;
    uint64_t rax = 0;
    bool ran = false;
    if (g_launcher) {
        ran = g_launcher(self->entry, self->arg, self->stackTop,
                         self->tcbVa, &rax);
    } else {
        PX5_LOGW(LogCategory::KERNEL,
                 "PTHREAD: no guest launcher installed — thread '%s' "
                 "recorded as exited without running",
                 self->name.c_str());
    }
    bool queueZombie = false;
    {
        std::lock_guard<std::mutex> lk(self->m);
        self->exitValue = rax;
        self->exited = true;
        (void)ran;
        g_stats.threadsExited.fetch_add(1, std::memory_order_relaxed);
        self->cv.notify_all();
        // detached-at-exit: the reaper owns the object from here (QueueZombie's
        // CAS settles the race with a concurrent HDetach on the exited thread)
        queueZombie = self->detached;
    }
    if (queueZombie) QueueZombie(self);
    t_currentGuest = nullptr;
    // bodyDone LAST: the reaper and the last UseEnd both gate deletion on it
    self->bodyDone.store(true, std::memory_order_release);
}

GuestThread* CurrentGuestOrMain() {
    if (t_currentGuest) return static_cast<GuestThread*>(t_currentGuest);
    // Main guest thread: the crt never went through Create, so its handle
    // materializes lazily on the first Self()/Exit() call it makes.
    std::lock_guard<std::mutex> lk(g_regMutex);
    if (!g_mainThread) {
        auto* t = new GuestThread();
        t->id = g_nextThreadId++;
        t->name = "MainThread";
        g_live.insert(t);
        g_mainThread = t;
        g_stats.threadsCreated.fetch_add(1, std::memory_order_relaxed);
    }
    return g_mainThread;
}

// ---------------------------------------------------------------------------
// the 27 imported handlers (arity matches the console ABI; see pthread_hle.h)
// ---------------------------------------------------------------------------
uint64_t HMutexattrInit(const ImportCall& c) {
    if (!c.args[0]) return Retc(kErrInval);
    auto* attr = NewTracked<AttrPrivate>();
    if (!WriteSlotHandle(c.args[0], reinterpret_cast<uint64_t>(attr)))
        return Retc(kErrFault);
    return 0;
}

uint64_t HMutexattrSettype(const ImportCall& c) {
    uint64_t h = 0;
    if (!ReadSlotHandle(c.args[0], &h)) return Retc(kErrFault);
    auto* attr = reinterpret_cast<AttrPrivate*>(h);
    if (!attr) return Retc(kErrInval);
    const int type = static_cast<int>(c.args[1]);
    if (type < ScePthread::kMutexDefault ||
        type > ScePthread::kMutexAdaptive) return Retc(kErrInval);
    attr->kind = type;
    return 0;
}

uint64_t HMutexattrDestroy(const ImportCall& c) {
    uint64_t h = 0;
    if (!ReadSlotHandle(c.args[0], &h)) return Retc(kErrFault);
    auto* attr = reinterpret_cast<AttrPrivate*>(h);
    if (!attr || attr->dead) return Retc(kErrInval);
    attr->dead = true;                   // reclaimed at session teardown
    return 0;
}

uint64_t HMutexInit(const ImportCall& c) {
    if (!c.args[0]) return Retc(kErrInval);
    int kind = ScePthread::kMutexErrorcheck;   // console default
    if (c.args[1]) {
        uint64_t h = 0;
        if (!ReadSlotHandle(c.args[1], &h)) return Retc(kErrFault);
        auto* attr = reinterpret_cast<AttrPrivate*>(h);
        if (!attr || attr->dead) return Retc(kErrInval);
        kind = attr->kind == ScePthread::kMutexDefault
                   ? ScePthread::kMutexErrorcheck : attr->kind;
    }
    auto* m = NewTracked<MutexPrivate>();
    m->kind = kind;
    m->name = ReadGuestName(c.args[2]);
    if (!WriteSlotHandle(c.args[0], reinterpret_cast<uint64_t>(m)))
        return Retc(kErrFault);
    return 0;
}

uint64_t HMutexLock(const ImportCall& c) {
    uint64_t h = 0;
    if (!ReadSlotHandle(c.args[0], &h)) return Retc(kErrFault);
    auto* m = reinterpret_cast<MutexPrivate*>(h);
    if (!m || m->dead) return Retc(kErrInval);
    const int rc = m->Lock(GuestTid());
    if (rc == 0) g_stats.mutexLocks.fetch_add(1, std::memory_order_relaxed);
    return Retc(rc);
}

uint64_t HMutexUnlock(const ImportCall& c) {
    uint64_t h = 0;
    if (!ReadSlotHandle(c.args[0], &h)) return Retc(kErrFault);
    auto* m = reinterpret_cast<MutexPrivate*>(h);
    if (!m || m->dead) return Retc(kErrInval);
    return Retc(m->Unlock(GuestTid()));
}

uint64_t HMutexDestroy(const ImportCall& c) {
    uint64_t h = 0;
    if (!ReadSlotHandle(c.args[0], &h)) return Retc(kErrFault);
    auto* m = reinterpret_cast<MutexPrivate*>(h);
    if (!m || m->dead) return Retc(kErrInval);
    {
        std::lock_guard<std::mutex> lk(m->meta);
        if (m->count != 0) return Retc(kErrBusy);
        m->dead = true;
    }
    return 0;
}

uint64_t HCreate(const ImportCall& c) {
    if (!c.args[0]) return Retc(kErrInval);
    if (g_resetting.load(std::memory_order_acquire)) {
        PX5_LOGW(LogCategory::KERNEL,
                 "PTHREAD: create refused — session teardown in progress");
        return Retc(-12);                // -ENOMEM: no thread can start now
    }
    if (!g_launcher) {
        PX5_LOGW(LogCategory::KERNEL,
                 "PTHREAD: create refused — no guest launcher installed "
                 "(device path must install the FEXCore dispatcher first)");
        return Retc(-34);                // -ENOSYS, named
    }
    AttrPrivate attrCfg;                 // defaults
    if (c.args[1]) {
        uint64_t h = 0;
        if (!ReadSlotHandle(c.args[1], &h)) return Retc(kErrFault);
        auto* attr = reinterpret_cast<AttrPrivate*>(h);
        if (!attr || attr->dead) return Retc(kErrInval);
        attrCfg = *attr;
    }
    uint64_t slot = 0;
    if (!AllocSlot(&slot)) {
        PX5_LOGW(LogCategory::KERNEL,
                 "PTHREAD: thread-slot zone exhausted (%d slots)", kMaxSlots);
        return Retc(-12);                // -ENOMEM
    }
    uint64_t stackBytes = attrCfg.stacksize
                              ? attrCfg.stacksize
                              : ScePthread::kDefaultStackBytes;
    const uint64_t kMaxStack = kSlotBytes - ScePthread::kTlsRegionBytes;
    if (stackBytes < ScePthread::kMinStackBytes)
        stackBytes = ScePthread::kMinStackBytes;
    if (stackBytes > kMaxStack) stackBytes = kMaxStack;
    stackBytes &= ~uint64_t{15};
    const uint64_t stackTop = slot + ScePthread::kTlsRegionBytes + stackBytes;

    auto* t = new GuestThread();
    t->id = g_nextThreadId++;
    t->entry = c.args[2];
    t->arg = c.args[3];
    t->name = ReadGuestName(c.args[4]);
    t->slotVa = slot;
    t->stackTop = stackTop;
    t->attr = attrCfg;
    t->detached = attrCfg.detachstate == ScePthread::kCreateDetached;
    {
        // handle validity comes up BEFORE the guest can observe the handle
        std::lock_guard<std::mutex> lk(g_regMutex);
        g_live.insert(t);
    }

    t->tcbVa = BuildThreadTcb(slot, reinterpret_cast<uint64_t>(t));
    if (!t->tcbVa) {
        {
            std::lock_guard<std::mutex> lk(g_regMutex);
            g_live.erase(t);
        }
        t->dead.store(true, std::memory_order_release);
        FreeSlot(slot);
        delete t;
        return Retc(-12);
    }
    if (!WriteSlotHandle(c.args[0], reinterpret_cast<uint64_t>(t))) {
        {
            std::lock_guard<std::mutex> lk(g_regMutex);
            g_live.erase(t);
        }
        t->dead.store(true, std::memory_order_release);
        FreeSlot(slot);
        delete t;
        return Retc(kErrFault);
    }
    t->host = std::thread(ThreadBody, t);
    // v1.69 — detached guest threads keep their host std::thread joinable:
    // the reaper joins it before reclaiming the object. Detaching here is
    // exactly what made detached-thread objects unleapable before.
    g_stats.threadsCreated.fetch_add(1, std::memory_order_relaxed);
    ReapDetached();   // opportunistic: reclaim exited detached threads
    PX5_LOGI(LogCategory::KERNEL,
             "PTHREAD: create id=%llu '%s' entry=0x%llx stack_top=0x%llx "
             "tcb=0x%llx detached=%d",
             (unsigned long long)t->id, t->name.c_str(),
             (unsigned long long)t->entry,
             (unsigned long long)t->stackTop,
             (unsigned long long)t->tcbVa, t->detached ? 1 : 0);
    return 0;
}

uint64_t HJoin(const ImportCall& c) {
    auto* t = UseBegin(c.args[0]);
    if (!t) return Retc(kErrInval);   // stale/reaped/garbage handle
    if (t == CurrentGuestOrMain()) {
        // v1.69 — covers BOTH a guest thread joining itself AND the main
        // thread joining its own lazily materialized handle (the review's
        // join-self hang: the predicate could never become true).
        UseEnd(t);
        return Retc(kErrDeadlk);
    }
    {
        std::unique_lock<std::mutex> lk(t->m);
        if (t->joined) {
            UseEnd(t);
            return Retc(kErrInval);   // already reaped handle
        }
        // session teardown wakes abandoned joiners through the per-object
        // flag (a global flag could be cleared before the woken thread reads
        // it — the object flag cannot)
        t->cv.wait(lk, [&] { return t->exited || t->sessionDead; });
        if (!t->exited) {
            UseEnd(t);
            return Retc(-3);          // -ESRCH: joiner abandoned at teardown
        }
        if (t->detached) {
            UseEnd(t);
            return Retc(kErrInval);
        }
        t->joined = true;
        if (c.args[1]) {
            auto* out = static_cast<uint64_t*>(WindowPtr(c.args[1], 8));
            if (!out) {
                UseEnd(t);
                return Retc(kErrFault);
            }
            *out = t->exitValue;
        }
    }
    // the host body recorded its exit; join it so bodyDone + the host thread
    // end are both settled before validity drops
    if (t->host.joinable()) t->host.join();
    {
        std::lock_guard<std::mutex> lk(g_regMutex);
        g_live.erase(t);              // the handle is consumed
    }
    g_stats.threadsJoined.fetch_add(1, std::memory_order_relaxed);
    PX5_LOGI(LogCategory::KERNEL,
             "PTHREAD: join id=%llu '%s' exit=0x%llx",
             (unsigned long long)t->id, t->name.c_str(),
             (unsigned long long)t->exitValue);
    // last use: deletes the object exactly once (out of g_live, body done,
    // not reaper-owned)
    UseEnd(t);
    return 0;
}

uint64_t HExit(const ImportCall& c) {
    auto* t = CurrentGuestOrMain();
    {
        std::lock_guard<std::mutex> lk(t->m);
        t->exitValue = c.args[0];
        t->exited = true;
        t->cv.notify_all();
    }
    g_stats.threadsExited.fetch_add(1, std::memory_order_relaxed);
    return 0;
}

uint64_t HAttrInit(const ImportCall& c) {
    if (!c.args[0]) return Retc(kErrInval);
    auto* attr = NewTracked<AttrPrivate>();
    if (!WriteSlotHandle(c.args[0], reinterpret_cast<uint64_t>(attr)))
        return Retc(kErrFault);
    return 0;
}

uint64_t HAttrSetdetachstate(const ImportCall& c) {
    uint64_t h = 0;
    if (!ReadSlotHandle(c.args[0], &h)) return Retc(kErrFault);
    auto* attr = reinterpret_cast<AttrPrivate*>(h);
    if (!attr || attr->dead) return Retc(kErrInval);
    const int ds = static_cast<int>(c.args[1]);
    if (ds != ScePthread::kCreateJoinable &&
        ds != ScePthread::kCreateDetached) return Retc(kErrInval);
    attr->detachstate = ds;
    return 0;
}

uint64_t HAttrSetschedparam(const ImportCall& c) {
    uint64_t h = 0;
    if (!ReadSlotHandle(c.args[0], &h)) return Retc(kErrFault);
    auto* attr = reinterpret_cast<AttrPrivate*>(h);
    if (!attr || attr->dead) return Retc(kErrInval);
    if (const auto* p = static_cast<const int32_t*>(WindowPtr(c.args[1], 4)))
        attr->priority = *p;             // stored, not scheduled (named)
    return 0;
}

uint64_t HAttrDestroy(const ImportCall& c) {
    uint64_t h = 0;
    if (!ReadSlotHandle(c.args[0], &h)) return Retc(kErrFault);
    auto* attr = reinterpret_cast<AttrPrivate*>(h);
    if (!attr || attr->dead) return Retc(kErrInval);
    attr->dead = true;
    return 0;
}

uint64_t HAttrSetstacksize(const ImportCall& c) {
    uint64_t h = 0;
    if (!ReadSlotHandle(c.args[0], &h)) return Retc(kErrFault);
    auto* attr = reinterpret_cast<AttrPrivate*>(h);
    if (!attr || attr->dead) return Retc(kErrInval);
    attr->stacksize = c.args[1];
    return 0;
}

uint64_t HAttrGet(const ImportCall& c) {
    auto* t = UseBegin(c.args[0]);
    if (!t) return Retc(kErrInval);   // stale/reaped/garbage handle
    if (!c.args[1]) {
        UseEnd(t);
        return Retc(kErrInval);
    }
    auto* attr = NewTracked<AttrPrivate>();
    {
        std::lock_guard<std::mutex> lk(t->m);
        attr->detachstate = t->attr.detachstate;
        attr->stacksize = t->attr.stacksize;
        attr->priority = t->attr.priority;
    }
    const bool wrote = WriteSlotHandle(c.args[1],
                                       reinterpret_cast<uint64_t>(attr));
    UseEnd(t);
    if (!wrote) return Retc(kErrFault);
    return 0;
}

uint64_t HAttrGetstack(const ImportCall& c) {
    uint64_t h = 0;
    if (!ReadSlotHandle(c.args[0], &h)) return Retc(kErrFault);
    auto* attr = reinterpret_cast<AttrPrivate*>(h);
    if (!attr || attr->dead) return Retc(kErrInval);
    if (c.args[1]) {
        auto* out = static_cast<uint64_t*>(WindowPtr(c.args[1], 8));
        if (!out) return Retc(kErrFault);
        *out = 0;                        // stacks allocate at Create time
    }
    if (c.args[2]) {
        auto* out = static_cast<uint64_t*>(WindowPtr(c.args[2], 8));
        if (!out) return Retc(kErrFault);
        *out = attr->stacksize ? attr->stacksize
                               : ScePthread::kDefaultStackBytes;
    }
    return 0;
}

uint64_t HDetach(const ImportCall& c) {
    auto* t = UseBegin(c.args[0]);
    if (!t) return Retc(kErrInval);   // stale/reaped/garbage handle
    bool queueZombie = false;
    {
        std::lock_guard<std::mutex> lk(t->m);
        if (t->joined) {
            UseEnd(t);
            return Retc(kErrInval);
        }
        t->detached = true;
        // detach of an already-exited thread: nobody will ever join it —
        // the reaper owns the object now (v1.69: previously its slot and
        // object leaked until session teardown)
        queueZombie = t->exited;
    }
    if (queueZombie) QueueZombie(t);
    UseEnd(t);
    return 0;
}

uint64_t HSelf() {
    return reinterpret_cast<uint64_t>(CurrentGuestOrMain());
}

uint64_t HYield() {
    std::this_thread::yield();
    return 0;
}

uint64_t HCondattrInit(const ImportCall& c) {
    if (!c.args[0]) return Retc(kErrInval);
    auto* attr = NewTracked<AttrPrivate>();
    if (!WriteSlotHandle(c.args[0], reinterpret_cast<uint64_t>(attr)))
        return Retc(kErrFault);
    return 0;
}

uint64_t HCondattrDestroy(const ImportCall& c) {
    uint64_t h = 0;
    if (!ReadSlotHandle(c.args[0], &h)) return Retc(kErrFault);
    auto* attr = reinterpret_cast<AttrPrivate*>(h);
    if (!attr || attr->dead) return Retc(kErrInval);
    attr->dead = true;
    return 0;
}

uint64_t HCondInit(const ImportCall& c) {
    if (!c.args[0]) return Retc(kErrInval);
    auto* cv = NewTracked<CondPrivate>();
    cv->name = ReadGuestName(c.args[2]);
    if (!WriteSlotHandle(c.args[0], reinterpret_cast<uint64_t>(cv)))
        return Retc(kErrFault);
    return 0;
}

uint64_t HCondDestroy(const ImportCall& c) {
    uint64_t h = 0;
    if (!ReadSlotHandle(c.args[0], &h)) return Retc(kErrFault);
    auto* cv = reinterpret_cast<CondPrivate*>(h);
    if (!cv || cv->dead) return Retc(kErrInval);
    cv->dead = true;
    return 0;
}

uint64_t HCondTimedwait(const ImportCall& c) {
    uint64_t ch = 0, mh = 0;
    if (!ReadSlotHandle(c.args[0], &ch)) return Retc(kErrFault);
    if (!ReadSlotHandle(c.args[1], &mh)) return Retc(kErrFault);
    auto* cv = reinterpret_cast<CondPrivate*>(ch);
    auto* mu = reinterpret_cast<MutexPrivate*>(mh);
    if (!cv || cv->dead || !mu || mu->dead) return Retc(kErrInval);
    const uint64_t tid = GuestTid();
    const int rel = mu->Unlock(tid);
    if (rel != 0) return Retc(rel);      // not the owner — POSIX EPERM
    const uint64_t usec = c.args[2];
    if (usec == 0) {
        mu->Lock(tid);
        return Retc(kErrTimedout);
    }
    const auto deadline =
        std::chrono::system_clock::now() + std::chrono::microseconds(usec);
    std::cv_status st = std::cv_status::no_timeout;
    {
        // v1.69 — wait on the cond's OWN mutex; signal/broadcast notify under
        // the same one, so a notify can never slip between the waiter's
        // predicate check and the wait itself.
        std::unique_lock<std::mutex> lk(cv->waitM);
        st = cv->cv.wait_until(lk, deadline);
    }
    mu->Lock(tid);
    return st == std::cv_status::timeout ? Retc(kErrTimedout) : 0;
}

uint64_t HCondSignal(const ImportCall& c) {
    uint64_t ch = 0;
    if (!ReadSlotHandle(c.args[0], &ch)) return Retc(kErrFault);
    auto* cv = reinterpret_cast<CondPrivate*>(ch);
    if (!cv || cv->dead) return Retc(kErrInval);
    {
        std::lock_guard<std::mutex> lk(cv->waitM);
        cv->cv.notify_one();
    }
    g_stats.condSignals.fetch_add(1, std::memory_order_relaxed);
    return 0;
}

uint64_t HCondBroadcast(const ImportCall& c) {
    uint64_t ch = 0;
    if (!ReadSlotHandle(c.args[0], &ch)) return Retc(kErrFault);
    auto* cv = reinterpret_cast<CondPrivate*>(ch);
    if (!cv || cv->dead) return Retc(kErrInval);
    {
        std::lock_guard<std::mutex> lk(cv->waitM);
        cv->cv.notify_all();
    }
    g_stats.condSignals.fetch_add(1, std::memory_order_relaxed);
    return 0;
}

const char* const kSymbols[] = {
    "scePthreadMutexattrInit",   "scePthreadMutexattrSettype",
    "scePthreadMutexattrDestroy","scePthreadMutexInit",
    "scePthreadMutexLock",       "scePthreadMutexUnlock",
    "scePthreadMutexDestroy",    "scePthreadCreate",
    "scePthreadJoin",            "scePthreadExit",
    "scePthreadAttrInit",        "scePthreadAttrSetdetachstate",
    "scePthreadAttrSetschedparam","scePthreadAttrDestroy",
    "scePthreadAttrSetstacksize","scePthreadAttrGet",
    "scePthreadAttrGetstack",    "scePthreadDetach",
    "scePthreadSelf",            "scePthreadYield",
    "scePthreadCondattrInit",    "scePthreadCondattrDestroy",
    "scePthreadCondInit",        "scePthreadCondDestroy",
    "scePthreadCondTimedwait",   "scePthreadCondSignal",
    "scePthreadCondBroadcast",
};

} // namespace

// ---------------------------------------------------------------------------
// public surface (still inside PX5::SceKernelHle)
// ---------------------------------------------------------------------------
struct PthreadHle::Impl {};   // pimpl shell — state lives in TU statics

PthreadHle& PthreadHle::Instance() {
    static PthreadHle inst;
    return inst;
}

void PthreadHle::PublishImageTls(uint64_t tlsVa, size_t filesz, size_t memsz,
                                 uint64_t align) {
    std::lock_guard<std::mutex> lk(g_tlsInfoMutex);
    g_tlsVa = tlsVa;
    g_tlsFilesz = filesz;
    g_tlsMemsz = memsz;
    g_tlsAlign = align;
    PX5_LOGI(LogCategory::KERNEL,
             "PTHREAD: image TLS published va=0x%llx filesz=%zu memsz=%zu "
             "align=%llu (per-thread TCBs carry it)",
             (unsigned long long)tlsVa, filesz, memsz,
             (unsigned long long)align);
}

void PthreadHle::SetGuestLauncher(GuestThreadLauncher launcher) {
    g_launcher = launcher;
}

void PthreadHle::ResetForSession() {
    // v1.69 — teardown that can never delete state under a running thread
    // (the review's use-after-free finding):
    //   1. refuse new creates, wake abandoned joiners (per-object flag),
    //   2. give live bodies a bounded grace to reach their end,
    //   3. join + free + delete everything that stopped,
    //   4. bodies STILL running move to the named leak list — their host
    //      threads finish naturally and nothing touches those objects again.
    g_resetting.store(true, std::memory_order_release);

    std::vector<GuestThread*> all;
    {
        std::lock_guard<std::mutex> lk(g_zombieMutex);
        for (auto* t : g_zombies) all.push_back(t);
        g_zombies.clear();
    }
    {
        std::lock_guard<std::mutex> lk(g_regMutex);
        for (auto* t : g_live) all.push_back(t);
        g_live.clear();
    }
    GuestThread* mainPtr = g_mainThread;

    for (auto* t : all) {
        std::lock_guard<std::mutex> lk(t->m);
        t->sessionDead = true;
        t->cv.notify_all();
    }

    for (auto* t : all) {
        // bodies record bodyDone as their very last action; stuck guest
        // code is the only way to miss it (bounded grace, then leak)
        for (int i = 0;
             i < 200 && !t->bodyDone.load(std::memory_order_acquire); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    std::vector<GuestThread*> leaked;
    {
        std::lock_guard<std::mutex> lk(g_regMutex);
        for (auto* t : all) {
            // main's "body" is the caller itself; its host std::thread is
            // not joinable and it cannot still be referenced through TLS
            const bool isMain = (t == mainPtr);
            const bool deletable =
                (isMain || t->bodyDone.load(std::memory_order_acquire)) &&
                t->uses.load(std::memory_order_acquire) == 0 &&
                !t->dead.exchange(true, std::memory_order_acq_rel);
            if (!deletable) {
                leaked.push_back(t);
                continue;
            }
            if (!isMain && t->host.joinable()) t->host.join();
            if (t->slotVa) {
                FreeSlotLocked(t->slotVa);
                t->slotVa = 0;
            }
            delete t;
        }
    }
    if (!leaked.empty()) {
        std::lock_guard<std::mutex> lk(g_regMutex);
        g_leaked.insert(g_leaked.end(), leaked.begin(), leaked.end());
        PX5_LOGW(LogCategory::KERNEL,
                 "PTHREAD: session reset parked %zu thread object(s) whose "
                 "bodies are still running — never touched again (bounded "
                 "by the %zu-slot zone)",
                 leaked.size(), kMaxSlots);
    }

    // v1.69 — one-generation graveyard for mutex/cond/attr objects: this
    // session's objects stay alive for stuck old bodies; the PREVIOUS
    // graveyard is reclaimed now (a full session boundary has passed).
    {
        std::vector<std::pair<void*, void (*)(void*)>> old;
        {
            std::lock_guard<std::mutex> lk(g_regMutex);
            g_typed.swap(g_graveyard);   // current objects sleep; old ones out
            old.swap(g_typed);
            g_slotInUse.clear();
            g_slotHint = 0;
        }
        for (auto it = old.rbegin(); it != old.rend(); ++it) it->second(it->first);
    }
    g_mainThread = nullptr;
    g_nextThreadId = 1;
    g_stats.threadsCreated.store(0, std::memory_order_relaxed);
    g_stats.threadsJoined.store(0, std::memory_order_relaxed);
    g_stats.threadsExited.store(0, std::memory_order_relaxed);
    g_stats.mutexLocks.store(0, std::memory_order_relaxed);
    g_stats.condSignals.store(0, std::memory_order_relaxed);
    g_resetting.store(false, std::memory_order_release);
}

uint64_t PthreadHle::Invoke(const std::string& symbol, const ImportCall& call,
                            uint64_t* xmmRetOut) {
    (void)xmmRetOut;   // pthread results are integral; nothing publishes
    if (symbol == "scePthreadMutexattrInit")     return HMutexattrInit(call);
    if (symbol == "scePthreadMutexattrSettype")  return HMutexattrSettype(call);
    if (symbol == "scePthreadMutexattrDestroy")  return HMutexattrDestroy(call);
    if (symbol == "scePthreadMutexInit")         return HMutexInit(call);
    if (symbol == "scePthreadMutexLock")         return HMutexLock(call);
    if (symbol == "scePthreadMutexUnlock")       return HMutexUnlock(call);
    if (symbol == "scePthreadMutexDestroy")      return HMutexDestroy(call);
    if (symbol == "scePthreadCreate")            return HCreate(call);
    if (symbol == "scePthreadJoin")              return HJoin(call);
    if (symbol == "scePthreadExit")              return HExit(call);
    if (symbol == "scePthreadAttrInit")          return HAttrInit(call);
    if (symbol == "scePthreadAttrSetdetachstate")return HAttrSetdetachstate(call);
    if (symbol == "scePthreadAttrSetschedparam") return HAttrSetschedparam(call);
    if (symbol == "scePthreadAttrDestroy")       return HAttrDestroy(call);
    if (symbol == "scePthreadAttrSetstacksize")  return HAttrSetstacksize(call);
    if (symbol == "scePthreadAttrGet")           return HAttrGet(call);
    if (symbol == "scePthreadAttrGetstack")      return HAttrGetstack(call);
    if (symbol == "scePthreadDetach")            return HDetach(call);
    if (symbol == "scePthreadSelf")              return HSelf();
    if (symbol == "scePthreadYield")             return HYield();
    if (symbol == "scePthreadCondattrInit")      return HCondattrInit(call);
    if (symbol == "scePthreadCondattrDestroy")   return HCondattrDestroy(call);
    if (symbol == "scePthreadCondInit")          return HCondInit(call);
    if (symbol == "scePthreadCondDestroy")       return HCondDestroy(call);
    if (symbol == "scePthreadCondTimedwait")     return HCondTimedwait(call);
    if (symbol == "scePthreadCondSignal")        return HCondSignal(call);
    if (symbol == "scePthreadCondBroadcast")     return HCondBroadcast(call);
    return 0;
}

bool PthreadHle::HasSymbol(const std::string& symbol) const {
    for (const char* s : kSymbols)
        if (symbol == s) return true;
    return false;
}

void PthreadHle::CollectSymbols(std::vector<SymbolEntry>* tableOut) {
    for (const char* s : kSymbols) tableOut->push_back({s, 0});
}

PthreadHle::Stats PthreadHle::GetStats() const {
    Stats s;
    s.threadsCreated = g_stats.threadsCreated.load(std::memory_order_relaxed);
    s.threadsJoined = g_stats.threadsJoined.load(std::memory_order_relaxed);
    s.threadsExited = g_stats.threadsExited.load(std::memory_order_relaxed);
    s.mutexLocks = g_stats.mutexLocks.load(std::memory_order_relaxed);
    s.condSignals = g_stats.condSignals.load(std::memory_order_relaxed);
    std::lock_guard<std::mutex> lk(g_regMutex);
    s.liveThreads = g_live.size();   // v1.69: threads only (was: every
                                     // tracked object incl. mutexes/conds)
    return s;
}

} // namespace PX5::SceKernelHle
