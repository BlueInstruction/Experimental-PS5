// SPDX-License-Identifier: MIT
// PX5 v1.31 — runtime linker implementation. See runtime_linker.h for the
// contract (registry + NID gate + bounded PT_DYNAMIC reader).

#include "runtime_linker.h"

#include "../utils/logger.h"

#include <cstdio>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace PX5 {

namespace {

// Bounded dynamic walk: a corrupt/unbounded table stops after this many
// entries and is reported as malformed, never swept to infinity.
constexpr size_t kMaxDynEntries = 8192;
// SCE reserves the 0x61000000-0x610000FF OS-specific dynamic-tag range
// (Kyty src/loader/elf.h: DT_OS_EXPORT_LIB = 0x61000013 et al.; shadPS4
// uses the same range as DT_SCE_*). Values are enumerated verbatim.
constexpr uint64_t kSceTagLo = 0x61000000ull;
constexpr uint64_t kSceTagHi = 0x610000FFull;

constexpr uint32_t PT_LOAD    = 1;
constexpr uint32_t PT_DYNAMIC = 2;

// Dynamic tags we interpret (values are the ELF standard's, verbatim).
constexpr uint64_t DT_NULL    = 0;
constexpr uint64_t DT_NEEDED  = 1;
constexpr uint64_t DT_STRTAB  = 5;
constexpr uint64_t DT_STRSZ   = 10;
constexpr uint64_t DT_SONAME  = 14;

struct LoadRange {           // PT_LOAD slice usable for vaddr translation
    uint64_t vaddr;
    uint64_t offset;
    uint64_t filesz;
};

bool Rd16(const uint8_t* d, size_t size, size_t off, uint16_t& v) {
    if (off + 2 > size) return false;
    memcpy(&v, d + off, 2);            // image is LE; hosts here are LE
    return true;
}
bool Rd32(const uint8_t* d, size_t size, size_t off, uint32_t& v) {
    if (off + 4 > size) return false;
    memcpy(&v, d + off, 4);
    return true;
}
bool Rd64(const uint8_t* d, size_t size, size_t off, uint64_t& v) {
    if (off + 8 > size) return false;
    memcpy(&v, d + off, 8);
    return true;
}

bool VaddrToOffset(const std::vector<LoadRange>& loads, uint64_t va,
                   uint64_t& out) {
    for (const auto& l : loads) {
        if (va >= l.vaddr && va < l.vaddr + l.filesz) {
            out = l.offset + (va - l.vaddr);
            return true;
        }
    }
    return false;
}

} // namespace

// ---------------------------------------------------------------------------
// Registry
// ---------------------------------------------------------------------------

RuntimeLinker& RuntimeLinker::GetInstance() {
    static RuntimeLinker inst;
    return inst;
}

void RuntimeLinker::Reset() {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_modules.clear();
    m_exports.clear();
    m_stats = {};
}

bool RuntimeLinker::RegisterModule(const std::string& name, uint64_t base,
                                   uint64_t highVa, uint64_t entry,
                                   bool isSelf, size_t segmentCount) {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (highVa <= base) return false;
    m_modules.push_back(ModuleRecord{name, base, highVa, entry, isSelf,
                                     segmentCount});
    PX5_LOGI(LogCategory::LOADER,
             "RuntimeLinker: module '%s' base=0x%llx high=0x%llx entry=0x%llx "
             "self=%d segs=%zu", name.c_str(),
             (unsigned long long)base, (unsigned long long)highVa,
             (unsigned long long)entry, isSelf ? 1 : 0, segmentCount);
    return true;
}

bool RuntimeLinker::RegisterHleExport(const std::string& library, uint64_t nid,
                                      const std::string& name, HleHostFn fn) {
    if (!fn) return false;
    std::lock_guard<std::mutex> lk(m_mutex);
    const auto [it, inserted] = m_exports.try_emplace(
        nid, NidEntry{library, name, /*isHle=*/true, std::move(fn), 0});
    if (!inserted) {
        PX5_LOGW(LogCategory::LOADER,
                 "RuntimeLinker: duplicate NID 0x%08llx ('%s' vs existing '%s')",
                 (unsigned long long)nid, name.c_str(),
                 it->second.name.c_str());
        return false;
    }
    return true;
}

