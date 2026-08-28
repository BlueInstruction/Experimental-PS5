#include "crash_handler.h"
#include "logger.h"

#include <android/log.h>
#include <csignal>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <unistd.h>
#include <unwind.h>
#include <sys/uio.h>

namespace PX5 {

using FaultIntercept = bool (*)(int sig, void* siginfo, void* ucontext);

namespace {

// _Unwind-based backtrace: available on all Android APIs (execinfo's
// backtrace() only exists for API >= 33).
struct BtCtx { void* frames[24]; int n; };

_Unwind_Reason_Code BtFn(struct _Unwind_Context* c, void* p) {
    auto* b = static_cast<BtCtx*>(p);
    if (b->n < 24) b->frames[b->n++] = reinterpret_cast<void*>(_Unwind_GetIP(c));
    return _URC_NO_REASON;
}
std::string g_logsDir;
FaultIntercept g_faultIntercept = nullptr;

const char* SignalName(int sig) {
    switch (sig) {
    case SIGSEGV: return "SIGSEGV";
    case SIGBUS:  return "SIGBUS";
    case SIGILL:  return "SIGILL";
    case SIGFPE:  return "SIGFPE";
    case SIGABRT: return "SIGABRT";
    case SIGTRAP: return "SIGTRAP";
    default:      return "SIG?";
    }
}

// The single write path. Async-signal-safety: only open/write/dprintf are
// used inside the handler; the backtrace call is best-effort (bionic's
// implementation is unwind-based and does not take locks we hold).
void WriteCrashReport(int sig, siginfo_t* info, void* uctx) {   // NOLINT(bugprone-easily-swappable-parameters)
    char path[512];
    snprintf(path, sizeof(path), "%s/px5_crash.log",
             g_logsDir.empty() ? "/data/local/tmp" : g_logsDir.c_str());

    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        fd = STDERR_FILENO;  // never lose the report entirely
    }

    char line[256];
    auto append = [&](const char* s) { write(fd, s, strlen(s)); };

    time_t now = time(nullptr);
    struct tm tmv{};
    localtime_r(&now, &tmv);
    snprintf(line, sizeof(line), "\n==== PX5 CRASH %04d-%02d-%02d %02d:%02d:%02d ====\n",
             tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
             tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
    append(line);

    snprintf(line, sizeof(line), "signal=%s(%d) si_addr=%p si_code=%d pid=%d tid=%lu\n",
             SignalName(sig), sig,
             info ? info->si_addr : nullptr,
             info ? info->si_code : 0,
             info ? static_cast<int>(info->si_pid) : -1,
             static_cast<unsigned long>(gettid()));
    append(line);

    if (uctx) {
        auto* uc = static_cast<ucontext_t*>(uctx);
#if defined(__aarch64__)
        const auto& mc = uc->uc_mcontext;
        append("registers:\n");
        for (int i = 0; i < 31; i += 3) {
            snprintf(line, sizeof(line),
                     "  x%-2d=0x%016llx x%-2d=0x%016llx x%-2d=0x%016llx\n",
                     i,   (unsigned long long)mc.regs[i],
                     i+1, (unsigned long long)(i+1 < 31 ? mc.regs[i+1] : 0),
                     i+2, (unsigned long long)(i+2 < 31 ? mc.regs[i+2] : 0));
            append(line);
        }
        snprintf(line, sizeof(line),
                 "  sp=0x%016llx pc=0x%016llx pstate=0x%08llx\n",
                 (unsigned long long)mc.sp,
                 (unsigned long long)mc.pc,
                 (unsigned long long)mc.pstate);
        append(line);

        // Best-effort backtrace: addresses only on-device (symbolization is
        // done by ndk-stack from the matching build). Honest about limits.
        BtCtx bt{};
        _Unwind_Backtrace(BtFn, &bt);
        append("backtrace (addresses; use ndk-stack with this build):\n");
        for (int i = 0; i < bt.n; ++i) {
            snprintf(line, sizeof(line), "  #%02d pc %p\n", i, bt.frames[i]);
            append(line);
        }
#else
        (void)uc;
        BtCtx bt{};
        _Unwind_Backtrace(BtFn, &bt);
        append("backtrace (addresses; register dump is arm64-only):\n");
        for (int i = 0; i < bt.n; ++i) {
            snprintf(line, sizeof(line), "  #%02d pc %p\n", i, bt.frames[i]);
            append(line);
        }
#endif
    } else {
        append("ucontext unavailable (synthetic raise)\n");
    }

    append(Logger::GetCurrentLogFilePath().c_str());
    append("\n");
    fsync(fd);
    if (fd != STDERR_FILENO) close(fd);
}

void Handler(int sig, siginfo_t* info, void* uctx) {
    // Routing question order (see class contract): engine-owned fault
    // classes are offered the fault BEFORE anything is written. A fault
    // nobody claims is a real crash and gets the full report below.
    if (g_faultIntercept && g_faultIntercept(sig, info, uctx)) {
        return;
    }
    WriteCrashReport(sig, info, uctx);
    // Restore default disposition and re-raise so Android tombstoning and
    // crash reporting still see the real signal.
    signal(sig, SIG_DFL);
    raise(sig);
}

} // namespace

void CrashHandler::Install(const std::string& logsDir) {
    static bool installed = false;
    if (installed) return;
    installed = true;

    g_logsDir = logsDir;

    struct sigaction sa{};
    sa.sa_sigaction = Handler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);

    const int signals[] = { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT, SIGTRAP };
    for (int s : signals) {
        if (sigaction(s, &sa, nullptr) != 0) {
            PX5_LOGW(LogCategory::CORE,
                     "CrashHandler: sigaction(%d) failed errno=%d", s, errno);
        }
    }
    PX5_LOGI(LogCategory::CORE,
             "CrashHandler installed: reports -> %s/px5_crash.log",
             logsDir.c_str());
}

const std::string& CrashHandler::LogsDir() { return g_logsDir; }

void CrashHandler::SetFaultIntercept(FaultIntercept fn) {
    g_faultIntercept = fn;
}

} // namespace PX5
