// SPDX-License-Identifier: MIT
// PX5 breadcrumbs — the last known native steps before a process death.
//
// WHY: the 2026-08-29 device logs show the process dying inside the GPU
// proof with ZERO evidence of which Vulkan step was executing (no crash
// report reached disk, no gpu_proof event was emitted). A pasted log with
// no step information cannot be diagnosed honestly.
//
// HOW: normal code stamps a short fixed-size line ("gpu.step=3
// create_image") into a preallocated ring BEFORE entering each risky step.
// The crash handler (async-signal-safe context: open/write only) drains
// the ring into the crash report, pinning the exact step in flight.
//
// Slot writes use snprintf + a mutex — fine in normal execution, never
// called from a signal handler. DumpToFd uses only write(), which is
// async-signal-safe.

#ifndef PX5_UTILS_BREADCRUMBS_H
#define PX5_UTILS_BREADCRUMBS_H

#include <cstdarg>
#include <cstddef>

namespace PX5::Breadcrumb {

// Record a step marker (printf-style). Never allocates beyond a fixed
// 128-byte slot; silently truncates. Safe from any normal thread.
void Set(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

// Async-signal-safe drain into `fd` (one line per slot, oldest first).
// Returns the number of bytes written.
long DumpToFd(int fd);

// Copies the most recent crumb (or an empty string if none yet) into `out`.
// Normal-context only (takes the ring mutex) — used by the heartbeat
// thread so a silent process death still names its last live stage.
void Last(char* out, size_t n);

} // namespace PX5::Breadcrumb

#endif // PX5_UTILS_BREADCRUMBS_H
