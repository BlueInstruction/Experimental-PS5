// Host-test shim: crash_handler.cpp (linked for CrashHandler::LogsDir, which
// the loader's elfdump path uses) calls one FexCoreIntegration symbol that
// only exists inside the app build (FEX headers are not available on the
// host). The trap tests never run guest state extraction, so a stub that
// reports "no live guest" is the honest host answer.
#include <cstdint>

namespace PX5::FexCoreIntegration {

bool GetLiveGuestState(uint64_t* /*rip*/, uint64_t* /*rsp*/,
                       uint64_t* /*fsBase*/, uint64_t* /*gsBase*/) {
    return false;
}

} // namespace PX5::FexCoreIntegration
