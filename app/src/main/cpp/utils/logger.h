#ifndef PX5_LOGGER_H
#define PX5_LOGGER_H

#include <android/log.h>
#include <string>

namespace PX5 {

enum class LogLevel {
    TRACE,
    DEBUG,
    INFO,
    WARNING,
    LOG_ERROR
};

enum class LogCategory {
    CORE,
    CPU,
    GPU,
    LOADER,
    FILESYSTEM,
    KERNEL,
    MEMORY,
    JNI,
    FEX
};

class Logger {
public:
    static void Log(LogLevel level, LogCategory category, const char* format, ...);
    static const char* CategoryToString(LogCategory category);
};

} // namespace PX5

#define PX5_LOGI(cat, fmt, ...) PX5::Logger::Log(PX5::LogLevel::INFO, cat, fmt, ##__VA_ARGS__)
#define PX5_LOGW(cat, fmt, ...) PX5::Logger::Log(PX5::LogLevel::WARNING, cat, fmt, ##__VA_ARGS__)
#define PX5_LOGE(cat, fmt, ...) PX5::Logger::Log(PX5::LogLevel::LOG_ERROR, cat, fmt, ##__VA_ARGS__)
#define PX5_LOGD(cat, fmt, ...) PX5::Logger::Log(PX5::LogLevel::DEBUG, cat, fmt, ##__VA_ARGS__)

#endif // PX5_LOGGER_H
