// PX5 v1.62 — guest-space C++ runtime ABI module (see guest_abi.h).
//
// The object set and the two vtable layouts mirror AnyPS5's device-verified
// libc exception implementation (core/libs/prx/libc/src/exception/,
// GPL-2.0-only): the 11-slot TypeInfoVtable contract for the __cxxabiv1
// classes and the 5-slot ExceptionVtable (offset, type, destroy, delete,
// what) for the std::exception family. Layouts are data contracts of the
// PS5 libc++abi, not creative choices; every non-imported symbol here
// exists only to keep emitted base/typeinfo chains self-consistent.
#include "guest_abi.h"

#include "../memory/memory.h"
#include "../utils/evidence.h"
#include "../utils/logger.h"
#include "nid.h"

#include <algorithm>
#include <cstring>
#include <mutex>
#include <vector>

namespace PX5::GuestAbi {

namespace {

// Reserved guest syscall range for ABI stubs: disjoint from the import-call
// range [0x5C510000, 0x5C520000) and from every real x86-64 Linux number.
constexpr uint32_t kAbiStubNrBase  = 0x5C530000u;
constexpr size_t   kStubStride     = 16;
constexpr size_t   kRegionBudget   = 64 * 1024;   // actual use is ~6 KiB
constexpr uint32_t kFileMagic      = 0x46463558u; // "X5FF" little-endian

// ABI stub ledger: id -> name. The order is the emission order; ids are
// part of the guest-visible stub encoding, so entries are append-only.
struct StubSpec {
    const char* name;
    const char* note;   ///< guest-visible semantics, logged on first hit
};
constexpr StubSpec kStubs[] = {
    {"ti_destroy",        "type_info dtor -> no-op"},
    {"ti_delete",         "type_info delete -> no-op (guest RTTI kept)"},
    {"ti_is_pointer",     "is_pointer_p -> false"},
    {"ti_is_function",    "is_function_p -> false"},
    {"ti_do_catch",       "do_catch -> no-match (throw path is a later tranche)"},
    {"ti_do_upcast",      "do_upcast -> no-match"},
    {"ti_upcast_result",  "__upcast_result -> no-match"},
    {"ti_dyncast",        "__dyncast -> no-match"},
    {"ti_find_public",    "__do_find_public_src -> no-match"},
    {"exception_destroy", "std exception dtor -> no-op"},
    {"exception_delete",  "std exception delete -> no-op"},
    {"exception_what",    "what() -> \"\" (empty message)"},
    {"doraise",           "_Doraise -> returns (no raise path)"},
    {"pure_virtual",      "__cxa_pure_virtual -> logged, guest continues"},
    {"personality",       "__gxx_personality_v0 -> no unwind (returns 0)"},
};
constexpr size_t kStubCount = sizeof(kStubs) / sizeof(kStubs[0]);
constexpr size_t kWhatStubId = 11;   // emits mov rax,&"" instead of a syscall

// ---------------------------------------------------------------------------
// Layout helpers. The blob is built host-side with FINAL guest VAs patched
// in, then mapped and written once (write + readback-verify, the same
// two-phase discipline the import-stub region uses).
// ---------------------------------------------------------------------------
struct Layout {
    std::vector<uint8_t> blob;
    std::unordered_map<std::string, uint64_t> symbols;  // name -> guest VA
    uint64_t base = 0;

