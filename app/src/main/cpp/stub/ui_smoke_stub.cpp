// ---------------------------------------------------------------------------
// PX5 UI smoke-test stub  (built ONLY for ANDROID_ABI == x86_64)
//
// WHY THIS FILE EXISTS
//   The Emulator Smoke Test workflow boots an x86_64 Android emulator and
//   installs the debug APK. An arm64-v8a-only APK cannot even be installed
//   there (INSTALL_FAILED_NO_MATCHING_ABIS), which is exactly what failed
//   in CI. We do NOT fake a CPU engine for that ABI: FEXCore's JIT here
//   translates x86-64 -> ARM64, so a guest engine on an x86_64 host has no
//   meaning for our pipeline.
//
// WHAT THIS STUB HONESTLY IS
//   * Same JNI symbol table as the real engine (Kotlin loads happily).
//   * REAL Vulkan runtime enumeration via gpu/vulkan_device.cpp which is
//     compiled into this target too (it has no FEX dependency).
//   * Real std::filesystem PKG copy install.
//   * Every CPU/guest action reports honestly that the engine requires
//     arm64-v8a. Nothing returns fabricated success.
// ---------------------------------------------------------------------------

#include <jni.h>
#include <android/log.h>

#include <cstring>
#include <filesystem>
#include <string>

#include "../gpu/vulkan_device.h"
#include "../utils/logger.h"

namespace fs = std::filesystem;

#define STUB_LOG(...) \
    __android_log_print(ANDROID_LOG_INFO, "PX5_Stub", __VA_ARGS__)

static constexpr const char* kStubArchNote =
    "[UI-smoke ABI: guest engine ships in arm64-v8a]";

