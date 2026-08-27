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
    static Emulator& GetInstance();

    // baseDir = app filesDir (used for temp ELF fixtures + VFS anchor).
    bool Initialize(const std::string& baseDir);
    void Shutdown();

    // Real ELF parse+map. After success call ExecuteLoadedGuest().
    bool LoadExecutable(const std::string& path, bool isSelf);

    // Bridges the loaded image into FEXCore and runs it until HLT/exit.
    FexCoreIntegration::ExecResult ExecuteLoadedGuest();

    // Full foundation proof, ordered and stop-on-first-honest-failure:
    //   1. memory window reservation
    //   2. Vulkan runtime enumeration
    //   3. FEXCore context creation (may crash on some devices - logged)
    //   4. raw x86-64 blob: write() -> exit_group(42) -> hlt
    //   5. embedded ELF file round-trip through the REAL loader pipeline
    // Returns multi-line report; last line is always "VERDICT: PASS|FAIL".
    std::string SelfTestFoundation();

    const LoadedElfImage& LoadedImage() const { return m_image; }

    // Legacy accessors used by JNI memory tests (now window-validated).
    uint64_t MapMemory(uint64_t addr, size_t size, uint32_t flags);
    bool     UnmapMemory(uint64_t addr, size_t size);

    bool IsInitializedEvidenceOnly() const { return m_initialized.load(); }
    std::string GetStatusString() const;

private:
    Emulator() = default;
    ~Emulator() = default;

    void EnsureMemoryWindow(std::vector<std::string>& report,
                            bool& ok, bool& fatal);

    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_running{false};
    std::string       m_baseDir;
    std::mutex        m_runMutex;      // one guest execution at a time
    LoadedElfImage    m_image;
};

} // namespace PX5

#endif // PX5_EMULATOR_H
