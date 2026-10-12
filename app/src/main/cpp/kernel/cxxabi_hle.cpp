// PX5 v1.67 — the C++ ABI runtime tranche implementation.
// See cxxabi_hle.h for the contract.
#include "cxxabi_hle.h"

#include "libc_hle.h"      // guest heap via memalign
#include "sce_kernel_hle.h"   // SymbolEntry

#include "../memory/memory.h"
#include "../utils/evidence.h"
#include "../utils/logger.h"

#include <atomic>
#include <condition_variable>
#include <cstring>
#include <map>
#include <mutex>

namespace PX5::SceKernelHle {

namespace {

using MemoryManager = ::PX5::MemoryManager;

constexpr int64_t kErrInval = -22;   // EINVAL
constexpr uint64_t Retc(int64_t v) { return static_cast<uint64_t>(v); }

void* WindowPtr(uint64_t va, size_t len) {
    auto& mm = MemoryManager::GetInstance();
    if (!mm.IsValidAddress(va, len ? len : 1)) return nullptr;
    return mm.GetHostPointer(va);
}

// ---------------------------------------------------------------------------
// guard registry: one host entry per guest guard address, real contention
// ---------------------------------------------------------------------------
struct GuardEntry {
    std::mutex m;
    std::condition_variable cv;
    bool inUse = false;          // mirrors guest byte1
};

std::mutex g_guardRegMutex;
std::map<uint64_t, GuardEntry*> g_guards;

GuardEntry* GuardFor(uint64_t va) {
    std::lock_guard<std::mutex> lk(g_guardRegMutex);
    auto it = g_guards.find(va);
    if (it != g_guards.end()) return it->second;
    auto* e = new GuardEntry();
    g_guards.emplace(va, e);
    return e;
}

// guest guard bytes: Itanium 64-bit guard — byte0 = initialized, byte1 =
// in-use (the upper half may carry implementation pointers; untouched).
bool GuardReadDone(uint64_t va, bool* done) {
    auto* p = static_cast<uint8_t*>(WindowPtr(va, 8));
    if (!p) return false;
    *done = p[0] != 0;
    return true;
}

bool GuardWrite(uint64_t va, uint8_t done, uint8_t inUse) {
    auto* p = static_cast<uint8_t*>(WindowPtr(va, 8));
    if (!p) return false;
    p[0] = done;
    p[1] = inUse;
    return true;
}

std::atomic<uint64_t> g_guardAcquires{0};
std::atomic<uint64_t> g_guardContended{0};
std::atomic<uint64_t> g_thrown{0};
std::atomic<uint64_t> g_caught{0};

// ---------------------------------------------------------------------------
// pending exceptions (per thread): throw records, catch consumes
// ---------------------------------------------------------------------------
struct PendingException {
    uint64_t objectVa = 0;
    uint64_t tinfoVa = 0;
    uint64_t destructorVa = 0;
};
thread_local PendingException t_pending;

// ---------------------------------------------------------------------------
// handlers
// ---------------------------------------------------------------------------
uint64_t HGuardAcquire(const ImportCall& c) {
    const uint64_t va = c.args[0];
    if (!va) return Retc(kErrInval);
    bool done = false;
    if (!GuardReadDone(va, &done)) return Retc(-14);   // EFAULT
    if (done) return 0;                                // already initialized
    auto* e = GuardFor(va);
    g_guardAcquires.fetch_add(1, std::memory_order_relaxed);
    std::unique_lock<std::mutex> lk(e->m);
    bool contended = false;
    while (e->inUse) {
        // another thread is initializing: block until it resolves
        e->cv.wait(lk);
        contended = true;
    }
    if (contended) g_guardContended.fetch_add(1, std::memory_order_relaxed);
    if (!GuardReadDone(va, &done)) return Retc(-14);
    if (done) return 0;                                // raced to completion
    e->inUse = true;
    GuardWrite(va, 0, 1);
    return 1;                                          // caller initializes
}

uint64_t HGuardRelease(const ImportCall& c) {
    const uint64_t va = c.args[0];
    if (!va) return Retc(kErrInval);
    auto* e = GuardFor(va);
    {
        std::lock_guard<std::mutex> lk(e->m);
        e->inUse = false;
    }
    GuardWrite(va, 1, 0);                              // committed
    e->cv.notify_all();
    return 0;
}

uint64_t HGuardAbort(const ImportCall& c) {
    const uint64_t va = c.args[0];
    if (!va) return Retc(kErrInval);
    auto* e = GuardFor(va);
    {
        std::lock_guard<std::mutex> lk(e->m);
        e->inUse = false;
    }
    GuardWrite(va, 0, 0);                              // failed: retryable
    e->cv.notify_all();
    return 0;
}

uint64_t HAllocateException(const ImportCall& c) {
    const uint64_t size = c.args[0];
    if (size == 0 || size > (64ull << 20)) return Retc(kErrInval);
    // guest heap bump allocation (libc_hle owns the region; route through
    // its public symbol so both share one heap truth)
    auto& libc = LibcHle::Instance();
    ImportCall heap{};
    heap.args[0] = size;
    heap.args[1] = 16;                                 // __cxa alignment
    const uint64_t va = libc.Invoke("memalign", heap);
    return va;                                         // 0 propagates as OOM
}

uint64_t HFreeException(const ImportCall& c) {
    // the bump heap reclaims at session teardown (named in libc_hle.h);
    // freeing here would corrupt the size table's ownership model.
    (void)c;
    return 0;
}

uint64_t HThrow(const ImportCall& c) {
    t_pending = PendingException{c.args[0], c.args[1], c.args[2]};
    g_thrown.fetch_add(1, std::memory_order_relaxed);
    Evidence::AppendLedger(
        "hle __cxa_throw exc=0x%llx tinfo=0x%llx dtor=0x%llx (unwind "
        "propagation not implemented — registry recorded)",
        (unsigned long long)c.args[0], (unsigned long long)c.args[1],
        (unsigned long long)c.args[2]);
    PX5_LOGW(LogCategory::KERNEL,
             "[GUEST] __cxa_throw(exc=0x%llx tinfo=0x%llx) — pending "
             "exception recorded; unwinder not built yet",
             (unsigned long long)c.args[0],
             (unsigned long long)c.args[1]);
    return 0;
}

uint64_t HBeginCatch(const ImportCall& c) {
    (void)c;
    const uint64_t objectVa = t_pending.objectVa;
    if (objectVa) g_caught.fetch_add(1, std::memory_order_relaxed);
    // the object leaves the pending slot on end_catch, not here (rethrow
    // must still see it)
    return objectVa;
}

uint64_t HEndCatch(const ImportCall& c) {
    (void)c;
    t_pending = PendingException{};
    return 0;
}

uint64_t HRethrow(const ImportCall& c) {
    (void)c;
    // pending slot untouched: the object re-enters the throw path as-is
    if (t_pending.objectVa) {
        g_thrown.fetch_add(1, std::memory_order_relaxed);
        Evidence::AppendLedger("hle __cxa_rethrow exc=0x%llx",
                               (unsigned long long)t_pending.objectVa);
    }
    return 0;
}

uint64_t HGetExceptionPtr(const ImportCall& c) {
    (void)c;
    return t_pending.objectVa;
}

uint64_t HUnwindResume(const ImportCall& c) {
    // fatal path: no handler caught. Loud, ledgered, returned — a guest
    // that genuinely expected unwinding faults next, visibly.
    Evidence::AppendLedger(
        "hle _Unwind_Resume exc=0x%llx tinfo=0x%llx — NO handler caught "
        "(fatal path)",
        (unsigned long long)c.args[0], (unsigned long long)c.args[1]);
    PX5_LOGE(LogCategory::KERNEL,
             "[GUEST] _Unwind_Resume(exc=0x%llx) — exception escaped all "
             "handlers",
             (unsigned long long)c.args[0]);
    return 0;
}

const char* const kSymbols[] = {
    "__cxa_guard_acquire", "__cxa_guard_release", "__cxa_guard_abort",
    "__cxa_allocate_exception", "__cxa_free_exception",
    "__cxa_throw", "__cxa_begin_catch", "__cxa_end_catch",
    "__cxa_rethrow", "__cxa_get_exception_ptr",
    "_Unwind_Resume",
    // __cxa_atexit rides the crt-bootstrap entries registered in
    // sce_kernel_hle (the device evidence showed it on the boot path).
};

} // namespace

struct CxxAbiHle::Impl {};   // pimpl shell — state lives in TU statics

CxxAbiHle& CxxAbiHle::Instance() {
    static CxxAbiHle inst;
    return inst;
}

uint64_t CxxAbiHle::Invoke(const std::string& symbol, const ImportCall& call,
                           uint64_t* xmmRetOut) {
    (void)xmmRetOut;
    if (symbol == "__cxa_guard_acquire")    return HGuardAcquire(call);
    if (symbol == "__cxa_guard_release")    return HGuardRelease(call);
    if (symbol == "__cxa_guard_abort")      return HGuardAbort(call);
    if (symbol == "__cxa_allocate_exception") return HAllocateException(call);
    if (symbol == "__cxa_free_exception")   return HFreeException(call);
    if (symbol == "__cxa_throw")            return HThrow(call);
    if (symbol == "__cxa_begin_catch")      return HBeginCatch(call);
    if (symbol == "__cxa_end_catch")        return HEndCatch(call);
    if (symbol == "__cxa_rethrow")          return HRethrow(call);
    if (symbol == "__cxa_get_exception_ptr") return HGetExceptionPtr(call);
    if (symbol == "_Unwind_Resume")         return HUnwindResume(call);
    return 0;
}

bool CxxAbiHle::HasSymbol(const std::string& symbol) const {
    for (const char* s : kSymbols)
        if (symbol == s) return true;
    return false;
}

void CxxAbiHle::CollectSymbols(std::vector<SymbolEntry>* tableOut) {
    for (const char* s : kSymbols) tableOut->push_back({s, 0});
}

void CxxAbiHle::ResetForSession() {
    std::lock_guard<std::mutex> lk(g_guardRegMutex);
    for (auto& kv : g_guards) delete kv.second;
    g_guards.clear();
    g_guardAcquires.store(0, std::memory_order_relaxed);
    g_guardContended.store(0, std::memory_order_relaxed);
    g_thrown.store(0, std::memory_order_relaxed);
    g_caught.store(0, std::memory_order_relaxed);
}

CxxAbiHle::Stats CxxAbiHle::GetStats() const {
    Stats s;
    s.guardAcquires = g_guardAcquires.load(std::memory_order_relaxed);
    s.guardContended = g_guardContended.load(std::memory_order_relaxed);
    s.exceptionsThrown = g_thrown.load(std::memory_order_relaxed);
    s.exceptionsCaught = g_caught.load(std::memory_order_relaxed);
    return s;
}

} // namespace PX5::SceKernelHle
