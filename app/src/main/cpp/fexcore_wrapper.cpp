#include <jni.h>
#include <string>
#include "core/emulator.h"
#include "gpu/vulkan_device.h"
#include "cpu/fex/fex_wrapper.h"
#include "utils/logger.h"

extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string info = "PX5 Core Engine: FEXCore ARM64 + Vulkan 1.3 GNM Renderer";
    return env->NewStringUTF(info.c_str());
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_initializeFexCore(
        JNIEnv* env,
        jobject /* this */) {
    return PX5::Emulator::GetInstance().Initialize("/sdcard/PX5") ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeShutdown(
        JNIEnv* env,
        jobject /* this */) {
    PX5::Emulator::GetInstance().Shutdown();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeInstallPkg(
        JNIEnv* env,
        jobject /* this */,
        jstring pkgPathStr,
        jstring destPathStr) {
    if (!pkgPathStr || !destPathStr) return JNI_FALSE;
    const char* pkgPath = env->GetStringUTFChars(pkgPathStr, nullptr);
    const char* destPath = env->GetStringUTFChars(destPathStr, nullptr);

    PX5_LOGI(PX5::LogCategory::LOADER, "Installing PKG %s -> %s", pkgPath, destPath);

    env->ReleaseStringUTFChars(pkgPathStr, pkgPath);
    env->ReleaseStringUTFChars(destPathStr, destPath);
    return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeLoadElf(
        JNIEnv* env,
        jobject /* this */,
        jstring elfPathStr) {
    if (!elfPathStr) return JNI_FALSE;
    const char* elfPath = env->GetStringUTFChars(elfPathStr, nullptr);
    bool res = PX5::Emulator::GetInstance().LoadExecutable(elfPath, false);
    env->ReleaseStringUTFChars(elfPathStr, elfPath);
    return res ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeLoadSelf(
        JNIEnv* env,
        jobject /* this */,
        jstring selfPathStr) {
    if (!selfPathStr) return JNI_FALSE;
    const char* selfPath = env->GetStringUTFChars(selfPathStr, nullptr);
    bool res = PX5::Emulator::GetInstance().LoadExecutable(selfPath, true);
    env->ReleaseStringUTFChars(selfPathStr, selfPath);
    return res ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeLoadElfPackage(
        JNIEnv* env,
        jobject /* this */,
        jstring elfPathStr) {
    if (!elfPathStr) return env->NewStringUTF("Error: Null path");
    const char* elfPath = env->GetStringUTFChars(elfPathStr, nullptr);
    bool ok = PX5::Emulator::GetInstance().LoadExecutable(elfPath, false);
    env->ReleaseStringUTFChars(elfPathStr, elfPath);
    return env->NewStringUTF(ok ? "Successfully loaded ELF" : "Failed to load ELF");
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeRun(
        JNIEnv* env,
        jobject /* this */) {
    return PX5::Emulator::GetInstance().Run() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativePause(
        JNIEnv* env,
        jobject /* this */) {
    PX5::Emulator::GetInstance().Pause();
}

extern "C" JNIEXPORT void JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeResume(
        JNIEnv* env,
        jobject /* this */) {
    PX5::Emulator::GetInstance().Resume();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeStep(
        JNIEnv* env,
        jobject /* this */) {
    return PX5::Emulator::GetInstance().Step() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeRunCpuConformanceTest(
        JNIEnv* env,
        jobject /* this */) {
    return PX5::FexCpuEngine::GetInstance().RunConformanceTest() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeReset(
        JNIEnv* env,
        jobject /* this */) {
    PX5::Emulator::GetInstance().Reset();
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeMapMemory(
        JNIEnv* env,
        jobject /* this */,
        jlong addr,
        jlong size,
        jint flags) {
    return (jlong)PX5::Emulator::GetInstance().MapMemory((uint64_t)addr, (size_t)size, (uint32_t)flags);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeUnmapMemory(
        JNIEnv* env,
        jobject /* this */,
        jlong addr,
        jlong size) {
    return PX5::Emulator::GetInstance().UnmapMemory((uint64_t)addr, (size_t)size) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeGetArchitectureSummary(
        JNIEnv* env,
        jobject /* this */) {
    std::string summary = PX5::FexCpuEngine::GetInstance().GetArchitectureSummary();
    return env->NewStringUTF(summary.c_str());
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeLoadThunksConfig(
        JNIEnv* env,
        jobject /* this */,
        jstring thunksJsonStr) {
    if (!thunksJsonStr) return JNI_FALSE;
    const char* json = env->GetStringUTFChars(thunksJsonStr, nullptr);
    bool res = PX5::FexCpuEngine::GetInstance().LoadThunks(json);
    env->ReleaseStringUTFChars(thunksJsonStr, json);
    return res ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeLoadFexConfig(
        JNIEnv* env,
        jobject /* this */,
        jstring fexConfigJsonStr) {
    if (!fexConfigJsonStr) return JNI_FALSE;
    const char* json = env->GetStringUTFChars(fexConfigJsonStr, nullptr);
    bool res = PX5::FexCpuEngine::GetInstance().LoadConfig(json);
    env->ReleaseStringUTFChars(fexConfigJsonStr, json);
    return res ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeInitAdrenotools(
        JNIEnv* env,
        jobject /* this */,
        jstring driverDirStr,
        jstring libNameStr,
        jstring hookLibStr) {
    const char* driverDir = env->GetStringUTFChars(driverDirStr, nullptr);
    const char* libName = env->GetStringUTFChars(libNameStr, nullptr);
    const char* hookLib = env->GetStringUTFChars(hookLibStr, nullptr);

    bool res = PX5::VulkanGpuDevice::GetInstance().InitAdrenotoolsDriver(driverDir, libName, hookLib);

    env->ReleaseStringUTFChars(driverDirStr, driverDir);
    env->ReleaseStringUTFChars(libNameStr, libName);
    env->ReleaseStringUTFChars(hookLibStr, hookLib);
    return res ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeGetTurnipDriverInfo(
        JNIEnv* env,
        jobject /* this */) {
    auto caps = PX5::VulkanGpuDevice::GetInstance().GetCapabilities();
    return env->NewStringUTF(caps.driverName.c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeSetTurnipBcnTextureSupport(
        JNIEnv* env,
        jobject /* this */,
        jboolean enabled) {
    PX5::VulkanGpuDevice::GetInstance().SetBCnTextureSupport(enabled == JNI_TRUE);
}

extern "C" JNIEXPORT void JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeSetTurnipPipelineCaching(
        JNIEnv* env,
        jobject /* this */,
        jboolean enabled) {
    PX5::VulkanGpuDevice::GetInstance().SetPipelineCaching(enabled == JNI_TRUE);
}
