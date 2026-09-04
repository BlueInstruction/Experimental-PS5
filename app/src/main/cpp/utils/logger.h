// SPDX-License-Identifier: MIT
// PX5 — Logger v2 (Eden-format structured log).
//
// v1.42 rework, modeled on the Eden/yuzu logging architecture
// (src/common/logging.{h,cpp} — verified line-by-line against the real
// source, see worklog Task 30-a) and Vita3K's stub-accounting discipline
// (worklog Task 30-b). The user supplied real Eden log lines as the
// reference format:
//
//   [   0.001979] Input <Info> input_common/drivers/udp_client.cpp:140:UDPClient: Udp Initialization started
//
// PX5 v2 line format (byte-compatible shape):
//   [  %4u.%06llu] <Class> <Level> <repo-relative-path>:<line>:<function>: <message>
//
// Why each field exists:
//   - monotonic uptime (seconds.microseconds since Initialize): lines are
//     sortable and correlate with probe time budgets and FEXCore traces;
//     wall-clock stays available via the session banner only.
//   - <Class>: subsystem taxonomy (Kernel, Loader, Cpu.Fex, ...) — the
//     grep/filter key. One name per module family, `X.Y` two levels max.
//   - <Level>: Trace/Debug/Info/Warning/Error/Critical.
//   - path:line:function: captured AUTOMATICALLY at the call site via
//     __FILE__/__LINE__/__func__ inside the macros — a line can never be
//     emitted detached from the code that produced it.
//
// Preserved from v1 (still load-bearing):
//   - rotating file set: px5_main.log + .1..4 (1 MB each)
//   - raw fd + write() (crash-safe), fsync on Error/Critical lines
//   - DiagBridge forwarding (Kotlin event stream)
//   - PeekLogFilePathUnsafe (signal-handler-safe path read)
//   - the PX5_LOGx(cat, fmt, ...) macro surface — hundreds of call sites
//     keep compiling unchanged; they now additionally capture __func__.

#ifndef PX5_LOGGER_H
#define PX5_LOGGER_H

#include <android/log.h>
#include <cstdarg>
#include <cstdint>
#include <string>
#include <string_view>

namespace PX5 {

enum class LogLevel : uint8_t {
    TRACE   = 0,
    DEBUG   = 1,
    INFO    = 2,
    WARNING = 3,
    ERROR   = 4,
    FATAL   = 5,
};

// Subsystem taxonomy. v1.42 renders these as Eden-style class names via
// CategoryToString (CORE -> "Core", FEX -> "Cpu.Fex", VULKAN ->
// "Render.Vulkan", ...). Call sites keep using the same enumerators.
enum class LogCategory : uint8_t {
    CORE,
    CPU,
    GPU,
    LOADER,
    FILESYSTEM,
    KERNEL,
    MEMORY,
    JNI,
    FEX,
    AUDIO,
    INPUT,
    MEDIA,
    VULKAN,
    NETWORK,
    SETTINGS,
    SYSTEM,
};

/**
 * Structured logger with Eden-format output (subsystem taxonomy, file/line/func tracking).
 */
class Logger {
public:
    /**
     * Initializes the logger with on-disk log directory.
     * Sets monotonic uptime epoch. Safe to call multiple times; first call wins.
     * @param log_dir Log directory path (typically externalFilesDir/logs)
     * @return true if initialization succeeded, false otherwise
     */
    static bool Initialize(std::string_view log_dir) noexcept;

    /**
     * Sets global minimum log level (messages below this are dropped).
     * @param level Minimum level (TRACE/DEBUG/INFO/WARNING/ERROR/FATAL)
     */
    static void SetMinLevel(LogLevel level) noexcept;

    /**
     * Returns current global minimum log level.
     * @return Current minimum level
     */
    static LogLevel GetMinLevel() noexcept;

    /**
     * Sets per-class log filter from space-separated rule string (Eden-style).
     * Example: "*:Warning Loader:Info Cpu.Fex:Trace"
     * @param rules Filter rule string
     */
    static void SetClassFilterString(const std::string& rules) noexcept;

    /**
     * Core log function routing to logcat and rotating file (Eden format).
     * @param level Log level
     * @param category Subsystem category
     * @param file Source file path (injected by macro)
     * @param line Source line number (injected by macro)
     * @param func Function name (injected by macro)
     * @param format Printf-style format string
     * @param ... Format arguments
     */
    static void Log(LogLevel level, LogCategory category,
                    const char* file, int line, const char* func,
                    const char* format, ...) noexcept;

