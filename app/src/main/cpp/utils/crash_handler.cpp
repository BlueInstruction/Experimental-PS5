#include "crash_handler.h"
#include "logger.h"
#include "utils/breadcrumbs.h"

#include <android/log.h>
#include <csignal>
#include <cerrno>
#include <exception>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <atomic>
#include <fcntl.h>
#include <unistd.h>
#include <unwind.h>
#include <sys/uio.h>
#include <sys/mman.h>
#include <sys/types.h>

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

// ---------------------------------------------------------------------------
// v1.16 — module resolution. The 2026-08-30 v1.15 session produced dumps
// with ABSOLUTE backtrace addresses only ("#00 pc 0x79c5634308"). Without
// the module table there is no way to say which library a frame belongs to,
// so every crash stayed "unclear log" for the user. The table below is read
// from /proc/self/maps INSIDE the handler (open/read only — async-signal-
// safe on bionic), and every reported address gains "lib.so+0xOFFSET" using
// the tombstone convention rel = addr - map_start + mapping_pgoff.
// ---------------------------------------------------------------------------
struct MapEntry { uintptr_t start; uintptr_t end; uintptr_t pgoff; char name[160]; };
constexpr int kMaxMapEntries = 64;
struct MapTable { MapEntry e[kMaxMapEntries]; int n = 0; };

bool ParseHex(const char*& p, uintptr_t& out) {
    while (*p == ' ') ++p;
    uintptr_t v = 0;
    int digits = 0;
    for (; digits < 16; ++digits) {
        const char c = *p;
        unsigned d;
        if (c >= '0' && c <= '9')      d = static_cast<unsigned>(c - '0');
        else if (c >= 'a' && c <= 'f') d = static_cast<unsigned>(c - 'a') + 10u;
        else if (c >= 'A' && c <= 'F') d = static_cast<unsigned>(c - 'A') + 10u;
        else break;
        v = (v << 4) | d;
        ++p;
    }
    if (digits == 0) return false;
    out = v;
    return true;
}

void ParseMapLine(char* line, MapTable& t) {
    if (t.n >= kMaxMapEntries) return;
    const char* p = line;
    uintptr_t start = 0, end = 0, pgoff = 0;
    if (!ParseHex(p, start)) return;
    if (*p != '-') return;
    ++p;
    if (!ParseHex(p, end)) return;
    while (*p == ' ') ++p;
    // perms "rwxp": only executable, file-backed entries are useful for
    // symbolization; skipping the rest keeps the table small.
    if (p[0] == '\0' || p[1] == '\0' || p[2] == '\0') return;
    if (p[2] != 'x') return;
    p += 4;
    if (!ParseHex(p, pgoff)) return;
    // Skip dev (nn:nn) and inode tokens, then the path is the remainder
    // after the last space.
    while (*p == ' ') ++p;                    // dev
    while (*p != ' ' && *p != '\0') ++p;
    while (*p == ' ') ++p;                    // inode
    while (*p != ' ' && *p != '\0') ++p;
    while (*p == ' ') ++p;
    if (*p == '\0' || *p != '/') return;      // anonymous mapping
    const size_t len = strlen(p);
    if (len == 0 || len >= sizeof(MapEntry::name)) return;

    MapEntry& e = t.e[t.n];
    e.start = start; e.end = end; e.pgoff = pgoff;
    memcpy(e.name, p, len + 1);
    ++t.n;
}

void BuildMapTable(MapTable& t) {
    int fd = open("/proc/self/maps", O_RDONLY);
    if (fd < 0) return;
    char buf[16384];
    size_t len = 0;
    for (;;) {
        if (len >= sizeof(buf) - 1) break;
        const ssize_t r = read(fd, buf + len, sizeof(buf) - 1 - len);
        if (r <= 0) break;
        len += static_cast<size_t>(r);
        buf[len] = '\0';
        char* line = buf;
        char* nl;
        while ((nl = strchr(line, '\n')) != nullptr) {
            *nl = '\0';
            ParseMapLine(line, t);
            line = nl + 1;
        }
        const size_t rem = static_cast<size_t>(buf + len - line);
        memmove(buf, line, rem);
        len = rem;
        if (t.n >= kMaxMapEntries) break;
    }
    close(fd);
}