bool RuntimeLinker::RegisterGuestExport(uint64_t nid, uint64_t guestAddr,
                                        const std::string& name) {
    if (guestAddr == 0) return false;
    std::lock_guard<std::mutex> lk(m_mutex);
    const auto [it, inserted] = m_exports.try_emplace(
        nid, NidEntry{"", name, /*isHle=*/false, nullptr, guestAddr});
    if (!inserted) {
        PX5_LOGW(LogCategory::LOADER,
                 "RuntimeLinker: duplicate NID 0x%08llx (guest export '%s')",
                 (unsigned long long)nid, name.c_str());
        return false;
    }
    return true;
}

GateResult RuntimeLinker::DispatchNid(uint64_t nid, const uint64_t* args,
                                      size_t argc) {
    GateResult r;
    HleHostFn fn;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        ++m_stats.gateCalls;
        const auto it = m_exports.find(nid);
        if (it == m_exports.end()) {
            ++m_stats.unresolved;
            r.error = "NID 0x" + [&]{ char b[24];
                        snprintf(b, sizeof(b), "%08llx",
                                 (unsigned long long)nid);
                        return std::string(b); }() +
                      " not registered";
            PX5_LOGW(LogCategory::LOADER, "RuntimeLinker: %s", r.error.c_str());
            return r;
        }
        if (!it->second.isHle) {
            ++m_stats.guestRouted;
            r.error = "NID 0x" + [&]{ char b[24];
                        snprintf(b, sizeof(b), "%08llx",
                                 (unsigned long long)nid);
                        return std::string(b); }() +
                      " is a guest export — the host gate does not execute "
                      "guest code (direct guest call expected)";
            PX5_LOGW(LogCategory::LOADER, "RuntimeLinker: %s", r.error.c_str());
            return r;
        }
        ++m_stats.resolvedHle;
        fn = it->second.hle;    // copy under lock, run unlocked
    }
    r.ok = true;
    r.value = fn(args, argc);   // HLE may re-enter the registry safely
    return r;
}

const DispatchStats& RuntimeLinker::Stats() {
    return m_stats;             // read-mostly evidence, benign races OK
}

std::string RuntimeLinker::GetSummaryString() {
    std::lock_guard<std::mutex> lk(m_mutex);
    char b[160];
    snprintf(b, sizeof(b),
             "modules=%zu exports=%zu gateCalls=%llu hle=%llu guest=%llu "
             "unresolved=%llu",
             m_modules.size(), m_exports.size(),
             (unsigned long long)m_stats.gateCalls,
             (unsigned long long)m_stats.resolvedHle,
             (unsigned long long)m_stats.guestRouted,
             (unsigned long long)m_stats.unresolved);
    return std::string(b);
}

size_t RuntimeLinker::ModuleCount() {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_modules.size();
}

size_t RuntimeLinker::ExportCount() {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_exports.size();
}

// ---------------------------------------------------------------------------
// PT_DYNAMIC reader
// ---------------------------------------------------------------------------

