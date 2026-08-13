#include "syscalls.h"
#include "../utils/logger.h"
#include <csignal>

namespace PX5 {

static void SignalHandler(int signal, siginfo_t* info, void* context) {
    PX5_LOGE(LogCategory::KERNEL, "Intercepted guest signal %d at address %p", signal, info->si_addr);
}

void RegisterSignalHandlers() {
    struct sigaction sa{};
    sa.sa_sigaction = SignalHandler;
    sa.sa_flags = SA_SIGINFO;

    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGBUS, &sa, nullptr);
    sigaction(SIGILL, &sa, nullptr);

    PX5_LOGI(LogCategory::KERNEL, "Registered ARM64 Bionic Signal Interceptors (SIGSEGV/SIGBUS/SIGILL)");
}

} // namespace PX5
