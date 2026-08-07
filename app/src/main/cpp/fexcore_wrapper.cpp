#include <jni.h>
#include <string>
#include <android/log.h>

#define LOG_TAG "PX5_FEXCore"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

extern "C" JNIEXPORT jstring JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string hello = "FEXCore CPU Emulator (C++ layer) initialized.";
    LOGI("FexCoreWrapper initialized.");
    return env->NewStringUTF(hello.c_str());
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_px5_emulator_core_FexCoreWrapper_initializeFexCore(
        JNIEnv* env,
        jobject /* this */) {
    // Placeholder for actual FEXCore initialization logic
    LOGI("Initializing FEXCore backend...");
    // Return true for success
    return JNI_TRUE;
}
