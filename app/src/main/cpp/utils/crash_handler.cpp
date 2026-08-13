// SPDX-License-Identifier: MIT
// PX5 — Native Crash Handler implementation

#include "crash_handler.h"
#include "logger.h"

#include <android/log.h>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <mutex>
#include <pthread.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ucontext.h>
#include <ucontext.h>
#include <unistd.h>
#include <unwind.h>

#ifndef PR_GET_NAME
#define PR_GET_NAME 16
#endif

namespace PX5 {

namespace {

struct CrashHandlerState {
    std::mutex      mtx;
    std::string     log_dir;
    std::atomic<bool> installed{false};
    // Save old sigactions so we can chain (or restore) them.
    struct sigaction old_sa[NSIG];
};

CrashHandlerState& State() {
    static CrashHandlerState s;
    return s;
}

// Async-signal-safe: write a C string to fd.
void safe_write(int fd, const char* s) {
    if (!s) return;
    ::write(fd, s, std::strlen(s));
}
void safe_write(int fd, const char* s, size_t n) {
    if (s) ::write(fd, s, n);
}

// Async-signal-safe-ish: format an unsigned integer.
size_t safe_utoa(char* buf, unsigned long long v) {
    char tmp[32];
    size_t i = 0;
    if (v == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return 1;
    }
    while (v > 0 && i < sizeof(tmp)) {
        tmp[i++] = '0' + (v % 10);
        v /= 10;
    }
    // reverse
    for (size_t j = 0; j < i; ++j) buf[j] = tmp[i - 1 - j];
    buf[i] = '\0';
    return i;
}

size_t safe_utoa_hex(char* buf, unsigned long long v) {
    char tmp[32];
    size_t i = 0;
    if (v == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return 1;
    }
    while (v > 0 && i < sizeof(tmp)) {
        unsigned d = v & 0xF;
        tmp[i++] = (d < 10) ? ('0' + d) : ('a' + (d - 10));
        v >>= 4;
    }
    for (size_t j = 0; j < i; ++j) buf[j] = tmp[i - 1 - j];
    buf[i] = '\0';
    return i;
}

// Compose crash file name: px5_crash_YYYYMMDD_HHMMSS_<pid>.log
// (we add pid to disambiguate when multiple crashes happen within the same
// second, e.g. during a fork.)
void make_crash_filename(char* buf, size_t buflen) {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto t = system_clock::to_time_t(now);
    struct tm tm_buf;
    localtime_r(&t, &tm_buf);

    char ts[32];
    std::snprintf(ts, sizeof(ts), "%04d%02d%02d_%02d%02d%02d",
                  tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
                  tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec);

    auto pid = static_cast<unsigned>(::getpid());
    std::snprintf(buf, buflen, "%s/px5_crash_%s_%u.log",
                  State().log_dir.c_str(), ts, pid);
}

// Async-signal-safe-ish: dump the ARM64 GP register file from ucontext.
void dump_registers(int fd, ucontext_t* uc) {
    if (!uc) {
        safe_write(fd, "  (no ucontext available)\n");
        return;
    }

    // ARM64: mcontext.regs is an array of 31 GP registers (x0..x30)
    // plus sp, pc, pstate.
    auto& regs = uc->uc_mcontext.regs;

    safe_write(fd, "\nRegisters (ARM64):\n");
    char buf[160];
    for (int i = 0; i < 31; ++i) {
        // x0..x28: 2 per line; x29 (FP) and x30 (LR) on their own line
        if (i < 29 && (i % 2) == 0) {
            char reg1[8], reg2[8];
            std::snprintf(reg1, sizeof(reg1), "x%d", i);
            std::snprintf(reg2, sizeof(reg2), "x%d", i + 1);
            char v1[32], v2[32];
            safe_utoa_hex(v1, reinterpret_cast<unsigned long long>(regs[i]));
            safe_utoa_hex(v2, reinterpret_cast<unsigned long long>(regs[i + 1]));
            std::snprintf(buf, sizeof(buf), "  %-5s = 0x%s   %-5s = 0x%s\n",
                          reg1, v1, reg2, v2);
            safe_write(fd, buf);
        }
    }
    // x29 (FP), x30 (LR)
    {
        char v29[32], v30[32];
        safe_utoa_hex(v29, reinterpret_cast<unsigned long long>(regs[29]));
        safe_utoa_hex(v30, reinterpret_cast<unsigned long long>(regs[30]));
        std::snprintf(buf, sizeof(buf), "  x29(FP) = 0x%s   x30(LR) = 0x%s\n",
                      v29, v30);
        safe_write(fd, buf);
    }
    // SP, PC, PSTATE
    {
        char sp_buf[32], pc_buf[32], ps_buf[32];
        safe_utoa_hex(sp_buf, reinterpret_cast<unsigned long long>(uc->uc_mcontext.sp));
        safe_utoa_hex(pc_buf, reinterpret_cast<unsigned long long>(uc->uc_mcontext.pc));
        safe_utoa_hex(ps_buf, static_cast<unsigned long long>(uc->uc_mcontext.pstate));
        std::snprintf(buf, sizeof(buf),
                      "  SP     = 0x%s   PC     = 0x%s   PSTATE = 0x%s\n",
                      sp_buf, pc_buf, ps_buf);
        safe_write(fd, buf);
    }
}

// _Unwind_Backtrace callback state.
struct UnwindState {
    int      fd{-1};
    int      count{0};
    int      max_frames{64};
};

_Unwind_Reason_Code unwind_callback(struct _Unwind_Context* ctx, void* arg) {
    auto* us = static_cast<UnwindState*>(arg);
    if (us->count >= us->max_frames) return _URC_END_OF_STACK;

    auto pc = reinterpret_cast<unsigned long long>(_Unwind_GetIP(ctx));
    if (pc == 0) return _URC_END_OF_STACK;

    char buf[80];
    char pc_buf[32];
    safe_utoa_hex(pc_buf, pc);
    std::snprintf(buf, sizeof(buf), "  #%02d  pc 0x%s\n", us->count, pc_buf);
    safe_write(us->fd, buf);

    us->count++;
    return _URC_CONTINUE_UNWIND;
}

void dump_backtrace(int fd) {
    safe_write(fd, "\nBacktrace:\n");
    UnwindState us;
    us.fd = fd;
    _Unwind_Backtrace(unwind_callback, &us);
}

// The actual signal handler.
extern "C" void px5_signal_handler(int signo, siginfo_t* info, void* ucontext) {
    auto& s = State();

    // Open a dedicated crash file (separate from px5_main.log so we never
    // lose prior context to overwrites).
    char crash_path[512];
    make_crash_filename(crash_path, sizeof(crash_path));
    int fd = ::open(crash_path, O_WRONLY | O_CREAT | O_TRUNC, 0660);
    if (fd < 0) {
        // Fallback: dump to logcat only.
        __android_log_print(ANDROID_LOG_FATAL, "PX5_Crash",
                            "Could not open crash file; dumping to logcat only");
    }

    auto write_line = [&](const char* line) {
        if (fd >= 0) safe_write(fd, line);
        // Also mirror to logcat so the line is visible even if file fails.
        __android_log_print(ANDROID_LOG_FATAL, "PX5_Crash", "%s", line);
    };

    // Header
    write_line("==============================================================\n");
    write_line("PX5 NATIVE CRASH REPORT\n");
    write_line("==============================================================\n");

    // Timestamp
    {
        using namespace std::chrono;
        auto now = system_clock::now();
        auto t = system_clock::to_time_t(now);
        struct tm tm_buf;
        localtime_r(&t, &tm_buf);
        auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
        char ts[64];
        std::snprintf(ts, sizeof(ts),
                      "Timestamp: %04d-%02d-%02d %02d:%02d:%02d.%03d\n",
                      tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
                      tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
                      static_cast<int>(ms.count()));
        write_line(ts);
    }

    // Process / thread info
    {
        char name_buf[16] = {0};
        prctl(PR_GET_NAME, name_buf);
        char hdr[256];
        char pid_buf[16], tid_buf[16];
        safe_utoa(pid_buf, static_cast<unsigned long long>(::getpid()));
        safe_utoa(tid_buf, static_cast<unsigned long long>(::gettid()));
        std::snprintf(hdr, sizeof(hdr),
                      "PID: %s   TID: %s   Thread: %s\n",
                      pid_buf, tid_buf, name_buf);
        write_line(hdr);
    }

    // Signal info
    {
        const char* signame = "UNKNOWN";
        switch (signo) {
            case SIGSEGV: signame = "SIGSEGV"; break;
            case SIGABRT: signame = "SIGABRT"; break;
            case SIGBUS:  signame = "SIGBUS";  break;
            case SIGILL:  signame = "SIGILL";  break;
            case SIGFPE:  signame = "SIGFPE";  break;
            case SIGPIPE: signame = "SIGPIPE"; break;
            case SIGTRAP: signame = "SIGTRAP"; break;
            case SIGSYS:  signame = "SIGSYS";  break;
        }
        char hdr[256];
        char addr_buf[32], code_buf[16];
        safe_utoa_hex(addr_buf, reinterpret_cast<unsigned long long>(info->si_addr));
        safe_utoa(code_buf, static_cast<unsigned long long>(info->si_code));
        std::snprintf(hdr, sizeof(hdr),
                      "Signal: %d (%s)   code: %s   fault address: 0x%s\n",
                      signo, signame, code_buf, addr_buf);
        write_line(hdr);
    }

    // Registers
    if (ucontext) {
        dump_registers(fd, static_cast<ucontext_t*>(ucontext));
    }

    // Backtrace
    dump_backtrace(fd);

    // Footer
    write_line("\n==============================================================\n");
    write_line("END OF CRASH REPORT — flushing and re-raising signal\n");
    write_line("==============================================================\n");

    // Flush the Logger's main log file too (so any pending messages land
    // before the process dies).
    Logger::Flush();

    if (fd >= 0) {
        ::fsync(fd);
        ::close(fd);
    }

    // Restore the default disposition and re-raise so the kernel produces
    // a tombstone and the process dies cleanly.
    struct sigaction sa{};
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    ::sigaction(signo, &sa, nullptr);
    ::raise(signo);
}

bool install_one(int signo) {
    auto& s = State();
    struct sigaction sa{};
    sa.sa_sigaction = px5_signal_handler;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&sa.sa_mask);
    return ::sigaction(signo, &sa, &s.old_sa[signo]) == 0;
}

}  // namespace

