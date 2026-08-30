// SPDX-License-Identifier: MIT
// PX5 heartbeat (implementation). See header for the contract.

#include "utils/heartbeat.h"

#include "utils/breadcrumbs.h"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

namespace PX5::Heartbeat {

namespace {

std::atomic<bool> started{false};

long NowMs() {
    return static_cast<long>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

void* Loop(void* arg) {
    std::string dir = *static_cast<std::string*>(arg);
    delete static_cast<std::string*>(arg);

    const std::string path = dir + "/px5_heartbeat.log";
    const long t0 = NowMs();

    for (;;) {
        char last[128] = "(no breadcrumb yet)";
        PX5::Breadcrumb::Last(last, sizeof(last));

        char line[256];
        snprintf(line, sizeof(line), "HB %lld %ld %s\n",
                 static_cast<long long>(time(nullptr)), NowMs() - t0, last);

        const int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) {
            const ssize_t n = write(fd, line, strlen(line));
            (void)n;
            close(fd);
        } else if (errno == ENOENT) {
            // logs dir vanished (storage tear-down): nothing to do; retry
            // on the next tick rather than dying — the loop must survive.
        }
        usleep(1000 * 1000);
    }
    return nullptr;
}

} // namespace

void Start(const std::string& logsDir) {
    if (logsDir.empty()) return;
    bool expected = false;
    if (!started.compare_exchange_strong(expected, true)) return;

    pthread_t th;
    auto* arg = new std::string(logsDir);
    if (pthread_create(&th, nullptr, Loop, arg) != 0) {
        delete arg;
        started.store(false);   // honest: report-and-retry on next call
        return;
    }
    pthread_detach(th);
}

} // namespace PX5::Heartbeat
