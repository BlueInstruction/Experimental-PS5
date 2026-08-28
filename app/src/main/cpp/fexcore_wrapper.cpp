#include <jni.h>
#include <android/log.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <filesystem>

#include "core/emulator.h"
#include "fexcore_integration.h"
#include "gpu/vulkan_device.h"
#include "utils/logger.h"

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

extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeRunCpuConformanceTest(
        JNIEnv*, jobject) {
    return PX5::FexCoreIntegration::RunConformanceTest() ? JNI_TRUE : JNI_FALSE;
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
    const std::string report = PX5::Emulator::GetInstance().SelfTestFoundation();
    return env->NewStringUTF(report.c_str());
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
#include "core/settings.h"

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

    if (verboseLog) Logger::SetMinLevel(LogLevel::DEBUG);
    else            Logger::SetMinLevel(LogLevel::INFO);

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

extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeGetInputSummary(
        JNIEnv* env, jobject) {
    return env->NewStringUTF(
        PX5::InputManager::GetInstance().GetSummaryString().c_str());
}

// ---- Driver slots --------------------------------------------------------
extern "C" JNIEXPORT jint JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeRegisterDriverSlot(
        JNIEnv* env, jobject, jstring labelJ, jstring soPathJ) {
    const char* l = env->GetStringUTFChars(labelJ, nullptr);
    const char* s = env->GetStringUTFChars(soPathJ, nullptr);
    const uint32_t id = PX5::GpuDriverManager::GetInstance()
                            .RegisterSlot(l ? l : "", s ? s : "");
    env->ReleaseStringUTFChars(labelJ, l);
    env->ReleaseStringUTFChars(soPathJ, s);
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
        jstring jTmpLibDir, jstring jDriverRootDir) {
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

    PX5::CrashHandler::Install(logsDir);
    PX5::GpuDriverManager::GetInstance().SetRuntimeDirs(hookDir, tmpDir, rootDir);
    PX5_LOGI(PX5::LogCategory::CORE,
             "Runtime context wired: crash reports + driver dirs ready");
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
