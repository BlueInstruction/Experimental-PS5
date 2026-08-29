#include "crash_handler.h"
#include "logger.h"
#include "utils/breadcrumbs.h"

#include <android/log.h>
#include <csignal>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <atomic>
#include <fcntl.h>
#include <unistd.h>
#include <unwind.h>
#include <sys/uio.h>
#include <sys/mman.h>

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

// Re-entrancy guard: a fault raised while the handler itself is reporting
// means the reporting path faulted. Previously this died silently inside
// the handler with ZERO evidence — the 2026-08-29 pattern. Now the nested
// entry writes a one-line marker and restores the default disposition.
std::atomic<bool> g_inHandler{false};

// Claim-loop watchdog: an intercept that claims the SAME faulting PC over
// and over is not "handling" anything — it resumes into the same faulting
// instruction forever (screen freezes, then the system SIGKILLs the app
// silently — the other 2026-08-29 death mode). After kMaxSamePcClaims
// consecutive claims of one PC the intercept verdict is overridden to
// "unclaimed" so the real crash report gets written.
constexpr int kMaxSamePcClaims = 4;
std::atomic<void*> g_lastClaimedPc{nullptr};
std::atomic<int> g_samePcClaims{0};

// Faulting PC from a ucontext. The member layout is architecture-specific:
// aarch64 exposes uc_mcontext.pc, x86_64 keeps RIP in uc_mcontext.gregs.
// Getting this wrong fails the foreign-ABI build (CI, 2026-08-29), so it
// lives in exactly one guarded function.
void* FaultingPc(void* uctx) {
    if (!uctx) return nullptr;
    auto* uc = static_cast<ucontext_t*>(uctx);
#if defined(__aarch64__)
    return reinterpret_cast<void*>(uc->uc_mcontext.pc);
#elif defined(__x86_64__)
    return reinterpret_cast<void*>(
        static_cast<uintptr_t>(uc->uc_mcontext.gregs[REG_RIP]));
#else
    return nullptr;
#endif
}

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
//
// Naming contract with the Kotlin log store (PX5Application.listCrashLogs
// reads "px5_crash_*.log", the live viewer tails "px5_crash_latest.log"):
// the native report writes BOTH a timestamped file and refreshes latest,
// otherwise real native crashes stay invisible in the Logs screen (this
// is why the 2026-08-28 device crashes left no visible evidence).
void WriteCrashReport(int sig, siginfo_t* info, void* uctx) {   // NOLINT(bugprone-easily-swappable-parameters)
    char path[512];
    char latestPath[512];
    time_t now = time(nullptr);
    struct tm tmv{};
    localtime_r(&now, &tmv);
    char stamp[32];
    strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H-%M-%S", &tmv);
    snprintf(path, sizeof(path), "%s/px5_crash_%s.log",
             g_logsDir.empty() ? "/data/local/tmp" : g_logsDir.c_str(), stamp);
    snprintf(latestPath, sizeof(latestPath), "%s/px5_crash_latest.log",
             g_logsDir.empty() ? "/data/local/tmp" : g_logsDir.c_str());

    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    int latestFd = open(latestPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0 && latestFd < 0) {
        fd = STDERR_FILENO;  // never lose the report entirely
    } else if (fd < 0) {
        fd = latestFd;       // at least the tail the UI reads
        latestFd = -1;
    }

    char line[256];
    auto appendTo = [&](int target, const char* s) {
        if (target >= 0) write(target, s, strlen(s));
    };
    auto append = [&](const char* s) {
        appendTo(fd, s);
        appendTo(latestFd, s);
    };

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

    // The last known native steps (fixed preallocated ring — async-signal-
    // safe drain). This is what pins WHICH engine step was in flight when
    // the process died, the exact information the 2026-08-29 pastes lacked.
    append("last breadcrumbs:\n");
    {
        // DumpToFd writes to one fd; call it for both targets.
        long n1 = Breadcrumb::DumpToFd(fd);
        (void)n1;
        if (latestFd >= 0) {
            long n2 = Breadcrumb::DumpToFd(latestFd);
            (void)n2;
        }
    }

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

    // One-liner into the pasted diagnostic stream (px5_diagnostic.log).
    // Process death is the one event a pasted log can never contain
    // afterwards — the 2026-08-29 "Let's Build A Zoo" crash left the event
    // stream silent because this report only went to its own file. Mirror
    // the essentials while we are still running in the handler; same
    // async-signal-safe discipline (open/write/fsync only).
    {
        char diagPath[512];
        snprintf(diagPath, sizeof(diagPath), "%s/px5_diagnostic.log",
                 g_logsDir.empty() ? "/data/local/tmp" : g_logsDir.c_str());
        int dfd = open(diagPath, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (dfd >= 0) {
            char diag[320];
#if defined(__aarch64__)
            const void* pc = uctx
                ? reinterpret_cast<const void*>(
                      reinterpret_cast<ucontext_t*>(uctx)->uc_mcontext.pc)
                : nullptr;
            snprintf(diag, sizeof(diag),
                     "[%04d-%02d-%02d %02d:%02d:%02d] NATIVE level=FATAL "
                     "cat=PX5_System process_crashed signal=%s(%d) pc=%p "
                     "dump=%s\n",
                     tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
                     tmv.tm_hour, tmv.tm_min, tmv.tm_sec,
                     SignalName(sig), sig, pc, path);
#else
            snprintf(diag, sizeof(diag),
                     "[%04d-%02d-%02d %02d:%02d:%02d] NATIVE level=FATAL "
                     "cat=PX5_System process_crashed signal=%s(%d) dump=%s\n",
                     tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
                     tmv.tm_hour, tmv.tm_min, tmv.tm_sec,
                     SignalName(sig), sig, path);
#endif
            write(dfd, diag, strlen(diag));
            fsync(dfd);
            close(dfd);
        }
    }

    fsync(fd);
    if (fd >= 0 && fd != STDERR_FILENO) close(fd);
    if (latestFd >= 0 && latestFd != STDERR_FILENO) {
        fsync(latestFd);
        close(latestFd);
    }
}

void Handler(int sig, siginfo_t* info, void* uctx) {
    // Nested fault while reporting: the report path itself faulted. Get a
    // minimal marker on stderr and die with the true signal — a corrupted
    // half-report is worse than a short honest marker.
    bool expected = false;
    if (!g_inHandler.compare_exchange_strong(expected, true)) {
        const char msg[] = "PX5: nested fault inside crash handler\n";
        ssize_t n = write(STDERR_FILENO, msg, sizeof(msg) - 1);
        (void)n;
        signal(sig, SIG_DFL);
        raise(sig);
        return;
    }

    // Routing question order (see class contract): engine-owned fault
    // classes are offered the fault BEFORE anything is written. A fault
    // nobody claims is a real crash and gets the full report below.
    bool claimed = false;
    if (g_faultIntercept) {
        claimed = g_faultIntercept(sig, info, uctx);
        if (claimed) {
            // Loop detection on claimed PCs (see constant comment).
            void* pc = FaultingPc(uctx);
            if (pc != nullptr && g_lastClaimedPc.load() == pc) {
                if (g_samePcClaims.fetch_add(1) + 1 >= kMaxSamePcClaims) {
                    // This "handled" class is actually a live-lock: fall
                    // through to the full crash report instead of
                    // resuming into the same fault forever.
                    claimed = false;
                    g_samePcClaims.store(0);
                    g_lastClaimedPc.store(nullptr);
                    const char msg[] =
                        "PX5: fault intercept claimed the same PC repeatedly "
                        "— overriding to crash report (live-lock guard)\n";
                    ssize_t n = write(STDERR_FILENO, msg, sizeof(msg) - 1);
                    (void)n;
                }
            } else {
                g_lastClaimedPc.store(pc);
                g_samePcClaims.store(1);
            }
        } else {
            g_lastClaimedPc.store(nullptr);
            g_samePcClaims.store(0);
        }
    }

    if (claimed) {
        g_inHandler.store(false);
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
    // ---- directory handling (EVERY call) ---------------------------------
    // JNI_OnLoad arms the handlers at library-load time with an EMPTY dir
    // (no Android context exists yet); MainActivity's
    // nativeInitRuntimeContext supplies the real app logs dir afterwards.
    // The old first-call-wins rule froze g_logsDir at "" forever, so every
    // report went to /data/local/tmp — unwritable for app processes — and
    // the evidence vanished while the UI claimed a dump had been written
    // (2026-08-30 device session: two self-test crashes, zero dumps).
    // Last non-empty dir wins now.
    if (!logsDir.empty() && logsDir != g_logsDir) {
        g_logsDir = logsDir;
        // Honest writability probe: better to learn EACCES here, in normal
        // execution with a logger, than inside a signal handler when the
        // process is already dying.
        const std::string probe = logsDir + "/px5_crash_probe.tmp";
        const int pfd = open(probe.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (pfd >= 0) {
            close(pfd);
            unlink(probe.c_str());
            PX5_LOGI(LogCategory::CORE,
                     "CrashHandler: logs dir writable: %s", logsDir.c_str());
        } else {
            PX5_LOGW(LogCategory::CORE,
                     "CrashHandler: logs dir NOT writable (%s): errno=%d — "
                     "crash reports will fall back to stderr/logcat",
                     logsDir.c_str(), errno);
        }
    }

    // ---- handler arming (ONCE) -------------------------------------------
    // Assignment to g_logsDir happens at startup, single-threaded, before
    // any engine threads exist — the crash handler reads it without a lock
    // by design (locking inside a signal handler can deadlock).
    static std::atomic<bool> armed{false};
    bool expected = false;
    if (!armed.compare_exchange_strong(expected, true)) return;

    // SA_ONSTACK without sigaltstack is a lie: on a real stack overflow the
    // handler would fault again on the dead stack. Allocate an honest one.
    // leaked deliberately — it must outlive every thread that may crash.
    static const size_t kAltStackSize = 256 * 1024;
    void* altStackMem = mmap(nullptr, kAltStackSize,
                             PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (altStackMem != MAP_FAILED) {
        stack_t ss{};
        ss.ss_sp = altStackMem;
        ss.ss_size = kAltStackSize;
        ss.ss_flags = 0;
        if (sigaltstack(&ss, nullptr) != 0) {
            PX5_LOGW(LogCategory::CORE,
                     "CrashHandler: sigaltstack failed errno=%d", errno);
        }
    } else {
        PX5_LOGW(LogCategory::CORE,
                 "CrashHandler: alt stack mmap failed errno=%d", errno);
    }

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
             "CrashHandler installed: reports -> %s/px5_crash_<timestamp>.log"
             " (+ px5_crash_latest.log)",
             logsDir.c_str());
}

const std::string& CrashHandler::LogsDir() { return g_logsDir; }

void CrashHandler::SetFaultIntercept(FaultIntercept fn) {
    g_faultIntercept = fn;
}

} // namespace PX5
