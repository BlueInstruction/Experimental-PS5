// SPDX-License-Identifier: MIT
// PX5 breadcrumbs (implementation). See header for the contract.

#include "utils/breadcrumbs.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
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

    char buf[kSlotBytes];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    std::lock_guard<std::mutex> lk(r.mu);
    memcpy(r.slots[idx], buf, kSlotBytes);
    r.slots[idx][kSlotBytes - 1] = '\0';
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
