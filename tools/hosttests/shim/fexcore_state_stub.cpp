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


// v1.65 — the pthread tranche's device launcher. Host tests never dispatch
// guest code through FEXCore (they install their own launcher via
// PthreadHle::SetGuestLauncher before creating threads); this stub only
// keeps the RegisterAll reference resolvable. If a test reaches it, the
// false return names the situation through ThreadBody's honest log.
bool RunGuestThread(uint64_t /*entryRip*/, uint64_t /*arg*/,
                    uint64_t /*stackTopVa*/, uint64_t /*fsBaseVa*/,
                    uint64_t* raxOut) {
    if (raxOut) *raxOut = 0;
    return false;
}

} // namespace PX5::FexCoreIntegration
