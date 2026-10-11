#ifndef PX5_EMULATOR_H
#define PX5_EMULATOR_H

#include <string>
#include <mutex>
#include <atomic>
#include "fexcore_integration.h"
#include "../loader/elf_loader.h"

namespace PX5 {

// ---------------------------------------------------------------------------
// Emulator — foundation orchestrator.
//
// Design principle of this rewrite: every public method either performs a
// REAL action and returns honest evidence, or returns false with an
// explanatory error string. Nothing here logs "success" without doing work.
// ---------------------------------------------------------------------------
class Emulator {
public:
    /**
     * Returns the singleton instance of the emulator.
     * @return Reference to the singleton Emulator instance
     */
    static Emulator& GetInstance();

    /**
     * Initializes the emulator foundation (memory window, VFS, GPU device, audio).
     * @param baseDir App filesDir used for temp ELF fixtures and VFS anchor
     * @return true if initialization succeeded, false otherwise
     */
    bool Initialize(const std::string& baseDir);

    /**
     * Shuts down the emulator, releasing all allocated resources.
     */
    void Shutdown();

    /**
     * Loads and maps an executable into guest memory (ELF or SELF container).
     * After success, call ExecuteLoadedGuest() to run it.
     * @param path Filesystem path to the executable file
     * @param isSelf true if the file is a SELF container, false for plain ELF
     * @return true if load succeeded, false otherwise (error in LoadedImage)
     */
    bool LoadExecutable(const std::string& path, bool isSelf);

    /**
     * Executes the loaded guest image at its entry point until HLT or exit.
     * @return ExecResult containing execution outcome (started, exitCode, output, errors)
     */
    FexCoreIntegration::ExecResult ExecuteLoadedGuest();

    /**
     * Runs comprehensive foundation self-test: memory, Vulkan, GPU proofs,
     * FEXCore, raw guest, ELF pipeline.
     * Ordered steps that stop on first honest failure. The two GPU evidence
     * steps (3-4) are self-contained and run BEFORE any guest execution:
     * a guest-exec crash later in the suite can no longer void the M7
     * readback evidence (T3). Every step line is also streamed to the
     * report trail fd when one is set, so a child death mid-suite still
     * leaves every completed line in the parent's report.
     *   1. memory window reservation
     *   2. Vulkan runtime enumeration
     *   3. GPU submission (offscreen clear proof)
     *   4. GPU IR clear readback (the M7 gate, T3)
     *   5. FEXCore context creation
     *   6. raw x86-64 blob execution (write/exit_group/hlt)
     *   7. embedded ELF file round-trip through real loader
     * @return Multi-line report; last line is always "VERDICT: PASS|FAIL"
     */
    std::string SelfTestFoundation();

    /**
     * Streams every self-test step line to the given fd ("# <line>\n") as
     * it happens, so the fork-isolated runner can recover completed steps
     * from a child that dies later in the suite. -1 disables streaming
     * (in-process runs). Configuration, not evidence: safe to leave set.
     */
    void SetReportTrailFd(int fd) { m_reportTrailFd = fd; }

    /**
     * Returns the most recently loaded guest image metadata.
     * @return Reference to LoadedElfImage containing segments, entry point, etc.
     */
    const LoadedElfImage& LoadedImage() const { return m_image; }

    /**
     * Maps guest memory at the specified address (legacy JNI accessor, window-validated).
     * @param addr Guest virtual address
     * @param size Memory region size in bytes
     * @param flags Protection flags (PAGE_READ | PAGE_WRITE | PAGE_EXEC)
     * @return Guest address on success, 0 on failure
     */
    uint64_t MapMemory(uint64_t addr, size_t size, uint32_t flags);

    /**
     * Unmaps guest memory at the specified address (legacy JNI accessor).
     * @param addr Guest virtual address
     * @param size Memory region size in bytes
     * @return true if unmap succeeded, false otherwise
     */
    bool     UnmapMemory(uint64_t addr, size_t size);

    /**
     * Returns whether the emulator has been initialized (evidence check only).
     * @return true if Initialize() succeeded, false otherwise
     */
    bool IsInitializedEvidenceOnly() const { return m_initialized.load(); }

    /**
     * Returns human-readable status string (Uninitialized / Ready / Running).
     * @return Current emulator state as string
     */
    std::string GetStatusString() const;

private:
    Emulator() = default;
    ~Emulator() = default;

    /**
     * Ensures the guest memory window is reserved (self-test helper).
     * @param report Output vector for test step results
     * @param ok Reference to overall success flag
     * @param fatal Reference to fatal failure flag
     */
    void EnsureMemoryWindow(std::vector<std::string>& report,
                            bool& ok, bool& fatal);

    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_running{false};
    std::string       m_baseDir;
    std::mutex        m_runMutex;      // one guest execution at a time
    int               m_reportTrailFd = -1;  // self-test line streaming
    LoadedElfImage    m_image;
};

} // namespace PX5

#endif // PX5_EMULATOR_H