// ============================================================================
// Public API
// ============================================================================

bool CrashHandler::Install(std::string_view log_dir) noexcept {
    auto& s = State();
    std::lock_guard<std::mutex> lock(s.mtx);
    if (s.installed.load()) return true;

    s.log_dir.assign(log_dir);

    // Make sure the dir exists (best-effort).
    struct stat st{};
    if (::stat(s.log_dir.c_str(), &st) != 0) {
        // Try to create it; ignore failure — the Logger may have already done it.
        ::mkdir(s.log_dir.c_str(), 0770);
    }

    bool ok = true;
    ok &= install_one(SIGSEGV);
    ok &= install_one(SIGABRT);
    ok &= install_one(SIGBUS);
    ok &= install_one(SIGILL);
    ok &= install_one(SIGFPE);
    ok &= install_one(SIGPIPE);
    ok &= install_one(SIGTRAP);
    ok &= install_one(SIGSYS);

    s.installed.store(ok);

    __android_log_print(ANDROID_LOG_INFO, "PX5_Crash",
                        "Crash handler installed (log dir: %s)",
                        s.log_dir.c_str());
    return ok;
}

void CrashHandler::Uninstall() noexcept {
    auto& s = State();
    std::lock_guard<std::mutex> lock(s.mtx);
    if (!s.installed.load()) return;

    struct sigaction sa{};
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    for (int signo : {SIGSEGV, SIGABRT, SIGBUS, SIGILL, SIGFPE, SIGPIPE, SIGTRAP, SIGSYS}) {
        ::sigaction(signo, &s.old_sa[signo] ? &s.old_sa[signo] : &sa, nullptr);
    }
    s.installed.store(false);
}

bool CrashHandler::IsInstalled() noexcept {
    return State().installed.load();
}

} // namespace PX5
