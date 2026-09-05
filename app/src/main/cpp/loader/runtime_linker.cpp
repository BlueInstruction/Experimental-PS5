// SPDX-License-Identifier: MIT
// PX5 v1.31 — runtime linker implementation. See runtime_linker.h for the
// contract (registry + NID gate + bounded PT_DYNAMIC reader).

#include "runtime_linker.h"

#include "../utils/evidence.h"
#include "../utils/logger.h"

#include <algorithm>
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
// Missing-NID accounting bounds (Vita3K pattern, modules/module_parent.cpp:
// 152-177): the unique set never grows past kMaxTrackedMissingNids, and
// the evidence ledger only records the first kMaxMissingLedger events —
// a game calling an unimplemented import 100k times must not flood either.
constexpr size_t kMaxTrackedMissingNids = 4096;
constexpr int     kMaxMissingLedger     = 64;
// Import-trap ledger bound (v1.45): same flood discipline as the NID
// ledger — first 64 DISTINCT named misses go to px5_evidence.log, the
// rest live in the counters only.
constexpr uint64_t kMaxTrapLedger = 64;
// Import-trap out-of-range log bound: a corrupted guest calling garbage
// indices must not flood logcat either.
constexpr uint64_t kMaxTrapOobLogs = 8;
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
    m_missingNids.clear();
    m_stats = {};
    // v1.45 — the import-trap table belongs to the loaded image; a reset
    // (new run / foundation suite) drops it with everything else.
    m_importTraps.clear();
    m_trapHits.clear();
    m_trapRegionBase = 0;
    m_trapRegionEnd = 0;
    m_trapTotalHits = 0;
    m_trapDistinct = 0;
    m_trapLedgered = 0;
    m_trapOob = 0;
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
            // v1.42 — Vita3K missing-NID accounting: log and ledger the
            // FIRST hit per distinct NID only; repeat hits are counted,
            // not spammed (modules/module_parent.cpp:171-175).
            auto miss = m_missingNids.find(nid);
            if (miss == m_missingNids.end()) {
                if (m_missingNids.size() < kMaxTrackedMissingNids) {
                    m_missingNids[nid] = 1;
                    ++m_stats.unresolvedUnique;
                    if (m_stats.unresolvedUnique <= kMaxMissingLedger) {
                        Evidence::AppendLedger(
                            "nid miss nid=0x%08llx unique=%llu",
                            (unsigned long long)nid,
                            (unsigned long long)m_stats.unresolvedUnique);
                    }
                    PX5_LOGW(LogCategory::LOADER,
                             "RuntimeLinker: %s (missing-NID %llu unique)",
                             r.error.c_str(),
                             (unsigned long long)m_stats.unresolvedUnique);
                }
            } else {
                ++miss->second;
            }
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
             "unresolved=%llu (unique=%llu)",
             m_modules.size(), m_exports.size(),
             (unsigned long long)m_stats.gateCalls,
             (unsigned long long)m_stats.resolvedHle,
             (unsigned long long)m_stats.guestRouted,
             (unsigned long long)m_stats.unresolved,
             (unsigned long long)m_stats.unresolvedUnique);
    return std::string(b);
}

// v1.42 — bounded top-N missing-NID list, hottest first. This names the
// next HLE work item from the device session itself (Vita3K's
// missing_nids, but rendered as evidence instead of a private set).
std::string RuntimeLinker::GetMissingNidsSummary() {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (m_missingNids.empty()) return "missing NIDs: none";
    // Collect (nid, hits) and take the top 8 by hits.
    std::vector<std::pair<uint64_t, uint32_t>> items(m_missingNids.begin(),
                                                     m_missingNids.end());
    std::sort(items.begin(), items.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    if (items.size() > 8) items.resize(8);
    std::string out = "missing NIDs (top " + std::to_string(items.size()) +
                      " of " + std::to_string(m_stats.unresolvedUnique) +
                      "):";
    char one[64];
    for (const auto& [nid, hits] : items) {
        snprintf(one, sizeof(one), " 0x%08llx(x%u)",
                 (unsigned long long)nid, hits);
        out += one;
    }
    return out;
}

size_t RuntimeLinker::MissingNidCount() {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_missingNids.size();
}

size_t RuntimeLinker::ModuleCount() {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_modules.size();
}

// ---------------------------------------------------------------------------
// Import traps (v1.45) — see runtime_linker.h for the contract.
// ---------------------------------------------------------------------------

void RuntimeLinker::SetImportTraps(uint64_t regionBase, uint64_t regionEnd,
                                   std::vector<ImportTrapEntry> entries) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_trapRegionBase = regionBase;
    m_trapRegionEnd  = regionEnd;
    m_importTraps    = std::move(entries);
    m_trapHits.assign(m_importTraps.size(), 0);
    m_trapTotalHits = 0;
    m_trapDistinct  = 0;
    m_trapLedgered  = 0;
    m_trapOob       = 0;
}

