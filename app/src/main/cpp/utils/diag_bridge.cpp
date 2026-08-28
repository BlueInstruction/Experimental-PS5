// SPDX-License-Identifier: MIT
// PX5 — Native → Kotlin diagnostic-stream bridge (implementation).

#include "diag_bridge.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <mutex>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace PX5 {

namespace {

constexpr size_t kLineBufSize  = 2048;
constexpr size_t kMaxLineLen   = 1024;   // truncate pathological lines
constexpr int    kSessionCap   = 4000;   // forwarded lines per process

struct BridgeState {
    std::mutex  mtx;
    std::string path;
    int         fd{-1};
    int         lines{0};      // forwarded this session
    bool        cap_reported{false};
};

BridgeState& State() {
    static BridgeState s;
    return s;
}

bool LevelPasses(LogLevel level, LogCategory category) noexcept {
    if (level >= LogLevel::WARNING) return true;
    if (level != LogLevel::INFO)    return false;
    switch (category) {
        case LogCategory::GPU:
        case LogCategory::LOADER:
        case LogCategory::VULKAN:
        case LogCategory::FEX:
        case LogCategory::CORE:
            return true;
        default:
            return false;
    }
}

std::string FormatTimestamp() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto t   = system_clock::to_time_t(now);
    auto ms  = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    struct tm tm_buf;
    localtime_r(&t, &tm_buf);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
                  tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
                  tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
                  static_cast<int>(ms.count()));
    return std::string(buf);
}

} // namespace

void DiagBridge::Enable(const std::string& diagnostic_log_path) noexcept {
    auto& s = State();
    std::lock_guard<std::mutex> lock(s.mtx);
    if (s.fd >= 0) {
        ::close(s.fd);
        s.fd = -1;
    }
    s.path = diagnostic_log_path;
    if (s.path.empty()) return;
    // Open lazily-but-eagerly once; O_APPEND keeps Kotlin and native lines
    // interleaved safely. If the open fails (dir not created yet), retry on
    // the first forwarded line.
    s.fd = ::open(s.path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0660);
}

void DiagBridge::Forward(LogLevel level, LogCategory category,
                         const char* message) noexcept {
    if (!message) return;
    auto& s = State();
    if (s.path.empty()) return;
    if (s.lines >= kSessionCap) {
        if (!s.cap_reported) {
            // One honest marker, then silence: the cap is a safety valve,
            // not a hidden drop. The full story stays in px5_main.log.
            std::lock_guard<std::mutex> lock(s.mtx);
            if (!s.cap_reported && s.fd >= 0) {
                const char* note =
                    "[native-bridge] session cap reached — further NATIVE "
                    "lines only in px5_main.log\n";
                ::write(s.fd, note, std::strlen(note));
                s.cap_reported = true;
            }
        }
        return;
    }
    if (!LevelPasses(level, category)) return;

    // Flatten the message to a single line and cap pathological length.
    char msg[kMaxLineLen];
    size_t out = 0;
    for (const char* p = message; *p != '\0' && out + 1 < sizeof(msg); ++p) {
        msg[out++] = (*p == '\n' || *p == '\r') ? ' ' : *p;
    }
    msg[out] = '\0';

    char line[kLineBufSize];
    int n = std::snprintf(line, sizeof(line), "[%s] NATIVE level=%s cat=%s %s\n",
                          FormatTimestamp().c_str(),
                          Logger::LevelToString(level),
                          Logger::CategoryToString(category),
                          msg);
    if (n <= 0) return;
    if (static_cast<size_t>(n) >= sizeof(line)) n = sizeof(line) - 1;

    std::lock_guard<std::mutex> lock(s.mtx);
    if (s.fd < 0) {
        // First use after Enable() failed to open (dir came late). Retry.
        s.fd = ::open(s.path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0660);
        if (s.fd < 0) return;
    }
    ssize_t r = ::write(s.fd, line, static_cast<size_t>(n));
    if (r > 0) ++s.lines;
    // WARNING+ must survive a crash mid-write.
    if (level >= LogLevel::ERROR) ::fsync(s.fd);
}

} // namespace PX5
