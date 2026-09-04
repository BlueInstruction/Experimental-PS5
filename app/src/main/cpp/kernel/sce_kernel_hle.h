#ifndef PX5_SCE_KERNEL_HLE_H
#define PX5_SCE_KERNEL_HLE_H

#include <cstdint>
#include <string>
#include <vector>

namespace PX5::SceKernelHle {

// ---------------------------------------------------------------------------
// libkernel-style HLE surface (v1).
//
// What this is:  a genuine, invocable symbol table mirroring the PS4/PS5
//   `libkernel.web` naming scheme, built ON TOP of the same honest plumbing
//   the Linux syscall bridge uses (MemoryManager window + captured console
//   + host fd handling). Two-phase DirectMemory (Allocate -> Map) mirrors
//   how real PS kernel hands out physical pages.
//
// What this is NOT yet:  a resolver that routes guest ELF imports through
//   this table. Import resolution / import thunks are Phase-C work. Guests
//   today still reach these code paths via raw Linux syscalls.
//
// Every function returns errno-style negatives on failure and logs loudly;
// nothing fabricates success.
// ---------------------------------------------------------------------------

constexpr int64_t SCE_OK = 0;

/**
 * HLE symbol table entry with invocation counter.
 */
struct SymbolEntry {
    const char* name;   ///< Symbol name (e.g., "sceKernelOpen")
    uint64_t    calls;  ///< Invocation counter (atomic-ish, benign races)
};

/**
 * High-level emulation layer for PS5 libkernel.sprx functions.
 */
class KernelHle {
public:
    /**
     * Returns the singleton KernelHle instance.
     * @return Reference to singleton
     */
    static KernelHle& GetInstance();

    /**
     * Registers the full symbol table (idempotent).
     */
    void RegisterAll();

    /**
     * Returns the complete symbol table.
     * @return Reference to symbol table vector
     */
    const std::vector<SymbolEntry>& Table() const { return m_table; }

    /**
     * Returns the number of registered symbols.
     * @return Symbol count
     */
    size_t SymbolCount() const;

    /**
     * Dispatches by symbol name with up to 6 arguments.
     * Used by self-tests to exercise wrappers without guest imports wired.
     * @param symbol Symbol name to invoke
     * @param a0 First argument
     * @param a1 Second argument
     * @param a2 Third argument
     * @param a3 Fourth argument
     * @param a4 Fifth argument
     * @param a5 Sixth argument
     * @return Function result (SCE_OK or errno-style negative)
     */
    uint64_t InvokeByName(const std::string& symbol,
                          uint64_t a0 = 0, uint64_t a1 = 0,
                          uint64_t a2 = 0, uint64_t a3 = 0,
                          uint64_t a4 = 0, uint64_t a5 = 0);

    /**
     * Returns human-readable summary of HLE state.
     * @return Summary string with symbol count and stats
     */
    std::string GetSummaryString() const;

    // --- direct entry points ------------------------------------------

    /**
     * HLE for sceKernelOpen: opens a file descriptor.
     * @param pathGuestOrHostPtr Guest pointer to path string
     * @param flags Open flags (O_RDONLY, O_WRONLY, etc.)
     * @param mode File mode
     * @return File descriptor on success, negative errno on failure
     */
    uint64_t Open(uint64_t pathGuestOrHostPtr, uint64_t flags, uint64_t mode);

    /**
     * HLE for sceKernelWrite: writes to a file descriptor.
     * @param fd File descriptor
     * @param buf Guest pointer to buffer
     * @param count Bytes to write
     * @return Bytes written on success, negative errno on failure
     */
    uint64_t Write(uint64_t fd, uint64_t buf, uint64_t count);

    /**
     * HLE for sceKernelClose: closes a file descriptor.
     * @param fd File descriptor
     * @return 0 on success, negative errno on failure
     */
    uint64_t Close(uint64_t fd);

    /**
     * HLE for sceKernelAllocateDirectMemory: allocates physical memory pages.
     * Two-phase allocation: Allocate returns physical offset, Map backs it.
     * @param searchStart Physical search start offset
     * @param searchEnd Physical search end offset
     * @param len Allocation size in bytes
     * @param alignment Alignment requirement
     * @param physAddrOut Guest pointer to receive physical address
     * @return SCE_OK on success, negative errno on failure
     */
    uint64_t AllocateDirectMemory(uint64_t searchStart, uint64_t searchEnd,
                                  uint64_t len, uint64_t alignment,
                                  uint64_t physAddrOut);

    /**
     * HLE for sceKernelMapDirectMemory: maps physical pages to virtual address.
     * @param addrRequested Requested virtual address (0 for any)
     * @param len Mapping size in bytes
     * @param prot Protection flags (PROT_READ | PROT_WRITE | PROT_EXEC)
     * @param flagsM Mapping flags
     * @param physical Physical offset from AllocateDirectMemory
     * @param maxPgOff Maximum page offset
     * @return Virtual address on success, 0 on failure
     */
    uint64_t MapDirectMemory(uint64_t addrRequested, uint64_t len,
                             uint64_t prot, uint64_t flagsM,
                             uint64_t physical, uint64_t maxPgOff);

    /**
     * HLE for sceKernelUnmapDirectMemory: unmaps virtual address range.
     * @param addr Virtual address
     * @param len Size in bytes
     * @return SCE_OK on success, negative errno on failure
     */
    uint64_t UnmapDirectMemory(uint64_t addr, uint64_t len);

    /**
     * HLE for sceKernelMprotect: changes memory protection.
     * @param addr Virtual address
     * @param len Size in bytes
     * @param prot New protection flags
     * @return SCE_OK on success, negative errno on failure
     */
    uint64_t Mprotect(uint64_t addr, uint64_t len, uint64_t prot);

    /**
     * HLE for sceKernelSleep: sleeps for specified seconds (capped at 60s).
     * @param seconds Sleep duration in seconds
     * @return SCE_OK
     */
    uint64_t SleepSeconds(uint64_t seconds);

    /**
     * HLE for sceKernelIsNeoMode: queries Neo/Pro mode (always returns 0 = base).
     * @return 0 (base console mode)
     */
    uint64_t IsNeoMode();

    /**
     * HLE for sceKernelGetCompiledSdkVersion: returns SDK version (0 until real value).
     * @return SDK version
     */
    uint64_t GetCompiledSdkVersion();

    /**
     * HLE for sceGnmSubmitCommandBuffers: submits GNM command buffers to GPU.
     * Routes to GnmSubmit -> Pm4Decoder -> GnmState.
     * @param count Number of command buffer descriptors
     * @param descriptorsPtr Guest pointer to descriptor array
     * @param userDataPtr Guest pointer to user data
     * @return SCE_OK on success, negative errno on failure
     */
    uint64_t GnmSubmitCommandBuffers(uint64_t count, uint64_t descriptorsPtr,
                                     uint64_t userDataPtr);

private:
    KernelHle() = default;

    std::vector<SymbolEntry> m_table;
    bool m_registered = false;

    // Direct-memory bump allocator state (window-internal "physical" space).
    uint64_t m_dmNext      = 0;    // next free offset inside DM region
    static constexpr uint64_t kDmRegionVa     = 0x14F000000ULL;
    static constexpr uint64_t kDmRegionSize   = 0x00800000ULL;  // 8 MiB
    static constexpr uint64_t kDmMapBase      = 0x147000000ULL; // map area
    static constexpr uint64_t kDmMapSize      = 0x00800000ULL;  // 8 MiB
};

} // namespace PX5::SceKernelHle

#endif // PX5_SCE_KERNEL_HLE_H
