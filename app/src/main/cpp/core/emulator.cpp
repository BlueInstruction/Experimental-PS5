#include "emulator.h"

#include "memory/memory.h"
#include "kernel/syscalls.h"
#include "kernel/sce_kernel_hle.h"
#include "gpu/vulkan_device.h"
#include "filesystem/vfs.h"
#include "audio/audio.h"
#include "../tests/test_guest.h"
#include "loader/elf_loader.h"
#include "loader/self_fixtures.h"
#include "loader/runtime_linker.h"
#include "loader/runtime_linker_selftest.h"
#include "utils/logger.h"
#include "utils/breadcrumbs.h"
#include "utils/crash_handler.h"
#include "utils/evidence.h"

#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <algorithm>

namespace PX5 {

namespace {

// Resolve a writable location for a scratch fixture.
//
// Every call site used to end its fallback chain with a literal
// "/data/local/tmp/...", directly under a comment explaining that
// /data/local/tmp is NOT writable by app processes. That path can only
// fail, and the failure surfaced as a confusing "fixture write failed"
// instead of naming the real cause. There is no third candidate: if
// neither the VFS base nor the crash-handler log dir is available, say so
// and let the caller report it.
//
// Returns an empty string when no app-writable anchor exists.
std::string ResolveFixturePath(const std::string& baseDir,
                               const char* fileName) {
    if (!baseDir.empty()) {
        return baseDir + "/" + fileName;
    }
    const std::string& logs = CrashHandler::LogsDir();
    if (!logs.empty()) {
        return logs + "/" + fileName;
    }
    return {};
}

}  // namespace

Emulator& Emulator::GetInstance() {
    static Emulator instance;
    return instance;
}

bool Emulator::Initialize(const std::string& baseDir) {
    if (m_initialized.load()) return true;

    PX5_LOGI(LogCategory::CORE, "PX5 foundation core initializing (base=%s)",
             baseDir.c_str());

    if (!MemoryManager::GetInstance().Initialize()) {
        PX5_LOGE(LogCategory::CORE, "Memory window reservation FAILED");
        return false;
    }

    VirtualFileSystem::GetInstance().Initialize(baseDir);
    VulkanGpuDevice::GetInstance().Initialize();   // may fail, reports detail
    // Audio is not implemented; Initialize() returns false and logs why.
    // Startup does not depend on it.
    (void)AudioEngine::GetInstance().Initialize();

    m_baseDir = baseDir;
    m_initialized.store(true);
    PX5_LOGI(LogCategory::CORE, "Foundation core ready. FEXCore is initialized "
                                "on-demand by the self-test / UI.");
    return true;
}

void Emulator::Shutdown() {
    if (!m_initialized.exchange(false)) return;
    FexCoreIntegration::Shutdown();
    VulkanGpuDevice::GetInstance().Shutdown();
    MemoryManager::GetInstance().Shutdown();
    m_running.store(false);
    PX5_LOGI(LogCategory::CORE, "Core shutdown complete");
}

bool Emulator::LoadExecutable(const std::string& path, bool isSelf) {
    std::lock_guard<std::mutex> lock(m_runMutex);
    // Stage breadcrumbs: a crash anywhere below now names the exact stage
    // in the dump (the 2026-08-30 eboot.bin death left the stage unknown).
    Breadcrumb::Set("load: enter isSelf=%d", isSelf ? 1 : 0);
    // v1.41 — this is the REAL-GUEST entry point (the UI only calls it for
    // a user-selected game executable). From here every loader line is
    // tagged [GUEST] and the evidence ledger records the chain. The
    // foundation suite flips the flag back to SYNTH at its own entry.
    Evidence::SetSessionRealGuest(true);
    if (!CrashHandler::LogsDir().empty())
        Evidence::SetLedgerPath(CrashHandler::LogsDir() + "/px5_evidence.log");
    // Lazy memory init keeps legacy JNI callers working without UI flow.
    if (!MemoryManager::GetInstance().Initialize()) {
        Breadcrumb::Set("load: memmgr init FAILED");
        return false;
    }
    Breadcrumb::Set("load: memmgr ready");
    if (m_baseDir.empty()) {
        // /data/local/tmp is not writable by app processes, so it is not a
        // fallback -- it is just a slower failure. Anchor the VFS under the
        // crash-handler logs dir (wired at startup, app-writable by
        // construction); if that is unavailable, report it instead of
        // pointing the VFS at a path that cannot work.
        const std::string& logs = CrashHandler::LogsDir();
        if (logs.empty()) {
            PX5_LOGE(LogCategory::CORE,
                     "No app-writable directory available (crash-handler logs "
                     "dir is unset): cannot anchor the guest VFS");
            Breadcrumb::Set("load: no writable VFS anchor");
            return false;
        }
        m_baseDir = logs + "/vfs";
        VirtualFileSystem::GetInstance().Initialize(m_baseDir);
    }

    Breadcrumb::Set("load: %s dispatch", isSelf ? "SELF" : "ELF");
    const bool ok = isSelf ? ElfLoader::LoadSelf(path, m_image)
                           : ElfLoader::LoadElfFile(path, m_image);
    Breadcrumb::Set("load: done ok=%d", ok ? 1 : 0);
    PX5_LOGI(LogCategory::CORE, "[GUEST] %s load: %s",
             isSelf ? "SELF" : "ELF", ok ? "OK" : m_image.error.c_str());
    if (ok) {
        // v1.41 — ledger: the load event references the hash the loader
        // just bound, so every later ledger line chains to the file.
        Evidence::AppendLedger("load ok path=%s entry=0x%llx segs=%zu",
                               path.c_str(),
                               (unsigned long long)m_image.entryPoint,
                               m_image.segments.size());
    }
    return ok;
}

FexCoreIntegration::ExecResult Emulator::ExecuteLoadedGuest() {
    std::lock_guard<std::mutex> lock(m_runMutex);
    FexCoreIntegration::ExecResult res;
    if (m_image.segments.empty() || m_image.entryPoint == 0) {
        res.error = "no image loaded";
        return res;
    }

    auto& mm = MemoryManager::GetInstance();

    // Guest stack INSIDE the canonical window (64 KiB below the image top
    // region chosen by the loader); host bridge converts it.
    constexpr uint64_t kStackGuestVa = 0x148000000ULL;   // inside window
    const size_t pageSize = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    const uint64_t stackLo = kStackGuestVa - pageSize * 16;
    mm.MapMemory(stackLo, pageSize * 16,
                 MemoryFlags::PAGE_READ | MemoryFlags::PAGE_WRITE,
                 "guest_stack");

    void* ripHost = mm.GetHostPointer(m_image.entryPoint);
    void* spHost  = mm.GetHostPointer(kStackGuestVa - 256);   // arg area headroom

    if (!ripHost || !spHost) {
        res.error = "host bridge failed for entry/stack";
        return res;
    }

    // v1.39 — REAL initial stack (FEXLoader ELFCodeLoader::SetupStack
    // layout). The vc39 session proved the guest RUNS on device, then died
    // doing an atomic on address 0: [rsp] was a zeroed page, so the PS5 crt
    // read argc=0/argv=NULL and no auxv at all. Layout mirrors the reference:
    //   [sp+0]  argc                     (8)
    //   [sp+8]  argv[0..n-1] pointers    (8 each)
    //           argv NULL                (8)  -- folded into the "pad" slot
    //           envp NULL                (8)
    //           auxv pairs               (16 each, AT_NULL terminated)
    //           argv[0] string, platform string ("x86_64")
    //           (16-byte align), AT_RANDOM 16 bytes
    // ELF facts (AT_PHDR/AT_PHNUM) are read from the MAPPED image header,
    // not from a cached parse — the report stays honest to what is mapped.
    {
        struct AuxPair { uint64_t type; uint64_t val; };
        std::vector<AuxPair> aux;
        auto pushAux = [&aux](uint64_t t, uint64_t v) { aux.push_back({t, v}); };
        constexpr uint64_t AT_NULL_ = 0, AT_PHDR_ = 3, AT_PHENT_ = 4,
                           AT_PHNUM_ = 5, AT_PAGESZ_ = 6, AT_BASE_ = 7,
                           AT_FLAGS_ = 8, AT_ENTRY_ = 9, AT_UID_ = 11,
                           AT_EUID_ = 12, AT_GID_ = 13, AT_EGID_ = 14,
                           AT_HWCAP_ = 16, AT_CLKTCK_ = 17, AT_SECURE_ = 23,
                           AT_RANDOM_ = 25, AT_EXECFN_ = 31,
                           AT_PLATFORM_ = 24, AT_HWCAP2_ = 26;
        pushAux(AT_UID_,    static_cast<uint64_t>(getuid()));
        pushAux(AT_EUID_,   static_cast<uint64_t>(geteuid()));
        pushAux(AT_GID_,    static_cast<uint64_t>(getgid()));
        pushAux(AT_EGID_,   static_cast<uint64_t>(getegid()));
        pushAux(AT_CLKTCK_, 100);
        pushAux(AT_PAGESZ_, static_cast<uint64_t>(pageSize));
        pushAux(AT_RANDOM_, 0);   // patched below
        pushAux(AT_SECURE_, 0);
        pushAux(AT_FLAGS_,  0);
        pushAux(AT_HWCAP_,  0);   // no x86 feature hints — generic paths
        pushAux(AT_HWCAP2_, 0);
        pushAux(AT_PLATFORM_, 0); // patched below
        pushAux(AT_EXECFN_, 0);   // patched below
        pushAux(AT_PHENT_,  0x38);
        pushAux(AT_PHDR_,   0);   // patched below
        pushAux(AT_PHNUM_,  0);   // patched below
        pushAux(AT_BASE_,   0);   // no interpreter — PS5 modules are prelinked
        pushAux(AT_ENTRY_,  m_image.entryPoint);
        pushAux(AT_NULL_,   0);

        // v1.40 — ELF facts from PARSE TRUTH (the loader stores them in
        // the image struct). The v1.39 approach re-read the mapped header
        // at imageLowVa+0x20/+0x38 — but SELF-extracted images map
        // segment CONTENT only, so those reads returned TEXT BYTES and
        // the vc40 log showed the lie in plain sight:
        //   AT_PHDR=0x4c1e73018942ab1d phnum=8412870648271685644
        // AT_PHDR points at the loader's phdr-table copy in guest VA
        // (auxPhdrVa) — a real mapping the crt can actually iterate.
        const uint64_t atPhdr = m_image.auxPhdrVa;
        const uint64_t atPhnum = m_image.phnum;
        for (auto& a : aux) {
            if (a.type == AT_PHDR_ && atPhdr)  a.val = atPhdr;
            if (a.type == AT_PHNUM_ && atPhnum) a.val = atPhnum;
        }

        const char* argv0 = "eboot.bin";
        const char* platform = "x86_64";
        const size_t argv0Len  = strlen(argv0) + 1;
        const size_t platLen   = strlen(platform) + 1;

        size_t ofArgs   = 8;                       // argc
        size_t ofEnvp   = ofArgs + 8 + 8;          // argv[0] ptr + argv NULL pad
        size_t ofAux    = ofEnvp + 8;              // envp NULL
        size_t ofStrA   = ofAux + aux.size() * 16; // argv string
        size_t ofStrP   = ofStrA + argv0Len;       // platform string
        size_t total    = ofStrP + platLen;
        total = (total + 15) & ~size_t{15};
        size_t ofRandom = total;
        total += 16;

        // Reserve headroom below the arg block for crt pushes, then build.
        const uint64_t argBlockVa = (kStackGuestVa - 256 - total) & ~uint64_t{15};
        void* blockHost = mm.GetHostPointer(argBlockVa);
        if (!blockHost) {
            res.error = "guest stack bridge failed for arg block";
            return res;
        }
        auto* base = static_cast<uint8_t*>(blockHost);
        auto write64 = [base](size_t off, uint64_t v) {
            memcpy(base + off, &v, 8);
        };
        write64(0, 1);                                       // argc = 1
        write64(ofArgs, argBlockVa + ofStrA);                // argv[0]
        write64(ofArgs + 8, 0);                              // argv NULL
        write64(ofEnvp, 0);                                  // envp NULL
        const uint64_t randomVa = argBlockVa + ofRandom;
        for (size_t i = 0; i < aux.size(); ++i) {
            uint64_t val = aux[i].val;
            if (aux[i].type == AT_RANDOM_)   val = randomVa;
            if (aux[i].type == AT_PLATFORM_) val = argBlockVa + ofStrP;
            if (aux[i].type == AT_EXECFN_)   val = argBlockVa + ofStrA;
            write64(ofAux + i * 16,      aux[i].type);
            write64(ofAux + i * 16 + 8,  val);
        }
        memcpy(base + ofStrA, argv0, argv0Len);
        memcpy(base + ofStrP, platform, platLen);
        // AT_RANDOM payload: fixed but non-null (crt treats NULL as fatal).
        uint64_t rnd[2] = {0x5058352D4F4BULL, 0}; // "PX5-OK" marker + 0
        memcpy(base + ofRandom, rnd, sizeof rnd);

        spHost = mm.GetHostPointer(argBlockVa);
        if (!spHost) {
            res.error = "host bridge failed for initial stack";
            return res;
        }
        PX5_LOGI(LogCategory::CORE,
                 "[GUEST] Initial guest stack: sp=0x%llx argc=1 auxv=%zu entries "
                 "AT_ENTRY=0x%llx AT_PHDR=0x%llx phnum=%llu",
                 (unsigned long long)argBlockVa, aux.size(),
                 (unsigned long long)m_image.entryPoint,
                 (unsigned long long)atPhdr,
                 (unsigned long long)atPhnum);
    }

    // v1.40 — kernel-side TLS/TCB setup, the ORBIS entry contract. The
    // vc40 session proved the real eboot's first guest block reads
    // through FS with the base still zero (LDAR w3,[x11], x11=0,
    // fs_base=0x0, zero arch_prctl calls): PS5 binaries expect the
    // "kernel" to have built the TCB and pointed FS at it BEFORE the
    // entry instruction. FreeBSD/ORBIS amd64 layout (variant II):
    //   [TLS init image (PT_TLS, filesz copied / memsz zeroed)]
    //   [pad to 16]
    //   [TCB: tp+0 = tp (tcb_self), tp+8 = dtv, tp+16 = pthread (null)]
    //   [dtv: [0]=1 count/gen, [1]=static block, [2]=0 terminator]
    // tp (the TCB address) becomes the guest FSBASE.
    uint64_t guestFsBase = 0;
    {
        constexpr uint64_t kTlsGuestVa = 0x147fe0000ULL; // below the stack
        const size_t tlsRegionSize = 0x10000;
        if (!mm.MapMemory(kTlsGuestVa, tlsRegionSize,
                          MemoryFlags::PAGE_READ | MemoryFlags::PAGE_WRITE,
                          "guest_tls")) {
            res.error = "guest TLS block map failed at 0x147fe0000";
            return res;
        }

        const size_t imageBytes = m_image.hasTls
            ? std::min(m_image.tlsMemsz, size_t{0xF000}) : 0;
        const size_t imageAligned = imageBytes ? (imageBytes + 15) & ~size_t{15} : 0;
        const uint64_t tcbVa = kTlsGuestVa + imageAligned;
        const uint64_t dtvVa = tcbVa + 32;

        auto* tlsHost = static_cast<uint8_t*>(mm.GetHostPointer(kTlsGuestVa));
        if (!tlsHost) {
            res.error = "host bridge failed for guest TLS block";
            return res;
        }
        if (m_image.hasTls && imageBytes) {
            if (!mm.ReadGuestMemory(m_image.tlsVa, tlsHost, m_image.tlsFilesz)) {
                PX5_LOGW(LogCategory::CORE,
                         "Guest TLS init image read FAILED at 0x%llx — TCB "
                         "still built, PT_TLS content left zeroed",
                         (unsigned long long)m_image.tlsVa);
            }
            // memsz > filesz tail stays zeroed (anonymous mapping).
        }
        auto write64At = [tlsHost](uint64_t off, uint64_t v) {
            memcpy(tlsHost + off, &v, 8);
        };
        write64At(imageAligned + 0,  tcbVa);        // tcb_self = tp
        write64At(imageAligned + 8,  dtvVa);        // tcb_dtv
        write64At(imageAligned + 16, 0);            // tcb_pthread (null)
        write64At(imageAligned + 32, 1);            // dtv[0] = count/gen
        write64At(imageAligned + 40, kTlsGuestVa);  // dtv[1] = static block
        write64At(imageAligned + 48, 0);            // dtv[2] = terminator
        guestFsBase = tcbVa;
        PX5_LOGI(LogCategory::CORE,
                 "[GUEST] Guest TLS: fs_base=0x%llx tcb=0x%llx dtv=0x%llx "
                 "(PT_TLS %s: va=0x%llx filesz=%zu memsz=%zu)",
                 (unsigned long long)guestFsBase,
                 (unsigned long long)tcbVa, (unsigned long long)dtvVa,
                 m_image.hasTls ? "present" : "absent — bare TCB",
                 (unsigned long long)m_image.tlsVa,
                 m_image.tlsFilesz, m_image.tlsMemsz);
    }

    m_running.store(true);
    // v1.41 — the dispatch is on the record before it happens: RIP/SP/FS
    // land in the ledger chained to the bound image hash.
    Evidence::AppendLedger(
        "dispatch enter rip=0x%llx sp=0x%llx fs_base=0x%llx",
        (unsigned long long)m_image.entryPoint,
        (unsigned long long)kStackGuestVa - 256,
        (unsigned long long)guestFsBase);
    res = FexCoreIntegration::ExecuteAtHostRip(reinterpret_cast<uint64_t>(ripHost),
                                               reinterpret_cast<uint64_t>(spHost),
                                               guestFsBase);
    m_running.store(false);
    // v1.41 — the result (clean exit, guest trap or host crash escape) is
    // a ledger event too: the reviewer reads the whole session from one
    // file without trusting the verbose log's interpretation.
    Evidence::AppendLedger(
        "dispatch result started=%d exit=%llu clean=%d error=%s",
        res.started ? 1 : 0, (unsigned long long)res.exitCode,
        res.error.empty() ? 1 : 0,
        res.error.empty() ? "none" : res.error.c_str());
    PX5_LOGI(LogCategory::CORE,
             "[GUEST] dispatch result: started=%d exitCode=%llu %s",
             res.started ? 1 : 0, (unsigned long long)res.exitCode,
             res.error.empty() ? "(no error text)" : res.error.c_str());

    // Release the stack region again for reproducibility across runs.
    mm.UnmapMemory(stackLo, pageSize * 16);
    return res;
}

uint64_t Emulator::MapMemory(uint64_t addr, size_t size, uint32_t flags) {
    return MemoryManager::GetInstance().MapMemory(addr, size, flags, "JNI");
}

bool Emulator::UnmapMemory(uint64_t addr, size_t size) {
    return MemoryManager::GetInstance().UnmapMemory(addr, size);
}

std::string Emulator::GetStatusString() const {
    if (!m_initialized.load()) return "Uninitialized";
    if (m_running.load())      return "Running guest (FEXCore x86_64 -> ARM64)";
    return "Ready (foundation core)";
}

// ---------------------------------------------------------------------------
// SelfTestFoundation — ordered end-to-end self-test of the engine pieces.
// ---------------------------------------------------------------------------
void Emulator::EnsureMemoryWindow(std::vector<std::string>& report,
                                  bool& ok, bool& fatal) {
    (void)ok;
    if (!MemoryManager::GetInstance().Initialize(256)) {
        report.push_back("[FAIL] 1. Memory window reservation");
        fatal = true;
        return;
    }
    report.push_back(std::string("[PASS] 1. Memory window | ") +
                     MemoryManager::GetInstance().GetWindowInfoString());
}

std::string Emulator::SelfTestFoundation() {
    std::lock_guard<std::mutex> lock(m_runMutex);
    std::vector<std::string> lines;
    const char* verdict = "FAIL";

    // v1.41 — the trust review demanded the log itself separate synthetic
    // test activity from real-guest activity. This suite is SYNTHETIC by
    // definition: fixtures built into the app, never game code. Every line
    // it logs carries [SYNTH]; the real-guest path (LoadExecutable) flips
    // the shared flag to [GUEST].
    Evidence::SetSessionRealGuest(false);
    if (!CrashHandler::LogsDir().empty())
        Evidence::SetLedgerPath(CrashHandler::LogsDir() + "/px5_evidence.log");

    auto push = [&](const std::string& s){ lines.push_back(s);
        PX5_LOGI(LogCategory::CORE, "[SYNTH] %s", s.c_str()); };

    bool ok = true, fatal = false;

    // --- Step 1: memory -------------------------------------------------
    EnsureMemoryWindow(lines, ok, fatal);
    if (fatal) { goto done; }

    // --- Step 2: Vulkan --------------------------------------------------
    {
        auto& gpu = VulkanGpuDevice::GetInstance();
        const bool vok = gpu.Initialize();
        lines.push_back(std::string(vok ? "[PASS] 2. Vulkan runtime | "
                                        : "[FAIL] 2. Vulkan runtime | ") +
                        gpu.GetSummaryString());
        // Non-fatal: CI/emulators sometimes lack a real GPU driver.
    }

    // --- Step 3: FEXCore context ----------------------------------------
    if (!FexCoreIntegration::Initialize()) {
        lines.push_back("[FAIL] 3. FEXCore context (see logcat FEX tag)");
        goto done;
    }
    lines.push_back("[PASS] 3. FEXCore context + syscall bridge");

    // --- Step 4: raw blob write/exit/hlt --------------------------------
    {
        constexpr uint64_t kRawVa = 0x140000000ULL;              // window anchor
        constexpr uint64_t kRawStackTop = 0x148000000ULL;         // inside window
        auto& mm = MemoryManager::GetInstance();
        const size_t ps = static_cast<size_t>(sysconf(_SC_PAGESIZE));

        mm.MapMemory(kRawVa, sizeof(TEST_GUEST_RAW_CODE),
                     MemoryFlags::PAGE_READ | MemoryFlags::PAGE_WRITE |
                     MemoryFlags::PAGE_EXEC, "raw_test");
        memcpy(mm.GetHostPointer(kRawVa), TEST_GUEST_RAW_CODE,
               sizeof(TEST_GUEST_RAW_CODE));
        mm.MapMemory(kRawStackTop - ps * 4, ps * 4,
                     MemoryFlags::PAGE_READ | MemoryFlags::PAGE_WRITE,
                     "raw_stack");

        auto r = FexCoreIntegration::ExecuteAtHostRip(
            reinterpret_cast<uint64_t>(mm.GetHostPointer(kRawVa)),
            reinterpret_cast<uint64_t>(mm.GetHostPointer(kRawStackTop)));

        const bool rawOk = r.started && r.output.find("PX5-OK!") !=
                                                       std::string::npos &&
                           r.exitCode == TEST_GUEST_EXPECTED_EXIT_CODE;
        if (!rawOk) { ok = false; r.error = "raw run did not meet the pass contract"; }
        lines.push_back(std::string(rawOk ? "[PASS] 4. RAW x86-64 | write+exit42+halt "
                                          : "[FAIL] 4. RAW x86-64 | ") +
                        "| out=\"" + r.output.substr(0, 64) + "\"" +
                        "| exitCode=" + std::to_string(r.exitCode) +
                        "| " + (r.error.empty() ? "" : r.error));
        mm.UnmapMemory(kRawVa, sizeof(TEST_GUEST_RAW_CODE));
        mm.UnmapMemory(kRawStackTop - ps * 4, ps * 4);
        if (!rawOk) goto done;
    }

    // --- Step 5: ELF file round-trip through real loader -----------------
    {
        auto& mm = MemoryManager::GetInstance();
        const std::string elfPath =
            ResolveFixturePath(m_baseDir, "px5_guest.elf");
        if (elfPath.empty()) {
            lines.push_back("[FAIL] 5. no app-writable dir for the ELF fixture");
            goto done;
        }

        std::ofstream f(elfPath, std::ios::binary | std::ios::trunc);
        f.write(reinterpret_cast<const char*>(TEST_GUEST_ELF),
                TEST_GUEST_ELF_SIZE);
        const bool wroteFixture = f.good();
        f.close();
        if (!wroteFixture) {
            lines.push_back("[FAIL] 5. ELF fixture write failed (" + elfPath + ")");
            goto done;
        }

        LoadedElfImage img;
        if (!ElfLoader::LoadElfFile(elfPath, img)) {
            lines.push_back("[FAIL] 5. loader: " + img.error);
            goto done;
        }
        if (img.entryPoint != TEST_GUEST_LOAD_VADDR ||
            img.segments.size() != 1) {
            // v1.29: the fixture's e_entry is now the true code VA (the
            // old generator double-counted p_offset and the vc29 session
            // executed zeros past the image — the app-killing SIGSEGV).
            lines.push_back("[FAIL] 5. loader metadata mismatch");
            goto done;
        }

        void* ripHost = mm.GetHostPointer(img.entryPoint);
        constexpr uint64_t kElfStackTop = 0x148000000ULL;
        const size_t ps = static_cast<size_t>(sysconf(_SC_PAGESIZE));
        mm.MapMemory(kElfStackTop - ps * 4, ps * 4,
                     MemoryFlags::PAGE_READ | MemoryFlags::PAGE_WRITE,
                     "elf_stack");
        void* spHost = mm.GetHostPointer(kElfStackTop);

        auto r = FexCoreIntegration::ExecuteAtHostRip(
            reinterpret_cast<uint64_t>(ripHost),
            reinterpret_cast<uint64_t>(spHost));

        const bool elfOk =
            r.started && r.exitCode == 42 &&
            r.output.find(TEST_GUEST_EXPECTED_OUTPUT) != std::string::npos;

        lines.push_back(std::string(elfOk ? "[PASS] 5. ELF pipeline | parse->map->bridge->JIT->write+exit42 "
                                          : "[FAIL] 5. ELF pipeline |") +
                        " out=\"" + r.output.substr(0, 48) + "\"" +
                        " exit=" + std::to_string(r.exitCode));
        if (!elfOk) goto done;
    }

    // --- Step 5b: SELF container -> real LoadSelf -> guest execution -----
    // Phase C milestone 3: the SAME synthetic container layout the
    // extractor self-test validates wraps the REAL executable guest
    // (TEST_GUEST_ELF_V2). The container goes through the production
    // path only: ElfLoader::LoadSelf -> SelfExtract::ExtractInnerElf ->
    // LoadElfFromMemory -> FEXCore ExecuteThread. No shortcut re-parses
    // the inner ELF outside the loader, so this step is the first
    // end-to-end proof that a SELF-carried image runs, not merely parses.
    {
        auto& mm = MemoryManager::GetInstance();
        const std::string selfPath =
            ResolveFixturePath(m_baseDir, "px5_guest.self");
        if (selfPath.empty()) {
            lines.push_back("[FAIL] 5b. no app-writable dir for the SELF fixture");
            goto done;
        }

        const std::vector<uint8_t> elfFile(
            TEST_GUEST_ELF_V2, TEST_GUEST_ELF_V2 + TEST_GUEST_ELF_V2_SIZE);
        // v1.29: the container is built in the orbis layout shadPS4's
        // production parser uses (self header + segment table + inner
        // ELF header/phdrs + payloads) — the same shape a real dump
        // carries, from one shared builder with the extractor self-test.
        const auto built = SelfFixtures::BuildSelfFromWholeElf(
            elfFile, {SelfExtract::kSegFlagSigned});
        if (built.bytes.empty()) {
            lines.push_back("[FAIL] 5b. SELF fixture build returned empty");
            goto done;
        }

        std::ofstream f(selfPath, std::ios::binary | std::ios::trunc);
        f.write(reinterpret_cast<const char*>(built.bytes.data()),
                static_cast<std::streamsize>(built.bytes.size()));
        const bool wroteFixture = f.good();
        f.close();
        if (!wroteFixture) {
            lines.push_back("[FAIL] 5b. SELF fixture write failed (" +
                            selfPath + ")");
            goto done;
        }

        LoadedElfImage img;
        if (!ElfLoader::LoadSelf(selfPath, img)) {
            lines.push_back("[FAIL] 5b. SELF->loader: " + img.error);
            goto done;
        }
        if (!img.isSelf || img.segments.empty() || img.entryPoint == 0) {
            lines.push_back("[FAIL] 5b. SELF metadata invalid (isSelf/entry/segments)");
            goto done;
        }

        const size_t ps5b = static_cast<size_t>(sysconf(_SC_PAGESIZE));
        constexpr uint64_t kSelfStackTop = 0x148000000ULL;
        mm.MapMemory(kSelfStackTop - ps5b * 4, ps5b * 4,
                     MemoryFlags::PAGE_READ | MemoryFlags::PAGE_WRITE,
                     "self_stack");

        GuestSyscalls::ResetRun();
        void* ripHost5b = mm.GetHostPointer(img.entryPoint);
        void* spHost5b  = mm.GetHostPointer(kSelfStackTop - 256);
        if (!ripHost5b || !spHost5b) {
            lines.push_back("[FAIL] 5b. SELF host bridge missing");
            goto done;
        }
        auto r5b = FexCoreIntegration::ExecuteAtHostRip(
            reinterpret_cast<uint64_t>(ripHost5b),
            reinterpret_cast<uint64_t>(spHost5b));

        mm.UnmapMemory(kSelfStackTop - ps5b * 4, ps5b * 4);

        const bool selfOk =
            r5b.started && r5b.exitCode == TEST_GUEST_V2_EXIT_OK &&
            r5b.output.find(TEST_GUEST_V2_EXPECTED_OUTPUT) != std::string::npos;
        lines.push_back(std::string(
                selfOk ? "[PASS] 5b. SELF container pipeline | extract->map->JIT->exec "
                       : "[FAIL] 5b. SELF container pipeline | ") +
                "out=\"" + r5b.output.substr(0, 40) + "\"" +
                " exit=" + std::to_string(r5b.exitCode));
        if (!selfOk) goto done;
    }

    // --- Step 6: ADVANCED ELF v2 — real mmap + memory round-trip --------
    {
        auto& mm = MemoryManager::GetInstance();
        const std::string elfPath =
            ResolveFixturePath(m_baseDir, "px5_guest_v2.elf");
        if (elfPath.empty()) {
            lines.push_back("[FAIL] 6. no app-writable dir for the ELFv2 fixture");
            goto done;
        }

        std::ofstream f(elfPath, std::ios::binary | std::ios::trunc);
        f.write(reinterpret_cast<const char*>(TEST_GUEST_ELF_V2),
                TEST_GUEST_ELF_V2_SIZE);
        f.close();
        if (!f.good()) { lines.push_back("[FAIL] 6. ELFv2 fixture write"); goto done; }

        LoadedElfImage img;
        if (!ElfLoader::LoadElfFile(elfPath, img)) {
            lines.push_back("[FAIL] 6. ELFv2 load: " + img.error);
            goto done;
        }

        const size_t ps = static_cast<size_t>(sysconf(_SC_PAGESIZE));
        constexpr uint64_t kStackTop2 = 0x148000000ULL;
        mm.MapMemory(kStackTop2 - ps * 4, ps * 4,
                     MemoryFlags::PAGE_READ | MemoryFlags::PAGE_WRITE,
                     "elf2_stack");

        GuestSyscalls::ResetRun();
        void* ripHost = mm.GetHostPointer(img.entryPoint);
        void* spHost  = mm.GetHostPointer(kStackTop2 - 256);
        if (!ripHost || !spHost) {
            lines.push_back("[FAIL] 6. ELFv2 host bridge missing");
            goto done;
        }
        auto r = FexCoreIntegration::ExecuteAtHostRip(
            reinterpret_cast<uint64_t>(ripHost),
            reinterpret_cast<uint64_t>(spHost));

        mm.UnmapMemory(kStackTop2 - ps * 4, ps * 4);

        const bool v2ok =
            r.started && r.exitCode == TEST_GUEST_V2_EXIT_OK &&
            r.output.find(TEST_GUEST_V2_EXPECTED_OUTPUT) != std::string::npos;
        lines.push_back(std::string(
                v2ok ? "[PASS] 6. ELFv2 mmap round-trip | "
                     : "[FAIL] 6. ELFv2 mmap | ") +
                "out=\"" + r.output.substr(0, 40) + "\"" +
                " exit=" + std::to_string(r.exitCode));
        if (!v2ok) goto done;
    }

    // --- Step 7: guest synchronous trap (ud2) routed, app survives ------
    // Proves the FEX frontend signal contract on Android end-to-end:
    // guest ud2 -> FEXCore SynchronousFaultData + dispatcher GuestSignal
    // block -> host SIGILL -> PX5 fault router -> clean unwind. Before this
    // routing existed, a guest ud2 KILLED the whole app as a "native crash"
    // with a confusing report (the Let's Build A Zoo failure class).
    {
        auto& mm = MemoryManager::GetInstance();
        const size_t ps = static_cast<size_t>(sysconf(_SC_PAGESIZE));
        constexpr uint64_t kTrapVa = 0x140000000ULL + 0x00400000ULL;
        mm.MapMemory(kTrapVa, TEST_GUEST_UD2_SIZE,
                     MemoryFlags::PAGE_READ | MemoryFlags::PAGE_WRITE |
                     MemoryFlags::PAGE_EXEC, "ud2_test");
        memcpy(mm.GetHostPointer(kTrapVa), TEST_GUEST_UD2_CODE,
               TEST_GUEST_UD2_SIZE);
        constexpr uint64_t kTrapStackTop = 0x148000000ULL;
        mm.MapMemory(kTrapStackTop - ps * 4, ps * 4,
                     MemoryFlags::PAGE_READ | MemoryFlags::PAGE_WRITE,
                     "ud2_stack");

        auto r = FexCoreIntegration::ExecuteAtHostRip(
            reinterpret_cast<uint64_t>(mm.GetHostPointer(kTrapVa)),
            reinterpret_cast<uint64_t>(mm.GetHostPointer(kTrapStackTop)));

        mm.UnmapMemory(kTrapVa, TEST_GUEST_UD2_SIZE);
        mm.UnmapMemory(kTrapStackTop - ps * 4, ps * 4);

        // We are still executing — that IS the app-survival proof. The
        // contract also demands the trap be recorded with exact values,
        // not a generic "something faulted".
        const bool trapOk = r.started && r.guestTrap.fired &&
                            r.guestTrap.signal  == TEST_GUEST_UD2_SIGNAL &&
                            r.guestTrap.trapNo  == TEST_GUEST_UD2_TRAPNO &&
                            r.guestTrap.siCode  == TEST_GUEST_UD2_SICODE;
        char trapLine[160];
        snprintf(trapLine, sizeof(trapLine),
                 "%s 7. Guest trap routing | ud2 -> %s | signal=%u trapNo=%u "
                 "si_code=%u guestRIP=0x%llx | app survived",
                 trapOk ? "[PASS]" : "[FAIL]",
                 r.guestTrap.fired ? "routed+unwound" : "NOT ROUTED",
                 (unsigned)r.guestTrap.signal, (unsigned)r.guestTrap.trapNo,
                 (unsigned)r.guestTrap.siCode,
                 (unsigned long long)r.guestTrap.guestRip);
        push(trapLine);
        if (!trapOk) goto done;
    }

    // --- Step 8: GPU logical device + offscreen submission proof --------
    {
        std::string gpuDetail;
        const bool gok = VulkanGpuDevice::GetInstance()
                             .RunOffscreenClearProof(gpuDetail);
        lines.push_back(std::string(gok ? "[PASS] 8. GPU submission | "
                                        : "[FAIL] 8. GPU submission | ") +
                        gpuDetail);
        if (!gok) goto done;   // honest: real device must submit commands
    }

    // --- Step 9: libkernel HLE DirectMemory exercise ---------------------
    {
        auto& kHle = SceKernelHle::KernelHle::GetInstance();
        kHle.RegisterAll();

        uint64_t phys = 0;
        uint64_t rc = kHle.AllocateDirectMemory(0, 0, 4096, 4096,
                                                reinterpret_cast<uint64_t>(&phys));
        if (rc != SceKernelHle::SCE_OK) {
            lines.push_back("[FAIL] 9. libkernel DM allocate rc=" + std::to_string(rc));
            goto done;
        }
        const uint64_t mapped =
            kHle.MapDirectMemory(0, 4096, 0x3 /*RW*/, 0, phys, 0);
        bool hleOk = false;
        auto& mm2 = MemoryManager::GetInstance();
        if (mapped && mm2.IsValidAddress(mapped, 4)) {
            constexpr uint32_t kMagic = 0x50583544u;   // "PX5D"
            uint32_t probe = 0;
            hleOk = mm2.WriteGuestMemory(mapped, &kMagic, 4) &&
                    mm2.ReadGuestMemory(mapped, &probe, 4) &&
                    probe == kMagic;
            kHle.UnmapDirectMemory(mapped, 4096);
        }
        // Exercise the named-symbol path through the captured console too.
        GuestSyscalls::ResetRun();
        char tag[] = "SCE-HLE\n";
        const uint64_t wrc = kHle.InvokeByName("sceKernelWrite",
                                               /*fd*/1,
                                               reinterpret_cast<uint64_t>(tag),
                                               sizeof(tag));
        hleOk = hleOk && wrc == sizeof(tag) &&
                GuestSyscalls::TakeOutput().find("SCE-HLE") != std::string::npos;

        lines.push_back(std::string(hleOk ? "[PASS] 9. libkernel HLE | "
                                          : "[FAIL] 9. libkernel HLE | ") +
                        kHle.GetSummaryString() +
                        " phys=0x" + ([&]{ char b[24];
                            snprintf(b,sizeof(b),"%llx",(unsigned long long)phys); return std::string(b);}()));
        if (!hleOk) goto done;
    }

    // --- Step 10: NID gate — first PS5-style import call lands on a
    // bionic-native HLE function (v1.31). Guest bytes call the reserved
    // gate syscall; GuestSyscalls routes to RuntimeLinker; the registered
    // HLE function returns 7+35=42; the guest exits with it. This is the
    // seam every future libSce* HLE module plugs into.
    {
        auto& mm = MemoryManager::GetInstance();
        const size_t ps10 = static_cast<size_t>(sysconf(_SC_PAGESIZE));
        auto& rl = RuntimeLinker::GetInstance();
        rl.Reset();
        rl.RegisterHleExport("libpx5test", kGateTestNid, "test_gate_add",
            [](const uint64_t* args, size_t argc) -> int64_t {
                int64_t s = 0;
                for (size_t i = 0; i < argc; ++i) s += (int64_t)args[i];
                return s;
            });

        constexpr uint64_t kGateVa = 0x140000000ULL + 0x00800000ULL;
        constexpr uint64_t kGateStackTop = 0x148000000ULL;
        mm.MapMemory(kGateVa, kNidGateStubSize,
                     MemoryFlags::PAGE_READ | MemoryFlags::PAGE_WRITE |
                     MemoryFlags::PAGE_EXEC, "nid_gate_stub");
        memcpy(mm.GetHostPointer(kGateVa), kNidGateStubCode, kNidGateStubSize);
        mm.MapMemory(kGateStackTop - ps10 * 4, ps10 * 4,
                     MemoryFlags::PAGE_READ | MemoryFlags::PAGE_WRITE,
                     "nid_gate_stack");

        GuestSyscalls::ResetRun();
        auto r10 = FexCoreIntegration::ExecuteAtHostRip(
            reinterpret_cast<uint64_t>(mm.GetHostPointer(kGateVa)),
            reinterpret_cast<uint64_t>(mm.GetHostPointer(kGateStackTop)));

        mm.UnmapMemory(kGateVa, kNidGateStubSize);
        mm.UnmapMemory(kGateStackTop - ps10 * 4, ps10 * 4);

        const auto& gst = rl.Stats();
        const bool gateOk = r10.started && r10.exitCode == kGateExpectedExit &&
                            gst.gateCalls == 1 && gst.resolvedHle == 1;
        char gline[192];
        snprintf(gline, sizeof(gline),
                 "%s 10. NID gate | guest->gate-syscall->bionic HLE->exit%llu "
                 "| nid=0x%08llx gateCalls=%llu resolvedHle=%llu",
                 gateOk ? "[PASS]" : "[FAIL]",
                 (unsigned long long)r10.exitCode,
                 (unsigned long long)kGateTestNid,
                 (unsigned long long)gst.gateCalls,
                 (unsigned long long)gst.resolvedHle);
        push(gline);
        if (!gateOk) goto done;
    }

    // --- Step 11: the evidence primitive itself (v1.41). The foundation
    // suite's SHA-256 must be proven on-device BEFORE any image hash can
    // be trusted: FIPS 180-4 known-answer vectors, including the 55-byte
    // padding boundary and a full second block.
    {
        const bool shaOk = Evidence::SelfTest();
        char sline[128];
        snprintf(sline, sizeof(sline),
                 "%s 11. SHA-256 evidence primitive | FIPS 180-4 KAT "
                 "(empty/abc/55B/64B)",
                 shaOk ? "[PASS]" : "[FAIL]");
        push(sline);
        if (!shaOk) { ok = false; goto done; }
    }

    verdict = "PASS";
done:
    lines.push_back(FexCoreIntegration::GetSyscallStatsString());
    lines.push_back(std::string("VERDICT: ") + verdict);

    std::string report;
    for (size_t i = 0; i < lines.size(); ++i)
        report += (i ? "\n" : "") + lines[i];
    return report;
}

} // namespace PX5
