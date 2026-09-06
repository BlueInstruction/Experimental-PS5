#ifndef PX5_PAGE_SIZE_H
#define PX5_PAGE_SIZE_H

#include <cstddef>
#include <cstdint>
#include <unistd.h>

namespace PX5 {

// Single source of truth for the HOST page size.
//
// Android 15 ships devices with 16 KiB pages, and Play requires targetSdk 35
// builds to support them. Hardcoding 4096 makes every mprotect() call in the
// memory manager and in the SMC tracker misaligned on such a device: the
// kernel answers EINVAL, or worse, a partial-page protect silently covers a
// neighbouring mapping. sysconf() is the only value the kernel agrees with.
inline size_t HostPageSize() {
    static const size_t kPageSize = []() -> size_t {
        const long v = sysconf(_SC_PAGESIZE);
        return v > 0 ? static_cast<size_t>(v) : 4096u;
    }();
    return kPageSize;
}

inline uint64_t PageAlignDown(uint64_t v) {
    const uint64_t p = static_cast<uint64_t>(HostPageSize());
    return v & ~(p - 1);
}

inline uint64_t PageAlignUp(uint64_t v) {
    const uint64_t p = static_cast<uint64_t>(HostPageSize());
    return (v + p - 1) & ~(p - 1);
}

} // namespace PX5

#endif // PX5_PAGE_SIZE_H