    /**
     * Variadic log helper for forwarding from other code.
     * @param level Log level
     * @param category Subsystem category
     * @param file Source file path
     * @param line Source line number
     * @param func Function name
     * @param format Printf-style format string
     * @param args va_list of format arguments
     */
    static void LogV(LogLevel level, LogCategory category,
                     const char* file, int line, const char* func,
                     const char* format, va_list args) noexcept;

    /**
     * Forces fsync of the underlying file (called automatically on ERROR/FATAL).
     */
    static void Flush() noexcept;

    /**
     * Closes the log file. Safe to call multiple times.
     */
    static void Shutdown() noexcept;

    /**
     * Converts log level enum to string.
     * @param level Log level
     * @return String representation (e.g., "Info")
     */
    static const char* LevelToString(LogLevel level) noexcept;

    /**
     * Converts log category enum to Eden-style class name.
     * @param category Log category
     * @return Class name string (e.g., "Kernel", "Cpu.Fex")
     */
    static const char* CategoryToString(LogCategory category) noexcept;

    /**
     * Converts log level to Android logcat priority.
     * @param level Log level
     * @return Android log priority constant
     */
    static android_LogPriority LevelToAndroidPriority(LogLevel level) noexcept;

    /**
     * Returns current log file path (for Kotlin side to read/share).
     * @return Absolute path to current log file
     */
    static std::string GetCurrentLogFilePath() noexcept;

    /**
     * Returns log file path without locking (crash-handler-only accessor).
     * @return Pointer to log file path string
     */
    static const char* PeekLogFilePathUnsafe() noexcept;

private:
    /**
     * Appends one formatted line to the rotating file (internal).
     * @param formatted_line Line to append
     * @param level Log level (for fsync decision)
     */
    static void WriteToFile(std::string_view formatted_line,
                            LogLevel level) noexcept;

    /**
     * Rotates log files when current file exceeds max size (internal).
     */
    static void MaybeRotate() noexcept;
};

} // namespace PX5

// ---- Convenience macros ----
// The v1 macro surface is preserved. v1.42 adds __func__ capture and
// LOG_ONCE variants (Vita3K's anti-spam pattern: stub paths and hot
// failures log once per process, not once per frame).

#define PX5_LOGT(cat, fmt, ...) \
    ::PX5::Logger::Log(::PX5::LogLevel::TRACE,   cat, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define PX5_LOGD(cat, fmt, ...) \
    ::PX5::Logger::Log(::PX5::LogLevel::DEBUG,   cat, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define PX5_LOGI(cat, fmt, ...) \
    ::PX5::Logger::Log(::PX5::LogLevel::INFO,    cat, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define PX5_LOGW(cat, fmt, ...) \
    ::PX5::Logger::Log(::PX5::LogLevel::WARNING, cat, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define PX5_LOGE(cat, fmt, ...) \
    ::PX5::Logger::Log(::PX5::LogLevel::ERROR,   cat, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define PX5_LOGF(cat, fmt, ...) \
    ::PX5::Logger::Log(::PX5::LogLevel::FATAL,   cat, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

// ---- LOG_ONCE variants ----
// Expand to a process-lifetime once-guard around the underlying macro.
#define PX5_LOG_ONCE_IMPL(fn, cat, fmt, ...)                             \
    do {                                                                 \
        static bool s_logged_once = false;                               \
        if (!s_logged_once) {                                            \
            s_logged_once = true;                                        \
            fn(cat, fmt, ##__VA_ARGS__);                                 \
        }                                                                \
    } while (0)

#define PX5_LOGW_ONCE(cat, fmt, ...) PX5_LOG_ONCE_IMPL(PX5_LOGW, cat, fmt, ##__VA_ARGS__)
#define PX5_LOGI_ONCE(cat, fmt, ...) PX5_LOG_ONCE_IMPL(PX5_LOGI, cat, fmt, ##__VA_ARGS__)
#define PX5_LOGE_ONCE(cat, fmt, ...) PX5_LOG_ONCE_IMPL(PX5_LOGE, cat, fmt, ##__VA_ARGS__)

#endif // PX5_LOGGER_H
