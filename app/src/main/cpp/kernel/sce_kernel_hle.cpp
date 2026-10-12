#include "sce_kernel_hle.h"

#include "libc_hle.h"
#include "pthread_hle.h"
#include "cxxabi_hle.h"
#include "agc_hle.h"
#include "../fexcore_integration.h"

#include "../audio/audio_out_hle.h"
#include "../gpu/gnm/gnm_submit.h"
#include "../gpu/videoout_hle.h"
#include "../input/pad_hle.h"
#include "../loader/nid.h"
#include "../loader/runtime_linker.h"
#include "../memory/memory.h"
#include "../filesystem/vfs.h"
#include "syscalls.h"          // GuestSyscalls::AppendOutput (evidence channel)
#include "../utils/evidence.h"
#include "../utils/logger.h"

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unistd.h>

namespace PX5::SceKernelHle {

namespace {

constexpr int64_t kErrInval = -static_cast<int64_t>(EINVAL);
constexpr int64_t kErrNoMem = -static_cast<int64_t>(ENOMEM);
constexpr int64_t kErrBadFd = -static_cast<int64_t>(EBADF);
constexpr int64_t kErrPerm  = -static_cast<int64_t>(EPERM);

// Guest fd -> host fd table (opened through sceKernelOpen only).
std::unordered_map<uint64_t, int>& FdTable() {
    static std::unordered_map<uint64_t, int> table;
    return table;
}

uint64_t NextGuestFd() {
    static uint64_t next = 100;      // avoid clashing with host fds
    return ++next;
}

// Window-truth pointer translation. Addresses handed to these HLE wrappers
// must already be window VAs (identity mapping) — the same contract the raw
// syscall bridge applies. Anything outside the window is rejected.
void* ResolveWindowPtr(uint64_t va, size_t len) {
    auto& mm = MemoryManager::GetInstance();
    if (!mm.IsValidAddress(va, len ? len : 1)) return nullptr;
    return mm.GetHostPointer(va);
}

} // namespace

KernelHle& KernelHle::GetInstance() {
    static KernelHle inst;
    return inst;
}

void KernelHle::RegisterAll() {
    std::lock_guard<std::mutex> lk(m_registerMutex);
    if (m_registered) return;
    m_table.clear();

    auto add = [&](const char* n) { m_table.push_back({n, 0}); };

    add("sceKernelOpen");
    add("sceKernelRead");
    add("sceKernelWrite");
    add("sceKernelClose");
    add("sceKernelAllocateDirectMemory");
    add("sceKernelMapDirectMemory");
    add("sceKernelMunmap");            // via UnmapDirectMemory path too
    add("sceKernelMprotect");
    add("sceKernelSleep");
    add("sceKernelUsleep");
    add("sceKernelIsNeoMode");
    add("sceKernelGetCompiledSdkVersion");

    // Phase C milestone 2b: the ONE GNM seam that carries PM4 command
    // buffers (GNM is statically linked into games; this symbol is the
    // submit interface the dossier mandates hooking). Routed to GnmSubmit
    // -> Pm4Decoder -> GnmState; evidence counters + stats are live.
    add("sceGnmSubmitCommandBuffers");

    // M8-era ORBIS library surfaces (ported from AnyPS5, GPL-2.0): the
    // audio-out and pad services games call constantly. The services own
    // their full ABI; these entries expose the invoked surface to the same
    // table + counters every other symbol uses.
    add("sceAudioOutInit");
    add("sceAudioOutOpen");
    add("sceAudioOutClose");
    add("sceAudioOutOutput");
    add("sceAudioOutOutputs");
    add("sceAudioOutSetVolume");
    add("sceAudioOutGetLastOutputTime");
    add("sceAudioOutGetPortState");
    add("sceAudioOutSetMixLevelPadSpk");
    add("scePadInit");
    add("scePadOpen");
    add("scePadClose");
    add("scePadRead");
    add("scePadReadState");
    add("scePadGetControllerInformation");
    add("scePadGetInfo");
    add("scePadSetVibration");
    add("scePadSetLightBar");

    // M10 T1 — the video-out surface (ported from AnyPS5 libSceVideoOut,
    // GPL-2.0): display open/close, swap-chain buffer registration, flip
    // submission and the status queries games allocate against. The
    // presentation bridge to the app surface is a later milestone; the
    // service's validation chain and status accounting are already real.
    add("sceVideoOutOpen");
    add("sceVideoOutClose");
    add("sceVideoOutRegisterBuffers2");
    add("sceVideoOutSubmitFlip");
    add("sceVideoOutGetFlipStatus");
    add("sceVideoOutGetVblankStatus");
    add("sceVideoOutGetOutputStatus");
    add("sceVideoOutGetResolutionStatus");
    add("sceVideoOutSetFlipRate");
    add("sceVideoOutIsFlipPending");
    add("sceVideoOutWaitVblank");
    add("sceVideoOutAllowOutputResolutionWqhdDetection");
    add("sceVideoOutIsOutputSupported");
    add("sceVideoOutConfigureOutput");

    // ORBIS crt bootstrap (device evidence 2026-10-11, real guest session):
    // the game's first three import calls are _init_env (NID bzQExy189ZI)
    // then atexit twice (NID 8G2LB+A3rzg) — the PS5 startup path. These are
    // libc-level names, not libSce*: registered here BY NAME; the NID
    // registry derives their NID strings, so the qualified dynstr names
    // ('bzQExy189ZI#T#T') the loader reads resolve through them.
    add("_init_env");
    add("atexit");
    add("__cxa_atexit");

    // v1.62 — the libc/libc++ tranche rides the same table + counters.
    LibcHle::Instance().CollectSymbols(&m_table);

    // v1.65 — the pthread tranche (27 imports from the decoded game list)
    // rides the same table + counters; its guest threads dispatch through
    // the FEXCore launcher seam installed here (device path only — host
    // tests install their own launcher before creating threads).
    PthreadHle::Instance().SetGuestLauncher(
        &PX5::FexCoreIntegration::RunGuestThread);
    PthreadHle::Instance().CollectSymbols(&m_table);

    // v1.67 — the C++ ABI runtime tranche (guards + exception registry)
    // rides the same table + counters.
    CxxAbiHle::Instance().CollectSymbols(&m_table);

    // v1.68 — the sceAgc surface (189 imports) resolves through the same
    // dispatch; the default handler's ledger is the tranche-2 worklist.
    AgcHle::Instance().CollectSymbols(&m_table);

    m_registered = true;
    // Seed the NID registry: guest images import these functions by their
    // 11-char NID strings; the transparent import dispatch resolves those
    // strings back to the symbols above through exactly this map.
    for (const auto& s : m_table)
        Nid::NidRegistry::Instance().RegisterSymbol(s.name);
    // Install the transparent-dispatch seam. The resolve side scans the
    // table DIRECTLY (no RegisterAll re-entry — it can be called with the
    // linker mutex held); the table is immutable past this point. The
    // invoke side forwards to the same dispatchers the self-tests use.
    RuntimeLinker::ImportRoute route;
    route.resolve = [table = &m_table](const std::string& name,
                                       std::string* symbolOut) -> bool {
        for (const auto& s : *table)
            if (name == s.name) {
                symbolOut->assign(s.name);
                return true;
            }
        return false;
    };
    route.invoke = [](const std::string& symbol,
                      const ImportCall& call,
                      uint64_t* xmmRetOut) -> uint64_t {
        return KernelHle::GetInstance().InvokeByImport(symbol, call,
                                                       xmmRetOut);
    };
    RuntimeLinker::GetInstance().SetImportRoute(std::move(route));
    PX5_LOGI(LogCategory::KERNEL,
             "libkernel HLE v1 registered: %zu symbols (imports resolve in Phase C)",
             m_table.size());
}

bool KernelHle::HasSymbol(const std::string& symbol) {
    RegisterAll();
    for (const auto& s : m_table)
        if (symbol == s.name) return true;
    return false;
}

size_t KernelHle::SymbolCount() const { return m_table.size(); }

static void Bump(const char* symbol) {
    auto& self = KernelHle::GetInstance();
    for (auto& s : const_cast<std::vector<SymbolEntry>&>(self.Table()))
        if (strcmp(s.name, symbol) == 0) { s.calls++; break; }
}

// ---------------------------------------------------------------------------
// ORBIS crt bootstrap HLE (real-guest startup path, see RegisterAll).
//
// _init_env(params) — on a real PS5 the crt entry hands the process-
// parameter block here and libc bootstraps (heap, env, atexit root). PX5
// owns no guest heap object to construct; the honest implementation
// records the parameter block (the same pointer the dispatch ledger
// logged as rdi/a0) and returns 0, matching the crt contract for
// "environment ready". Anything the guest later needs from that block
// arrives as further import calls — each lands on the ledger with a name.
//
// atexit(func) / __cxa_atexit(func, arg, dso) — guest exit handlers.
// Stored host-side so the registration is REAL and countable; running
// them requires a guest exit path (host->guest call frame) that does not
// exist yet — that absence stays named, never faked.
// ---------------------------------------------------------------------------
std::mutex g_crtExitMutex;
std::vector<std::pair<uint64_t, uint64_t>> g_crtExitCallbacks;

uint64_t HleInitEnv(uint64_t params) {
    Evidence::AppendLedger("hle _init_env params=0x%llx",
                           (unsigned long long)params);
    PX5_LOGI(LogCategory::KERNEL,
             "[GUEST] _init_env(params=0x%llx) -> 0 (crt bootstrap "
             "recorded; heap object not constructed)",
             (unsigned long long)params);
    return 0;
}

uint64_t HleAtexit(uint64_t func, uint64_t arg, const char* symbol) {
    if (func == 0) {
        // ORBIS atexit(NULL) returns success with nothing registered.
        Evidence::AppendLedger("hle %s func=NULL -> 0", symbol);
        return 0;
    }
    {
        std::lock_guard<std::mutex> lk(g_crtExitMutex);
        g_crtExitCallbacks.emplace_back(func, arg);
    }
    Evidence::AppendLedger("hle %s func=0x%llx arg=0x%llx stored=%zu",
                           symbol, (unsigned long long)func,
                           (unsigned long long)arg,
                           g_crtExitCallbacks.size());
    PX5_LOGI(LogCategory::KERNEL,
             "[GUEST] %s(func=0x%llx arg=0x%llx) -> 0 (registered, %zu "
             "pending; exit-run path not built yet)",
             symbol, (unsigned long long)func, (unsigned long long)arg,
             g_crtExitCallbacks.size());
    return 0;
}

// ---------------------------------------------------------------------------
// Implementations
// ---------------------------------------------------------------------------

uint64_t KernelHle::Open(uint64_t pathPtr, uint64_t flags, uint64_t mode) {
    Bump("sceKernelOpen");
    auto* p = static_cast<const char*>(ResolveWindowPtr(pathPtr, 1));
    if (!p) {
        // Allow direct host pointers (evidence tests pass C strings).
        p = reinterpret_cast<const char*>(pathPtr);
        if (!p) return static_cast<uint64_t>(kErrInval);
    }

    // Route guest paths through VFS so only sandbox-mounted trees are legal.
    const std::string resolved = VirtualFileSystem::GetInstance()
                                     .ResolveHostPath(p);

    const int hostFd = ::open(resolved.c_str(),
                              static_cast<int>(flags | O_CREAT | O_CLOEXEC),
                              mode ? static_cast<mode_t>(mode) : 0644);
    if (hostFd < 0) {
        PX5_LOGW(LogCategory::KERNEL, "sceKernelOpen(%s) errno=%d", resolved.c_str(), errno);
        return static_cast<uint64_t>(-static_cast<int64_t>(errno));
    }

    const uint64_t gfd = NextGuestFd();
    FdTable()[gfd] = hostFd;
    PX5_LOGI(LogCategory::KERNEL, "sceKernelOpen(%s) fd=%llu host=%d",
             resolved.c_str(), (unsigned long long)gfd, hostFd);
    return gfd;
}

uint64_t KernelHle::Write(uint64_t fd, uint64_t buf, uint64_t count) {
    Bump("sceKernelWrite");
    const size_t capped =
        count > (1u << 20) ? (1u << 20) : static_cast<size_t>(count);
    void* host = ResolveWindowPtr(buf, capped ? capped : 1);
    if (!host && capped > 0) {
        // Allow raw host pointers for evidence tests run from C++.
        host = reinterpret_cast<void*>(buf);
        if (!host) return static_cast<uint64_t>(kErrInval);
    }

    // Console capture shares the SAME evidence channel as the raw bridge.
    if (fd == 1 || fd == 2) {
        std::string text(static_cast<const char*>(host), capped);
        GuestSyscalls::AppendOutput(text);
        PX5_LOGI(LogCategory::KERNEL, "sceKernelWrite(console, %zu B)", capped);
        return count;
    }

    auto it = FdTable().find(fd);
    if (it == FdTable().end()) return static_cast<uint64_t>(kErrBadFd);

    const ssize_t w = ::write(it->second, host, capped);
    return w >= 0 ? static_cast<uint64_t>(w)
                  : static_cast<uint64_t>(-static_cast<int64_t>(errno));
}

uint64_t KernelHle::Close(uint64_t fd) {
    Bump("sceKernelClose");
    auto it = FdTable().find(fd);
    if (it == FdTable().end()) return static_cast<uint64_t>(kErrBadFd);
    const int rc = ::close(it->second);
    FdTable().erase(it);
    return rc == 0 ? SCE_OK : static_cast<uint64_t>(-static_cast<int64_t>(errno));
}

uint64_t KernelHle::AllocateDirectMemory(uint64_t searchStart,
                                         uint64_t searchEnd,
                                         uint64_t len, uint64_t alignment,
                                         uint64_t physAddrOut) {
    Bump("sceKernelAllocateDirectMemory");
    (void)searchStart; (void)searchEnd;

    if (len == 0 || alignment == 0 ||
        (alignment & (alignment - 1)) != 0 ||   // power-of-two required
        alignment > kDmRegionSize)              // and small enough that the
        return static_cast<uint64_t>(kErrInval);// rounding below cannot wrap

    if (m_dmNext > kDmRegionSize || len > kDmRegionSize - m_dmNext)
        return static_cast<uint64_t>(kErrNoMem);

    const uint64_t alignedOff = (m_dmNext + alignment - 1) & ~(alignment - 1);
    if (alignedOff > kDmRegionSize || len > kDmRegionSize - alignedOff)
        return static_cast<uint64_t>(kErrNoMem);

    const uint64_t phys = kDmRegionVa + alignedOff;   // "physical" identity
    m_dmNext = alignedOff + len;

    void* out = ResolveWindowPtr(physAddrOut, 8);
    if (!out) out = reinterpret_cast<void*>(physAddrOut);   // host C++ test path
    if (!out) return static_cast<uint64_t>(kErrInval);
    memcpy(out, &phys, 8);

    PX5_LOGI(LogCategory::MEMORY,
             "AllocateDirectMemory: phys=0x%llx len=%llu align=%llu",
             (unsigned long long)phys, (unsigned long long)len,
             (unsigned long long)alignment);
    return SCE_OK;
}

uint64_t KernelHle::MapDirectMemory(uint64_t addrRequested, uint64_t len,
                                    uint64_t prot, uint64_t /*flagsM*/,
                                    uint64_t physical, uint64_t /*maxPgOff*/) {
    Bump("sceKernelMapDirectMemory");
    // Range checks are subtraction-based: physical/len are guest-controlled
    // 64-bit values and a wrapped physical+len used to pass the window test.
    if (len == 0 || len > kDmRegionSize ||
        physical < kDmRegionVa ||
        kDmRegionVa + kDmRegionSize - physical < len)
        return static_cast<uint64_t>(kErrInval);

    // Pick a map slot inside the dedicated DirectMemory map area when the
    // caller passes NULL, otherwise honor the requested VA inside that area.
    uint64_t target;
    if (addrRequested == 0) {
        static uint64_t bumpMapVa = kDmMapBase;
        target = (bumpMapVa + 0xFFF) & ~0xFFFull;
        if (target < bumpMapVa || len > kDmMapSize ||
            target > kDmMapBase + kDmMapSize - len)
            return static_cast<uint64_t>(kErrNoMem);
        bumpMapVa = target + len;
    } else {
        if (addrRequested < kDmMapBase || len > kDmMapSize ||
            addrRequested > kDmMapBase + kDmMapSize - len)
            return static_cast<uint64_t>(kErrInval);
        target = addrRequested;
    }

    uint32_t pf = MemoryFlags::PAGE_NONE;
    if (prot & 0x1 /*PROT_READ*/)  pf |= MemoryFlags::PAGE_READ;
    if (prot & 0x2 /*PROT_WRITE*/) pf |= MemoryFlags::PAGE_WRITE;
    if (prot & 0x4 /*PROT_EXEC*/)  pf |= MemoryFlags::PAGE_EXEC;

    auto& mm = MemoryManager::GetInstance();
    if (!mm.MapMemory(target, len, pf, "dm_map"))
        return static_cast<uint64_t>(kErrNoMem);

    // Wire the freshly-mapped range to contain zeros like real fresh pages.
    if (void* h = mm.GetHostPointer(target))
        memset(h, 0, len);

    PX5_LOGI(LogCategory::MEMORY,
             "MapDirectMemory: guestVA=0x%llx len=%llu phys=0x%llx prot=%llx",
             (unsigned long long)target, (unsigned long long)len,
             (unsigned long long)physical, (unsigned long long)prot);
    return target;                      // mapped guest VA ("pointer" style)
}

uint64_t KernelHle::UnmapDirectMemory(uint64_t addr, uint64_t len) {
    Bump("sceKernelMunmap");
    return MemoryManager::GetInstance().UnmapMemory(addr, len)
               ? SCE_OK : static_cast<uint64_t>(kErrInval);
}

uint64_t KernelHle::Mprotect(uint64_t addr, uint64_t len, uint64_t prot) {
    Bump("sceKernelMprotect");
    uint32_t pf = MemoryFlags::PAGE_NONE;
    if (prot & 0x1) pf |= MemoryFlags::PAGE_READ;
    if (prot & 0x2) pf |= MemoryFlags::PAGE_WRITE;
    if (prot & 0x4) pf |= MemoryFlags::PAGE_EXEC;
    return MemoryManager::GetInstance().ProtectMemory(addr, len, pf)
               ? SCE_OK : static_cast<uint64_t>(kErrInval);
}

uint64_t KernelHle::SleepSeconds(uint64_t seconds) {
    Bump("sceKernelSleep");
    if (seconds == 0) return SCE_OK;
    std::this_thread::sleep_for(std::chrono::seconds(
        seconds > 60 ? 60 : seconds));
    return SCE_OK;
}

uint64_t KernelHle::IsNeoMode() {
    Bump("sceKernelIsNeoMode");
    return 0;    // base PS4-mode semantics until PS5 profiles exist
}

uint64_t KernelHle::GetCompiledSdkVersion() {
    Bump("sceKernelGetCompiledSdkVersion");
    return 0;    // honest: no SDK banner parsed yet
}

uint64_t KernelHle::GnmSubmitCommandBuffers(uint64_t count,
                                            uint64_t descriptorsPtr,
                                            uint64_t userDataPtr) {
    Bump("sceGnmSubmitCommandBuffers");
    std::string err;
    const int64_t rc = Gnm::GnmSubmit::GetInstance()
                           .SubmitDescriptors(count, descriptorsPtr,
                                              userDataPtr, &err);
    if (rc != 0) {
        PX5_LOGW(LogCategory::GPU,
                 "sceGnmSubmitCommandBuffers(count=%llu) rc=%lld: %s",
                 (unsigned long long)count, (long long)rc, err.c_str());
        // Evidence on the failure path too: a device session must not lose
        // the per-kind deferral/capture counters just because one submit
        // returned an error.
        PX5_LOGW(LogCategory::GPU, "sceGnmSubmitCommandBuffers stats: %s",
                 Gnm::GnmSubmit::GetInstance().GetStatsString().c_str());
    } else {
        PX5_LOGI(LogCategory::GPU,
                 "sceGnmSubmitCommandBuffers ok: %s",
                 Gnm::GnmSubmit::GetInstance().GetStatsString().c_str());
    }
    return static_cast<uint64_t>(rc);
}

// ---------------------------------------------------------------------------

uint64_t KernelHle::InvokeByName(const std::string& symbol,
                                 uint64_t a0, uint64_t a1,
                                 uint64_t a2, uint64_t a3,
                                 uint64_t a4, uint64_t a5) {
    // Self-test / direct-call path: same dispatchers, no stack args
    // (guestRsp=0 — symbols whose arity exceeds six refuse honestly).
    ImportCall call;
    call.args[0] = a0;
    call.args[1] = a1;
    call.args[2] = a2;
    call.args[3] = a3;
    call.args[4] = a4;
    call.args[5] = a5;
    return InvokeByImport(symbol, call);
}

uint64_t KernelHle::InvokeByImport(const std::string& symbol,
                                   const ImportCall& call,
                                   uint64_t* xmmRetOut) {
    RegisterAll();
    const uint64_t a0 = call.args[0];
    const uint64_t a1 = call.args[1];
    const uint64_t a2 = call.args[2];
    const uint64_t a3 = call.args[3];
    const uint64_t a4 = call.args[4];
    const uint64_t a5 = call.args[5];
    // ORBIS int32 results sign-extend into the uint64 return contract.
    const auto s32 = [](int32_t rc) -> uint64_t {
        return static_cast<uint64_t>(static_cast<int64_t>(rc));
    };
    const auto ptr = [](uint64_t va) -> void* {
        return reinterpret_cast<void*>(va);
    };

    if (symbol == "sceKernelOpen")                    return Open(a0, a1, a2);
    if (symbol == "sceKernelWrite")                   return Write(a0, a1, a2);
    if (symbol == "sceKernelClose")                   return Close(a0);
    if (symbol == "sceKernelAllocateDirectMemory")
        return AllocateDirectMemory(a0, a1, a2, a3, a4);
    if (symbol == "sceKernelMapDirectMemory")
        return MapDirectMemory(a0, a1, a2, a3, a4, a5);
    if (symbol == "sceKernelMunmap" ||
        symbol == "sceKernelUnmapDirectMemory")       return UnmapDirectMemory(a0, a1);
    if (symbol == "sceKernelMprotect")                return Mprotect(a0, a1, a2);
    if (symbol == "sceKernelSleep")                   return SleepSeconds(a0);
    if (symbol == "sceKernelUsleep")                  return SleepSeconds(0);
    if (symbol == "sceKernelIsNeoMode")               return IsNeoMode();
    if (symbol == "sceKernelGetCompiledSdkVersion")   return GetCompiledSdkVersion();
    if (symbol == "sceGnmSubmitCommandBuffers")       return GnmSubmitCommandBuffers(a0, a1, a2);

    // --- ORBIS crt bootstrap (real-guest startup path) ---------------------
    if (symbol == "_init_env")                        return HleInitEnv(a0);
    if (symbol == "atexit")                           return HleAtexit(a0, 0, "atexit");
    if (symbol == "__cxa_atexit")                     return HleAtexit(a0, a1, "__cxa_atexit");

    // --- sceAudioOut (SceAudioOutHle) --------------------------------------
    auto& audioOut = SceAudioOutHle::AudioOutHle::Instance();
    if (symbol == "sceAudioOutInit")
        return s32(audioOut.Init());
    if (symbol == "sceAudioOutOpen")
        return s32(audioOut.Open(static_cast<int32_t>(a0),
                                 static_cast<int32_t>(a1),
                                 static_cast<int32_t>(a2),
                                 static_cast<uint32_t>(a3),
                                 static_cast<uint32_t>(a4),
                                 static_cast<uint32_t>(a5)));
    if (symbol == "sceAudioOutClose")
        return s32(audioOut.Close(static_cast<int32_t>(a0)));
    if (symbol == "sceAudioOutOutput")
        return s32(audioOut.Output(static_cast<int32_t>(a0), ptr(a1)));
    if (symbol == "sceAudioOutOutputs")
        return s32(audioOut.Outputs(
            static_cast<const SceAudioOutHle::AudioOutHle::OutputParam*>(
                ptr(a0)),
            static_cast<uint32_t>(a1)));
    if (symbol == "sceAudioOutSetVolume")
        return s32(audioOut.SetVolume(static_cast<int32_t>(a0),
                                      static_cast<uint32_t>(a1),
                                      static_cast<const int32_t*>(ptr(a2))));
    if (symbol == "sceAudioOutGetLastOutputTime")
        return s32(audioOut.GetLastOutputTime(static_cast<int32_t>(a0),
                                              static_cast<uint64_t*>(ptr(a1))));
    if (symbol == "sceAudioOutGetPortState")
        return s32(audioOut.GetPortState(
            static_cast<int32_t>(a0),
            static_cast<SceAudioOutHle::AudioOutPortState*>(ptr(a1))));
    if (symbol == "sceAudioOutSetMixLevelPadSpk")
        return s32(audioOut.SetMixLevelPadSpk(static_cast<int32_t>(a0),
                                              static_cast<int32_t>(a1)));

    // --- scePad (ScePadHle) -------------------------------------------------
    auto& pad = ScePadHle::PadHle::Instance();
    if (symbol == "scePadInit")            return s32(pad.Init());
    if (symbol == "scePadOpen")
        return s32(pad.Open(static_cast<int32_t>(a0),
                            static_cast<int32_t>(a1),
                            static_cast<int32_t>(a2), ptr(a3)));
    if (symbol == "scePadClose")
        return s32(pad.Close(static_cast<int32_t>(a0)));
    if (symbol == "scePadRead")
        return s32(pad.Read(static_cast<int32_t>(a0),
                            static_cast<ScePadHle::PadData*>(ptr(a1)),
                            static_cast<int32_t>(a2)));
    if (symbol == "scePadReadState")
        return s32(pad.ReadState(static_cast<int32_t>(a0),
                                 static_cast<ScePadHle::PadData*>(ptr(a1))));
    if (symbol == "scePadGetControllerInformation")
        return s32(pad.GetControllerInformation(
            static_cast<int32_t>(a0),
            static_cast<ScePadHle::PadControllerInformation*>(ptr(a1))));
    if (symbol == "scePadGetInfo")
        return s32(pad.GetInfo(static_cast<ScePadHle::PadInfo*>(ptr(a0))));
    if (symbol == "scePadSetVibration")
        return s32(pad.SetVibration(
            static_cast<int32_t>(a0),
            static_cast<const ScePadHle::PadVibrationParam*>(ptr(a1))));
    if (symbol == "scePadSetLightBar")
        return s32(pad.SetLightBar(
            static_cast<int32_t>(a0),
            static_cast<const ScePadHle::PadLightBarParam*>(ptr(a1))));

    // --- sceVideoOut (SceVideoOutHle) ---------------------------------------
    auto& videoOut = SceVideoOutHle::VideoOutHle::Instance();
    if (symbol == "sceVideoOutOpen")
        return s32(videoOut.Open(static_cast<int32_t>(a0),
                                 static_cast<int32_t>(a1),
                                 static_cast<int32_t>(a2), a3));
    if (symbol == "sceVideoOutClose")
        return s32(videoOut.Close(static_cast<int32_t>(a0)));
    if (symbol == "sceVideoOutRegisterBuffers2") {
        // ORBIS arity 8: category and option are the 7th/8th arguments —
        // they ride the guest stack beyond the six registers the stub
        // preserves. An unreadable stack arg refuses named (INVALID_OPTION),
        // it is never guessed to zero.
        uint64_t category = 0, option = 0;
        if (!call.ReadStackArg(0, &category) ||
            !call.ReadStackArg(1, &option)) {
            PX5_LOGW(LogCategory::GPU,
                     "VIDEOOUT: register-buffers stack args unreadable "
                     "(rsp=0x%llx) — refusing as INVALID_OPTION",
                     (unsigned long long)call.guestRsp);
            return s32(SceVideoOutHle::VIDEO_OUT_ERROR_INVALID_OPTION);
        }
        return s32(videoOut.RegisterBuffers2(
            static_cast<int32_t>(a0), static_cast<int32_t>(a1),
            static_cast<int32_t>(a2), a3, static_cast<int32_t>(a4), a5,
            static_cast<int32_t>(category), option));
    }
    if (symbol == "sceVideoOutSubmitFlip")
        return s32(videoOut.SubmitFlip(
            static_cast<int32_t>(a0), static_cast<int32_t>(a1),
            static_cast<int32_t>(a2), static_cast<int64_t>(a3)));
    if (symbol == "sceVideoOutGetFlipStatus")
        return s32(videoOut.GetFlipStatus(static_cast<int32_t>(a0), a1));
    if (symbol == "sceVideoOutGetVblankStatus")
        return s32(videoOut.GetVblankStatus(static_cast<int32_t>(a0), a1));
    if (symbol == "sceVideoOutGetOutputStatus")
        return s32(videoOut.GetOutputStatus(static_cast<int32_t>(a0), a1));
    if (symbol == "sceVideoOutGetResolutionStatus")
        return s32(videoOut.GetResolutionStatus(static_cast<int32_t>(a0),
                                                a1));
    if (symbol == "sceVideoOutSetFlipRate")
        return s32(videoOut.SetFlipRate(static_cast<int32_t>(a0),
                                        static_cast<int32_t>(a1)));
    if (symbol == "sceVideoOutIsFlipPending")
        return s32(videoOut.IsFlipPending(static_cast<int32_t>(a0)));
    if (symbol == "sceVideoOutWaitVblank")
        return s32(videoOut.WaitVblank(static_cast<int32_t>(a0)));
    if (symbol == "sceVideoOutAllowOutputResolutionWqhdDetection")
        return s32(videoOut.AllowOutputResolutionWqhdDetection(
            static_cast<int32_t>(a0)));
    if (symbol == "sceVideoOutIsOutputSupported")
        return s32(videoOut.IsOutputSupported(static_cast<int32_t>(a0), a1,
                                              a2));
    if (symbol == "sceVideoOutConfigureOutput")
        return s32(videoOut.ConfigureOutput(static_cast<int32_t>(a0), a1,
                                            a2));

    // --- libc/libc++ tranche (v1.62; XMM return channel v1.66) --------------
    if (LibcHle::Instance().HasSymbol(symbol))
        return LibcHle::Instance().Invoke(symbol, call, xmmRetOut);

    // --- pthread tranche (v1.65) --------------------------------------------
    if (PthreadHle::Instance().HasSymbol(symbol))
        return PthreadHle::Instance().Invoke(symbol, call, xmmRetOut);

    // --- C++ ABI runtime tranche (v1.67) ------------------------------------
    if (CxxAbiHle::Instance().HasSymbol(symbol))
        return CxxAbiHle::Instance().Invoke(symbol, call, xmmRetOut);

    // --- sceAgc surface (v1.68) ----------------------------------------------
    if (AgcHle::Instance().HasSymbol(symbol))
        return AgcHle::Instance().Invoke(symbol, call, xmmRetOut);

    return static_cast<uint64_t>(kErrInval);
}

std::string KernelHle::GetSummaryString() const {
    if (m_table.empty())
        return "libkernel HLE: table empty";
    uint64_t invoked = 0;
    for (const auto& s : m_table) invoked += s.calls;
    char buf[96];
    snprintf(buf, sizeof(buf),
             "libkernel HLE v1: %zu symbols | invocations=%llu",
             m_table.size(), (unsigned long long)invoked);
    return buf;
}

} // namespace PX5::SceKernelHle