    size_t Emit(const void* data, size_t len, size_t align) {
        size_t off = blob.size();
        size_t pad = (align - off % align) % align;
        blob.insert(blob.end(), pad, 0);
        off += pad;
        const auto* p = static_cast<const uint8_t*>(data);
        blob.insert(blob.end(), p, p + len);
        return off;
    }
    size_t EmitZero(size_t len, size_t align) {
        size_t off = blob.size();
        size_t pad = (align - off % align) % align;
        blob.insert(blob.end(), pad, 0);
        off += pad;
        blob.insert(blob.end(), len, 0);
        return off;
    }
    uint64_t Va(size_t off) const { return base + off; }
    void Ptr(size_t off, uint64_t va) {  // patch a pointer slot
        std::memcpy(blob.data() + off, &va, sizeof va);
    }
    void EmitString(const char* s, size_t* offOut) {
        *offOut = Emit(s, std::strlen(s) + 1, 1);
    }
};

struct FamilySpec {
    const char* mangled;      // full symbol root, e.g. "St9exception"
    const char* tiName;       // type_info::name() (mangled, no _ZTI)
    const char* baseMangled;  // "" = class_type_info (no imported base)
    bool needVtable;          // game imports the TV too
};

// The std::exception family closure. Inheritance per libc++:
// logic_error/runtime_error/bad_cast -> exception; system_error ->
// runtime_error; domain_error/invalid_argument/out_of_range -> logic_error;
// ios_base::failure -> system_error.
constexpr FamilySpec kFamily[] = {
    {"St9exception",            "St9exception",            "",                 true},
    {"St11logic_error",         "St11logic_error",         "St9exception",     true},
    {"St13runtime_error",       "St13runtime_error",       "St9exception",     true},
    {"St12system_error",        "St12system_error",        "St13runtime_error",true},
    {"St12domain_error",        "St12domain_error",        "St11logic_error",  true},
    {"St16invalid_argument",    "St16invalid_argument",    "St11logic_error",  true},
    {"St12out_of_range",        "St12out_of_range",        "St11logic_error",  true},
    {"St8bad_cast",             "St8bad_cast",             "St9exception",     true},
    {"NSt8ios_base7failureE",   "NSt8ios_base7failureE",   "St12system_error", true},
};
constexpr size_t kFamilyCount = sizeof(kFamily) / sizeof(kFamily[0]);

std::mutex g_mutex;
std::unordered_map<std::string, uint64_t> g_symbols;
std::vector<uint32_t> g_stubHits(kStubCount, 0);
bool g_built = false;

void WriteStub(uint8_t* p, size_t id, uint64_t whatStringVa) {
    std::memset(p, 0, kStubStride);
    if (id == kWhatStubId) {
        // what() must return a valid const char* — point at the region's
        // empty string instead of the ledger syscall (rax=0 would hand the
        // guest a null and a printf("%s", e.what()) would fault).
        p[0] = 0x48; p[1] = 0xB8;   // mov rax, imm64
        std::memcpy(p + 2, &whatStringVa, sizeof whatStringVa);
        p[10] = 0xC3;               // ret
        return;
    }
    const uint32_t nr = kAbiStubNrBase + static_cast<uint32_t>(id);
    p[0] = 0xB8;                    // mov eax, nr
    p[1] = static_cast<uint8_t>(nr);
    p[2] = static_cast<uint8_t>(nr >> 8);
    p[3] = static_cast<uint8_t>(nr >> 16);
    p[4] = static_cast<uint8_t>(nr >> 24);
    p[5] = 0x0F; p[6] = 0x05;       // syscall
    p[7] = 0xC3;                    // ret
}

} // namespace

BuildResult Build(const char* label, uint64_t avoidBase, uint64_t avoidEnd) {
    BuildResult res;
    std::lock_guard<std::mutex> lk(g_mutex);

    auto& mm = MemoryManager::GetInstance();
    const uint64_t guestBase = mm.GetGuestBase();
    const uint64_t winEnd = mm.GetGuestEnd();
    if (guestBase == 0 || winEnd == 0 || winEnd <= guestBase) {
        res.error = "no guest window reserved";
        return res;
    }

    // ---- pass 1: compute the blob with a provisional base, then rebase ---
    // Sizes are base-independent, so one pass at base=0 yields the offsets;
    // pointer patches then use the final VA.
    Layout lay;
    lay.base = 0;

    const size_t stubOff = lay.blob.size();
    lay.blob.insert(lay.blob.end(), kStubCount * kStubStride, 0);
    // Code/data page split: pad the code section to its own page boundary so
    // the RX protect below never covers a data byte (locale id counters and
    // exception bookkeeping are WRITTEN by the guest's C++ runtime; a shared
    // page would hand the guest a spurious fault on its first static write).
    const size_t codePageBytes =
        ((lay.blob.size() + 0xFFF) & ~size_t(0xFFF));
    lay.blob.insert(lay.blob.end(), codePageBytes - lay.blob.size(), 0);

    size_t emptyStrOff = 0;
    lay.EmitString("", &emptyStrOff);

    // typeinfo name strings
    size_t nameOff[kFamilyCount] = {};
    for (size_t i = 0; i < kFamilyCount; ++i)
        lay.EmitString(kFamily[i].tiName, &nameOff[i]);
    size_t metaNameOff[3] = {};
    lay.EmitString("N10__cxxabiv117__class_type_infoE", &metaNameOff[0]);
    lay.EmitString("N10__cxxabiv120__si_class_type_infoE", &metaNameOff[1]);
    lay.EmitString("N10__cxxabiv121__vmi_class_type_infoE", &metaNameOff[2]);

    // FILE-ish structs for _Stdout/_Stderr: magic + guest fd (1/2). The
    // stdio HLE tranche recognizes the magic; guest code only holds the
    // pointer.
    struct FileObj {
        uint32_t magic;
        uint32_t fd;
        uint64_t reserved[3];
    };
    FileObj fo[2] = {{kFileMagic, 1, {}}, {kFileMagic, 2, {}}};
    const size_t fileOff[2] = {lay.Emit(&fo[0], sizeof fo[0], 8),
                               lay.Emit(&fo[1], sizeof fo[1], 8)};

    // misc data slots (8 bytes each unless noted)
    const uint64_t canary = 0x5f5f5f5f5f5f5f5full;   // __stack_chk_guard
    const uint64_t badOff = ~0ull;                   // _ZSt7_BADOFF
    const uint32_t needLibc = 1;                     // Need_sceLibc
    const uint8_t classicLocale = 0;                 // _ZSt21_sceLibcClassicLocale
    size_t guardOff    = lay.Emit(&canary, 8, 8);
    size_t stdoutOff   = lay.EmitZero(8, 8);   // patched below
    size_t stderrOff   = lay.EmitZero(8, 8);
    size_t needOff     = lay.Emit(&needLibc, 4, 4);
    size_t badoffOff   = lay.Emit(&badOff, 8, 8);
    size_t fpzOff      = lay.EmitZero(8, 8);
    size_t classicOff  = lay.Emit(&classicLocale, 1, 1);
    size_t ctypeCOff   = lay.EmitZero(8, 8);
    size_t ctypeWOff   = lay.EmitZero(8, 8);
    size_t localeIdOff = lay.EmitZero(8, 8);
    size_t collateWOff = lay.EmitZero(8, 8);
    size_t numPutOff   = lay.EmitZero(8, 8);

    // meta typeinfos for the __cxxabiv1 classes: class and vmi are class-
    // instance shaped ({vptr,name}, 24B) — vmi derives directly from
    // __class_type_info — while si is {vptr,name,base}, 32B.
    size_t metaTiOff[3] = {};
    for (int k = 0; k < 3; ++k) {
        const size_t sz = (k == 1) ? 32 : 24;
        metaTiOff[k] = lay.EmitZero(sz, 8);
    }

    // family typeinfos: exception = class instance, rest = si instances
    size_t famTiOff[kFamilyCount] = {};
    for (size_t i = 0; i < kFamilyCount; ++i) {
        const size_t sz = (kFamily[i].baseMangled[0] == '\0') ? 24 : 32;
        famTiOff[i] = lay.EmitZero(sz, 8);
    }

    // cxxabiv1 vtables: 11 slots (offset, type, 9 virtuals)
    size_t metaVtOff[3] = {};
    for (int k = 0; k < 3; ++k)
        metaVtOff[k] = lay.EmitZero(11 * 8, 8);

    // family vtables: 5 slots (offset, type, destroy, delete, what)
    size_t famVtOff[kFamilyCount] = {};
    for (size_t i = 0; i < kFamilyCount; ++i)
        famVtOff[i] = lay.EmitZero(5 * 8, 8);

    // ---- pass 2: choose the final guest base, rebase every patch --------
    const size_t blobBytes = ((lay.blob.size() + 0xFFF) & ~size_t(0xFFF));
    if (blobBytes > kRegionBudget) {
        res.error = "ABI blob exceeds its region budget";
        lay.blob.clear();
        return res;
    }
    // Preferred: right after the import-stub region's preferred anchor area
    // (guestBase + 0x20000000 + 16 MiB) keeps the ABI far from the image;
    // fallbacks walk downward from the window ceiling like the stub region.
    const uint64_t anchor = guestBase + 0x21000000ull;
    const uint64_t guard = 0x1000000ull;
    // Candidates, each rejection-checked against the avoided range (the
    // import-stub region): below it, above it, then the window-top fallback.
    const bool hasAvoid = avoidBase != 0 && avoidEnd > avoidBase;
    auto overlapsAvoid = [&](uint64_t lo, uint64_t hi) {
        return hasAvoid && lo < avoidEnd && hi > avoidBase;
    };
    uint64_t regionVa = 0;
    const uint64_t anchorLo = anchor;
    const uint64_t anchorHi = anchor + blobBytes;
    if (anchor >= guestBase && anchorHi + guard <= winEnd &&
        !overlapsAvoid(anchorLo, anchorHi)) {
        regionVa = anchor;
    } else if (hasAvoid && avoidBase > guestBase + blobBytes &&
               !overlapsAvoid(avoidBase - blobBytes, avoidBase)) {
        regionVa = (avoidBase - blobBytes) & ~uint64_t(0xFFF);
        if (regionVa < guestBase) regionVa = 0;
    } else if (winEnd > blobBytes + guard) {
        const uint64_t lo = (winEnd - blobBytes - guard) & ~uint64_t(0xFFF);
        if (!overlapsAvoid(lo, lo + blobBytes)) regionVa = lo;
    }
    if (regionVa == 0 || regionVa < guestBase) {
        res.error = "no window space for the ABI region";
        lay.blob.clear();
        return res;
    }
    lay.base = regionVa;
    // Re-run the pointer patches now that base is final: every Emit above
    // returned a blob offset; recompute the pointer writes directly.
    auto vaOf = [&](size_t off) { return regionVa + off; };

    // stub bytes (need emptyStr VA)
    for (size_t i = 0; i < kStubCount; ++i) {
        WriteStub(lay.blob.data() + stubOff + i * kStubStride, i,
                  vaOf(emptyStrOff));
    }

    // _Stdout/_Stderr -> FILE structs
    lay.Ptr(stdoutOff, vaOf(fileOff[0]));
    lay.Ptr(stderrOff, vaOf(fileOff[1]));

    // meta typeinfos: [0]=class {vptr,name}, [1]=si {vptr,name,base},
    // [2]=vmi (class-instance shape {vptr,name} — vmi derives directly
    // from __class_type_info, so 24 bytes are enough and honest).
    lay.Ptr(metaTiOff[0] + 0, vaOf(metaVtOff[0]) + 16);
    lay.Ptr(metaTiOff[0] + 8, vaOf(metaNameOff[0]));
    lay.Ptr(metaTiOff[1] + 0, vaOf(metaVtOff[1]) + 16);
    lay.Ptr(metaTiOff[1] + 8, vaOf(metaNameOff[1]));
    lay.Ptr(metaTiOff[1] + 16, vaOf(metaTiOff[0]));
    lay.Ptr(metaTiOff[2] + 0, vaOf(metaVtOff[2]) + 16);
    lay.Ptr(metaTiOff[2] + 8, vaOf(metaNameOff[2]));

    // family typeinfos
    for (size_t i = 0; i < kFamilyCount; ++i) {
        const bool si = kFamily[i].baseMangled[0] != '\0';
        lay.Ptr(famTiOff[i] + 0, vaOf((si ? metaVtOff[1] : metaVtOff[0])) + 16);
        lay.Ptr(famTiOff[i] + 8, vaOf(nameOff[i]));
        if (si) {
            // base -> the family TI named by baseMangled
            for (size_t j = 0; j < kFamilyCount; ++j) {
                if (std::strcmp(kFamily[j].mangled,
                                kFamily[i].baseMangled) == 0) {
                    lay.Ptr(famTiOff[i] + 16, vaOf(famTiOff[j]));
                    break;
                }
            }
        }
    }

    // cxxabiv1 vtables: [0]=0, [1]=&meta TI, [2..10]=stub ids 0..8
    for (int k = 0; k < 3; ++k) {
        lay.Ptr(metaVtOff[k] + 8, vaOf(metaTiOff[k]));
        for (size_t s = 0; s < 9; ++s) {
            lay.Ptr(metaVtOff[k] + 16 + s * 8, vaOf(stubOff + s * kStubStride));
        }
    }

    // family vtables: [0]=0, [1]=&own TI, [2]=destroy(9), [3]=delete(10),
    // [4]=what(11)
    for (size_t i = 0; i < kFamilyCount; ++i) {
        lay.Ptr(famVtOff[i] + 8, vaOf(famTiOff[i]));
        lay.Ptr(famVtOff[i] + 16, vaOf(stubOff + 9 * kStubStride));
        lay.Ptr(famVtOff[i] + 24, vaOf(stubOff + 10 * kStubStride));
        lay.Ptr(famVtOff[i] + 32, vaOf(stubOff + kWhatStubId * kStubStride));
    }

    // ---- map + write + readback ----------------------------------------
    if (!mm.MapMemory(regionVa, blobBytes,
                      MemoryFlags::PAGE_READ | MemoryFlags::PAGE_WRITE |
                          MemoryFlags::PAGE_EXEC,
                      label ? label : "guest_abi")) {
        res.error = "ABI region map rejected by the memory manager";
        lay.blob.clear();
        return res;
    }
    void* host = mm.GetHostPointer(regionVa);
    if (!host) {
        res.error = "ABI region host bridge lost";
        lay.blob.clear();
        return res;
    }
    std::memcpy(host, lay.blob.data(), lay.blob.size());
    if (std::memcmp(host, lay.blob.data(), lay.blob.size()) != 0) {
        res.error = "ABI region readback mismatch";
        lay.blob.clear();
        return res;
    }
    // Split protection: the leading code pages go RX (same discipline as
    // the import stubs); the data pages stay RW — the guest's C++ runtime
    // writes its own statics (locale id counters, exception bookkeeping).
    (void)mm.ProtectMemory(regionVa, codePageBytes,
                           MemoryFlags::PAGE_READ |
                               MemoryFlags::PAGE_EXEC);

    // ---- registry --------------------------------------------------------
    g_symbols.clear();
    std::fill(g_stubHits.begin(), g_stubHits.end(), 0);
    auto reg = [&](const char* name, size_t off) {
        g_symbols.emplace(name, regionVa + off);
    };
    for (int k = 0; k < 3; ++k) {
        static const char* kMetaVtNames[3] = {
            "_ZTVN10__cxxabiv117__class_type_infoE",
            "_ZTVN10__cxxabiv120__si_class_type_infoE",
            "_ZTVN10__cxxabiv121__vmi_class_type_infoE"};
        reg(kMetaVtNames[k], metaVtOff[k]);
    }
    for (size_t i = 0; i < kFamilyCount; ++i) {
        std::string tv = std::string("_ZTV") + kFamily[i].mangled;
        std::string ti = std::string("_ZTI") + kFamily[i].mangled;
        g_symbols.emplace(tv, vaOf(famVtOff[i]));
        g_symbols.emplace(ti, vaOf(famTiOff[i]));
    }
    // function-shaped ABI symbols (vtable contents; also callable)
    reg("_ZNKSt9exception8_DoraiseEv", stubOff + 12 * kStubStride);
    reg("_ZNSt12domain_errorD1Ev",     stubOff + 9 * kStubStride);
    reg("_ZNSt12out_of_rangeD1Ev",     stubOff + 9 * kStubStride);
    reg("_ZNSt16invalid_argumentD1Ev", stubOff + 9 * kStubStride);
    reg("_ZNSt8bad_castD1Ev",          stubOff + 9 * kStubStride);
    reg("_ZNSt8ios_base7failureD1Ev",  stubOff + 9 * kStubStride);
    reg("__cxa_pure_virtual",          stubOff + 13 * kStubStride);
    reg("__gxx_personality_v0",        stubOff + 14 * kStubStride);
    reg("__stack_chk_guard",           guardOff);
    reg("_Stdout",                     stdoutOff);
    reg("_Stderr",                     stderrOff);
    reg("Need_sceLibc",                needOff);
    reg("_ZSt7_BADOFF",                badoffOff);
    reg("_ZSt4_Fpz",                   fpzOff);
    reg("_ZSt21_sceLibcClassicLocale", classicOff);
    reg("_ZNSt5ctypeIcE2idE",          ctypeCOff);
    reg("_ZNSt5ctypeIwE2idE",          ctypeWOff);
    reg("_ZNSt6locale2id7_Id_cntE",    localeIdOff);
    reg("_ZNSt7collateIwE2idE",        collateWOff);
    reg("_ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE2idE",
        numPutOff);
    g_built = true;

    // Seed the NID registry so the loader's dynstr NID strings ("NID#T#T")
    // resolve back to these mangled names at DAT-slot processing time.
    for (const auto& s : g_symbols)
        Nid::NidRegistry::Instance().RegisterSymbol(s.first);

    res.ok = true;
    res.regionVa = regionVa;
    res.regionBytes = blobBytes;
    res.symbolCount = g_symbols.size();
    res.stubCount = kStubCount;
    res.summary = "abi objects";
    PX5_LOGI(LogCategory::LOADER,
             "GUEST-ABI: region=[0x%llx..0x%llx] symbols=%zu stubs=%zu "
             "codeRX=[0x%llx..0x%llx] — C++ ABI data objects live in guest "
             "memory (typeinfo vtables, exception RTTI, statics)",
             (unsigned long long)regionVa,
             (unsigned long long)(regionVa + blobBytes),
             g_symbols.size(), kStubCount,
             (unsigned long long)regionVa,
             (unsigned long long)(regionVa + codePageBytes));
    Evidence::AppendLedger("guest abi region=0x%llx symbols=%zu stubs=%zu",
                           (unsigned long long)regionVa, g_symbols.size(),
                           kStubCount);
    return res;
}

bool Resolve(const std::string& name, uint64_t* vaOut) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_built || name.empty() || vaOut == nullptr) return false;
    auto it = g_symbols.find(name);
    if (it == g_symbols.end()) return false;
    *vaOut = it->second;
    return true;
}

