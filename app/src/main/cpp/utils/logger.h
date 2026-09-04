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

class Logger {
public:
    // ---- One-time initialization ----
    // Called from JNI_OnLoad. Sets the on-disk log directory (typically
    // <externalFilesDir>/logs/). The monotonic uptime epoch starts here.
    // Safe to call multiple times; only the first call wins.
    static bool Initialize(std::string_view log_dir) noexcept;

    // ---- Runtime level filter ----
    // Global floor: messages below this level are dropped for every class.
    // Default: INFO in release, TRACE in debug.
    static void SetMinLevel(LogLevel level) noexcept;
    static LogLevel GetMinLevel() noexcept;

    // ---- Per-class filter (Eden-style) ----
    // Parses a space-separated rule string applied left to right:
    //   "*:Warning Loader:Info Cpu.Fex:Trace"
    // Unknown class or level names are ignored (reported once).
    // The global floor above still applies on top.
    static void SetClassFilterString(const std::string& rules) noexcept;

    // ---- Core log function ----
    // Routes the message to:
    //   1. Android logcat (tag "PX5.<Class>")
    //   2. The rotating file (if Initialize() was called), Eden format
    // `func` is injected by the macros — call sites never pass it.
    static void Log(LogLevel level, LogCategory category,
                    const char* file, int line, const char* func,
                    const char* format, ...) noexcept;

    // ---- Variadic helper (for forwarding from other code) ----
    static void LogV(LogLevel level, LogCategory category,
                     const char* file, int line, const char* func,
                     const char* format, va_list args) noexcept;

    // ---- Flush ----
    // Forces an fsync of the underlying file. Called automatically on
    // ERROR/FATAL. Exposed so the crash handler can call it before
    // writing its own report.
    static void Flush() noexcept;

    // ---- Shutdown ----
    // Closes the file. Safe to call multiple times.
    static void Shutdown() noexcept;

    // ---- Helpers ----
    static const char* LevelToString(LogLevel level) noexcept;      // "Info"
    static const char* CategoryToString(LogCategory category) noexcept; // "Kernel"
    static android_LogPriority LevelToAndroidPriority(LogLevel level) noexcept;

    // ---- File-path getter (for the Kotlin side to read/share) ----
    static std::string GetCurrentLogFilePath() noexcept;

    // v1.16 — crash-handler-only accessor. Takes NO lock: the log path is
    // set once at Initialize (single-threaded startup) and never changes
    // (rotation renames FILES, not the path).
    static const char* PeekLogFilePathUnsafe() noexcept;

private:
    // Internal: appends one formatted line to the rotating file.
    static void WriteToFile(std::string_view formatted_line,
                            LogLevel level) noexcept;

    // Internal: rotates files when current file exceeds max size.
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