uint64_t RuntimeLinker::DispatchImportTrap(uint64_t importIndex) {
    std::string name;
    bool firstHit = false;
    bool ledger = false;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (importIndex >= m_importTraps.size()) {
            // A guest can only land here with a corrupted/stale stub —
            // count and bound the noise, never trust the index.
            ++m_trapOob;
            if (m_trapOob <= kMaxTrapOobLogs) {
                PX5_LOGW(LogCategory::LOADER,
                         "IMPORT-TRAP: index %llu out of range (stubs=%zu) "
                         "— stale/corrupt guest stub call refused",
                         (unsigned long long)importIndex,
                         m_importTraps.size());
            }
            return 0;
        }
        auto& e = m_importTraps[importIndex];
        uint32_t& hits = m_trapHits[importIndex];
        if (hits == 0) {
            firstHit = true;
            ++m_trapDistinct;
            if (m_trapLedgered < kMaxTrapLedger) {
                ++m_trapLedgered;
                ledger = true;
            }
        }
        ++hits;
        ++m_trapTotalHits;
        name = e.name;
    }
    // Evidence + logging outside m_mutex (AppendLedger fsyncs; the HLE
    // gate path may re-enter the registry).
    if (firstHit) {
        PX5_LOGW(LogCategory::LOADER,
                 "IMPORT-TRAP hit idx=%llu name='%s' — missing import "
                 "called by guest, RAX=0 returned (hit #%llu distinct)",
                 (unsigned long long)importIndex, name.c_str(),
                 (unsigned long long)m_trapDistinct);
        if (ledger) {
            Evidence::AppendLedger("import miss idx=%llu name='%s'",
                                   (unsigned long long)importIndex,
                                   name.c_str());
        }
    }
    return 0;
}

std::string RuntimeLinker::GetImportTrapSummary() {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (m_importTraps.empty()) return "import traps: none installed";
    // Top 8 by hits — the concrete next-HLE worklist, hottest first.
    std::vector<std::pair<size_t, uint32_t>> items;
    items.reserve(m_importTraps.size());
    for (size_t i = 0; i < m_importTraps.size(); ++i)
        if (m_trapHits[i] > 0) items.emplace_back(i, m_trapHits[i]);
    std::sort(items.begin(), items.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    const size_t show = std::min<size_t>(items.size(), 8);
    std::string out = "import traps: stubs=" +
                      std::to_string(m_importTraps.size()) +
                      " region=[0x" +
                      [&]{ char b[24]; snprintf(b, sizeof(b), "%llx",
                           (unsigned long long)m_trapRegionBase);
                           return std::string(b); }() +
                      "..0x" +
                      [&]{ char b[24]; snprintf(b, sizeof(b), "%llx",
                           (unsigned long long)m_trapRegionEnd);
                           return std::string(b); }() +
                      "] hits=" + std::to_string(m_trapTotalHits) +
                      " (distinct=" + std::to_string(m_trapDistinct) +
                      ")";
    if (show == 0) return out;
    out += " top:";
    char one[128];
    for (size_t i = 0; i < show; ++i) {
        const auto& e = m_importTraps[items[i].first];
        snprintf(one, sizeof(one), " '%s'(x%u)",
                 e.name.c_str(), items[i].second);
        out += one;
    }
    return out;
}

size_t RuntimeLinker::ImportTrapCount() {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_importTraps.size();
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
