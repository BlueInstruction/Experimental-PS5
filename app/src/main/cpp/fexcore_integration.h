#ifndef PSX5_FEXCORE_INTEGRATION_H
#define PSX5_FEXCORE_INTEGRATION_H

#include <cstdint>
#include <string>

namespace PX5::FexCoreIntegration {

/**
 * Guest synchronous trap information (ud2/int3/div0/...) captured and unwound.
 */
struct GuestTrapInfo {
    bool     fired   = false;  ///< Whether a trap was captured this run
    uint32_t signal  = 0;      ///< Host signal FEXCore's generated fault raised
    uint32_t trapNo  = 0;      ///< x86 trap number FEXCore synthesized (GP/UD/BP...)
    uint32_t siCode  = 0;      ///< si_code FEXCore synthesized (ILL_ILLOPN etc.)
    uint64_t guestRip = 0;     ///< Guest instruction that caused the trap
};

/**
 * Honest result of one guest execution attempt.
 */
struct ExecResult {
    bool        started   = false;      ///< FEXCore created and ran a thread
    bool        exitedCleanly = false;  ///< exit_group captured OR HLT reached
    uint64_t    exitCode  = 0;          ///< From GuestSyscalls (when captured)
    std::string output;                 ///< Guest stdout/stderr text
    double      elapsedMs = 0.0;        ///< Execution duration in milliseconds
    std::string error;                  ///< Non-empty => start/run failure reason
    GuestTrapInfo guestTrap;            ///< Populated when the guest trapped
};

/**
 * Reads live guest CPU state (RIP, RSP, FS, GS) from the executing thread.
 * Safe from syscall bridge (on guest thread) and crash handler.
 * @param rip Output pointer for guest RIP (optional, can be null)
 * @param rsp Output pointer for guest RSP (optional, can be null)
 * @param fsBase Output pointer for guest FS base (optional, can be null)
 * @param gsBase Output pointer for guest GS base (optional, can be null)
 * @return true if a guest thread is executing, false otherwise
 */
bool GetLiveGuestState(uint64_t* rip, uint64_t* rsp,
                       uint64_t* fsBase, uint64_t* gsBase);

/**
 * Sets guest segment base register (FS or GS) for the live thread.
 * Backend for arch_prctl(ARCH_SET_FS / ARCH_SET_GS).
 * @param isFs true for FS, false for GS
 * @param base New segment base address
 * @return true if guest thread is live and base was set, false otherwise
 */
bool SetLiveGuestSegmentBase(bool isFs, uint64_t base);

/**
 * Initializes FEXCore: Config -> features -> context -> core.
 * @return true if initialization succeeded, false otherwise
 */
bool Initialize();

/**
 * Shuts down FEXCore, releasing context and clearing all state.
 */
void Shutdown();

/**
 * Returns whether FEXCore has been initialized.
 * @return true if Initialize() succeeded, false otherwise
 */
bool IsInitialized();

/**
 * Rebuilds the engine from scratch in a fork-isolated child (Shutdown + Initialize).
 * Removes fork-inherited multithreaded state that can cause child crashes.
 */
void ResetForChild();

/**
 * Executes guest code already mapped in the PX5 memory window until HLT or exit.
 * Both pointers are HOST-side values bridged through MemoryManager.
 * @param hostRip Host pointer to guest entry point
 * @param hostStackTop Host pointer to top of guest stack
 * @param initialFsBase Guest FSBASE to set before dispatch (ORBIS contract); 0 = unset
 * @return ExecResult containing execution outcome
 */
ExecResult ExecuteAtHostRip(uint64_t hostRip, uint64_t hostStackTop,
                            uint64_t initialFsBase = 0);

/**
 * Runs classic arithmetic conformance test (mov eax,40; add eax,2; hlt).
 * @return true if guest blob executed correctly, false otherwise
 */
bool RunConformanceTest();

/**
 * Returns architecture summary string (FEXCore version, x86-64 -> ARM64).
 * @return Human-readable architecture description
 */
std::string GetArchitectureSummary();

/**
 * Returns syscall statistics string (total/handled/unhandled/bytes).
 * @return Formatted syscall stats
 */
std::string GetSyscallStatsString();

/**
 * Returns comprehensive engine counters (syscalls, SMC, traps, memory, threads).
 * @return Multi-line engine diagnostics
 */
std::string GetEngineCounters();

/**
 * Applies a FEXCore config override (real FEX option keys from Config.json.in).
 * Must be called before Initialize(); returns false if key unknown or engine live.
 * @param key FEXCore config key (e.g., "TSOEnabled", "Multiblock")
 * @param value Config value as string
 * @return true if override applied or deferred, false if rejected
 */
bool ApplyEngineConfigOverride(const std::string& key, const std::string& value);

} // namespace PX5::FexCoreIntegration

#endif
