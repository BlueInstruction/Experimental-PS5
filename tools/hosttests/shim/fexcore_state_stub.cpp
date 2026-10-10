// Host-test shim: crash_handler.cpp (linked for CrashHandler::LogsDir, which
// the loader's elfdump path uses) and kernel/syscalls.cpp call symbols that
// only exist inside the app build (FEX headers are not available on the
// host). The trap/dispatch tests never run guest state extraction or guest
// thread entry, so stubs that report "no live guest" are the honest answer.
#include <cstdint>

namespace PX5::FexCoreIntegration {

bool GetLiveGuestState(uint64_t* /*rip*/, uint64_t* /*rsp*/,
                       uint64_t* /*fsBase*/, uint64_t* /*gsBase*/) {
    return false;
}

void SetLiveGuestSegmentBase(bool /*enable*/, uint64_t /*base*/) {
    // No live guest on the host: nothing to publish.
}

} // namespace PX5::FexCoreIntegration
