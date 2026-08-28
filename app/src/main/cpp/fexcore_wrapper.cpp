#include <jni.h>
#include <android/log.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <functional>
#include <string>
#include <unistd.h>
#include <sys/wait.h>

#include "core/emulator.h"
#include "fexcore_integration.h"
#include "gpu/vulkan_device.h"
#include "core/settings.h"
#include "utils/logger.h"
#include "utils/diag_bridge.h"

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
        "PX5 Foundation Core: FEXCore ARM64 CPU bridge | REAL Linux syscall "
        "HLE | Vulkan runtime enumeration");
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
    const bool res =
        PX5::Emulator::GetInstance().LoadExecutable(p, /*isSelf=*/true);
    env->ReleaseStringUTFChars(pathStr, p);
    return res ? JNI_TRUE : JNI_FALSE;
}

// --- Foundation evidence additions ----------------------------------------

namespace {

// Runs `work` in a fork()ed child and returns its honest report.
//
// WHY: a JIT defect must kill the TEST, not the app. The 2026-08-28 device
// logs show both proof buttons terminating the whole process right after
// "Guest thread created" — the Kotlin try/catch cannot catch a native
// signal. With this wrapper:
//   * the child inherits the crash handler, so a fault still writes a full
//     register dump (px5_crash_<timestamp>.log + px5_crash_latest.log);
//   * the parent survives and reports the real wait status (exit code or
//     signal) back to the UI;
//   * the child's own report line comes back through a pipe.
std::string RunIsolated(const char* name,
                        const std::function<std::string()>& work) {
    std::fflush(nullptr);

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
        close(fds[0]);
        std::string rep;
        try {
            rep = work();
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

    // Parent: drain the child report, then read the honest exit status.
    close(fds[1]);
    std::string rep;
    char buf[512];
    ssize_t n;
    while ((n = read(fds[0], buf, sizeof buf)) > 0) rep.append(buf, static_cast<size_t>(n));
    close(fds[0]);

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        return std::string(name) + ": waitpid failed (errno=" +
               std::to_string(errno) + ")";
    }
    if (WIFSIGNALED(status)) {
        const int sig = WTERMSIG(status);
        std::string out = std::string(name) + ": CRASHED in isolated child (signal " +
                          std::to_string(sig) + ")\n";
        out += rep.empty()
            ? std::string("full register dump was written to the crash log "
                          "(Settings > Diagnostics > Logs)")
            : ("partial report before death: " + rep);
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

extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeRunCpuConformanceTest(
        JNIEnv* env, jobject) {
    // Fork-isolated: a JIT fault reports evidence instead of killing the app.
    const std::string report = RunIsolated(
        "FEXCore JIT conformance",
        []() -> std::string {
            const bool ok = PX5::FexCoreIntegration::RunConformanceTest();
            return ok
                ? "PASSED — guest blob (mov eax,40; add eax,2; hlt) executed "
                  "on the ARM64 JIT and reached its HLT exit"
                : "FAILED — guest blob did not run cleanly (see engine log)";
        });
    return env->NewStringUTF(report.c_str());
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
    // Fork-isolated like the conformance test: evidence over fatal crashes.
    const std::string report = RunIsolated(
        "foundation proof pipeline",
        []() -> std::string {
            return PX5::Emulator::GetInstance().SelfTestFoundation();
        });
    return env->NewStringUTF(report.c_str());
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
#include "utils/crash_handler.h"

#include <android/native_window_jni.h>

extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeRunGpuProof(JNIEnv* env,
                                                            jobject) {
    std::string detail;
    const bool ok = PX5::VulkanGpuDevice::GetInstance()
                        .RunOffscreenClearProof(detail);
    return env->NewStringUTF(
        (std::string(ok ? "PASS | " : "FAIL | ") + detail).c_str());
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
        jint driverModeSlot, jboolean verboseLog, jstring logDirJ) {
    using namespace PX5;
    EngineSettings::resScalePct.store(
        resScalePct < 50 ? 50 : (resScalePct > 200 ? 200 : resScalePct));
    EngineSettings::vsyncEnabled.store(vsync == JNI_TRUE);
    EngineSettings::verboseLogging.store(verboseLog == JNI_TRUE);
    EngineSettings::driverMode.store(
        static_cast<uint32_t>(driverModeSlot < 0 ? 0 : driverModeSlot));

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
             "settings applied: scale=%d%% vsync=%d verbose=%d driverMode=%u",
             static_cast<int>(EngineSettings::resScalePct.load()),
             static_cast<int>(EngineSettings::vsyncEnabled.load()),
             static_cast<int>(EngineSettings::verboseLogging.load()),
             EngineSettings::driverMode.load());
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
    // Mirror filtered native lines (driver loader outcomes, engine errors)
    // into the same px5_diagnostic.log the Kotlin EVENT/STATE stream writes,
    // so a pasted log is self-sufficient for diagnosis.
    if (!logsDir.empty()) {
        PX5::DiagBridge::Enable(logsDir + "/px5_diagnostic.log");
    }
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
