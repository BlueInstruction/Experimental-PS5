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

struct SymbolEntry {
    const char* name;
    uint64_t    calls;      // invocation counter (atomic-ish, benign races)
};

class KernelHle {
public:
    static KernelHle& GetInstance();

    // Registers the full symbol table. Idempotent.
    void RegisterAll();

    const std::vector<SymbolEntry>& Table() const { return m_table; }
    size_t SymbolCount() const;

    // Dispatch by symbol name with up to 6 args — used by the evidence
    // self-test to exercise wrappers without guest imports wired yet.
    uint64_t InvokeByName(const std::string& symbol,
                          uint64_t a0 = 0, uint64_t a1 = 0,
                          uint64_t a2 = 0, uint64_t a3 = 0,
                          uint64_t a4 = 0, uint64_t a5 = 0);

    std::string GetSummaryString() const;

    // --- direct entry points ------------------------------------------
    // int  sceKernelOpen(const char* path, int flags, mode_t mode)
    uint64_t Open(uint64_t pathGuestOrHostPtr, uint64_t flags, uint64_t mode);
    // ssize sceKernelWrite(int fd, void* buf, size_t count)
    uint64_t Write(uint64_t fd, uint64_t buf, uint64_t count);
    // int  sceKernelClose(int fd)
    uint64_t Close(uint64_t fd);
    // int  sceKernelAllocateDirectMemory(off_t searchStart, off_t searchEnd,
    //                                    size_t len, size_t alignment,
    //                                    off_t* physAddrOut)
    uint64_t AllocateDirectMemory(uint64_t searchStart, uint64_t searchEnd,
                                  uint64_t len, uint64_t alignment,
                                  uint64_t physAddrOut);
    // void* sceKernelMapDirectMemory(void* addr, size_t len, int prot,
    //                                int flags, off_t physical, size_t maxPgOff)
    uint64_t MapDirectMemory(uint64_t addrRequested, uint64_t len,
                             uint64_t prot, uint64_t flagsM,
                             uint64_t physical, uint64_t maxPgOff);
    uint64_t UnmapDirectMemory(uint64_t addr, uint64_t len);
    uint64_t Mprotect(uint64_t addr, uint64_t len, uint64_t prot);
    uint64_t SleepSeconds(uint64_t seconds);          // capped at 60 s
    uint64_t IsNeoMode();                              // always 0 (base)
    uint64_t GetCompiledSdkVersion();                  // 0 until a real value

    // int64 sceGnmSubmitCommandBuffers(uint32 count, void* descArray,
    //                                  void* userData)
    // Phase C milestone 2b: routes to GnmSubmit -> Pm4Decoder -> GnmState.
    // SCE-style 0 on success, negative errno-style on failure.
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
