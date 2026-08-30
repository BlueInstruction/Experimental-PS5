// SPDX-License-Identifier: MIT
// PX5 — Enhanced Logger
//
// Improvements over the old PX5 logger:
//   1. Writes to BOTH Android logcat AND a rotating file set
//   2. File rotation: 5 files × 1 MB each (px5_main.log, .1, .2, .3, .4)
//   3. Thread-safe (mutex-guarded file writes)
//   4. Millisecond timestamps + thread ID + thread name in every line
//   5. Log-level filtering (compile-time + runtime)
//   6. Flushed on every ERROR/FATAL line (so logs survive a crash mid-write)
//   7. Auto-init from JNI_OnLoad with the app's external files dir
//
// File format:
//   [YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] [TID:threadname] [CATEGORY] message
//
// Path: <externalFilesDir>/logs/px5_main.log(.N)

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

class Logger {
public:
    // ---- One-time initialization ----
    // Called from JNI_OnLoad. Sets the on-disk log directory (typically
    // <externalFilesDir>/logs/). After this call, file logging is enabled.
    // Safe to call multiple times; only the first call wins.
    // Returns true if file logging was successfully enabled.
    static bool Initialize(std::string_view log_dir) noexcept;

    // ---- Runtime level filter ----
    // Messages below this level are dropped. Default: INFO in release, TRACE in debug.
    static void SetMinLevel(LogLevel level) noexcept;
    static LogLevel GetMinLevel() noexcept;

    // ---- Core log function ----
    // Routes the message to:
    //   1. Android logcat (__android_log_vprint)
    //   2. The rotating file (if Initialize() was called)
    // Every file line carries [file:line] of the CALL SITE so diagnostics
    // point at real code locations, not pre-planted strings.
    // Thread-safe.
    static void Log(LogLevel level, LogCategory category,
                    const char* file, int line,
                    const char* format, ...) noexcept;

    // ---- Variadic helper (for forwarding from other code) ----
    static void LogV(LogLevel level, LogCategory category,
                     const char* file, int line,
                     const char* format, va_list args) noexcept;

    // ---- Flush ----
    // Forces a flush of the underlying file stream. Called automatically
    // on ERROR/FATAL. Exposed so the crash handler can call it before
    // dumping its own report.
    static void Flush() noexcept;

    // ---- Shutdown ----
    // Closes the file. Safe to call multiple times.
    static void Shutdown() noexcept;

    // ---- Helpers ----
    static const char* LevelToString(LogLevel level) noexcept;
    static const char* CategoryToString(LogCategory category) noexcept;
    static android_LogPriority LevelToAndroidPriority(LogLevel level) noexcept;

    // ---- File-path getter (for the Kotlin side to read/share) ----
    // Returns the absolute path of the current main log file, or empty
    // string if file logging is not initialized.
    static std::string GetCurrentLogFilePath() noexcept;

    // v1.16 — crash-handler-only accessor. Takes NO lock: the log path is
    // set once at Initialize (single-threaded startup) and never changes
    // (rotation renames FILES, not the path). Reading it from a signal
    // handler is safe; GetCurrentLogFilePath() would deadlock there when
    // the faulting thread (or any thread) held the logger mutex.
    static const char* PeekLogFilePathUnsafe() noexcept;

private:
    // Internal: appends one formatted line to the rotating file.
    static void WriteToFile(std::string_view formatted_line,
                            LogLevel level) noexcept;

    // Internal: rotates files when current file exceeds max size.
    static void MaybeRotate() noexcept;
};

} // namespace PX5

// ---- Convenience macros (preserve the old API surface) ----
// Existing call sites using PX5_LOGI(cat, fmt, ...) keep working unchanged.

#define PX5_LOGT(cat, fmt, ...) \
    ::PX5::Logger::Log(::PX5::LogLevel::TRACE,   cat, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define PX5_LOGD(cat, fmt, ...) \
    ::PX5::Logger::Log(::PX5::LogLevel::DEBUG,   cat, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define PX5_LOGI(cat, fmt, ...) \
    ::PX5::Logger::Log(::PX5::LogLevel::INFO,    cat, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define PX5_LOGW(cat, fmt, ...) \
    ::PX5::Logger::Log(::PX5::LogLevel::WARNING, cat, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define PX5_LOGE(cat, fmt, ...) \
    ::PX5::Logger::Log(::PX5::LogLevel::ERROR,   cat, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define PX5_LOGF(cat, fmt, ...) \
    ::PX5::Logger::Log(::PX5::LogLevel::FATAL,   cat, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

// Backward-compat: the old code also used PX5_LOGI etc. — keep them.
// (defined above)

#endif // PX5_LOGGER_H
