#ifndef PSX5_FEXCORE_INTEGRATION_H
#define PSX5_FEXCORE_INTEGRATION_H

#include <cstdint>
#include <string>

namespace PX5::FexCoreIntegration {

// One guest-visible synchronous trap (ud2/int3/div0/...) that the fault
// router unwound instead of killing the process. Zeroes reported too.
struct GuestTrapInfo {
    bool     fired   = false;
    uint32_t signal  = 0;    // host signal FEXCore's generated fault raised
    uint32_t trapNo  = 0;    // x86 trap number FEXCore synthesized (GP/UD/BP...)
    uint32_t siCode  = 0;    // si_code FEXCore synthesized (ILL_ILLOPN etc.)
    uint64_t guestRip = 0;   // guest instruction that caused the trap
};

// Honest result of one guest-execution attempt.
struct ExecResult {
    bool        started   = false;   // FEXCore created and ran a thread
    bool        exitedCleanly = false; // exit_group captured OR HLT reached
    uint64_t    exitCode  = 0;       // from GuestSyscalls (when captured)
    std::string output;              // guest stdout/stderr text
    double      elapsedMs = 0.0;
    std::string error;               // non-empty => start/run failure reason
    GuestTrapInfo guestTrap;         // populated when the guest trapped
};

// v1.39 — live guest CPU-state accessors (no locks; safe from the syscall
// bridge on the guest thread itself and from the crash handler).
// GetLiveGuestState returns false when no guest thread is executing.
bool GetLiveGuestState(uint64_t* rip, uint64_t* rsp,
                       uint64_t* fsBase, uint64_t* gsBase);
// arch_prctl(ARCH_SET_FS / ARCH_SET_GS) backend: writes the guest-visible
// segment base into the executing thread's CPUState. Returns false when no
// guest thread is live (the bridge then reports EINVAL honestly).
bool SetLiveGuestSegmentBase(bool isFs, uint64_t base);

bool Initialize();                    // Config -> features -> context -> core
void Shutdown();
bool IsInitialized();

// v1.16: rebuild the engine from scratch inside a fork-isolated probe
// child (Shutdown + Initialize). Removes the fork-inherited multithreaded
// state that killed both v1.15 execution children at one deterministic PC.
void ResetForChild();

// Execute guest code that is ALREADY mapped inside the PX5 memory window.
// Both pointers are HOST-side values bridged through MemoryManager.
// v1.40: initialFsBase pre-sets the guest FSBASE before dispatch — the
// ORBIS kernel contract. PS5 crt code never issues arch_prctl (Linux is
// the only guest family that sets TLS that way); its very first block
// dereferences fs:[...] and expects the kernel to have pointed FS at the
// thread's TCB already. 0 = leave FSBASE unset (fixture guests).
ExecResult ExecuteAtHostRip(uint64_t hostRip, uint64_t hostStackTop,
                            uint64_t initialFsBase = 0);

// Classic arithmetic conformance blob (kept for the existing UI button).
bool RunConformanceTest();

std::string GetArchitectureSummary();
std::string GetSyscallStatsString();
std::string GetEngineCounters();

// Bridge one FEXCore config override (real FEX option keys, see
// FEXCore/Source/Interface/Config/Config.json.in). Must be called before
// engine Initialize(); returns false when the key is unknown or the engine
// is already live (no silent lies — the caller shows the failure).
bool ApplyEngineConfigOverride(const std::string& key, const std::string& value);

} // namespace PX5::FexCoreIntegration

#endif
