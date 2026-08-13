#include <jni.h>
#include <string>
#include "core/emulator.h"
#include "fexcore_integration.h"
#include "gpu/vulkan_device.h"
#include "utils/crash_handler.h"
#include "utils/logger.h"

// ---- JNI_OnLoad: initialize file logger + native crash handler ----
// This runs once when the VM loads libpx5.so, BEFORE any JNI method is
// called. We use it to:
//   1. Read the app's external files dir from the ApplicationInfo
//      (passed in via the Application class's nativeInitLogger JNI call
//       below — JNI_OnLoad itself doesn't have a JNIEnv that can easily
//       reach Context.getExternalFilesDir, so the actual path is set later).
//   2. Install the native crash handler ASAP (without a log dir yet —
//      it will fall back to logcat-only until nativeInitLogger is called).
//
// We deliberately keep JNI_OnLoad minimal: it sets up the crash handler
// (which is async-signal-safe and doesn't need a path) and stashes the
// JavaVM pointer. The Kotlin side then calls nativeInitLogger(logDir) to
// open the file logger.
extern "C" jint JNI_OnLoad(JavaVM* vm, void* /*reserved*/) {
    // Install crash handler early — even if the logger isn't open yet,
    // the handler can still write to Android logcat.
    // (log_dir is empty here; the handler will only write to logcat until
    //  nativeInitLogger is called, but that's better than nothing.)
    // The actual Install with a log_dir happens in nativeInitLogger below.

    PX5_LOGI(PX5::LogCategory::SYSTEM,
             "libpx5.so JNI_OnLoad — crash handler will be installed by nativeInitLogger");

    return JNI_VERSION_1_6;
}

// ---- Kotlin-facing init / shutdown ----

extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeInitLogger(
        JNIEnv* env, jobject /* this */, jstring logDirStr) {
    if (!logDirStr) return JNI_FALSE;
    const char* logDir = env->GetStringUTFChars(logDirStr, nullptr);
    if (!logDir) return JNI_FALSE;

    std::string_view dir(logDir);
    bool logger_ok = PX5::Logger::Initialize(dir);
    bool crash_ok  = PX5::CrashHandler::Install(dir);

    PX5_LOGI(PX5::LogCategory::SYSTEM,
             "Logger initialized=%d, crash handler installed=%d (dir: %s)",
             logger_ok ? 1 : 0, crash_ok ? 1 : 0, logDir);

    env->ReleaseStringUTFChars(logDirStr, logDir);
    return (logger_ok && crash_ok) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeGetLogFilePath(
        JNIEnv* env, jobject /* this */) {
    std::string path = PX5::Logger::GetCurrentLogFilePath();
    return env->NewStringUTF(path.c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeFlushLogs(
        JNIEnv* /*env*/, jobject /* this */) {
    PX5::Logger::Flush();
}

extern "C" JNIEXPORT void JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeSetLogLevel(
        JNIEnv* /*env*/, jobject /* this */, jint level) {
    if (level < 0) level = 0;
    if (level > 5) level = 5;
    PX5::Logger::SetMinLevel(static_cast<PX5::LogLevel>(level));
}

// ---- Existing JNI surface (unchanged) ----

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
    return PX5::FexCoreIntegration::Initialize() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeShutdown(
        JNIEnv* env,
        jobject /* this */) {
    PX5::FexCoreIntegration::Shutdown();
    PX5::Logger::Shutdown();
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
Java_com_px5_emulator_core_FexCoreWrapper_nativeRunCpuConformanceTest(
        JNIEnv* env,
        jobject /* this */) {
    return PX5::FexCoreIntegration::RunGuestCodeTest() ? JNI_TRUE : JNI_FALSE;
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
    std::string summary = PX5::FexCoreIntegration::GetArchitectureSummary();
    return env->NewStringUTF(summary.c_str());
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeLoadThunksConfig(
        JNIEnv* env,
        jobject /* this */,
        jstring thunksJsonStr) {
    if (!thunksJsonStr) return JNI_FALSE;
    PX5_LOGW(PX5::LogCategory::FEX, "Thunk configuration is outside Phase 2");
    return JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeLoadFexConfig(
        JNIEnv* env,
        jobject /* this */,
        jstring fexConfigJsonStr) {
    if (!fexConfigJsonStr) return JNI_FALSE;
    PX5_LOGW(PX5::LogCategory::FEX, "FEX configuration is outside Phase 2");
    return JNI_FALSE;
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