DynamicInfo ParseDynamicFromElfImage(const uint8_t* data, size_t size) {
    DynamicInfo out;
    if (!data || size < 64) {
        out.error = "image too small for an ELF64 header";
        return out;
    }
    static const uint8_t kElfMagic[4] = {0x7F, 'E', 'L', 'F'};
    if (memcmp(data, kElfMagic, 4) != 0) {
        out.error = "bad ELF magic";
        return out;
    }
    if (data[4] != 2)  { out.error = "not ELF64 (EI_CLASS)"; return out; }
    if (data[5] != 1)  { out.error = "not little-endian (EI_DATA)"; return out; }

    uint64_t phoff = 0;
    uint16_t phentsize = 0, phnum = 0;
    if (!Rd64(data, size, 0x20, phoff) ||
        !Rd16(data, size, 0x36, phentsize) ||
        !Rd16(data, size, 0x38, phnum)) {
        out.error = "ELF header fields out of bounds";
        return out;
    }
    if (phnum == 0)          { out.error = "no program headers"; return out; }
    if (phentsize != 56) {
        out.error = "unexpected e_phentsize=" + std::to_string(phentsize);
        return out;
    }
    if (phoff + (uint64_t)phnum * 56 > size) {
        out.error = "phdr table out of bounds";
        return out;
    }

    std::vector<LoadRange> loads;
    uint64_t dynOff = 0, dynSz = 0;
    bool haveDyn = false;
    for (uint16_t i = 0; i < phnum; ++i) {
        const size_t base = phoff + (size_t)i * 56;
        uint32_t ptype = 0;
        uint64_t poff = 0, pvaddr = 0, pfilesz = 0;
        if (!Rd32(data, size, base, ptype) ||
            !Rd64(data, size, base + 8, poff) ||
            !Rd64(data, size, base + 0x10, pvaddr) ||
            !Rd64(data, size, base + 0x20, pfilesz)) {
            out.error = "phdr entry out of bounds";
            return out;
        }
        if (ptype == PT_LOAD && pfilesz > 0) {
            if (poff + pfilesz > size) continue;   // can't translate from it
            loads.push_back(LoadRange{pvaddr, poff, pfilesz});
        } else if (ptype == PT_DYNAMIC && pfilesz > 0) {
            dynOff = poff;
            dynSz  = pfilesz;
            haveDyn = true;
        }
    }
    if (!haveDyn) { out.error = "no PT_DYNAMIC segment"; return out; }
    if (dynOff + dynSz > size) {
        out.error = "PT_DYNAMIC segment out of bounds";
        return out;
    }

    // Pass 1: walk entries, defer string resolution until DT_STRTAB lands.
    uint64_t strVa = 0;
    uint64_t strSz = 0;
    bool haveStr = false;
    std::vector<uint64_t> neededOffs;
    uint64_t sonameOff = 0;
    bool haveSoname = false;
    const size_t maxN = dynSz / 16;
    for (size_t i = 0; i < maxN && i < kMaxDynEntries; ++i) {
        uint64_t tag = 0, val = 0;
        if (!Rd64(data, size, dynOff + i * 16, tag) ||
            !Rd64(data, size, dynOff + i * 16 + 8, val)) {
            out.error = "dynamic entry out of bounds";
            return out;
        }
        if (tag == DT_NULL) break;
        ++out.dynEntries;
        switch (tag) {
        case DT_STRTAB: strVa = val; haveStr = true; break;
        case DT_STRSZ:  strSz = val; break;
        case DT_NEEDED: neededOffs.push_back(val); break;
        case DT_SONAME: sonameOff = val; haveSoname = true; break;
        default:
            if (tag >= kSceTagLo && tag <= kSceTagHi)
                out.sceTags.emplace_back(tag, val);
            break;
        }
    }
    if (out.dynEntries >= kMaxDynEntries && maxN >= kMaxDynEntries) {
        out.error = "dynamic table exceeds the entry bound (corrupt?)";
        return out;
    }
    if (!haveStr) { out.error = "DT_STRTAB missing"; return out; }
    if (strSz == 0 || strSz > (1ull << 20)) {
        out.error = "DT_STRSZ missing or beyond the 1 MiB bound";
        return out;
    }
    uint64_t strOff = 0;
    if (!VaddrToOffset(loads, strVa, strOff)) {
        out.error = "DT_STRTAB vaddr not covered by any readable PT_LOAD";
        return out;
    }
    if (strOff + strSz > size) {
        out.error = "string table out of image bounds";
        return out;
    }

    auto readStr = [&](uint64_t off, std::string& s) -> bool {
        if (off >= strSz) return false;
        const size_t cap = static_cast<size_t>(strSz - off);
        const char* p = reinterpret_cast<const char*>(data + strOff + off);
        size_t n = strnlen(p, cap);
        if (n == cap) return false;            // unterminated: malformed
        s.assign(p, n);
        return true;
    };

    for (const uint64_t off : neededOffs) {
        std::string s;
        if (!readStr(off, s)) {
            out.error = "DT_NEEDED offset " + std::to_string(off) +
                        " malformed in the string table";
            return out;
        }
        out.needed.push_back(std::move(s));
    }
    if (haveSoname && !readStr(sonameOff, out.soname)) {
        out.error = "DT_SONAME offset malformed in the string table";
        return out;
    }

    out.ok = true;
    char b[128];
    snprintf(b, sizeof(b), "dynEntries=%zu needed=%zu soname='%s' sceTags=%zu",
             out.dynEntries, out.needed.size(), out.soname.c_str(),
             out.sceTags.size());
    out.summary = b;
    return out;
}

} // namespace PX5
