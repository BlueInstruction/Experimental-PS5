// SPDX-License-Identifier: MIT
// PX5 — Logger v2 implementation (Eden-format structured log).
// See logger.h for the format contract and provenance.

#include "logger.h"
#include "diag_bridge.h"

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

#ifndef PR_GET_NAME
#define PR_GET_NAME 16
#endif

namespace PX5 {

namespace {
constexpr size_t kMaxFileSize   = 1 * 1024 * 1024;  // 1 MB per file
constexpr int     kMaxRotations = 5;                 // px5_main.log + .1..4
constexpr size_t kLineBufSize   = 4096;
constexpr size_t kNumCategories = 16;

struct LoggerState {
    std::mutex          mtx;
    std::string         log_dir;
    std::string         log_path;         // log_dir + "/px5_main.log"
    int                 fd{-1};           // raw fd; write() for crash-safety
    LogLevel            min_level{LogLevel::INFO};
    LogLevel            class_level[kNumCategories];  // per-class filter
    bool                initialized{false};
    size_t              bytes_written{0};
    std::chrono::steady_clock::time_point epoch{};    // uptime epoch
};

LoggerState& State() {
    static LoggerState s;
    return s;
}

std::string CurrentThreadName() {
    char buf[16] = {0};
    if (prctl(PR_GET_NAME, buf) != 0) {
        return "unknown";
    }
    return std::string(buf);
}

// Monotonic uptime as "   12.345678" — Eden's FormatLogMessage shape
// ([%4d.%06d], seconds.microseconds since logger epoch). Lines sort
// naturally and correlate with probe time budgets.
void FormatUptime(char out[24]) {
    const auto& s = State();
    if (!s.initialized) {
        snprintf(out, 24, "%4u.%06llu", 0u, 0ull);
        return;
    }
    const auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now() - s.epoch).count();
    const unsigned long long total = us > 0 ? (unsigned long long)us : 0ull;
    snprintf(out, 24, "%4llu.%06llu", total / 1000000ull, total % 1000000ull);
}

// Trim __FILE__ to a repo-relative path. Build trees emit absolute paths
// like .../app/src/main/cpp/loader/elf_loader.cpp — keep everything from
// the last "cpp/" (our source root) so lines read like Eden's
// "core/loader/loader.cpp". Fallback: basename.
std::string TrimSourcePath(const char* file) {
    if (!file || !*file) return "?";
    const char* p = file;
    const char* found = nullptr;
    while ((p = strstr(p, "cpp/")) != nullptr) {
        found = p + 4;      // keep the suffix AFTER "cpp/"
        p += 4;
    }
    if (found && *found) return std::string(found);
    const char* base = strrchr(file, '/');
    return std::string(base ? base + 1 : file);
}

// Ensure the log directory exists. Returns true on success.
bool EnsureDirExists(const std::string& path) {
    struct stat st{};
    if (stat(path.c_str(), &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    std::string acc;
    acc.reserve(path.size());
    size_t i = 0;
    if (!path.empty() && path[0] == '/') { acc.push_back('/'); i = 1; }
    for (; i < path.size(); ++i) {
        char c = path[i];
        acc.push_back(c);
        if (c == '/' || i == path.size() - 1) {
            if (!acc.empty() && acc.back() == '/') {
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
    s.fd = ::open(s.log_path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0660);
    if (s.fd < 0) {
        return false;
    }
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
    for (int i = kMaxRotations - 1; i >= 1; --i) {
        std::string src = s.log_path + "." + std::to_string(i);
        std::string dst = s.log_path + "." + std::to_string(i + 1);
        ::unlink(dst.c_str());
        ::rename(src.c_str(), dst.c_str());
    }
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

    for (size_t i = 0; i < kNumCategories; ++i) {
        s.class_level[i] = LogLevel::TRACE;   // per-class filter open by
                                              // default; the floor governs
    }
    s.epoch = std::chrono::steady_clock::now();

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

    // Session banner: wall-clock anchor for the monotonic uptime stamps.
    char ts[64];
    {
        auto now = std::chrono::system_clock::now();
        auto t   = std::chrono::system_clock::to_time_t(now);
        struct tm tm_buf;
        localtime_r(&t, &tm_buf);
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_buf);
    }
    char banner[160];
    snprintf(banner, sizeof(banner),
             "==== PX5 SESSION START %s (uptime=0.000000) ====\n", ts);
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

void Logger::SetClassFilterString(const std::string& rules) noexcept {
    auto& s = State();
    // Tokenize on whitespace: "<Class>:<Level>" tokens, "*" wildcard.
    size_t pos = 0;
    while (pos < rules.size()) {
        while (pos < rules.size() && isspace((unsigned char)rules[pos])) ++pos;
        size_t end = pos;
        while (end < rules.size() && !isspace((unsigned char)rules[end])) ++end;
        std::string tok = rules.substr(pos, end - pos);
        pos = end;
        if (tok.empty()) continue;
        const size_t colon = tok.rfind(':');
        if (colon == std::string::npos) continue;
        const std::string cls = tok.substr(0, colon);
        const std::string lvl = tok.substr(colon + 1);
        LogLevel lv;
        if (lvl == "Trace" || lvl == "trace")      lv = LogLevel::TRACE;
        else if (lvl == "Debug" || lvl == "debug") lv = LogLevel::DEBUG;
        else if (lvl == "Info" || lvl == "info")   lv = LogLevel::INFO;
        else if (lvl == "Warning" || lvl == "warn") lv = LogLevel::WARNING;
        else if (lvl == "Error" || lvl == "error") lv = LogLevel::ERROR;
        else if (lvl == "Critical" || lvl == "fatal") lv = LogLevel::FATAL;
        else continue;
        if (cls == "*") {
            for (size_t i = 0; i < kNumCategories; ++i) s.class_level[i] = lv;
        } else {
            for (size_t i = 0; i < kNumCategories; ++i) {
                if (cls == CategoryToString(static_cast<LogCategory>(i))) {
                    s.class_level[i] = lv;
                    break;
                }
            }
        }
    }
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
        case LogLevel::TRACE:   return "Trace";
        case LogLevel::DEBUG:   return "Debug";
        case LogLevel::INFO:    return "Info";
        case LogLevel::WARNING: return "Warning";
        case LogLevel::ERROR:   return "Error";
        case LogLevel::FATAL:   return "Critical";
    }
    return "?";
}

const char* Logger::CategoryToString(LogCategory category) noexcept {
    switch (category) {
        case LogCategory::CORE:       return "Core";
        case LogCategory::CPU:        return "Cpu";
        case LogCategory::GPU:        return "Gnm";
        case LogCategory::LOADER:     return "Loader";
        case LogCategory::FILESYSTEM: return "VFS";
        case LogCategory::KERNEL:     return "Kernel";
        case LogCategory::MEMORY:     return "Memory";
        case LogCategory::JNI:        return "Frontend";
        case LogCategory::FEX:        return "Cpu.Fex";
        case LogCategory::AUDIO:      return "Audio";
        case LogCategory::INPUT:      return "Input";
        case LogCategory::MEDIA:      return "Media";
        case LogCategory::VULKAN:     return "Render.Vulkan";
        case LogCategory::NETWORK:    return "Net";
        case LogCategory::SETTINGS:   return "Config";
        case LogCategory::SYSTEM:     return "System";
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

// Lock-free read for the signal handler (see header note).
const char* Logger::PeekLogFilePathUnsafe() noexcept {
    auto& s = State();
    return s.log_path.empty() ? "" : s.log_path.c_str();
}

// ============================================================================
// Internal helpers
// ============================================================================

void Logger::MaybeRotate() noexcept {
    const auto& s = State();
    if (s.bytes_written >= kMaxFileSize) {
        RotateFiles();
    }
}

void Logger::WriteToFile(std::string_view formatted_line, LogLevel level) noexcept {
    auto& s = State();
    if (!s.initialized || s.fd < 0) return;

    MaybeRotate();

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
                  const char* file, int line, const char* func,
                  const char* format, va_list args) noexcept {
    auto& s = State();

    // Filter: per-class gate first (Eden's Filter::CheckMessage shape),
    // then the global floor.
    if (static_cast<uint8_t>(level) <
        static_cast<uint8_t>(s.class_level[static_cast<size_t>(category)])) {
        return;
    }
    if (static_cast<uint8_t>(level) < static_cast<uint8_t>(s.min_level)) {
        return;
    }

    const char* class_name = CategoryToString(category);
    const char* level_name = LevelToString(level);

    // 1. Always emit to Android logcat (tag "PX5.<Class>").
    {
        char tag[48];
        snprintf(tag, sizeof(tag), "PX5.%s", class_name);
        va_list args_copy;
        va_copy(args_copy, args);
        __android_log_vprint(LevelToAndroidPriority(level), tag,
                             format, args_copy);
        va_end(args_copy);
    }

    // 2. Format the message body once.
    char msg_buf[kLineBufSize];
    vsnprintf(msg_buf, kLineBufSize, format, args);

    // 3. Eden-format full line:
    //    [   0.001979] Kernel <Info> kernel/sce_kernel_hle.cpp:294:InvokeByName: msg
    char up[24];
    FormatUptime(up);
    const std::string src = TrimSourcePath(file);
    char prelude[224];
    snprintf(prelude, sizeof(prelude), "[%s] %s <%s> %s:%d:%s: ",
             up, class_name, level_name, src.c_str(), line,
             func ? func : "?");

    // Thread identity rides at the END of the line (off Eden's format but
    // load-bearing for our fork-isolated probes: correlating a child's
    // lines with its report needs the tid; Eden has no fork probes).
    char line_buf[kLineBufSize + 288];
    const auto tid = static_cast<unsigned long>(gettid());
    const std::string tname = CurrentThreadName();
    snprintf(line_buf, sizeof(line_buf), "%s%s |%lu:%s",
             prelude, msg_buf, tid, tname.c_str());

    // 4. Write to file under the lock.
    {
        std::lock_guard<std::mutex> lock(s.mtx);
        WriteToFile(line_buf, level);
    }

    // 5. Mirror the filtered subset into the Kotlin diagnostic stream.
    DiagBridge::Forward(level, category, msg_buf);
}

void Logger::Log(LogLevel level, LogCategory category,
                 const char* file, int line, const char* func,
                 const char* format, ...) noexcept {
    va_list args;
    va_start(args, format);
    LogV(level, category, file, line, func, format, args);
    va_end(args);
}

} // namespace PX5
