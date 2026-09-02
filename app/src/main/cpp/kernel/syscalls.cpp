#include "syscalls.h"
#include "../memory/memory.h"
#include "../loader/runtime_linker.h"
#include "../utils/logger.h"

#include <cerrno>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <mutex>
#include <sys/mman.h>
#include <sys/uio.h>
#include <unistd.h>
#include <android/log.h>

namespace PX5 {

namespace {

// x86-64 Linux syscall numbers actually implemented by this bridge.
constexpr uint32_t NR_read            = 0;
constexpr uint32_t NR_write           = 1;
constexpr uint32_t NR_mmap            = 9;
constexpr uint32_t NR_mprotect        = 10;
constexpr uint32_t NR_munmap          = 11;
constexpr uint32_t NR_brk             = 12;
constexpr uint32_t NR_rt_sigaction    = 13;
constexpr uint32_t NR_rt_sigprocmask  = 14;
constexpr uint32_t NR_ioctl           = 16;
constexpr uint32_t NR_madvise         = 28;
constexpr uint32_t NR_getpid          = 39;
constexpr uint32_t NR_clock_gettime   = 228;
constexpr uint32_t NR_uname           = 63;
constexpr uint32_t NR_arch_prctl      = 158;
constexpr uint32_t NR_gettid          = 186;
constexpr uint32_t NR_set_tid_address = 218;
constexpr uint32_t NR_futex           = 202;
constexpr uint32_t NR_sched_getaffinity = 204;
constexpr uint32_t NR_exit            = 60;
constexpr uint32_t NR_exit_group      = 231;

constexpr uint64_t kErrNoSys   = -static_cast<int64_t>(ENOSYS);
constexpr uint64_t kErrBadFd   = -static_cast<int64_t>(EBADF);
constexpr uint64_t kErrInval   = -static_cast<int64_t>(EINVAL);
constexpr size_t   kMaxCapture = 8 * 1024;   // output ring kept for evidence

std::mutex g_stateMutex;
std::string g_output;                       // captured guest stdout/stderr
bool       g_hasExitCode = false;
uint64_t   g_exitCode    = 0;
GuestSyscallStats g_stats;

// Convert a guest pointer argument to a host pointer through the window.
inline bool GuestToHost(uint64_t ga, uint64_t len, void** out) {
    // MemoryManager lock is recursive-free; GetHostPointer does raw math but
    // needs range validity. Route via public mapper that locks properly.
    if (!MemoryManager::GetInstance().IsValidAddress(ga, len ? len : 1)) {
        // Fall back: allow reads/writes of recently mapped regions even when
        // the granularity table missed (e.g., inside a mapping's tail page).
        auto& mm = MemoryManager::GetInstance();
        void* h = mm.GetHostPointer(ga);
        if (!h) return false;
        *out = h;
        return true;
    }
    *out = MemoryManager::GetInstance().GetHostPointer(ga);
    return true;
}

void LogUnimplemented(uint32_t nr, const char* name,
                      uint64_t a0, uint64_t a1, uint64_t a2) {
    PX5_LOGW(LogCategory::KERNEL,
             "SYSCALL %u (%s): UNIMPLEMENTED - args=0x%llx,0x%llx,0x%llx -> ENOSYS",
             nr, name, (unsigned long long)a0, (unsigned long long)a1,
             (unsigned long long)a2);
}

} // namespace

uint64_t GuestSyscalls::Dispatch(uint32_t nr,
                                 uint64_t a0, uint64_t a1, uint64_t a2,
                                 uint64_t a3, uint64_t a4, uint64_t a5) {
    {
        std::lock_guard<std::mutex> lk(g_stateMutex);
        ++g_stats.totalCalls;
    }

    switch (nr) {
    case NR_write: {
        // write(fd, buf, count)
        void* buf = nullptr;
        if (a1 != 0 && !GuestToHost(a1, a2, &buf)) return kErrInval;
        const size_t count = static_cast<size_t>(a2 > (1u << 20) ? (1u << 20) : a2);

        if (a0 == 1 || a0 == 2 || a0 == ~0ull /* Android log proxy */) {
            std::string text(static_cast<const char*>(buf), count);
            AppendOutput(text);
            __android_log_print(
                a0 == 2 ? ANDROID_LOG_ERROR : ANDROID_LOG_INFO,
                "PX5_GuestOut", "%.*s",
                count > 512 ? 512 : static_cast<int>(count), text.c_str());
            PX5_LOGI(LogCategory::KERNEL, "guest write(fd=%llu, %zu B)",
                      (unsigned long long)a0, count);
            std::lock_guard<std::mutex> lk(g_stateMutex);
            g_stats.handledCalls++;
            g_stats.bytesWritten += count;
            return count;                     // full bytes written
        }
        LogUnimplemented(nr, "write(non-console)", a0, a1, a2);
        return kErrBadFd;
    }

    case NR_read: {                          // stdin not wired honestly
        LogUnimplemented(nr, "read", a0, a1, a2);
        return kErrBadFd;
    }

    case NR_brk: {
        if (a0 == 0) return 0;               // query before loader sets base
        MemoryManager::GetInstance().SetProgramBreak(a0);
        std::lock_guard<std::mutex> lk(g_stateMutex);
        g_stats.handledCalls++;
        return a0;
    }

    case NR_mmap: {                          // (addr,len,prot,flags,fd,off)
        if (a1 == 0) return kErrInval;
        const bool anonFixed = (a3 & 0x20 /*MAP_ANON*/) && (a3 & 0x10 /*MAP_FIXED*/);
        if (!(anonFixed)) {
            LogUnimplemented(nr, "mmap(non-fixed/anon)", a0, a1, a3);
            return kErrNoSys;
        }
        uint32_t flags = MemoryFlags::PAGE_NONE;
        if (a2 & PROT_READ)  flags |= MemoryFlags::PAGE_READ;
        if (a2 & PROT_WRITE) flags |= MemoryFlags::PAGE_WRITE;
        if (a2 & PROT_EXEC)  flags |= MemoryFlags::PAGE_EXEC;
        const uint64_t mapped =
            MemoryManager::GetInstance().MapMemory(a0, a1, flags, "guest_mmap");
        std::lock_guard<std::mutex> lk(g_stateMutex);
        g_stats.handledCalls++;
        return mapped ? mapped : kErrInval;
    }

    case NR_munmap: {
        const bool ok = MemoryManager::GetInstance().UnmapMemory(a0, a1);
        std::lock_guard<std::mutex> lk(g_stateMutex);
        g_stats.handledCalls++;
        return ok ? 0 : kErrInval;
    }

    case NR_mprotect: {
        // Real: re-protect through the memory manager so the SMC registry
        // and the executable-range query stay truthful. The manager fires
        // the code-invalidation notify when an exec range's W bit drops.
        const bool ok = MemoryManager::GetInstance().ProtectMemory(
            a0, static_cast<size_t>(a1),
            (a2 & PROT_READ  ? MemoryFlags::PAGE_READ  : 0) |
            (a2 & PROT_WRITE ? MemoryFlags::PAGE_WRITE : 0) |
            (a2 & PROT_EXEC  ? MemoryFlags::PAGE_EXEC  : 0));
        std::lock_guard<std::mutex> lk(g_stateMutex);
        g_stats.handledCalls++;
        return ok ? 0 : kErrInval;
    }
    case NR_madvise: {                       // advisory no-op in window model
        std::lock_guard<std::mutex> lk(g_stateMutex);
        g_stats.handledCalls++;
        return 0;
    }

    case NR_exit:
    case NR_exit_group: {
        {
            std::lock_guard<std::mutex> lk(g_stateMutex);
            g_exitCode = a0;
            g_hasExitCode = true;
            g_stats.handledCalls++;
        }
        PX5_LOGI(LogCategory::KERNEL,
                 "guest exit_group(%llu) recorded%s",
                 (unsigned long long)a0,
                 nr == NR_exit_group ? "" : " (single-exit)");
        return 0;
    }

    case NR_getpid:  { std::lock_guard<std::mutex> lk(g_stateMutex); g_stats.handledCalls++; return 1000; }
    case NR_gettid:  { std::lock_guard<std::mutex> lk(g_stateMutex); g_stats.handledCalls++; return 1001; }
    case NR_set_tid_address:
    case NR_rt_sigaction:
    case NR_rt_sigprocmask:
    case NR_arch_prctl: {
        std::lock_guard<std::mutex> lk(g_stateMutex);
        g_stats.handledCalls++;
        return 0;
    }

    case NR_uname: {
        struct GuestUtsname { char sysname[65]; char nodename[65]; char release[65];
                               char version[65]; char machine[65]; char domain[65]; };
        void* p = nullptr;
        if (!GuestToHost(a0, sizeof(GuestUtsname), &p)) return kErrInval;
        auto* u = static_cast<GuestUtsname*>(p);
        memset(u, 0, sizeof(GuestUtsname));
        strcpy(u->sysname, "Linux");
        strcpy(u->release, "6.6.0-px5-foundation");
        strcpy(u->version, "#1 PX5 HLE bridge");
        strcpy(u->machine, "x86_64");     // guest-world identity
        std::lock_guard<std::mutex> lk(g_stateMutex);
        g_stats.handledCalls++;
        return 0;
    }

    case NR_clock_gettime: {
        void* ts = nullptr;
        if (!GuestToHost(a1, 16, &ts)) return kErrInval;
        struct timespec host{};
        clock_gettime(CLOCK_MONOTONIC, &host);
        memcpy(ts, &host, 16);
        std::lock_guard<std::mutex> lk(g_stateMutex);
        g_stats.handledCalls++;
        return 0;
    }

    case PX5::RuntimeLinker::kPx5NidGateSyscall: {
        // PX5 NID gate (v1.31): a0 = NID, a1..a5 = HLE arguments. Real
        // dispatch into the RuntimeLinker registry: registered HLE exports
        // are bionic-native host functions; unknown NIDs and guest-kind
        // exports fail by name (see DispatchNid) and return ENOSYS here.
        const uint64_t gateArgs[5] = {a1, a2, a3, a4, a5};
        const RuntimeLinker::GateResult gr =
            RuntimeLinker::GetInstance().DispatchNid(a0, gateArgs, 5);
        {
            std::lock_guard<std::mutex> lk(g_stateMutex);
            g_stats.handledCalls++;   // outcome tracked in linker stats
        }
        return gr.ok ? static_cast<uint64_t>(gr.value) : kErrNoSys;
    }

    case NR_futex:                           // single-threaded guests only
    case NR_sched_getaffinity: {
        std::lock_guard<std::mutex> lk(g_stateMutex);
        g_stats.handledCalls++;
        return nr == NR_sched_getaffinity ? 1 : 0;
    }

    default: {
        std::lock_guard<std::mutex> lk(g_stateMutex);
        g_stats.unhandledCalls++;
        PX5_LOGW(LogCategory::KERNEL,
                 "SYSCALL %u: unknown number -> ENOSYS (calls handled=%llu unhandled=%llu)",
                 nr, (unsigned long long)g_stats.handledCalls,
                 (unsigned long long)g_stats.unhandledCalls);
        return kErrNoSys;
    }
    }
}

std::string GuestSyscalls::TakeOutput() {
    std::lock_guard<std::mutex> lk(g_stateMutex);
    std::string out = std::move(g_output);
    g_output.clear();
    return out;
}

void GuestSyscalls::AppendOutput(const std::string& s) {
    std::lock_guard<std::mutex> lk(g_stateMutex);
    if (g_output.size() + s.size() <= kMaxCapture) g_output += s;
}

bool GuestSyscalls::HasExitCode() {
    std::lock_guard<std::mutex> lk(g_stateMutex);
    return g_hasExitCode;
}

uint64_t GuestSyscalls::ExitCode() {
    std::lock_guard<std::mutex> lk(g_stateMutex);
    return g_exitCode;
}

void GuestSyscalls::ResetRun() {
    std::lock_guard<std::mutex> lk(g_stateMutex);
    g_output.clear();
    g_hasExitCode = false;
    g_exitCode = 0;
    g_stats = {};
}

const GuestSyscallStats& GuestSyscalls::Stats() {
    return g_stats; // read-mostly; UI path tolerates benign races
}

} // namespace PX5
