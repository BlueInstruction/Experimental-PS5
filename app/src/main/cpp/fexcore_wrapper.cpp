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
