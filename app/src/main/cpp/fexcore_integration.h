#ifndef PSX5_FEXCORE_INTEGRATION_H
#define PSX5_FEXCORE_INTEGRATION_H

#include <cstdint>
#include <string>

namespace PX5::FexCoreIntegration {

// Honest result of one guest-execution attempt.
struct ExecResult {
    bool        started   = false;   // FEXCore created and ran a thread
    bool        exitedCleanly = false; // exit_group captured OR HLT reached
    uint64_t    exitCode  = 0;       // from GuestSyscalls (when captured)
    std::string output;              // guest stdout/stderr text
    double      elapsedMs = 0.0;
    std::string error;               // non-empty => start/run failure reason
};

bool Initialize();                    // Config -> features -> context -> core
void Shutdown();
bool IsInitialized();

// Execute guest code that is ALREADY mapped inside the PX5 memory window.
// Both pointers are HOST-side values bridged through MemoryManager.
ExecResult ExecuteAtHostRip(uint64_t hostRip, uint64_t hostStackTop);

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