bool IsBuilt() {
    std::lock_guard<std::mutex> lk(g_mutex);
    return g_built;
}

void Reset() {
    std::lock_guard<std::mutex> lk(g_mutex);
    g_symbols.clear();
    std::fill(g_stubHits.begin(), g_stubHits.end(), 0);
    g_built = false;
}

uint64_t DispatchStub(uint64_t stubId) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (stubId >= kStubCount) {
        PX5_LOGW(LogCategory::LOADER,
                 "GUEST-ABI: stub id %llu out of range — ignored",
                 (unsigned long long)stubId);
        return 0;
    }
    if (g_stubHits[stubId] == 0) {
        PX5_LOGI(LogCategory::LOADER,
                 "[GUEST] abi stub %s: %s",
                 kStubs[stubId].name, kStubs[stubId].note);
        Evidence::AppendLedger("abi stub %s hit", kStubs[stubId].name);
    }
    ++g_stubHits[stubId];
    return 0;
}

std::string StubLedgerSummary() {
    std::lock_guard<std::mutex> lk(g_mutex);
    size_t distinct = 0, total = 0, hottest = 0;
    for (uint32_t h : g_stubHits) {
        if (h > 0) ++distinct;
        total += h;
        hottest = std::max(hottest, static_cast<size_t>(h));
    }
    return "abi stubs built=" + std::to_string(g_built ? 1 : 0) +
           " distinct=" + std::to_string(distinct) +
           " total=" + std::to_string(total) +
           " hottest=" + std::to_string(hottest);
}

} // namespace PX5::GuestAbi
