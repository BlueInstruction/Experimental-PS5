// SPDX-License-Identifier: MIT
// PX5 breadcrumbs (implementation). See header for the contract.

#include "utils/breadcrumbs.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <sys/types.h>
#include <unistd.h>

namespace PX5::Breadcrumb {

namespace {

constexpr int kSlots = 16;
constexpr size_t kSlotBytes = 128;

struct Ring {
    std::atomic<uint32_t> next{0};
    std::atomic<uint32_t> wrapped{0};
    char slots[kSlots][kSlotBytes];
    std::mutex mu;
};

Ring& RingInstance() {
    static Ring ring;   // fixed storage, no dynamic allocation
    return ring;
}

} // namespace

void Set(const char* fmt, ...) {
    Ring& r = RingInstance();
    const uint32_t idx = r.next.fetch_add(1, std::memory_order_relaxed) % kSlots;

    // v1.16: every crumb is stamped with its thread id. The v1.15 session
    // produced a dump whose global ring mixed three threads (engine preset,
    // GPU proof, GNM self-test) — without the tid the report could not say
    // WHICH step belonged to the crashing thread.
    char buf[kSlotBytes];
    const unsigned long tid = static_cast<unsigned long>(gettid());
    va_list ap;
    va_start(ap, fmt);
    int n = snprintf(buf, sizeof(buf), "[%lu] ", tid);
    if (n < 0 || static_cast<size_t>(n) >= sizeof(buf)) n = 0;
    vsnprintf(buf + n, sizeof(buf) - static_cast<size_t>(n), fmt, ap);
    va_end(ap);

    std::lock_guard<std::mutex> lk(r.mu);
    memcpy(r.slots[idx], buf, kSlotBytes);
    r.slots[idx][kSlotBytes - 1] = '\0';
}

void Last(char* out, size_t n) {
    if (!out || n == 0) return;
    out[0] = '\0';
    Ring& r = RingInstance();
    std::lock_guard<std::mutex> lk(r.mu);
    const uint32_t prev = (r.next.load(std::memory_order_relaxed) + kSlots - 1) % kSlots;
    const char* s = r.slots[prev];
    const size_t len = strnlen(s, kSlotBytes);
    if (len == 0) return;
    const size_t copy = len < n - 1 ? len : n - 1;
    memcpy(out, s, copy);
    out[copy] = '\0';
}

long DumpToFd(int fd) {
    if (fd < 0) return 0;
    Ring& r = RingInstance();
    const uint32_t next = r.next.load(std::memory_order_relaxed);
    const uint32_t start = next > kSlots ? next - kSlots : 0;

    long total = 0;
    // The mutex is NOT taken here (async-signal context). A torn slot shows
    // as garbage text in the report — acceptable, stated, never fatal.
    for (uint32_t i = start; i < next; ++i) {
        const char* s = r.slots[i % kSlots];
        // Slot may be mid-write; cap the scan at the slot size either way.
        size_t len = strnlen(s, kSlotBytes);
        if (len == 0) continue;
        char line[kSlotBytes + 4];
        memcpy(line, "  crumb: ", 9);
        memcpy(line + 9, s, len);
        line[9 + len] = '\n';
        ssize_t n = write(fd, line, 9 + len + 1);
        if (n > 0) total += n;
    }
    return total;
}

} // namespace PX5::Breadcrumb