int FindMap(const MapTable& t, uintptr_t addr) {
    for (int i = 0; i < t.n; ++i) {
        if (addr >= t.e[i].start && addr < t.e[i].end) return i;
    }
    return -1;
}

// Formats "0xADDR (lib.so+0xREL)" — the pair that makes a dump symbolizable
// against the build's unstripped libraries without any other tooling.
void FormatAddr(const MapTable& t, uintptr_t addr, char* out, size_t n) {
    if (addr == 0) { snprintf(out, n, "0x0"); return; }
    const int i = FindMap(t, addr);
    if (i < 0) {
        snprintf(out, n, "0x%lx", static_cast<unsigned long>(addr));
        return;
    }
    const char* slash = strrchr(t.e[i].name, '/');
    const char* base = slash ? slash + 1 : t.e[i].name;
    snprintf(out, n, "0x%lx (%s+0x%lx)",
             static_cast<unsigned long>(addr), base,
             static_cast<unsigned long>(addr - t.e[i].start + t.e[i].pgoff));
}

// The two-target writer. Target 1: px5_crash_latest.log (the UI's quick
// read + VerifyChildDump contract). Target 2: px5_main.log — v1.16
// unification: the full report is appended to THE log the user pastes, so
// one file carries the whole story (events + native lines + the crash).
// Both opens are fresh O_APPEND opens inside the handler: rotation-proof
// and lock-free.
struct ReportWriter { int latestFd; int mainFd; };

void WAppend(ReportWriter& w, const char* s) {
    const size_t len = strlen(s);
    if (w.latestFd >= 0) { ssize_t r = write(w.latestFd, s, len); (void)r; }
    if (w.mainFd   >= 0) { ssize_t r = write(w.mainFd,   s, len); (void)r; }
}

