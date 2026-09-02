#include <jni.h>
#include <android/log.h>

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <filesystem>
#include <functional>
#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <csignal>
#include <sys/stat.h>
#include <sys/wait.h>

#include "core/emulator.h"
#include "fexcore_integration.h"
#include "gpu/vulkan_device.h"
#include "core/settings.h"
#include "utils/logger.h"
#include "utils/diag_bridge.h"
#include "utils/breadcrumbs.h"
#include "utils/crash_handler.h"
#include "utils/heartbeat.h"
#include "loader/self_extract.h"
#include "loader/runtime_linker_selftest.h"

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// JNI surface v2 (honest contract).
//
// REMOVED vs v1: every fake adrenotools/Turnip toggle, thunks/FEX config
// stubs that always returned false, and the "install" that lied about
// copying a PKG. ADDED: foundation self-test + raw guest proof + real
// Vulkan summary.
// ---------------------------------------------------------------------------

extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_stringFromJNI(JNIEnv* env, jobject) {
    return env->NewStringUTF(
        "PX5 core: FEXCore x86-64→ARM64 CPU translation, Vulkan GPU device, "
        "libkernel HLE seam");
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_initializeFexCore(JNIEnv*, jobject) {
    return PX5::FexCoreIntegration::Initialize() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeShutdown(JNIEnv*, jobject) {
    PX5::Emulator::GetInstance().Shutdown();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeInstallPkg(
        JNIEnv* env, jobject, jstring pkgPathStr, jstring destPathStr) {
    if (!pkgPathStr || !destPathStr) return JNI_FALSE;
    const char* pkg = env->GetStringUTFChars(pkgPathStr, nullptr);
    const char* dst = env->GetStringUTFChars(destPathStr, nullptr);

    bool ok = false;
    std::error_code ec;
    try {
        fs::create_directories(fs::path(dst).parent_path(), ec);
        ok = fs::copy_file(pkg, dst, fs::copy_options::overwrite_existing, ec);
    } catch (...) { ok = false; }

    PX5_LOGI(PX5::LogCategory::LOADER,
             "PKG install %s -> %s : %s (%s)", pkg, dst,
             ok ? "OK" : "FAIL", ec.message().c_str());

    env->ReleaseStringUTFChars(pkgPathStr, pkg);
    env->ReleaseStringUTFChars(destPathStr, dst);
    return ok ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeLoadElf(JNIEnv* env, jobject,
                                                        jstring pathStr) {
    if (!pathStr) return JNI_FALSE;
    const char* p = env->GetStringUTFChars(pathStr, nullptr);
    PX5::Breadcrumb::Set("jni: LoadElf %s", p);
    const bool res =
        PX5::Emulator::GetInstance().LoadExecutable(p, /*isSelf=*/false);
    env->ReleaseStringUTFChars(pathStr, p);
    return res ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeLoadSelf(JNIEnv* env, jobject,
                                                         jstring pathStr) {
    if (!pathStr) return JNI_FALSE;
    const char* p = env->GetStringUTFChars(pathStr, nullptr);
    PX5::Breadcrumb::Set("jni: LoadSelf %s", p);
    const bool res =
        PX5::Emulator::GetInstance().LoadExecutable(p, /*isSelf=*/true);
    env->ReleaseStringUTFChars(pathStr, p);
    return res ? JNI_TRUE : JNI_FALSE;
}

// Milestone 3: one honest entry point for the game-boot button. The file
// format is detected from ITS OWN magic bytes (SELF containers start with
// 0x1D3D154F — orbis/shadPS4-verified; the 0x1D22154F guess retired in
// v1.29 — and eboot.bin dumps are SELF, homebrew payloads are ELF)
// instead of the caller guessing. Unknown magics leave their first 16
// bytes in the log: the vc29 session's "bad ELF magic" with no bytes
// named cost us the whole round-trip.
namespace {
// Magic-based format dispatch shared by the direct and isolated loaders:
// SELF containers go to the extractor path, anything else to the plain
// ELF loader (same rule as nativeLoadExecutable).
bool PathLooksLikeSelf(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    uint8_t head[16] = {};
    f.read(reinterpret_cast<char*>(head), sizeof(head));
    const size_t got = static_cast<size_t>(f.gcount());

    uint32_t m = 0;
    if (got >= 4) memcpy(&m, head, 4);
    if (m == PX5::SelfExtract::kSelfMagic) return true;

    if (m != 0) {
        char b[3 * 16 + 1] = {};
        size_t o = 0;
        for (size_t i = 0; i < got; ++i)
            o += static_cast<size_t>(snprintf(b + o, sizeof(b) - o,
                                              "%s%02X", i ? " " : "",
                                              head[i]));
        PX5_LOGI(PX5::LogCategory::LOADER,
                 "format sniff %s: magic 0x%X is neither SELF(0x1D3D154F) "
                 "nor routed onward — first bytes: %s",
                 path.c_str(), (unsigned)m, b);
    }
    return false;
}

} // namespace

extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeLoadExecutable(
        JNIEnv* env, jobject, jstring pathStr) {
    if (!pathStr) return JNI_FALSE;
    const char* p = env->GetStringUTFChars(pathStr, nullptr);
    PX5::Breadcrumb::Set("jni: LoadExecutable(auto) %s", p);

    const bool isSelf = PathLooksLikeSelf(p ? p : "");
    PX5_LOGI(PX5::LogCategory::LOADER,
             "LoadExecutable: %s -> %s", p, isSelf ? "SELF" : "ELF");

    const bool res =
        PX5::Emulator::GetInstance().LoadExecutable(p, isSelf);
    env->ReleaseStringUTFChars(pathStr, p);
    return res ? JNI_TRUE : JNI_FALSE;
}

// --- Foundation evidence additions ----------------------------------------

namespace {

// The UI may only claim a dump exists when a dump file actually exists.
// The forked child inherits the armed crash handler, whose report lands in
// <logsDir>/px5_crash_latest.log BEFORE the child dies — so by the time
// waitpid returns the file should be there. Poll briefly, then report the
// verified path + size, or an explicit honest failure. This replaces the
// v1.11 text that ASSERTED "full register dump was written" without ever
// looking — exactly the unverified claim the 2026-08-30 device session
// caught (two real crashes, zero dumps, one false message).
std::string VerifyChildDump(time_t forkWall) {
    const std::string& dir = PX5::CrashHandler::LogsDir();
    if (dir.empty()) {
        return "no dump captured: crash handler has no logs dir "
               "(nativeInitRuntimeContext not wired?)";
    }
    const std::string latest = dir + "/px5_crash_latest.log";
    struct stat st{};
    for (int i = 0; i < 5; ++i) {
        if (stat(latest.c_str(), &st) == 0 && st.st_size > 0 &&
            st.st_mtime >= forkWall - 1) {
            return "register dump verified: " + latest + " (" +
                   std::to_string(static_cast<long long>(st.st_size)) +
                   " bytes) — open Settings > Diagnostics > Logs";
        }
        usleep(50 * 1000);
    }
    return "no dump file appeared in '" + dir +
           "' — the child's crash report could not be written there; "
           "its stderr copy is in logcat";
}

// Runs `work` in a fork()ed child and returns its honest report.
//
// WHY: a JIT defect must kill the TEST, not the app. The 2026-08-28 device
// logs show both proof buttons terminating the whole process right after
// "Guest thread created" — the Kotlin try/catch cannot catch a native
// signal. With this wrapper:
//   * the child inherits the crash handler, so a fault writes a register
//     dump into the app logs dir (path + size VERIFIED by the parent, see
//     VerifyChildDump — the report never promises a dump it did not see);
//   * the parent survives and reports the real wait status (exit code or
//     signal) back to the UI;
//   * the child's own report line comes back through a pipe.
//
// v1.22 — streamed child trail. The work function emits "# step" lines
// straight into the pipe as it progresses (raw write(), no buffering).
// When the child dies mid-run, the kernel has ALREADY copied every line
// into the pipe buffer, so the parent still receives the exact step that
// never completed. The 2026-08-31 session needed precisely this: both
// probe children died inside ExecuteThread with an EMPTY pipe (the report
// is written only at the end), and the death point could only be narrowed
// by reading the engine log around the crash.
struct ChildTrail {
    int fd = -1;
    __attribute__((format(printf, 2, 3)))
    void step(const char* fmt, ...) {
        if (fd < 0) return;
        char text[256];
        va_list ap;
        va_start(ap, fmt);
        int n = vsnprintf(text, sizeof(text), fmt, ap);
        va_end(ap);
        if (n < 0) return;
        if (n > static_cast<int>(sizeof(text)) - 1) n = sizeof(text) - 1;
        // v1.22: ASCII prefix, byte-exact length. The v1.20 "· " prefix
        // was UTF-8 (3 bytes) written with a 2-byte length — the missing
        // space made the parent's split miss every trail line, and they
        // leaked into the report body (seen on-device: the GPU-proof
        // result began with "·self-contained proof enter"). That broke
        // the LOAD OK / PASS prefix contracts. "# " is 2 ASCII bytes.
        ssize_t r = write(fd, "# ", 2);
        r = write(fd, text, static_cast<size_t>(n));
        r = write(fd, "\n", 1);
        (void)r;
    }
};

std::string RunIsolated(const char* name,
                        const std::function<std::string(ChildTrail&)>& work,
                        int timeoutMs = 0) {
    // v1.21: arm THIS thread's alternate signal stack — the probe runs on
    // a Kotlin/DefaultDispatch worker that has no altstack of its own.
    PX5::CrashHandler::ArmThreadAltStack();
    std::fflush(nullptr);
    const time_t forkWall = time(nullptr);

    int fds[2];
    if (pipe(fds) != 0) {
        return std::string(name) + ": pipe() failed (errno=" +
               std::to_string(errno) + ")";
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]); close(fds[1]);
        return std::string(name) + ": fork failed (errno=" +
               std::to_string(errno) + ")";
    }
    if (pid == 0) {
        // Child: only this test runs here. No JNI, no shared state writes.
        // sigaltstack is per-thread and NOT inherited across fork — arm the
        // child's main thread before any engine work.
        PX5::CrashHandler::ArmThreadAltStack();
        close(fds[0]);
        ChildTrail trail;
        trail.fd = fds[1];
        std::string rep;
        try {
            rep = work(trail);
        } catch (const std::exception& e) {
            rep = std::string("FAILED — native exception: ") + e.what();
        } catch (...) {
            rep = "FAILED — native exception (unknown)";
        }
        ssize_t n = write(fds[1], rep.data(), rep.size());
        (void)n;
        close(fds[1]);
        _exit(0);
    }

    // Parent: wait WITHOUT blocking forever. The pipe read end is made
    // non-blocking and drained alongside a WNOHANG poll loop, so a guest
    // that hangs (execution probe) cannot wedge the caller: past
    // timeoutMs the child is SIGKILLed and the report says exactly that.
    // (v1.13 — the load probe could rely on children that always
    // terminate quickly; an EXECUTION probe cannot promise that.)
    close(fds[1]);
    fcntl(fds[0], F_SETFL, fcntl(fds[0], F_GETFL) | O_NONBLOCK);
    std::string raw;   // v1.20: everything the pipe carried (trail + report)
    char buf[512];
    ssize_t n;
    int status = 0;
    bool timedOut = false;
    int waitedMs = 0;
    for (;;) {
        while ((n = read(fds[0], buf, sizeof buf)) > 0)
            raw.append(buf, static_cast<size_t>(n));
        if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            close(fds[0]);
            return std::string(name) + ": pipe read failed (errno=" +
                   std::to_string(errno) + ")";
        }
        const pid_t w = waitpid(pid, &status, WNOHANG);
        if (w == pid) {
            while ((n = read(fds[0], buf, sizeof buf)) > 0)
                raw.append(buf, static_cast<size_t>(n));
            break;
        }
        if (w < 0 && errno != EINTR) {
            close(fds[0]);
            return std::string(name) + ": waitpid failed (errno=" +
                   std::to_string(errno) + ")";
        }
        if (timeoutMs > 0 && waitedMs >= timeoutMs) {
            timedOut = true;
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);  // reap the killed child
            while ((n = read(fds[0], buf, sizeof buf)) > 0)
                raw.append(buf, static_cast<size_t>(n));
            break;
        }
        usleep(20 * 1000);
        waitedMs += 20;
    }
    close(fds[0]);

    // v1.22: split the pipe stream — "# " lines are the child's progress
    // trail, everything else is the report body. The body keeps the exact
    // string contract callers already match on (LOAD OK / EXEC EXIT / ...);
    // the trail is appended only in the free-form branches below.
    std::string trail;
    std::string rep;
    {
        size_t pos = 0;
        while (pos < raw.size()) {
            size_t nl = raw.find('\n', pos);
            if (nl == std::string::npos) nl = raw.size();
            const std::string ln = raw.substr(pos, nl - pos);
            if (ln.rfind("# ", 0) == 0) trail += "  " + ln.substr(2) + "\n";
            else if (!ln.empty()) rep += ln + "\n";
            pos = nl + 1;
        }
    }

    if (timedOut) {
        std::string out = std::string(name) + ": TIMEOUT after " +
                          std::to_string(waitedMs) + " ms — the probe child "
                          "was still running and was killed. NO CRASH: the "
                          "guest simply did not exit within the budget.";
        out += "\n" + VerifyChildDump(forkWall);
        if (!trail.empty()) out += "\nchild trail (in order):\n" + trail;
        if (!rep.empty()) out += "\npartial report before kill: " + rep;
        return out;
    }
    if (WIFSIGNALED(status)) {
        const int sig = WTERMSIG(status);
        std::string out = std::string(name) + ": CRASHED in isolated child (signal " +
                          std::to_string(sig) + ")\n";
        out += VerifyChildDump(forkWall);
        if (!trail.empty()) out += "\nchild trail (last completed step is the one "
                                  "whose successor never appeared):\n" + trail;
        if (!rep.empty()) out += "\npartial report before death: " + rep;
        return out;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        return std::string(name) + ": child exited with code " +
               std::to_string(WEXITSTATUS(status)) + " | " +
               (rep.empty() ? "(no report)" : rep);
    }
    if (rep.empty()) rep = "(child produced no report)";
    return rep;
}

} // namespace

// v1.12 crash containment: the ENTIRE load pipeline (file read, SELF
// extraction, ELF parse, guest-window mapping) runs in a fork-isolated
// throwaway child. A load-stage native fault now costs the probe child —
// not the app: the parent survives, reports the real signal plus the
// VERIFIED dump path, and the UI never reaches the in-process load.
// The 2026-08-30 device session showed the alternative: a real eboot.bin
// killed the whole app instantly and left zero evidence behind.
// On success the child's mapping is discarded (fork copy-on-write) and the
// caller maps the image again in-process — the process that will actually
// execute it re-validates everything.
extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeLoadExecutableIsolated(
        JNIEnv* env, jobject, jstring pathStr) {
    if (!pathStr) return env->NewStringUTF("isolated load: no path given");
    const char* p = env->GetStringUTFChars(pathStr, nullptr);
    const std::string path = p ? p : "";
    if (p) env->ReleaseStringUTFChars(pathStr, p);
    PX5::Breadcrumb::Set("jni: isolated load probe %s", path.c_str());

    const std::string report = RunIsolated(
        "isolated load",
        [path](ChildTrail& t) -> std::string {
            t.step("magic dispatch begin");
            const bool isSelf = PathLooksLikeSelf(path);
            t.step("magic dispatch: %s", isSelf ? "SELF" : "ELF");
            auto& emu = PX5::Emulator::GetInstance();
            const bool ok = emu.LoadExecutable(path, isSelf);
            t.step("LoadExecutable returned ok=%d", ok ? 1 : 0);
            const auto& img = emu.LoadedImage();
            if (ok) {
                char line[256];
                snprintf(line, sizeof(line),
                         "LOAD OK — image=[0x%llx..0x%llx] entry=0x%llx "
                         "(probe child; app maps again for real)",
                         (unsigned long long)img.imageLowVa,
                         (unsigned long long)img.imageHighVa,
                         (unsigned long long)img.entryPoint);
                return line;
            }
            return "LOAD FAILED: " +
                   (img.error.empty() ? std::string("(no detail — see engine log)")
                                      : img.error);
        },
        20000);
    return env->NewStringUTF(report.c_str());
}

// v1.13 execution containment: format one guest-run attempt honestly.
namespace {
std::string FormatExecResult(const PX5::FexCoreIntegration::ExecResult& res) {
    if (!res.started) {
        return std::string("EXEC FAILED: ") +
               (res.error.empty() ? std::string("(no detail)") : res.error);
    }
    char line[512];
    if (res.exitedCleanly) {
        snprintf(line, sizeof(line),
                 "EXEC EXIT %llu — guest ran %.1f ms and captured exit_group",
                 (unsigned long long)res.exitCode, res.elapsedMs);
    } else {
        snprintf(line, sizeof(line),
                 "EXEC RETURNED without clean exit — ran %.1f ms, "
                 "guestTrap fired=%d signal=%u trapNo=%u guestRIP=0x%llx, "
                 "captured output bytes=%zu",
                 res.elapsedMs, res.guestTrap.fired ? 1 : 0,
                 res.guestTrap.signal, res.guestTrap.trapNo,
                 (unsigned long long)res.guestTrap.guestRip,
                 res.output.size());
    }
    return line;
}
} // namespace

// Runs the FULL game pipeline — load (SELF extract / ELF parse / map) AND
// real guest execution at the image entry — in a fork-isolated child with
// a hard timeout. This is the containment the self-tests have had since
// v1.10, extended to EXECUTION (the 2026-08-30 session showed the
// remaining gap: the app died on game boot with zero evidence). A fault
// now costs the probe child + a verified register dump; a hang costs the
// timeout budget and an honest "still running" report. The mapping the
// child makes is discarded (fork copy-on-write); nothing here renders.
extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeRunExecutionProbe(
        JNIEnv* env, jobject, jstring pathStr, jint timeoutMs) {
    if (!pathStr) return env->NewStringUTF("execution probe: no path given");
    const char* p = env->GetStringUTFChars(pathStr, nullptr);
    const std::string path = p ? p : "";
    if (p) env->ReleaseStringUTFChars(pathStr, p);
    PX5::Breadcrumb::Set("jni: execution probe %s (%d ms budget)",
                         path.c_str(), static_cast<int>(timeoutMs));

    const std::string report = RunIsolated(
        "execution probe",
        [path](ChildTrail& t) -> std::string {
            t.step("engine rebuild (ResetForChild) begin");
            // v1.16: rebuild the engine in the child — the inherited context
            // was built pre-fork in a multithreaded parent; both v1.15
            // execution children died deterministically inside
            // ExecuteThread. Also applies deferred (preset) overrides.
            PX5::FexCoreIntegration::ResetForChild();
            t.step("engine rebuild ok");
            const bool isSelf = PathLooksLikeSelf(path);
            auto& emu = PX5::Emulator::GetInstance();
            if (!emu.LoadExecutable(path, isSelf)) {
                const auto& err = emu.LoadedImage().error;
                return std::string("LOAD FAILED: ") +
                       (err.empty()
                            ? std::string("(no detail — see engine log)")
                            : err);
            }
            const auto& img = emu.LoadedImage();
            PX5::Breadcrumb::Set("exec probe: load ok entry=0x%llx",
                                 (unsigned long long)img.entryPoint);
            t.step("load ok entry=0x%llx — execute enter",
                   (unsigned long long)img.entryPoint);
            const auto res = emu.ExecuteLoadedGuest();
            t.step("execute returned started=%d", res.started ? 1 : 0);
            PX5::Breadcrumb::Set("exec probe: exec done started=%d",
                                 res.started ? 1 : 0);
            return FormatExecResult(res);
        },
        static_cast<int>(timeoutMs));
    return env->NewStringUTF(report.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeRunCpuConformanceTest(
        JNIEnv* env, jobject) {
    PX5::Breadcrumb::Set("jni: CpuConformanceTest enter");
    // Fork-isolated: a JIT fault reports evidence instead of killing the app.
    const std::string report = RunIsolated(
        "CPU conformance",
        [](ChildTrail& t) -> std::string {
            t.step("engine rebuild (ResetForChild) begin");
            // v1.16: fresh engine in the child (see execution probe note).
            PX5::FexCoreIntegration::ResetForChild();
            t.step("engine rebuild ok");
            t.step("conformance enter (blob: mov eax,40; add eax,2; hlt)");
            const bool ok = PX5::FexCoreIntegration::RunConformanceTest();
            t.step("conformance returned ok=%d", ok ? 1 : 0);
            return ok
                ? "PASSED — guest blob (mov eax,40; add eax,2; hlt) executed "
                  "on the ARM64 JIT and reached its HLT exit"
                : "FAILED — guest blob did not run cleanly (see engine log)";
        },
        15000);
    return env->NewStringUTF(report.c_str());
}

// v1.20 — THE decisive discriminator for the CPU gate.
//
// Same synthetic blob, same RunConformanceTest — but NO fork. The
// 2026-08-31 device session proved the fork child dies inside
// ExecuteThread (si_addr=0x4, right after "Guest thread created") even
// after a full ResetForChild rebuild — and that signature has been
// identical since v1.15. The open question is whether the JIT itself
// works on this device:
//   * IN-PROCESS PASSES  → the JIT is healthy; the fork+rebuild path is
//     the poison (process-level state that Shutdown/Initialize does not
//     rebuild). The fix moves to the probe architecture.
//   * IN-PROCESS CRASHES → the FEXCore JIT is broken on this device at a
//     deeper level. The armed crash handler (v1.20, evidence-first) then
//     writes the module-resolved PC + faulting instruction bytes into
//     px5_main.log before the process dies — symbolizable against the CI
//     unstripped libpx5.so artifact.
// Honest contract: this call CAN kill the app. That is the price of the
// evidence, and the UI labels the button as such.
extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeRunCpuConformanceInProcess(
        JNIEnv* env, jobject) {
    // v1.21: this thread may end up executing the dispatcher on the GUEST
    // stack — the 256KB altstack reserve is what lets the crash report
    // survive long enough to write the pc= line.
    PX5::CrashHandler::ArmThreadAltStack();
    PX5::Breadcrumb::Set("jni: CpuConformance IN-PROCESS (no fork)");
    const bool ok = PX5::FexCoreIntegration::RunConformanceTest();
    // v1.26: the success text no longer asserts WHY the isolated-child runs
    // crashed. In-process passing proves THIS path executes the guest; the
    // causality of the fork-child crashes (fork inheritance vs engine-rebuild
    // vs probe architecture) is an open item that only a same-version
    // comparison experiment could settle. Report exactly what is known.
    return env->NewStringUTF(ok
        ? "PASSED — guest blob (mov eax,40; add eax,2; hlt) executed on the "
          "device JIT, in-process, no fork. (isolated-child crashes remain an "
          "open item; this test alone does not establish their cause)"
        : "FAILED — in-process blob did not meet the strict contract "
          "(started && HLT exit && RAX==42 && no trap; no fork involved; "
          "the crash handler captured the evidence if this died)");
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeMapMemory(
        JNIEnv*, jobject, jlong addr, jlong size, jint flags) {
    return static_cast<jlong>(
        PX5::Emulator::GetInstance().MapMemory(static_cast<uint64_t>(addr),
                                               static_cast<size_t>(size),
                                               static_cast<uint32_t>(flags)));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeUnmapMemory(
        JNIEnv*, jobject, jlong addr, jlong size) {
    return PX5::Emulator::GetInstance().UnmapMemory(
               static_cast<uint64_t>(addr),
               static_cast<size_t>(size)) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeGetArchitectureSummary(
        JNIEnv* env, jobject) {
    return env->NewStringUTF(
        PX5::FexCoreIntegration::GetArchitectureSummary().c_str());
}

// --- Foundation evidence additions ----------------------------------------

extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeRunFoundationSelfTest(
        JNIEnv* env, jobject) {
    PX5::Breadcrumb::Set("jni: EngineSelfTest enter");
    // Fork-isolated like the conformance test: evidence over fatal crashes.
    const std::string report = RunIsolated(
        "engine self-test",
        [](ChildTrail& t) -> std::string {
            t.step("engine rebuild (ResetForChild) begin");
            // v1.16: fresh engine in the child — SelfTestFoundation executes
            // guests (conformance + raw pipeline) and must not inherit a
            // pre-fork FEXCore context built by the multithreaded parent.
            PX5::FexCoreIntegration::ResetForChild();
            t.step("engine rebuild ok — SelfTestFoundation enter");
            const std::string r = PX5::Emulator::GetInstance().SelfTestFoundation();
            t.step("SelfTestFoundation returned (%zu chars)", r.size());
            return r;
        },
        30000);
    return env->NewStringUTF(report.c_str());
}

// v1.26 — IN-PROCESS foundation self-test (no fork, no ResetForChild).
// The vc26 device session (2026-08-31 11:03) proved in-process guest
// execution end-to-end: conformance blob entered the JIT, executed, and
// unwound through the HLT exit with RAX=42. The fork-isolated variant
// above remains for forensic comparison, but the once-per-build auto-run
// gate uses THIS path: same process, live engine, crash handler armed.
// If a foundation step kills the process, the evidence-first crash report
// lands inline in px5_main.log — the same contract the conformance
// auto-run has already honored twice on this device (v1.23 SIGSEGV,
// v1.24 SIGILL, both symbolized to a named fix).
extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeRunFoundationSelfTestInProcess(
        JNIEnv* env, jobject) {
    PX5::CrashHandler::ArmThreadAltStack();
    PX5::Breadcrumb::Set("jni: FoundationSelfTest IN-PROCESS (no fork)");
    const std::string r = PX5::Emulator::GetInstance().SelfTestFoundation();
    return env->NewStringUTF(r.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeGetEngineCounters(
        JNIEnv* env, jobject) {
    return env->NewStringUTF(
        PX5::FexCoreIntegration::GetEngineCounters().c_str());
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeApplyEngineConfigOverride(
        JNIEnv* env, jobject, jstring jKey, jstring jValue) {
    if (!jKey || !jValue) return JNI_FALSE;
    const char* k = env->GetStringUTFChars(jKey, nullptr);
    const char* v = env->GetStringUTFChars(jValue, nullptr);
    const bool ok = PX5::FexCoreIntegration::ApplyEngineConfigOverride(
        k ? k : "", v ? v : "");
    if (k) env->ReleaseStringUTFChars(jKey, k);
    if (v) env->ReleaseStringUTFChars(jValue, v);
    return ok ? JNI_TRUE : JNI_FALSE;
}

// Kotlin level ids: 0=none 1=error 2=warn 3=info 4=debug 5=trace.
// Level 4/5 also clear the verbose flag dependency — the explicit selector
// is the master gate from the moment it is first used.
extern "C" JNIEXPORT void JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeSetLogLevel(
        JNIEnv*, jobject, jint level) {
    using namespace PX5;
    EngineSettings::logLevel.store(level);
    Logger::SetMinLevel(
        level <= 0 ? static_cast<LogLevel>(6)   // none: drop everything
                   : static_cast<LogLevel>(level - 1)); // error..trace
    PX5_LOGI(LogCategory::SETTINGS, "log level set to %d", level);
}

// Kotlin present-mode ids: 0=auto 1=FIFO 2=FIFO_RELAXED 3=MAILBOX
// 4=IMMEDIATE 5=FIFO_LATEST_READY. Validation against the device's
// supported modes happens at swapchain creation (vulkan_device.cpp) —
// an unsupported explicit choice falls back loudly, never silently.
extern "C" JNIEXPORT void JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeSetPresentMode(
        JNIEnv*, jobject, jint mode) {
    PX5::EngineSettings::presentMode.store(mode < 0 ? 0 : (mode > 5 ? 0 : mode));
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeGetVulkanSummary(JNIEnv* env,
                                                                 jobject) {
    auto& gpu = PX5::VulkanGpuDevice::GetInstance();
    gpu.Initialize();   // idempotent; report current truth either way
    return env->NewStringUTF(gpu.GetSummaryString().c_str());
}

// ===========================================================================
// Phase-2 surface: GPU proof + on-screen renderer + live settings + input
// + libkernel HLE summary + driver selection. Every entry performs REAL
// work on the arm64 engine; the x86_64 smoke stub mirrors these symbols.
// ===========================================================================

#include "input/controller.h"
#include "kernel/sce_kernel_hle.h"
#include "gpu/driver_manager.h"
#include "gpu/gnm/gnm_selftest.h"
#include "loader/self_extract_selftest.h"
#include "utils/crash_handler.h"

#include <android/native_window_jni.h>

extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeRunGpuProof(JNIEnv* env,
                                                            jobject) {
    PX5::Breadcrumb::Set("jni: GpuProof enter");
    // v1.16 — fork-isolated containment: a driver fault inside the proof
    // costs the child (+ a verified dump), never the app.
    //
    // v1.18 — the child no longer submits on the SINGLETON's queue at all.
    // The old child ran RunOffscreenClearProof on the inherited device:
    // driver-internal state created by the PARENT process, fork-COW'd —
    // and a forked child submitting on parent-created driver state is the
    // hazard the 2026-08-30 device video shows ("CRASHED in isolated child
    // (signal 11)", si_addr=0x0 right after the gpu.proof:submit
    // breadcrumb, before fence_wait). The child now builds a completely
    // fresh instance/device (RunSelfContainedProof) that opens its OWN drm
    // render-node fd — a clean kernel context sharing nothing with the
    // parent — so the proof measures the actual driver stack (system or
    // hooked Turnip: the loader + hook + ICD are inherited mappings,
    // binding happens per fresh instance) without ever touching
    // parent-created driver objects.
    auto& gpu = PX5::VulkanGpuDevice::GetInstance();
    const bool wasRendering = gpu.StopRenderLoopForProbe();

    const std::string report = RunIsolated(
        "GPU proof",
        [&gpu](ChildTrail& t) -> std::string {
            t.step("self-contained proof enter");
            std::string detail;
            const bool ok = gpu.RunSelfContainedProof(detail);
            t.step("proof returned ok=%d", ok ? 1 : 0);
            return std::string(ok ? "PASS | " : "FAIL | ") + detail;
        },
        15000);

    if (wasRendering) gpu.ResumeRenderLoopAfterProbe();

    // Keep the UI contract: PASS/FAIL/CRASHED prefix + detail. The crash
    // path carries the verified dump path from VerifyChildDump.
    PX5::Breadcrumb::Set("jni: GpuProof done rep=%.64s", report.c_str());
    return env->NewStringUTF(report.c_str());
}

// Phase C milestone 1: runs the synthetic-stream GNM PM4 decoder self-test
// (pure CPU, no Vulkan involved). The report is the decoder's own output —
// it proves decoder/state-model mechanics, nothing about game compat.
extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeRunGnmSelfTest(JNIEnv* env,
                                                               jobject) {
    PX5::Breadcrumb::Set("jni: GnmSelfTest enter");
    // v1.15: fork-isolated like the engine self-tests. The GNM decoder is
    // pure C++ (no Vulkan, no JNI in the child) — a fault now costs the
    // test child plus a verified dump, never the app process.
    const std::string report = RunIsolated(
        "GNM self-test",
        [](ChildTrail& t) -> std::string {
            t.step("GNM decoder self-test enter");
            std::string rep;
            PX5::Gnm::RunGnmSelfTest(&rep);
            t.step("GNM self-test returned (%zu chars)", rep.size());
            return rep;
        },
        10000);
    return env->NewStringUTF(report.c_str());
}

// Phase C milestone 2a: SELF container extractor self-test (synthetic
// round-trip: plain / compressed / encrypted-refused / bad-magic). Proves
// the loader mechanics — real dumps arrive in the loader milestone.
extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeRunLoaderSelfTest(JNIEnv* env,
                                                                  jobject) {
    PX5::Breadcrumb::Set("jni: LoaderSelfTest enter");
    // v1.15: fork-isolated (pure C++ extractor round-trips, no JNI in the
    // child) — same containment contract as the other diagnostics.
    const std::string report = RunIsolated(
        "SELF loader self-test",
        [](ChildTrail& t) -> std::string {
            t.step("SELF extractor self-test enter");
            std::string rep;
            PX5::SelfExtract::RunSelfExtractSelfTest(&rep);
            t.step("extractor self-test returned (%zu chars)", rep.size());
            // v1.31: the runtime linker self-test is pure C++ too — run it
            // in the same isolated child and append both reports verbatim.
            std::string rl;
            const bool rlOk =
                PX5::RunRuntimeLinkerSelfTest(&rl);
            t.step("runtime linker self-test ok=%d (%zu chars)",
                   rlOk ? 1 : 0, rl.size());
            return rep + "\n--- runtime linker ---\n" + rl;
        },
        10000);
    return env->NewStringUTF(report.c_str());
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeAttachRenderSurface(
        JNIEnv* env, jobject, jobject surface) {
    if (!surface) return JNI_FALSE;
    ANativeWindow* win = ANativeWindow_fromSurface(env, surface);
    if (!win) return JNI_FALSE;
    const bool ok = PX5::VulkanGpuDevice::GetInstance()
                        .AttachWindowSurface(win);
    // AttachWindowSurface acquires its own reference; release ours.
    ANativeWindow_release(win);
    return ok ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeDetachRenderSurface(
        JNIEnv*, jobject) {
    PX5::VulkanGpuDevice::GetInstance().DetachWindowSurface();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeStartRenderer(JNIEnv*,
                                                              jobject) {
    return PX5::VulkanGpuDevice::GetInstance().StartRenderLoop()
               ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeStopRenderer(JNIEnv*,
                                                             jobject) {
    PX5::VulkanGpuDevice::GetInstance().StopRenderLoop();
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeGetRenderStats(
        JNIEnv* env, jobject) {
    return env->NewStringUTF(
        PX5::VulkanGpuDevice::GetInstance().GetRenderStatsString().c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeApplySettings(
        JNIEnv* env, jobject, jint resScalePct, jboolean vsync,
        jint driverModeSlot, jboolean verboseLog, jint vramMode,
        jstring logDirJ) {
    using namespace PX5;
    EngineSettings::resScalePct.store(
        resScalePct < 50 ? 50 : (resScalePct > 200 ? 200 : resScalePct));
    EngineSettings::vsyncEnabled.store(vsync == JNI_TRUE);
    EngineSettings::verboseLogging.store(verboseLog == JNI_TRUE);
    EngineSettings::driverMode.store(
        static_cast<uint32_t>(driverModeSlot < 0 ? 0 : driverModeSlot));
    EngineSettings::vramUsageMode.store(
        vramMode < 0 ? 0 : (vramMode > 2 ? 1 : vramMode));

    GpuDriverManager::GetInstance().SetActiveMode(EngineSettings::driverMode.load());

    // The explicit level selector (nativeSetLogLevel) is the master gate;
    // the legacy verbose boolean only widens INFO to DEBUG when no explicit
    // level has been chosen yet (levelDefault sentinel -1).
    if (EngineSettings::logLevel.load() < 0) {
        if (verboseLog) Logger::SetMinLevel(LogLevel::DEBUG);
        else            Logger::SetMinLevel(LogLevel::INFO);
    }

    if (logDirJ) {
        const char* d = env->GetStringUTFChars(logDirJ, nullptr);
        if (d && *d) Logger::Initialize(d);   // idempotent; first call wins
        if (d) env->ReleaseStringUTFChars(logDirJ, d);
    }
    PX5_LOGI(LogCategory::SETTINGS,
             "settings applied: scale=%d%% vsync=%d verbose=%d driverMode=%u "
             "vramMode=%d",
             static_cast<int>(EngineSettings::resScalePct.load()),
             static_cast<int>(EngineSettings::vsyncEnabled.load()),
             static_cast<int>(EngineSettings::verboseLogging.load()),
             EngineSettings::driverMode.load(),
             EngineSettings::vramUsageMode.load());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeGetKernelHleSummary(
        JNIEnv* env, jobject) {
    auto& kHle = PX5::SceKernelHle::KernelHle::GetInstance();
    kHle.RegisterAll();
    return env->NewStringUTF(kHle.GetSummaryString().c_str());
}

// ---- Input bridge -------------------------------------------------------
extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeSetButtonState(
        JNIEnv*, jobject, jint buttonBit, jboolean pressed) {
    if (buttonBit <= 0 || buttonBit > 0x80000000) return JNI_FALSE;
    PX5::InputManager::GetInstance().SetButton(
        static_cast<uint32_t>(buttonBit), pressed == JNI_TRUE);
    return JNI_TRUE;
}

// Analog axes from the on-screen sticks; values are normalized [-1..1]
// (triggers [0..1]) and land in the same lock-free atomics a real
// controller endpoint would feed.
extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeSetLeftStick(
        JNIEnv*, jobject, jfloat lx, jfloat ly) {
    PX5::InputManager::GetInstance().SetLeftStick(lx, ly);
    return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeSetRightStick(
        JNIEnv*, jobject, jfloat rx, jfloat ry) {
    PX5::InputManager::GetInstance().SetRightStick(rx, ry);
    return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeSetTriggers(
        JNIEnv*, jobject, jfloat l2, jfloat r2) {
    PX5::InputManager::GetInstance().SetTriggers(l2, r2);
    return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeSetTouchpad(
        JNIEnv*, jobject, jboolean pressed) {
    PX5::InputManager::GetInstance().TouchpadPressed(pressed == JNI_TRUE);
    return JNI_TRUE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeGetInputSummary(
        JNIEnv* env, jobject) {
    return env->NewStringUTF(
        PX5::InputManager::GetInstance().GetSummaryString().c_str());
}

// ---- Driver slots --------------------------------------------------------
extern "C" JNIEXPORT jint JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeRegisterDriverSlot(
        JNIEnv* env, jobject, jstring labelJ, jstring soPathJ,
        jstring sonameJ) {
    const char* l = env->GetStringUTFChars(labelJ, nullptr);
    const char* s = env->GetStringUTFChars(soPathJ, nullptr);
    const char* n = sonameJ ? env->GetStringUTFChars(sonameJ, nullptr) : nullptr;
    const uint32_t id = PX5::GpuDriverManager::GetInstance()
                            .RegisterSlot(l ? l : "", s ? s : "",
                                          (n && *n) ? n : "libvulkan_adreno.so");
    env->ReleaseStringUTFChars(labelJ, l);
    env->ReleaseStringUTFChars(soPathJ, s);
    if (n) env->ReleaseStringUTFChars(sonameJ, n);
    return static_cast<jint>(id);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeSetDriverMode(
        JNIEnv*, jobject, jint mode) {
    if (mode < 0) return JNI_FALSE;
    PX5::GpuDriverManager::GetInstance().SetActiveMode(
        static_cast<uint32_t>(mode));
    return JNI_TRUE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeClearDriverSlots(
        JNIEnv*, jobject) {
    PX5::GpuDriverManager::GetInstance().ClearSlots();
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeGetDriverManagerSummary(
        JNIEnv* env, jobject) {
    return env->NewStringUTF(
        PX5::GpuDriverManager::GetInstance().SummaryString().c_str());
}

// v1.16 — eager driver verification. The v1.15 session left the summary at
// "driverVerified=not-run" until a full Vulkan init happened to run, and
// the import UI never reflected a fresh slot until the user pressed
// refresh. This entry runs the REAL load path (adrenotools dlopen via the
// linker-namespace hook) for the given slot index (1-based; 0 = system ICD)
// WITHOUT creating a Vulkan instance, then re-checks /proc/self/maps.
// Loading the driver library alone creates no DRM context — it is safe in
// the main process — and after it the same maps check the engine uses at
// real init proves the driver actually mapped. The returned string is the
// fresh manager summary (mode/slots/verify state) for the UI.
extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeVerifyDriverSlot(
        JNIEnv* env, jobject, jint slotIndex) {
    auto& mgr = PX5::GpuDriverManager::GetInstance();
    if (slotIndex > 0) {
        const bool loaded = mgr.PreloadActiveDriverForVerification();
        // v1.18 wording: this entry runs the PRELOAD (plain dlopen, then a
        // shared-namespace dlopen) — not adrenotools' hooked load, which
        // happens later at the loader's first vkCreateInstance.
        // v1.23: "FAILED" claimed a verdict plain dlopen cannot deliver —
        // Turnip needs libhardware.so, which no app-visible namespace
        // provides, so preload unavailability is INCONCLUSIVE. The real
        // verdict is the hook load + maps check at first vkCreateInstance.
        PX5_LOGI(PX5::LogCategory::GPU,
                 "Driver slot %d eager verification: preload %s",
                 static_cast<int>(slotIndex),
                 loaded ? "OK"
                        : "UNAVAILABLE via plain dlopen (INCONCLUSIVE — "
                          "platform libs not visible to app namespaces; "
                          "the designed load is the adrenotools hook at "
                          "first vkCreateInstance, proven by the maps check)");
    }
    mgr.VerifyActiveDriverMapped();
    return env->NewStringUTF(mgr.SummaryString().c_str());
}

// ---------------------------------------------------------------------------
// Runtime context wiring (diagnostics + driver directories).
// Called once from MainActivity.onCreate before any engine use.
// ---------------------------------------------------------------------------
extern "C" JNIEXPORT void JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeInitRuntimeContext(
        JNIEnv* env, jobject,
        jstring jLogsDir, jstring jHookLibDir,
        jstring jTmpLibDir, jstring jDriverRootDir,
        jstring jIdentity) {
    auto toStr = [env](jstring s) -> std::string {
        if (!s) return {};
        const char* c = env->GetStringUTFChars(s, nullptr);
        std::string out = c ? c : "";
        if (c) env->ReleaseStringUTFChars(s, c);
        return out;
    };
    const std::string logsDir  = toStr(jLogsDir);
    const std::string hookDir  = toStr(jHookLibDir);
    const std::string tmpDir   = toStr(jTmpLibDir);
    const std::string rootDir  = toStr(jDriverRootDir);
    const std::string identity = toStr(jIdentity);

    PX5::CrashHandler::Install(logsDir);
    PX5::GpuDriverManager::GetInstance().SetRuntimeDirs(hookDir, tmpDir, rootDir);
    // v1.16 — LOG UNIFICATION. The EVENT/STATE stream (Kotlin PX5EventLog)
    // and the bridged NATIVE lines now land in px5_main.log itself, not a
    // separate px5_diagnostic.log. One pasted file carries the whole story:
    // session banners, events, native evidence, and (since this build) the
    // full crash reports inline. DiagBridge revalidates its fd per line, so
    // the logger's 1 MB rotation cannot split the stream.
    if (!logsDir.empty()) {
        PX5::DiagBridge::Enable(logsDir + "/px5_main.log");
    }
    // v1.14: SIGKILL-class death attribution. lmkd / ANR-watchdog kills
    // cannot run ANY in-process handler (the crash handler is structurally
    // blind to that class) — so the heartbeat keeps the last live breadcrumb
    // on disk at 1 Hz at all times, and a silent death still names its stage.
    PX5::Heartbeat::Start(logsDir);
    // Build identity in the NATIVE stream too. The 2026-08-29 paste proved
    // that users may paste the engine log (px5_main.log / native bridged
    // lines) while the Kotlin identity line only ever reached the app-log
    // side — leaving the paste unidentifiable. Now every stream answers
    // "which APK produced this?" on its own.
    if (!identity.empty()) {
        PX5_LOGI(PX5::LogCategory::CORE, "build identity: %s",
                 identity.c_str());
    }
    PX5_LOGI(PX5::LogCategory::CORE,
             "Runtime context wired: crash reports + driver dirs ready");
}

// Single Kotlin->native event passthrough for boot-critical moments.
// Purpose: the game-boot path previously logged only into the Kotlin event
// file, so a paste of the engine log showed NOTHING between app start and
// process death ("no logs when running the game"). Events routed through
// here land in px5_main.log AND the bridged diagnostic stream.
extern "C" JNIEXPORT void JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeLogEvent(
        JNIEnv* env, jobject, jstring jCategory, jstring jMessage) {
    auto toStr = [env](jstring s) -> std::string {
        if (!s) return {};
        const char* c = env->GetStringUTFChars(s, nullptr);
        std::string out = c ? c : "";
        if (c) env->ReleaseStringUTFChars(s, c);
        return out;
    };
    const std::string cat = toStr(jCategory);
    const std::string msg = toStr(jMessage);
    if (msg.empty()) return;
    const PX5::LogCategory category = (cat == "gameBoot")
        ? PX5::LogCategory::LOADER : PX5::LogCategory::CORE;
    if (cat.empty()) {
        PX5_LOGI(category, "%s", msg.c_str());
    } else {
        PX5_LOGI(category, "[%s] %s", cat.c_str(), msg.c_str());
    }
}

// JNI_OnLoad runs at System.loadLibrary("px5") time — before any UI code.
// Crash handler is installed here with a provisional dir; MainActivity
// refines it via nativeInitRuntimeContext once the context dirs are known.
extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void*) {
    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }
    PX5::CrashHandler::Install({});   // provisional: /data/local/tmp fallback
    return JNI_VERSION_1_6;
}