static std::string CopyString(JNIEnv* env, jstring s) {
    if (!s) return {};
    const char* c = env->GetStringUTFChars(s, nullptr);
    std::string out(c ? c : "");
    env->ReleaseStringUTFChars(s, c);
    return out;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_stringFromJNI(JNIEnv* env, jobject) {
    return env->NewStringUTF(
        "PX5 UI-smoke core (x86_64): Compose surface + REAL Vulkan "
        "enumeration active; guest execution = arm64-v8a only.");
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_initializeFexCore(JNIEnv*, jobject) {
    STUB_LOG("initializeFexCore: unavailable on UI-smoke ABI");
    return JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeShutdown(JNIEnv*, jobject) {}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeInstallPkg(
        JNIEnv* env, jobject, jstring pkgPathStr, jstring destPathStr) {
    if (!pkgPathStr || !destPathStr) return JNI_FALSE;
    const std::string pkg = CopyString(env, pkgPathStr);
    const std::string dst = CopyString(env, destPathStr);

    bool ok = false;
    std::error_code ec;
    try {
        fs::create_directories(fs::path(dst).parent_path(), ec);
        ok = fs::copy_file(pkg, dst, fs::copy_options::overwrite_existing, ec);
    } catch (...) { ok = false; }

    STUB_LOG("PKG install %s -> %s : %s (%s)", pkg.c_str(), dst.c_str(),
             ok ? "OK" : "FAIL", ec.message().c_str());
    return ok ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeLoadElf(
        JNIEnv*, jobject, jstring) {
    STUB_LOG("nativeLoadElf: guest engine unavailable on UI-smoke ABI");
    return JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeLoadSelf(
        JNIEnv*, jobject, jstring) {
    return JNI_FALSE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeRunCpuConformanceTest(
        JNIEnv* env, jobject) {
    return env->NewStringUTF(
        "SKIPPED — x86_64 UI-smoke ABI has no FEXCore bridge");
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeMapMemory(
        JNIEnv*, jobject, jlong, jlong, jint) {
    return 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeUnmapMemory(
        JNIEnv*, jobject, jlong, jlong) {
    return JNI_FALSE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeGetArchitectureSummary(
        JNIEnv* env, jobject) {
    return env->NewStringUTF(
        "x86_64 UI-smoke ABI: Vulkan enumeration active; FEXCore bridge "
        "not built for this ABI.");
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeGetEngineCounters(
        JNIEnv* env, jobject) {
    return env->NewStringUTF(
        "x86_64 UI-smoke ABI: no FEXCore counters on this ABI.");
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeApplyEngineConfigOverride(
        JNIEnv*, jobject, jstring, jstring) {
    return JNI_FALSE;   // no FEXCore config on the smoke ABI
}

extern "C" JNIEXPORT void JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeSetLogLevel(
        JNIEnv*, jobject, jint) {}

extern "C" JNIEXPORT void JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeSetPresentMode(
        JNIEnv*, jobject, jint) {}

extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeRunFoundationSelfTest(
        JNIEnv* env, jobject) {
    auto& gpu = PX5::VulkanGpuDevice::GetInstance();
    const bool vok = gpu.Initialize();
    std::string r;
    r += "[SKIP] Memory window / FEXCore / guest steps — engine ABI absent\n";
    r += std::string(vok ? "[PASS] Vulkan runtime | "
                         : "[INFO] Vulkan runtime | ") +
         gpu.GetSummaryString() + "\n";
    r += kStubArchNote;
    r += "\nVERDICT: SKIPPED (UI-smoke ABI)\n";
    return env->NewStringUTF(r.c_str());
}

// ---------------------------------------------------------------------------
// Renderer-facing APIs (honest no-ops on this ABI)
// ---------------------------------------------------------------------------
extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeAttachRenderSurface(
        JNIEnv*, jobject, jobject) { return JNI_FALSE; }
extern "C" JNIEXPORT void JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeDetachRenderSurface(
        JNIEnv*, jobject) {}
extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeStartRenderer(
        JNIEnv*, jobject) { return JNI_FALSE; }
extern "C" JNIEXPORT void JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeStopRenderer(
        JNIEnv*, jobject) {}
extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeGetRenderStats(
        JNIEnv* env, jobject) {
    return env->NewStringUTF("renderer: unavailable on UI-smoke ABI");
}

// ---------------------------------------------------------------------------
// Settings / input / kernel-HLE / driver bridges (inert but present)
// ---------------------------------------------------------------------------
extern "C" JNIEXPORT void JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeApplySettings(
        JNIEnv* env, jobject, jint, jboolean, jint, jboolean,
        jstring logDirJ) {
    const std::string dir = CopyString(env, logDirJ);
    if (!dir.empty()) PX5::Logger::Initialize(dir);   // honest file logging
    STUB_LOG("settings applied (engine-inert on UI-smoke ABI)");
}
extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeGetKernelHleSummary(
        JNIEnv* env, jobject) {
    return env->NewStringUTF(
        "kernel HLE table: unavailable on UI-smoke ABI");
}
extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeRunGpuProof(
        JNIEnv* env, jobject) {
    return env->NewStringUTF("FAIL | GPU proof requires the arm64-v8a "
                             "engine ABI (ui-smoke stub)");
}
extern "C" JNIEXPORT void JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeInitRuntimeContext(
        JNIEnv*, jobject, jstring, jstring, jstring, jstring) {
    // Smoke ABI: no crash-handler/driver-dir wiring needed (no engine).
}
extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeGetVulkanSummary(
        JNIEnv* env, jobject) {
    auto& gpu = PX5::VulkanGpuDevice::GetInstance();
    gpu.Initialize();
    return env->NewStringUTF(gpu.GetSummaryString().c_str());
}
extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeSetButtonState(
        JNIEnv*, jobject, jint, jboolean) { return JNI_FALSE; }
extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeSetLeftStick(
        JNIEnv*, jobject, jfloat, jfloat) { return JNI_FALSE; }
extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeSetRightStick(
        JNIEnv*, jobject, jfloat, jfloat) { return JNI_FALSE; }
extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeSetTriggers(
        JNIEnv*, jobject, jfloat, jfloat) { return JNI_FALSE; }
extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeSetTouchpad(
        JNIEnv*, jobject, jboolean) { return JNI_FALSE; }
extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeGetInputSummary(
        JNIEnv* env, jobject) {
    return env->NewStringUTF("input: unavailable on UI-smoke ABI");
}
extern "C" JNIEXPORT jint JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeRegisterDriverSlot(
        JNIEnv*, jobject, jstring, jstring, jstring) { return 0; }
extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeSetDriverMode(
        JNIEnv*, jobject, jint) { return JNI_FALSE; }
extern "C" JNIEXPORT void JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeClearDriverSlots(
        JNIEnv*, jobject) {}
extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeGetDriverManagerSummary(
        JNIEnv* env, jobject) {
    return env->NewStringUTF(
        "driver manager: unavailable on UI-smoke ABI");
}
