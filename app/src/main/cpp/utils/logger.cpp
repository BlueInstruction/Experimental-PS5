#include "logger.h"
#include <cstdarg>
#include <cstdio>

namespace PX5 {

const char* Logger::CategoryToString(LogCategory category) {
    switch (category) {
        case LogCategory::CORE: return "PX5_Core";
        case LogCategory::CPU: return "PX5_CPU";
        case LogCategory::GPU: return "PX5_GPU";
        case LogCategory::LOADER: return "PX5_Loader";
        case LogCategory::FILESYSTEM: return "PX5_VFS";
        case LogCategory::KERNEL: return "PX5_Kernel";
        case LogCategory::MEMORY: return "PX5_Mem";
        case LogCategory::JNI: return "PX5_JNI";
        case LogCategory::FEX: return "PX5_FEXCore";
        default: return "PX5";
    }
}

void Logger::Log(LogLevel level, LogCategory category, const char* format, ...) {
    android_LogPriority priority = ANDROID_LOG_INFO;
    switch (level) {
        case LogLevel::TRACE: priority = ANDROID_LOG_VERBOSE; break;
        case LogLevel::DEBUG: priority = ANDROID_LOG_DEBUG; break;
        case LogLevel::INFO: priority = ANDROID_LOG_INFO; break;
        case LogLevel::WARNING: priority = ANDROID_LOG_WARN; break;
        case LogLevel::LOG_ERROR: priority = ANDROID_LOG_ERROR; break;
    }

    va_list args;
    va_start(args, format);
    __android_log_vprint(priority, CategoryToString(category), format, args);
    va_end(args);
}

} // namespace PX5
