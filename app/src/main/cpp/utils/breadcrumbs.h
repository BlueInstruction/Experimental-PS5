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

/**
 * Breadcrumb trail for pinning execution steps before process death.
 * Records last known native steps in a fixed-size ring for crash forensics.
 */
namespace PX5::Breadcrumb {

/**
 * Records a step marker (printf-style) in the breadcrumb ring.
 * Never allocates beyond a fixed 128-byte slot; silently truncates.
 * Safe from any normal thread (not signal handlers).
 * @param fmt Printf-style format string
 * @param ... Format arguments
 */
void Set(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

/**
 * Dumps breadcrumb ring to file descriptor (async-signal-safe).
 * One line per slot, oldest first.
 * @param fd File descriptor to write to
 * @return Number of bytes written
 */
long DumpToFd(int fd);

/**
 * Copies most recent breadcrumb to output buffer (normal-context only).
 * Takes ring mutex; used by heartbeat thread for silent death attribution.
 * @param out Output buffer
 * @param n Buffer size
 */
void Last(char* out, size_t n);

} // namespace PX5::Breadcrumb

#endif // PX5_UTILS_BREADCRUMBS_H
