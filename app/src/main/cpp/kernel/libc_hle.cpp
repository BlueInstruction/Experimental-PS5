// PX5 v1.62 — libc/libc++ HLE tranche (see libc_hle.h).
#include "libc_hle.h"

#include "../loader/runtime_linker.h"
#include "../memory/memory.h"
#include "../utils/evidence.h"
#include "../utils/logger.h"
#include "syscalls.h"          // GuestSyscalls::AppendOutput (console)
#include "sce_kernel_hle.h"    // SymbolEntry

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <strings.h>
#include <unordered_map>

namespace PX5::SceKernelHle {

namespace {

// ---------------------------------------------------------------------------
// window-truth pointer translation (same contract as the kernel HLE):
// guest VAs must resolve inside the window; raw host pointers are accepted
// for C++-driven self-tests only.
// ---------------------------------------------------------------------------
void* WinPtr(uint64_t va, size_t len) {
    auto& mm = MemoryManager::GetInstance();
    if (mm.IsValidAddress(va, len ? len : 1)) return mm.GetHostPointer(va);
    // Raw host pointer allowance mirrors Open()/Write() in the kernel HLE.
    return reinterpret_cast<void*>(va);
}

char* WinStr(uint64_t va, size_t* lenOut) {
    // Bounded scan: strings the guest hands libc are NUL-terminated inside
    // the window; refuse pointers whose NUL never appears within 1 MiB.
    constexpr size_t kMaxStr = 1u << 20;
    auto& mm = MemoryManager::GetInstance();
    if (!mm.IsValidAddress(va, 1)) return nullptr;
    char* base = static_cast<char*>(mm.GetHostPointer(va));
    if (!base) return nullptr;
    size_t n = 0;
    while (n < kMaxStr && base[n] != '\0') ++n;
    if (n >= kMaxStr) return nullptr;
    if (lenOut) *lenOut = n;
    return base;
}

// ---------------------------------------------------------------------------
// guest heap: one 256 MiB RW region 256 MiB into the window, 16-byte bump
// allocator with a recorded size table for realloc. free() is a no-op —
// memory reclaims at session teardown (named in the header, not hidden).
// ---------------------------------------------------------------------------
constexpr uint64_t kHeapOffsetInWindow = 0x10000000ull;  // 256 MiB
constexpr size_t   kHeapBytes          = 256u << 20;     // 256 MiB
constexpr size_t   kMaxLiveAllocs      = 1u << 20;

std::mutex g_heapMutex;
uint64_t g_heapBase = 0;      // guest VA of the heap region
uint64_t g_heapBrk = 0;       // next free offset (absolute VA)
uint64_t g_heapEnd = 0;
std::unordered_map<uint64_t, size_t> g_sizes;

bool EnsureHeap() {
    if (g_heapBase != 0) return true;
    auto& mm = MemoryManager::GetInstance();
    const uint64_t base = mm.GetGuestBase();
    const uint64_t end = mm.GetGuestEnd();
    if (base == 0 || end == 0) return false;
    const uint64_t va = base + kHeapOffsetInWindow;
    if (va + kHeapBytes > end) return false;
    if (!mm.MapMemory(va, kHeapBytes,
                      MemoryFlags::PAGE_READ | MemoryFlags::PAGE_WRITE,
                      "guest_heap")) {
        return false;
    }
    g_heapBase = va;
    g_heapBrk = va;
    g_heapEnd = va + kHeapBytes;
    PX5_LOGI(LogCategory::KERNEL,
             "LIBC: heap mapped [0x%llx..0x%llx] (256 MiB, bump allocator)",
             (unsigned long long)va, (unsigned long long)g_heapEnd);
    return true;
}

size_t AlignUp(size_t v, size_t a) { return (v + a - 1) & ~(a - 1); }

uint64_t HeapTake(size_t size, size_t align) {
    std::lock_guard<std::mutex> lk(g_heapMutex);
    if (!EnsureHeap()) return 0;
    if (g_sizes.size() >= kMaxLiveAllocs) {
        PX5_LOGW(LogCategory::KERNEL,
                 "LIBC: heap live-alloc table full (%zu) — refusing",
                 g_sizes.size());
        return 0;
    }
    const uint64_t va = AlignUp(g_heapBrk, align ? align : 16);
    if (size > kHeapBytes || va > g_heapEnd ||
        size > static_cast<size_t>(g_heapEnd - va)) {
        PX5_LOGW(LogCategory::KERNEL,
                 "LIBC: heap exhausted at brk=0x%llx (asked %zu bytes)",
                 (unsigned long long)g_heapBrk, size);
        return 0;
    }
    void* host = MemoryManager::GetInstance().GetHostPointer(va);
    if (!host) return 0;
    g_heapBrk = va + size;
    g_sizes.emplace(va, size);
    return va;
}

// ---------------------------------------------------------------------------
// console routing: printf/puts land in the same captured console the raw
// bridge feeds, so guest output shows up in the session log verbatim.
// ---------------------------------------------------------------------------
void ConsoleOut(const std::string& text) {
    GuestSyscalls::AppendOutput(text);
}

// ---------------------------------------------------------------------------
// printf-lite: walks the format string against the caller's argument
// sequence (call.args[0..5], then guest stack via ReadStackArg).
// v1.66 — FP conversions consume their own XMM sequence (ReadFloat) and
// format for real; a frame-less host without an XMM snapshot yields the
// named "<f>" marker instead of a fabricated number.
// ---------------------------------------------------------------------------
struct VarargSource {
    const ImportCall* call;
    size_t next = 1;                 // args[0] is the format string
    size_t fnext = 0;                // v1.66 — XMM cursor (separate sequence)
    bool Read(uint64_t* out) {
        if (next < ImportCall::kRegArgCount) {
            *out = call->args[next++];
            return true;
        }
        const size_t k = next++ - ImportCall::kRegArgCount;
        return call->ReadStackArg(k, out);
    }
    // v1.66 — SysV varargs: FP conversions consume XMM slots in their OWN
    // order (xmm0..7), independent of the integer sequence. Beyond the
    // eighth FP arg the stack layout mixes classes; that stays named-off.
    bool ReadFloat(double* out, bool singlePrecision) {
        if (fnext >= ImportCall::kXmmCount) return false;
        if (singlePrecision) {
            float f = 0.f;
            if (!call->ReadXmmFloat(fnext++, &f)) return false;
            *out = static_cast<double>(f);
            return true;
        }
        return call->ReadXmmDouble(fnext++, out);
    }
};

void AppendFormatted(std::string* out, const char* fmt, VarargSource& va) {
    size_t i = 0;
    while (fmt[i] != '\0') {
        if (fmt[i] != '%') {
            out->push_back(fmt[i++]);
            continue;
        }
        ++i;
        if (fmt[i] == '%') { out->push_back('%'); ++i; continue; }
        // flags
        bool leftJust = false, zeroPad = false, plusSign = false, alt = false;
        for (;; ++i) {
            if (fmt[i] == '-') leftJust = true;
            else if (fmt[i] == '0') zeroPad = true;
            else if (fmt[i] == '+') plusSign = true;
            else if (fmt[i] == '#') alt = true;
            else if (fmt[i] == ' ') {}
            else break;
        }
        // width
        int width = 0;
        if (fmt[i] == '*') { ++i; uint64_t w = 0; va.Read(&w); width = static_cast<int>(w); }
        else while (fmt[i] >= '0' && fmt[i] <= '9') width = width * 10 + (fmt[i++] - '0');
        // precision
        int prec = -1;
        if (fmt[i] == '.') {
            ++i; prec = 0;
            if (fmt[i] == '*') { ++i; uint64_t p = 0; va.Read(&p); prec = static_cast<int>(p); }
            else while (fmt[i] >= '0' && fmt[i] <= '9') prec = prec * 10 + (fmt[i++] - '0');
        }
        // length
        enum Len { INT, LONG, LONGLONG, SIZE } len = INT;
        if (fmt[i] == 'l') {
            ++i; len = LONG;
            if (fmt[i] == 'l') { ++i; len = LONGLONG; }
        } else if (fmt[i] == 'z') { ++i; len = SIZE; }
        else if (fmt[i] == 'h') { ++i; }

        char conv = fmt[i];
        if (conv == '\0') break;
        ++i;
        char num[48];
        const char* s = nullptr;
        std::string owned;
        switch (conv) {
        case 'd': case 'i': {
            int64_t v = 0;
            uint64_t raw = 0;
            va.Read(&raw);
            if (len == INT) v = static_cast<int32_t>(raw);
            else v = static_cast<int64_t>(raw);
            const char* neg = v < 0 ? "-" : (plusSign ? "+" : "");
            unsigned long long mag = v < 0
                ? static_cast<unsigned long long>(-(v + 1)) + 1ull
                : static_cast<unsigned long long>(v);
            snprintf(num, sizeof(num), "%s%llu", neg, mag);
            s = num;
            break;
        }
        case 'u': {
            uint64_t raw = 0; va.Read(&raw);
            snprintf(num, sizeof(num), "%llu", (unsigned long long)raw);
            s = num;
            break;
        }
        case 'x': case 'X': {
            uint64_t raw = 0; va.Read(&raw);
            snprintf(num, sizeof(num), alt ? (conv == 'x' ? "0x%llx" : "0X%llX")
                                           : (conv == 'x' ? "%llx" : "%llX"),
                     (unsigned long long)raw);
            s = num;
            break;
        }
        case 'o': {
            uint64_t raw = 0; va.Read(&raw);
            snprintf(num, sizeof(num), "%llo", (unsigned long long)raw);
            s = num;
            break;
        }
        case 'c': {
            uint64_t raw = 0; va.Read(&raw);
            owned.assign(1, static_cast<char>(raw));
            s = owned.c_str();
            break;
        }
        case 's': {
            uint64_t raw = 0; va.Read(&raw);
            size_t n = 0;
            const char* p = raw ? WinStr(raw, &n) : nullptr;
            owned = p ? std::string(p, prec >= 0
                          ? std::min<size_t>(prec, n) : n)
                      : std::string("(null)");
            s = owned.c_str();
            break;
        }
        case 'p': {
            uint64_t raw = 0; va.Read(&raw);
            snprintf(num, sizeof(num), "0x%llx", (unsigned long long)raw);
            s = raw ? num : "(nil)";
            break;
        }
        case 'f': case 'e': case 'g': case 'E': case 'G': {
            // v1.66 — the FP argument rides its own XMM sequence; with a
            // snapshot present the value formats for real, missing-XMM
            // (frame-less host) falls back to the named "<f>" marker.
            double d = 0;
            if (va.ReadFloat(&d, false)) {
                char fnum[64];
                if (prec >= 0) {
                    snprintf(fnum, sizeof fnum, "%.*f", prec, d);
                } else {
                    snprintf(fnum, sizeof fnum, "%f", d);
                }
                owned = fnum;
                s = owned.c_str();
            } else {
                s = "<f>";
            }
            break;
        }
        case 'n': {
            uint64_t raw = 0; va.Read(&raw);
            void* p = raw ? WinPtr(raw, 4) : nullptr;
            if (p) {
                int written = static_cast<int>(out->size());
                std::memcpy(p, &written, sizeof written);
            }
            s = "";
            break;
        }
        default:
            out->push_back('%');
            out->push_back(conv);
            continue;
        }
        if (!s) s = "";
        const size_t sl = std::strlen(s);
        if (sl < static_cast<size_t>(std::max(width, 0))) {
            const size_t padN = static_cast<size_t>(width) - sl;
            if (leftJust) {
                out->append(s);
                out->append(padN, ' ');
            } else {
                out->append(padN, zeroPad ? '0' : ' ');
                out->append(s);
            }
        } else {
            out->append(s);
        }
    }
}

// ---------------------------------------------------------------------------
// libc data the game imports by pointer: __error()'s per-session errno and
// the ctype table base. One 4 KiB RW page inside the window, named.
// ---------------------------------------------------------------------------
constexpr uint64_t kLibcDataOffsetInWindow = 0x10400000ull;  // +260 MiB
uint64_t g_libcDataVa = 0;

uint64_t LibcData() {
    if (g_libcDataVa != 0) return g_libcDataVa;
    auto& mm = MemoryManager::GetInstance();
    const uint64_t base = mm.GetGuestBase();
    const uint64_t end = mm.GetGuestEnd();
    if (base == 0 || end == 0) return 0;
    const uint64_t va = base + kLibcDataOffsetInWindow;
    if (va + 4096 > end) return 0;
    if (!mm.MapMemory(va, 4096,
                      MemoryFlags::PAGE_READ | MemoryFlags::PAGE_WRITE,
                      "libc_hle_data")) {
        return 0;
    }
    g_libcDataVa = va;
    return va;
}

const char* kSymbols[] = {
    "memcpy", "memmove", "memset", "memcmp", "memchr",
    "strlen", "strcmp", "strncmp", "strcpy", "strncpy", "strcat", "strncat",
    "strchr", "strrchr", "strstr", "strcasecmp", "strncasecmp", "strlcpy",
    "strdup",
    "malloc", "calloc", "realloc", "free", "memalign",
    "puts", "fputs", "printf", "snprintf", "sprintf",
    // v1.66 — the libm tranche (decoded from the game's import list)
    "sin", "cos", "tan", "asin", "acos", "atan", "atan2",
    "exp", "log", "log2", "log10", "pow", "fmod", "round",
    "sinf", "cosf",
    "__error", "_Locksyslock", "_Unlocksyslock",
    "exit", "__stack_chk_fail",
};
constexpr size_t kSymbolCount = sizeof(kSymbols) / sizeof(kSymbols[0]);

} // namespace

LibcHle& LibcHle::Instance() {
    static LibcHle inst;
    return inst;
}

void LibcHle::CollectSymbols(std::vector<SymbolEntry>* tableOut) {
    if (!tableOut) return;
    for (const char* n : kSymbols) tableOut->push_back({n, 0});
}

bool LibcHle::HasSymbol(const std::string& symbol) const {
    for (const char* n : kSymbols)
        if (symbol == n) return true;
    return false;
}

uint64_t LibcHle::Invoke(const std::string& symbol, const ImportCall& call,
                         uint64_t* xmmRetOut) {
    const uint64_t a0 = call.args[0];
    const uint64_t a1 = call.args[1];
    const uint64_t a2 = call.args[2];
    const auto s32 = [](int32_t rc) -> uint64_t {
        return static_cast<uint64_t>(static_cast<int64_t>(rc));
    };

    // --- libm (v1.66 — the game's decoded math imports) --------------------
    // Double args ride XMM0/XMM1 (float for the *f forms); the result
    // publishes through the bridge's double-return channel into XMM0.
    // A missing XMM snapshot (frame-less hosts) refuses named instead of
    // computing on garbage.
    if (xmmRetOut) {
        if (!call.xmmValid) {
            // No live-frame snapshot (frame-less host): computing on a
            // fabricated zero would corrupt game math invisibly. Refuse
            // named through RAX; XMM0 stays untouched.
            PX5_LOGW(LogCategory::KERNEL,
                     "LIBC: %s refused — no XMM snapshot on the import "
                     "bridge (frame-less host)",
                     symbol.c_str());
            return s32(-ENODATA);
        }
        double x = 0, y = 0;
        float xf = 0.f;
        auto fbits = [](double d) -> uint64_t {
            uint64_t b = 0;
            std::memcpy(&b, &d, sizeof b);
            return b;
        };

        auto d0 = [&]() { return call.ReadXmmDouble(0, &x); };
        auto d1 = [&]() { return call.ReadXmmDouble(1, &y); };

        if (symbol == "sin")    { if (!d0()) return s32(-ENODATA); *xmmRetOut = fbits(std::sin(x));   return 0; }
        if (symbol == "cos")    { if (!d0()) return s32(-ENODATA); *xmmRetOut = fbits(std::cos(x));   return 0; }
        if (symbol == "tan")    { if (!d0()) return s32(-ENODATA); *xmmRetOut = fbits(std::tan(x));   return 0; }
        if (symbol == "asin")   { if (!d0()) return s32(-ENODATA); *xmmRetOut = fbits(std::asin(x));  return 0; }
        if (symbol == "acos")   { if (!d0()) return s32(-ENODATA); *xmmRetOut = fbits(std::acos(x));  return 0; }
        if (symbol == "atan")   { if (!d0()) return s32(-ENODATA); *xmmRetOut = fbits(std::atan(x));  return 0; }
        if (symbol == "atan2")  { if (!d0() || !d1()) return s32(-ENODATA); *xmmRetOut = fbits(std::atan2(x, y)); return 0; }
        if (symbol == "exp")    { if (!d0()) return s32(-ENODATA); *xmmRetOut = fbits(std::exp(x));   return 0; }
        if (symbol == "log")    { if (!d0()) return s32(-ENODATA); *xmmRetOut = fbits(std::log(x));   return 0; }
        if (symbol == "log2")   { if (!d0()) return s32(-ENODATA); *xmmRetOut = fbits(std::log2(x));  return 0; }
        if (symbol == "log10")  { if (!d0()) return s32(-ENODATA); *xmmRetOut = fbits(std::log10(x)); return 0; }
        if (symbol == "pow")    { if (!d0() || !d1()) return s32(-ENODATA); *xmmRetOut = fbits(std::pow(x, y)); return 0; }
        if (symbol == "fmod")   { if (!d0() || !d1()) return s32(-ENODATA); *xmmRetOut = fbits(std::fmod(x, y)); return 0; }
        if (symbol == "round")  { if (!d0()) return s32(-ENODATA); *xmmRetOut = fbits(std::round(x)); return 0; }
        if (symbol == "sinf")   { if (!call.ReadXmmFloat(0, &xf)) return s32(-ENODATA);
                                  const float r = std::sin(xf); uint32_t fb = 0;
                                  std::memcpy(&fb, &r, sizeof fb);
                                  *xmmRetOut = fb; return 0; }   // SysV: float ret = XMM0[31:0]
        if (symbol == "cosf")   { if (!call.ReadXmmFloat(0, &xf)) return s32(-ENODATA);
                                  const float r = std::cos(xf); uint32_t fb = 0;
                                  std::memcpy(&fb, &r, sizeof fb);
                                  *xmmRetOut = fb; return 0; }   // SysV: float ret = XMM0[31:0]
    }

    // --- memory ------------------------------------------------------------
    if (symbol == "memcpy" || symbol == "memmove") {
        size_t n = static_cast<size_t>(a2);
        void* d = n ? WinPtr(a0, n) : nullptr;
        const void* s = n ? WinPtr(a1, n) : nullptr;
        if (!d || !s) return s32(EFAULT);
        std::memmove(d, s, n);
        return a0;
    }
    if (symbol == "memset") {
        size_t n = static_cast<size_t>(a2);
        void* d = n ? WinPtr(a0, n) : nullptr;
        if (!d) return s32(EFAULT);
        std::memset(d, static_cast<int>(a1 & 0xff), n);
        return a0;
    }
    if (symbol == "memcmp") {
        size_t n = static_cast<size_t>(a2);
        const void* p = n ? WinPtr(a0, n) : nullptr;
        const void* q = n ? WinPtr(a1, n) : nullptr;
        if (!p || !q) return s32(EFAULT);
        return s32(std::memcmp(p, q, n));
    }
    if (symbol == "memchr") {
        size_t n = static_cast<size_t>(a2);
        const void* p = n ? WinPtr(a0, n) : nullptr;
        if (!p) return 0;
        const void* hit = std::memchr(p, static_cast<int>(a1 & 0xff), n);
        if (!hit) return 0;
        return a0 + (static_cast<const uint8_t*>(hit) -
                     static_cast<const uint8_t*>(p));
    }

    // --- strings -------------------------------------------------------------
    if (symbol == "strlen") {
        size_t n = 0;
        if (!WinStr(a0, &n)) return s32(EFAULT);
        return n;
    }
    if (symbol == "strcmp" || symbol == "strcasecmp") {
        size_t x = 0, y = 0;
        const char* p = WinStr(a0, &x);
        const char* q = WinStr(a1, &y);
        if (!p || !q) return s32(EFAULT);
        return s32(symbol == "strcmp" ? std::strcmp(p, q)
                                      : ::strcasecmp(p, q));
    }
    if (symbol == "strncmp" || symbol == "strncasecmp") {
        size_t n = static_cast<size_t>(a2);
        size_t x = 0, y = 0;
        const char* p = WinStr(a0, &x);
        const char* q = WinStr(a1, &y);
        if (!p || !q) return s32(EFAULT);
        return s32(symbol == "strncmp" ? std::strncmp(p, q, n)
                                       : ::strncasecmp(p, q, n));
    }
    if (symbol == "strcpy" || symbol == "strlcpy") {
        size_t n = 0;
        const char* s = WinStr(a1, &n);
        if (!s) return s32(EFAULT);
        char* d = static_cast<char*>(WinPtr(a0, n + 1));
        if (!d) return s32(EFAULT);
        if (symbol == "strcpy") {
            std::memcpy(d, s, n + 1);
            return a0;
        }
        // strlcpy: copies up to dstsize-1, always NUL-terminates, returns
        // the length of the SOURCE string.
        const size_t cap = static_cast<size_t>(a2);
        if (cap > 0) {
            const size_t cp = std::min(n, cap - 1);
            std::memcpy(d, s, cp);
            d[cp] = '\0';
        }
        return n;
    }
    if (symbol == "strncpy") {
        size_t n = static_cast<size_t>(a2);
        const char* s = WinPtr(a1, 1) ? static_cast<const char*>(WinPtr(a1, 1))
                                      : nullptr;
        char* d = WinPtr(a0, 1) ? static_cast<char*>(WinPtr(a0, 1)) : nullptr;
        if (!s || !d) return s32(EFAULT);
        size_t i = 0;
        for (; i < n && s[i] != '\0'; ++i) d[i] = s[i];
        for (; i < n; ++i) d[i] = '\0';
        return a0;
    }
    if (symbol == "strcat" || symbol == "strncat") {
        size_t dn = 0, sn = 0;
        char* d = WinStr(a0, &dn);
        const char* s = WinStr(a1, &sn);
        if (!d || !s) return s32(EFAULT);
        size_t n = sn;
        if (symbol == "strncat") n = std::min(n, static_cast<size_t>(a2));
        std::memcpy(d + dn, s, n);
        d[dn + n] = '\0';
        return a0;
    }
    if (symbol == "strchr" || symbol == "strrchr") {
        size_t n = 0;
        const char* p = WinStr(a0, &n);
        if (!p) return 0;
        const char c = static_cast<char>(a1);
        if (symbol == "strchr") {
            for (size_t i = 0; i <= n; ++i)
                if (p[i] == c) return a0 + i;
        } else {
            for (size_t i = n + 1; i-- > 0;)
                if (p[i] == c) return a0 + i;
        }
        return 0;
    }
    if (symbol == "strstr") {
        size_t hn = 0, nn = 0;
        const char* h = WinStr(a0, &hn);
        const char* needle = WinStr(a1, &nn);
        if (!h || !needle) return 0;
        if (nn == 0) return a0;
        for (size_t i = 0; i + nn <= hn; ++i)
            if (std::memcmp(h + i, needle, nn) == 0) return a0 + i;
        return 0;
    }
    if (symbol == "strdup") {
        size_t n = 0;
        const char* s = WinStr(a0, &n);
        if (!s) return 0;
        const uint64_t va = HeapTake(n + 1, 16);
        if (!va) return 0;
        std::memcpy(WinPtr(va, n + 1), s, n + 1);
        return va;
    }

    // --- heap ------------------------------------------------------------------
    if (symbol == "malloc") return HeapTake(static_cast<size_t>(a0), 16);
    if (symbol == "calloc") return HeapCalloc(static_cast<size_t>(a0),
                                              static_cast<size_t>(a1));
    if (symbol == "realloc") return HeapRealloc(a0, static_cast<size_t>(a1));
    if (symbol == "memalign") {
        const size_t align = static_cast<size_t>(a0);
        if (align == 0 || (align & (align - 1)) != 0) return s32(EINVAL);
        return HeapTake(static_cast<size_t>(a1), align);
    }
    if (symbol == "free") {
        HeapFree(a0);
        return 0;
    }

    // --- stdio ------------------------------------------------------------------
    if (symbol == "puts") {
        size_t n = 0;
        const char* s = WinStr(a0, &n);
        if (!s) return s32(EFAULT);
        ConsoleOut(std::string(s, n) + "\n");
        return 1;   // nonneg on success
    }
    if (symbol == "fputs") {
        size_t n = 0;
        const char* s = WinStr(a0, &n);
        if (!s) return s32(EFAULT);
        ConsoleOut(std::string(s, n));
        return 1;
    }
    if (symbol == "printf") {
        size_t n = 0;
        const char* f = WinStr(a0, &n);
        if (!f) return s32(EFAULT);
        std::string out;
        VarargSource va{&call, 1};
        AppendFormatted(&out, f, va);
        ConsoleOut(out);
        return static_cast<uint64_t>(static_cast<int64_t>(out.size()));
    }
    if (symbol == "snprintf" || symbol == "sprintf") {
        // snprintf(dst, dstsize, fmt, ...) — fmt = a2, varargs from a3
        // sprintf(dst, fmt, ...)         — fmt = a1, varargs from a2
        const bool isSn = symbol == "snprintf";
        size_t n = 0;
        const char* f = WinStr(isSn ? a2 : a1, &n);
        if (!f) return s32(EFAULT);
        std::string out;
        VarargSource va{&call, isSn ? 3u : 2u};
        AppendFormatted(&out, f, va);
        const uint64_t dstVa = a0;
        if (isSn) {
            const size_t cap = static_cast<size_t>(a1);
            char* d = cap ? static_cast<char*>(WinPtr(dstVa, cap)) : nullptr;
            if (!d && cap) return s32(EFAULT);
            if (cap) {
                const size_t cp = std::min(out.size(), cap - 1);
                std::memcpy(d, out.data(), cp);
                d[cp] = '\0';
            }
        } else {
            char* d = static_cast<char*>(WinPtr(dstVa, out.size() + 1));
            if (!d) return s32(EFAULT);
            std::memcpy(d, out.data(), out.size() + 1);
        }
        return static_cast<uint64_t>(static_cast<int64_t>(out.size()));
    }

    // --- libc data + locks ----------------------------------------------------
    if (symbol == "__error") {
        // Returns a pointer to one persistent errno slot for the session.
        const uint64_t data = LibcData();
        return data ? data : 0;
    }
    if (symbol == "_Locksyslock" || symbol == "_Unlocksyslock") {
        // Single-guest-thread runtime: the lock object exists but contends
        // with nothing. First acquisition is ledgered for honesty.
        static bool logged = false;
        if (!logged) {
            logged = true;
            Evidence::AppendLedger("libc %s (single-thread no-op)",
                                   symbol.c_str());
        }
        return 0;
    }

    // --- exit paths -------------------------------------------------------------
    if (symbol == "exit") {
        PX5_LOGW(LogCategory::KERNEL,
                 "[GUEST] exit(%lld) — the process-exit path is the "
                 "syscall bridge's exit_group; this libc frame cannot "
                 "invoke it yet, the guest keeps running",
                 static_cast<long long>(static_cast<int32_t>(a0)));
        Evidence::AppendLedger("libc exit(%lld) (no exit path yet)",
                               (long long)static_cast<int32_t>(a0));
        return 0;
    }
    if (symbol == "__stack_chk_fail") {
        PX5_LOGE(LogCategory::KERNEL,
                 "[GUEST] __stack_chk_fail — the guest's canary check "
                 "tripped; reported, not aborted (no abort path yet)");
        Evidence::AppendLedger("libc __stack_chk_fail hit");
        return 0;
    }

    return static_cast<uint64_t>(-static_cast<int64_t>(EINVAL));
}

uint64_t LibcHle::HeapAlloc(size_t size, size_t align) {
    return HeapTake(size, align);
}

uint64_t LibcHle::HeapCalloc(size_t n, size_t size) {
    if (n != 0 && size > (1u << 28) / n) return 0;   // overflow guard
    const size_t total = n * size;
    const uint64_t va = HeapTake(total, 16);
    if (!va) return 0;
    void* p = WinPtr(va, total);
    if (p) std::memset(p, 0, total);
    return va;
}

uint64_t LibcHle::HeapRealloc(uint64_t oldVa, size_t size) {
    if (!oldVa) return HeapTake(size, 16);
    const size_t oldSize = HeapSizeOf(oldVa);
    const uint64_t va = HeapTake(size, 16);
    if (!va) return 0;
    if (oldSize) {
        const size_t cp = std::min(oldSize, size);
        void* d = WinPtr(va, cp);
        const void* s = WinPtr(oldVa, cp);
        if (d && s) std::memcpy(d, s, cp);
    }
    return va;
}

void LibcHle::HeapFree(uint64_t va) {
    // Bump allocator: per-free reclamation is not implemented (see header).
    (void)va;
}

size_t LibcHle::HeapSizeOf(uint64_t va) const {
    std::lock_guard<std::mutex> lk(g_heapMutex);
    auto it = g_sizes.find(va);
    return it == g_sizes.end() ? 0 : it->second;
}

} // namespace PX5::SceKernelHle
