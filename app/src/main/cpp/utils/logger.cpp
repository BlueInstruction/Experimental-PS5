// SPDX-License-Identifier: MIT
// PX5 — Enhanced Logger implementation

#include "logger.h"

#include <android/log.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <mutex>
#include <pthread.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef PR_SET_NAME
#define PR_SET_NAME 15
#endif
#ifndef PR_GET_NAME
#define PR_GET_NAME 16
#endif

namespace PX5 {

namespace {
constexpr size_t kMaxFileSize   = 1 * 1024 * 1024;  // 1 MB per file
constexpr int     kMaxRotations = 5;                 // px5_main.log + .1..4 = 5 files
constexpr size_t kLineBufSize   = 4096;

struct LoggerState {
    std::mutex          mtx;
    std::string         log_dir;          // e.g. /storage/emulated/0/Android/data/com.px5.emulator/files/logs
    std::string         log_path;         // log_dir + "/px5_main.log"
    int                 fd{-1};           // raw fd; using write() for crash-safety
    LogLevel            min_level{LogLevel::INFO};
    bool                initialized{false};
    size_t              bytes_written{0};
};

LoggerState& State() {
    static LoggerState s;
    return s;
}

// Get current thread name via prctl (Bionic-safe; returns up to 16 bytes).
std::string CurrentThreadName() {
    char buf[16] = {0};
    if (prctl(PR_GET_NAME, buf) != 0) {
        return "unknown";
    }
    return std::string(buf);
}

// Format timestamp as "YYYY-MM-DD HH:MM:SS.mmm"
std::string FormatTimestamp() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto t  = system_clock::to_time_t(now);
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    struct tm tm_buf;
    localtime_r(&t, &tm_buf);

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
                  tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
                  tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
                  static_cast<int>(ms.count()));
    return std::string(buf);
}

// Ensure the log directory exists. Returns true on success.
bool EnsureDirExists(const std::string& path) {
    struct stat st{};
    if (stat(path.c_str(), &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    // mkdir -p style: walk components
    std::string acc;
    acc.reserve(path.size());
    size_t i = 0;
    if (!path.empty() && path[0] == '/') { acc.push_back('/'); i = 1; }
    for (; i < path.size(); ++i) {
        char c = path[i];
        acc.push_back(c);
        if (c == '/' || i == path.size() - 1) {
            if (!acc.empty() && acc.back() == '/') {
                // skip trailing slash for mkdir
                std::string p = acc.substr(0, acc.size() - 1);
                if (!p.empty()) {
                    if (stat(p.c_str(), &st) != 0) {
                        if (mkdir(p.c_str(), 0770) != 0 && errno != EEXIST) {
                            return false;
                        }
                    }
                }
            } else {
                if (stat(acc.c_str(), &st) != 0) {
                    if (mkdir(acc.c_str(), 0770) != 0 && errno != EEXIST) {
                        return false;
                    }
                }
            }
        }
    }
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

// Open (or reopen) the log file for appending. Returns true on success.
bool OpenLogFile() {
    auto& s = State();
    if (s.log_path.empty()) return false;
    s.fd = ::open(s.log_path.c_str(),
                  O_WRONLY | O_CREAT | O_APPEND,
                  0660);
    if (s.fd < 0) {
        return false;
    }
    // Determine current file size so we know when to rotate.
    struct stat st{};
    if (::fstat(s.fd, &st) == 0) {
        s.bytes_written = static_cast<size_t>(st.st_size);
    } else {
        s.bytes_written = 0;
    }
    return true;
}

void RotateFiles() {
    auto& s = State();
    if (s.fd >= 0) {
        ::close(s.fd);
        s.fd = -1;
    }
    // px5_main.log.(N-1) -> px5_main.log.N
    for (int i = kMaxRotations - 1; i >= 1; --i) {
        std::string src = s.log_path + "." + std::to_string(i);
        std::string dst = s.log_path + "." + std::to_string(i + 1);
        ::unlink(dst.c_str());  // ignore error
        ::rename(src.c_str(), dst.c_str());
    }
    // px5_main.log -> px5_main.log.1
    std::string dst = s.log_path + ".1";
    ::unlink(dst.c_str());
    ::rename(s.log_path.c_str(), dst.c_str());

    s.bytes_written = 0;
    OpenLogFile();
}

}  // namespace

// ============================================================================
// Public API
// ============================================================================

bool Logger::Initialize(std::string_view log_dir) noexcept {
    auto& s = State();
    std::lock_guard<std::mutex> lock(s.mtx);
    if (s.initialized) return true;

    s.log_dir.assign(log_dir);
    if (!EnsureDirExists(s.log_dir)) {
        __android_log_print(ANDROID_LOG_ERROR, "PX5_Logger",
                            "Failed to create log dir: %s", s.log_dir.c_str());
        return false;
    }
    s.log_path = s.log_dir + "/px5_main.log";
    if (!OpenLogFile()) {
        __android_log_print(ANDROID_LOG_ERROR, "PX5_Logger",
                            "Failed to open log file: %s", s.log_path.c_str());
        return false;
    }
    s.initialized = true;
    __android_log_print(ANDROID_LOG_INFO, "PX5_Logger",
                        "File logging initialized at %s", s.log_path.c_str());

    // Write a session-start marker so it's easy to find boot boundaries.
    const char* banner =
        "==================== PX5 SESSION START ====================\n";
    ::write(s.fd, banner, std::strlen(banner));
    s.bytes_written += std::strlen(banner);
    return true;
}

void Logger::SetMinLevel(LogLevel level) noexcept {
    State().min_level = level;
}

LogLevel Logger::GetMinLevel() noexcept {
    return State().min_level;
}

void Logger::Flush() noexcept {
    auto& s = State();
    std::lock_guard<std::mutex> lock(s.mtx);
    if (s.fd >= 0) {
        ::fsync(s.fd);
    }
}

void Logger::Shutdown() noexcept {
    auto& s = State();
    std::lock_guard<std::mutex> lock(s.mtx);
    if (s.fd >= 0) {
        const char* banner =
            "==================== PX5 SESSION END ====================\n";
        ::write(s.fd, banner, std::strlen(banner));
        ::fsync(s.fd);
        ::close(s.fd);
        s.fd = -1;
    }
    s.initialized = false;
}

const char* Logger::LevelToString(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::TRACE:   return "TRACE";
        case LogLevel::DEBUG:   return "DEBUG";
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARN";
        case LogLevel::ERROR:   return "ERROR";
        case LogLevel::FATAL:   return "FATAL";
    }
    return "?";
}

const char* Logger::CategoryToString(LogCategory category) noexcept {
    switch (category) {
        case LogCategory::CORE:       return "PX5_Core";
        case LogCategory::CPU:        return "PX5_CPU";
        case LogCategory::GPU:        return "PX5_GPU";
        case LogCategory::LOADER:     return "PX5_Loader";
        case LogCategory::FILESYSTEM: return "PX5_VFS";
        case LogCategory::KERNEL:     return "PX5_Kernel";
        case LogCategory::MEMORY:     return "PX5_Mem";
        case LogCategory::JNI:        return "PX5_JNI";
        case LogCategory::FEX:        return "PX5_FEXCore";
        case LogCategory::AUDIO:      return "PX5_Audio";
        case LogCategory::INPUT:      return "PX5_Input";
        case LogCategory::MEDIA:      return "PX5_Media";
        case LogCategory::VULKAN:     return "PX5_Vulkan";
        case LogCategory::NETWORK:    return "PX5_Net";
        case LogCategory::SETTINGS:   return "PX5_Settings";
        case LogCategory::SYSTEM:     return "PX5_System";
    }
    return "PX5";
}

android_LogPriority Logger::LevelToAndroidPriority(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::TRACE:   return ANDROID_LOG_VERBOSE;
        case LogLevel::DEBUG:   return ANDROID_LOG_DEBUG;
        case LogLevel::INFO:    return ANDROID_LOG_INFO;
        case LogLevel::WARNING: return ANDROID_LOG_WARN;
        case LogLevel::ERROR:   return ANDROID_LOG_ERROR;
        case LogLevel::FATAL:   return ANDROID_LOG_FATAL;
    }
    return ANDROID_LOG_DEFAULT;
}