void WriteCrashReport(int sig, siginfo_t* info, void* uctx) {   // NOLINT(bugprone-easily-swappable-parameters)
    char latestPath[512];
    time_t now = time(nullptr);
    struct tm tmv{};
    localtime_r(&now, &tmv);
    char stamp[32];
    strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H-%M-%S", &tmv);
    snprintf(latestPath, sizeof(latestPath), "%s/px5_crash_latest.log",
             g_logsDir.empty() ? "/data/local/tmp" : g_logsDir.c_str());

    ReportWriter w{-1, -1};
    w.latestFd = open(latestPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    // Lock-free path read (PeekLogFilePathUnsafe): the locking getter could
    // deadlock the handler when a thread died holding the logger mutex.
    // No std::string here either — allocation inside a signal handler can
    // deadlock on the allocator lock; raw pointer only.
    const char* mainPath = Logger::PeekLogFilePathUnsafe();
    if (mainPath && *mainPath) {
        w.mainFd = open(mainPath, O_WRONLY | O_APPEND, 0644);
    }
    if (w.latestFd < 0 && w.mainFd < 0) {
        // Never lose the report entirely.
        w.latestFd = STDERR_FILENO;
    }

    char line[512];
    auto append = [&](const char* s) { WAppend(w, s); };

    snprintf(line, sizeof(line), "\n==== PX5 CRASH %04d-%02d-%02d %02d:%02d:%02d ====\n",
             tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
             tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
    append(line);

    // v1.16: pid comes from getpid() — si_pid is 0 for synchronous signals
    // (the 2026-08-30 dumps read "pid=0", which is meaningless for a
    // synchronous SEGV and confused the child-vs-parent attribution).
    snprintf(line, sizeof(line),
             "signal=%s(%d) si_addr=%p si_code=%d pid=%d tid=%lu stamp=%s\n",
             SignalName(sig), sig,
             info ? info->si_addr : nullptr,
             info ? info->si_code : 0,
             static_cast<int>(getpid()),
             static_cast<unsigned long>(gettid()),
             stamp);
    append(line);

    // Module table FIRST: every address below gains meaning from it.
    MapTable maps;
    BuildMapTable(maps);
    if (maps.n > 0) {
        append("modules (exec, file-backed — rel=addr-start+pgoff):\n");
        for (int i = 0; i < maps.n; ++i) {
            const char* slash = strrchr(maps.e[i].name, '/');
            snprintf(line, sizeof(line), "  %08lx-%08lx %s+0..  %s\n",
                     static_cast<unsigned long>(maps.e[i].start),
                     static_cast<unsigned long>(maps.e[i].end),
                     slash ? slash + 1 : maps.e[i].name,
                     maps.e[i].name);
            append(line);
        }
    } else {
        append("modules: /proc/self/maps unavailable or no exec mappings\n");
    }

    // The last known native steps. v1.16: each crumb carries its tid, so
    // parallel diagnostic threads can no longer blur which step belonged
    // to which thread (the v1.15 08:15:31 dump mixed three threads).
    append("last breadcrumbs (tid-prefixed):\n");
    {
        long n1 = Breadcrumb::DumpToFd(w.latestFd);
        (void)n1;
        if (w.mainFd >= 0) {
            long n2 = Breadcrumb::DumpToFd(w.mainFd);
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
        char pcBuf[256], lrBuf[256];
        FormatAddr(maps, static_cast<uintptr_t>(mc.pc), pcBuf, sizeof(pcBuf));
        FormatAddr(maps, static_cast<uintptr_t>(mc.regs[30]), lrBuf, sizeof(lrBuf));
        snprintf(line, sizeof(line),
                 "  sp=0x%016llx pc=%s lr(x30)=%s pstate=0x%08llx\n",
                 (unsigned long long)mc.sp, pcBuf, lrBuf,
                 (unsigned long long)mc.pstate);
        append(line);

        // v1.16: every frame resolved against the module table — the raw
        // absolute address is kept alongside so ndk-stack still works.
        BtCtx bt{};
        _Unwind_Backtrace(BtFn, &bt);
        append("backtrace (raw + module-resolved):\n");
        for (int i = 0; i < bt.n; ++i) {
            char addrBuf[256];
            FormatAddr(maps, reinterpret_cast<uintptr_t>(bt.frames[i]),
                       addrBuf, sizeof(addrBuf));
            snprintf(line, sizeof(line), "  #%02d pc %s\n", i, addrBuf);
            append(line);
        }
#else
        (void)uc;
        BtCtx bt{};
        _Unwind_Backtrace(BtFn, &bt);
        append("backtrace (register dump is arm64-only):\n");
        for (int i = 0; i < bt.n; ++i) {
            char addrBuf[256];
            FormatAddr(maps, reinterpret_cast<uintptr_t>(bt.frames[i]),
                       addrBuf, sizeof(addrBuf));
            snprintf(line, sizeof(line), "  #%02d pc %s\n", i, addrBuf);
            append(line);
        }
#endif
    } else {
        append("ucontext unavailable (synthetic raise)\n");
    }

    append("full session log: ");
    append((mainPath && *mainPath) ? mainPath : "(logger not initialized)");
    append("\n");

    if (w.latestFd >= 0 && w.latestFd != STDERR_FILENO) {
        fsync(w.latestFd);
        close(w.latestFd);
    }
    if (w.mainFd >= 0) {
        fsync(w.mainFd);
        close(w.mainFd);
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

    // v1.13: uncaught C++ exceptions land in std::terminate -> abort() ->
    // SIGABRT. The register dump that follows is complete, but without
    // the active exception's message a SIGABRT looks unmotivated — the
    // 2026-08-30 device session captured exactly such a signal-6 dump and
    // could only guess at the cause. Capture what() into the breadcrumb
    // ring BEFORE abort() so the signal dump itself carries the reason.
    // Normal (non-signal) context: normal library calls are fine here.
    std::set_terminate([] {
        std::string what = "(non-standard exception object)";
        if (std::current_exception()) {
            try {
                std::rethrow_exception(std::current_exception());
            } catch (const std::exception& e) {
                what = e.what();
            } catch (...) {
            }
        }
        PX5_LOGE(LogCategory::CORE,
                 "FATAL std::terminate — uncaught C++ exception: %s",
                 what.c_str());
        Breadcrumb::Set("terminate: uncaught exception: %s", what.c_str());
        std::abort();  // SIGABRT -> the armed handler writes the full dump
    });

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
             "CrashHandler installed: full report -> px5_crash_latest.log "
             "AND appended into the unified px5_main.log (module-resolved "
             "backtrace since v1.16)");
}

const std::string& CrashHandler::LogsDir() { return g_logsDir; }

void CrashHandler::SetFaultIntercept(FaultIntercept fn) {
    g_faultIntercept = fn;
}

} // namespace PX5
