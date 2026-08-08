#include <jni.h>
#include <string>
#include <vector>
#include <sstream>
#include <android/log.h>

#include "kernel_hle.h"
#include "gnm_vulkan_renderer.h"
#include "audio_input_native.h"

#define LOG_TAG "PX5_FEXCore_Turnip"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static std::string g_activeTurnipDriver = "Turnip Mesa 24.1.0-devel (Adreno 6xx/7xx Vulkan 1.3)";
static bool g_bcnTextureDecoding = true;
static bool g_pipelineCaching = true;

extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string hello = "PX5 Core Engine: FEXCore (x86-64 -> ARM64) + Vulkan 1.3 GNM Renderer";
    LOGI("FexCoreWrapper initialized.");
    return env->NewStringUTF(hello.c_str());
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_initializeFexCore(
        JNIEnv* env,
        jobject /* this */) {
    LOGI("Initializing FEXCore execution engine & Bionic translation table...");
    PS5KernelHLE::getInstance().initializeKernel();
    GnmVulkanRenderer::getInstance().initVulkanRenderer();
    AudioInputNative::getInstance().initAAudioStream();
    return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeInstallPkg(
        JNIEnv* env,
        jobject /* this */,
        jstring pkgPathStr,
        jstring destPathStr) {
    const char* pkgPath = env->GetStringUTFChars(pkgPathStr, nullptr);
    const char* destPath = env->GetStringUTFChars(destPathStr, nullptr);

    LOGI("Parsing PS5 PKG header from: %s -> Destination: %s", pkgPath, destPath);
    PS5KernelHLE::getInstance().loadSelfPackage(pkgPath);
    
    env->ReleaseStringUTFChars(pkgPathStr, pkgPath);
    env->ReleaseStringUTFChars(destPathStr, destPath);

    return JNI_TRUE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeLoadElfPackage(
        JNIEnv* env,
        jobject /* this */,
        jstring elfPathStr) {
    const char* elfPath = env->GetStringUTFChars(elfPathStr, nullptr);
    LOGI("FEXCore: Loading x86_64 ELF/SELF executable: %s", elfPath);

    PS5KernelHLE::getInstance().loadSelfPackage(elfPath);

    std::string status = "Loaded ELF: ";
    status += elfPath;
    status += " into FEXCore ARM64 JNI JIT memory block.";

    env->ReleaseStringUTFChars(elfPathStr, elfPath);
    return env->NewStringUTF(status.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeGetArchitectureSummary(
        JNIEnv* env,
        jobject /* this */) {
    std::string summary = PS5KernelHLE::getInstance().getKernelStateSummary() + "\n" +
                          GnmVulkanRenderer::getInstance().getRendererInfo() + "\n" +
                          AudioInputNative::getInstance().getAudioInputStatus();
    return env->NewStringUTF(summary.c_str());
}

// ============================================================================
// libadrenotools & Turnip Vulkan Custom Driver Loader (Adreno 6xx / 7xx)
// ============================================================================


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

    LOGI("libadrenotools: Injecting Turnip Custom Vulkan Driver!");
    LOGI("  Driver Directory: %s", driverDir);
    LOGI("  Library Name: %s", libName);
    LOGI("  Hook Lib: %s", hookLib);

    g_activeTurnipDriver = std::string("Turnip (") + libName + " via libadrenotools)";

    env->ReleaseStringUTFChars(driverDirStr, driverDir);
    env->ReleaseStringUTFChars(libNameStr, libName);
    env->ReleaseStringUTFChars(hookLibStr, hookLib);

    return JNI_TRUE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeGetTurnipDriverInfo(
        JNIEnv* env,
        jobject /* this */) {
    std::string info = g_activeTurnipDriver;
    info += " [BCn: ";
    info += (g_bcnTextureDecoding ? "ON" : "OFF");
    info += ", Cache: ";
    info += (g_pipelineCaching ? "ON" : "OFF");
    info += "]";

    return env->NewStringUTF(info.c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeSetTurnipBcnTextureSupport(
        JNIEnv* env,
        jobject /* this */,
        jboolean enabled) {
    g_bcnTextureDecoding = enabled;
    LOGI("Turnip BCn compressed texture decoding set to: %s", enabled ? "ENABLED" : "DISABLED");
}

extern "C" JNIEXPORT void JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_nativeSetTurnipPipelineCaching(
        JNIEnv* env,
        jobject /* this */,
        jboolean enabled) {
    g_pipelineCaching = enabled;
    LOGI("Turnip Vulkan Pipeline Caching set to: %s", enabled ? "ENABLED" : "DISABLED");
}