std::string Logger::GetCurrentLogFilePath() noexcept {
    auto& s = State();
    std::lock_guard<std::mutex> lock(s.mtx);
    return s.log_path;
}

// ============================================================================
// Internal helpers
// ============================================================================

void Logger::MaybeRotate() noexcept {
    auto& s = State();
    if (s.bytes_written >= kMaxFileSize) {
        RotateFiles();
    }
}

void Logger::WriteToFile(std::string_view formatted_line, LogLevel level) noexcept {
    auto& s = State();
    if (!s.initialized || s.fd < 0) return;

    MaybeRotate();

    // Append a trailing newline if the line doesn't have one.
    std::string line(formatted_line);
    if (line.empty() || line.back() != '\n') {
        line.push_back('\n');
    }

    ::write(s.fd, line.data(), line.size());
    s.bytes_written += line.size();

    // Flush immediately on ERROR/FATAL so logs survive a crash mid-write.
    if (level >= LogLevel::ERROR) {
        ::fsync(s.fd);
    }
}

void Logger::LogV(LogLevel level, LogCategory category,
                  const char* format, va_list args) noexcept {
    auto& s = State();
    if (static_cast<uint8_t>(level) < static_cast<uint8_t>(s.min_level)) {
        return;
    }

    // 1. Always emit to Android logcat.
    {
        va_list args_copy;
        va_copy(args_copy, args);
        __android_log_vprint(LevelToAndroidPriority(level),
                             CategoryToString(category),
                             format, args_copy);
        va_end(args_copy);
    }

    // 2. Format the full line: [TS] [LEVEL] [TID:threadname] [CATEGORY] msg
    char msg_buf[kLineBufSize];
    vsnprintf(msg_buf, kLineBufSize, format, args);

    char line_buf[kLineBufSize + 128];
    auto ts = FormatTimestamp();
    auto tid = static_cast<unsigned long>(gettid());
    auto tname = CurrentThreadName();

    std::snprintf(line_buf, sizeof(line_buf),
                  "[%s] [%-5s] [%lu:%s] [%s] %s",
                  ts.c_str(),
                  LevelToString(level),
                  tid, tname.c_str(),
                  CategoryToString(category),
                  msg_buf);

    // 3. Write to file under the lock.
    {
        std::lock_guard<std::mutex> lock(s.mtx);
        WriteToFile(line_buf, level);
    }
}

void Logger::Log(LogLevel level, LogCategory category,
                 const char* format, ...) noexcept {
    va_list args;
    va_start(args, format);
    LogV(level, category, format, args);
    va_end(args);
}

} // namespace PX5
