// SPDX-License-Identifier: MIT
// PX5 Phase C milestone 2a — SELF container extractor (implementation).
// v1.29: layout corrected to the orbis map shadPS4's production parser
// uses (see self_extract.h header comment for the field-by-field source).
// v1.30: resolution corrected to shadPS4's REAL contract — the SELF
// entry table and the inner phdr table are independent counts (real
// eboot.bin: 12 entries vs 14 phdrs, vc30 device log). A phdr's bytes
// are served by the Blocked entry whose id field names the phdr index
// (shadPS4 elf.cpp Elf::LoadSegment); the v1.29 by-order pairing and
// its count-equality gate were inventions, refuted on device, removed.

#include "loader/self_extract.h"

#include <zlib.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace PX5::SelfExtract {

namespace {

#pragma pack(push, 1)
struct SelfHeaderRaw {          // 0x20 @ 0x00 (orbis, shadPS4-verified)
    uint32_t magic;
    uint8_t  version;
    uint8_t  mode;
    uint8_t  endian;
    uint8_t  attributes;
    uint8_t  category;
    uint8_t  programType;
    uint16_t padding1;
    uint16_t headerSize;
    uint16_t metaSize;
    uint32_t fileSize;
    uint32_t padding2;
    uint16_t segmentCount;
    uint16_t unknown1A;
    uint32_t padding3;
};
static_assert(sizeof(SelfHeaderRaw) == 0x20, "SELF header must be 0x20");

struct SegmentEntryRaw {        // 0x20 each @0x20 + i*0x20
    uint64_t flags;
    uint64_t fileOffset;
    uint64_t fileSize;          // stored bytes
    uint64_t memorySize;        // bytes in memory
};
static_assert(sizeof(SegmentEntryRaw) == 0x20, "segment entry must be 0x20");

struct Elf64HeaderRaw {
    uint8_t  ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
};
static_assert(sizeof(Elf64HeaderRaw) == 0x40, "inner ELF header must be 0x40");

struct Elf64PhdrRaw {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
};
static_assert(sizeof(Elf64PhdrRaw) == 0x38, "phdr must be 0x38");
#pragma pack(pop)

template <typename T>
bool ReadStruct(const uint8_t* data, size_t size, size_t offset, T* out) {
    if (offset > size || sizeof(T) > size - offset) return false;
    memcpy(out, data + offset, sizeof(T));
    return true;
}

// Bounded zlib inflate of one segment into `out` (already sized).
bool InflateSegment(const uint8_t* src, uint64_t srcLen,
                    uint8_t* dst, uint64_t dstLen, std::string* err) {
    z_stream zs{};
    if (inflateInit(&zs) != Z_OK) {
        if (err) *err = "zlib init failed";
        return false;
    }
    zs.next_in  = const_cast<uint8_t*>(src);
    zs.avail_in = static_cast<uInt>(srcLen);
    zs.next_out = dst;
    zs.avail_out = static_cast<uInt>(dstLen);
    const int rc = inflate(&zs, Z_FINISH);
    inflateEnd(&zs);
    if (rc != Z_STREAM_END || zs.total_out != dstLen) {
        if (err) *err = "zlib inflate mismatch (rc=" + std::to_string(rc) +
                        " out=" + std::to_string(zs.total_out) +
                        " want=" + std::to_string(dstLen) + ")";
        return false;
    }
    return true;
}

std::string Hex32(uint32_t v) {
    char b[16];
    std::snprintf(b, sizeof(b), "%X", v);
    return b;
}

// phdr types shadPS4 routes through Elf::LoadSegment (module.cpp
// LoadModuleToMemory: PT_LOAD/PT_SCE_RELRO mapped, PT_DYNAMIC and
// PT_SCE_DYNLIBDATA read into buffers; everything else is header-only).
bool PhdrNeedsData(uint32_t type) {
    return type == 1 /*PT_LOAD*/ || type == 2 /*PT_DYNAMIC*/ ||
           type == 0x61000000u /*PT_SCE_DYNLIBDATA*/ ||
           type == 0x61000010u /*PT_SCE_RELRO*/;
}

} // namespace

ExtractResult ExtractInnerElf(const uint8_t* data, size_t size) {
    ExtractResult res;
    // Facts accumulate here and are stamped onto res on EVERY exit —
    // a container that fails anywhere still leaves its numbers in the
    // log (the vc29/vc30 lesson: no evidence, no fix).
    std::string facts;

    if (!data || size < 0x20) {
        res.error = "input smaller than SELF header";
        return res;
    }

    SelfHeaderRaw hdr{};
    memcpy(&hdr, data, sizeof(hdr));

    if (hdr.magic != kSelfMagic) {
        res.error = "bad SELF magic 0x" + Hex32(hdr.magic) +
                    " (want 0x1D3D154F, orbis/shadPS4-verified)";
        return res;
    }

    // Header facts are logged by the caller through res.headerFacts;
    // the per-field expectations shadPS4 enforces are NOT hard-failed
    // here (PS5-era values may differ) — the structural checks below are
    // the real gate, and an ELF that fails them never reaches execution.
    res.segmentCount = hdr.segmentCount;
    {
        char b[160];
        std::snprintf(b, sizeof(b),
                      "ver=%u mode=%u endian=%u attr=0x%X cat=%u prog=%u "
                      "hdrSize=%u metaSize=%u unk1A=0x%X fileSize=%u",
                      hdr.version, hdr.mode, hdr.endian, hdr.attributes,
                      hdr.category, hdr.programType, hdr.headerSize,
                      hdr.metaSize, hdr.unknown1A, hdr.fileSize);
        facts = b;
    }
    auto bail = [&res, &facts](const std::string& e) {
        res.error = e;
        res.headerFacts = facts;
        return res;
    };

    // file_size is a u32 some dumps leave zero; the buffer the caller
    // read IS the file, so an absent field trusts the real length while
    // a present one is honored (refusal stays named on truncation).
    uint64_t fileLimit = size;
    if (hdr.fileSize != 0) {
        if (hdr.fileSize > size) {
            return bail("SELF fileSize (" + std::to_string(hdr.fileSize) +
                        ") exceeds buffer (" + std::to_string(size) + ")");
        }
        fileLimit = hdr.fileSize;
    }

    if (hdr.segmentCount == 0 || hdr.segmentCount > kMaxSegments) {
        return bail("segment count " + std::to_string(hdr.segmentCount) +
                    " outside [1.." + std::to_string(kMaxSegments) + "]");
    }

    // --- inner ELF header directly after the segment table --------------
    // (shadPS4 Elf::Open reads it from the stream position right after
    // the segment headers; header_size/meta_size are never navigated by)
    const size_t innerElfPos = 0x20 + static_cast<size_t>(hdr.segmentCount) *
                                        sizeof(SegmentEntryRaw);
    Elf64HeaderRaw ehdr{};
    if (!ReadStruct(data, size, innerElfPos, &ehdr)) {
        return bail("inner ELF header out of bounds (table ends at 0x" +
                    Hex32(static_cast<uint32_t>(innerElfPos)) + ")");
    }
    if (memcmp(ehdr.ident, "\x7f""ELF", 4) != 0 ||
        ehdr.ident[4] != 2 /*ELFCLASS64*/ || ehdr.ident[5] != 1 /*LSB*/) {
        return bail("inner image is not an ELF64-LSB (ident[0..5]="
                    + std::to_string(ehdr.ident[0]) + ","
                    + std::to_string(ehdr.ident[1]) + ","
                    + std::to_string(ehdr.ident[2]) + ","
                    + std::to_string(ehdr.ident[3]) + ","
                    + std::to_string(ehdr.ident[4]) + ","
                    + std::to_string(ehdr.ident[5]) + ")");
    }
    res.innerEntry = ehdr.entry;
    if (ehdr.phnum == 0 || ehdr.phnum > kMaxSegments) {
        return bail("inner phnum " + std::to_string(ehdr.phnum) +
                    " outside [1.." + std::to_string(kMaxSegments) + "]");
    }
    if (ehdr.phentsize < sizeof(Elf64PhdrRaw)) {
        return bail("inner phentsize " + std::to_string(ehdr.phentsize) +
                    " smaller than a 64-bit phdr (56)");
    }
    // shadPS4 locates the tables at elf_header_pos + e_phoff/e_shoff —
    // the inner ELF's own offsets, relative to where its header sits.
    // e_phoff is untrusted 64-bit input: innerElfPos + phoff is checked by
    // SUBTRACTION, never by an addition that can wrap and pass the bounds
    // test while data+phdrPos+i*stride reads out of bounds.
    const size_t phStride = ehdr.phentsize;
    if (ehdr.phoff > static_cast<uint64_t>(size - innerElfPos) ||
        static_cast<uint64_t>(ehdr.phnum) * phStride >
            static_cast<uint64_t>(size) - innerElfPos - ehdr.phoff) {
        return bail("inner phdr table out of bounds");
    }
    const size_t phdrPos = innerElfPos + static_cast<size_t>(ehdr.phoff);
    res.innerPhdrs = ehdr.phnum;
    {
        char b[96];
        std::snprintf(b, sizeof(b), " innerType=%u phoff=0x%llX phentsize=%u",
                      ehdr.type, (unsigned long long)ehdr.phoff,
                      ehdr.phentsize);
        facts += b;
    }

    // --- read the phdr table (resolution needs id-phdr lookups) ---------
    std::vector<Elf64PhdrRaw> phdrs(ehdr.phnum);
    for (uint16_t i = 0; i < ehdr.phnum; ++i) {
        memcpy(&phdrs[i], data + phdrPos + static_cast<size_t>(i) * phStride,
               sizeof(Elf64PhdrRaw));
    }

    // --- parse the SELF segment table ------------------------------------
    // v1.30: NO count-equality gate here (real eboot.bin: 12 entries vs
    // 14 phdrs — shadPS4 never compares them). Size plausibility and
    // file-extent checks move to pull time: an unused entry with odd
    // numbers is container metadata shadPS4 would also never touch, and
    // hard-failing on it would re-create the vc30 refusal class.
    res.segments.reserve(hdr.segmentCount);
    uint32_t refusedEncrypted = 0;
    for (uint32_t i = 0; i < hdr.segmentCount; ++i) {
        SegmentEntryRaw ent{};
        const size_t off = 0x20 + static_cast<size_t>(i) *
                                    sizeof(SegmentEntryRaw);
        if (!ReadStruct(data, size, off, &ent)) {
            return bail("segment entry " + std::to_string(i) +
                        " out of bounds");
        }
        res.segments.push_back({ent.flags, ent.fileOffset,
                                ent.fileSize, ent.memorySize});
        if (ent.flags & kSegFlagEncrypted) ++refusedEncrypted;
    }
    {
        // Entry digest into the facts line (capped): flags say which
        // entries are Blocked and which phdr id they name — exactly the
        // numbers the resolution below acts on.
        char b[48];
        const uint32_t shown =
            std::min<uint32_t>(hdr.segmentCount, 12);
        facts += " entries[";
        for (uint32_t i = 0; i < shown; ++i) {
            const SegmentInfo& s = res.segments[i];
            std::snprintf(b, sizeof(b), "%s%u:0x%llX%s%u",
                          i ? " " : "", i,
                          (unsigned long long)s.flags,
                          (s.flags & kSegFlagBlocked) ? "+id" : "~id",
                          SegId(s.flags));
            facts += b;
        }
        facts += shown < hdr.segmentCount ? " ..more]" : "]";
    }
    if (refusedEncrypted > 0) {
        res.refusedEncrypted = refusedEncrypted;
        return bail(std::to_string(refusedEncrypted) +
                    " encrypted segment(s) refused — nothing will load " +
                    "without Sony keys");
    }

    // --- rebuild a standalone ELF (inner headers + segment payloads) -----
    std::vector<uint8_t> out;
    out.reserve(1 << 20);
    out.resize(sizeof(Elf64HeaderRaw) +
               static_cast<size_t>(ehdr.phnum) * phStride);
    memcpy(out.data(), &ehdr, sizeof(ehdr));

    // Section headers are NOT carried: zero them honestly.
    {
        Elf64HeaderRaw patched{};
        memcpy(&patched, out.data(), sizeof(patched));
        patched.shoff = 0;
        patched.shentsize = 0;
        patched.shnum = 0;
        patched.shstrndx = 0;
        memcpy(out.data(), &patched, sizeof(patched));
    }

    uint32_t extracted = 0, inflated = 0;
    for (uint16_t i = 0; i < ehdr.phnum; ++i) {
        const Elf64PhdrRaw& ph = phdrs[i];

        // Copy the phdr verbatim first (patched below when served).
        const size_t phOutPos = sizeof(Elf64HeaderRaw) +
                                static_cast<size_t>(i) * phStride;
        memcpy(out.data() + phOutPos, &ph, sizeof(ph));
        if (ph.filesz == 0 || !PhdrNeedsData(ph.type)) continue;

        // shadPS4 Elf::LoadSegment scan: a Blocked entry whose id names
        // a phdr whose [p_offset, p_offset+p_filesz) range covers this
        // phdr's bytes; the read sits at seg.fileOffset + delta. First
        // match wins, exactly as the reference loops the entry table.
        const SegmentInfo* seg = nullptr;
        uint32_t segIdx = 0;
        uint64_t delta = 0;
        for (uint32_t j = 0; j < res.segments.size(); ++j) {
            const SegmentInfo& s = res.segments[j];
            if (!(s.flags & kSegFlagBlocked)) continue;
            const uint32_t id = SegId(s.flags);
            if (id >= ehdr.phnum) continue;
            const Elf64PhdrRaw& phId = phdrs[id];
            if (ph.offset < phId.offset) continue;
            const uint64_t d = ph.offset - phId.offset;
            // The pull must fit the entry's real extent (shadPS4 trusts
            // the container here; PX5 refuses out-of-extent reads by
            // name instead of silently serving neighbour bytes).
            // Overflow-safe: subtract instead of add — adversarial
            // headers must never turn a comparison into a wild read.
            if (ph.filesz > kMaxSegmentBytes) continue;
            const uint64_t extent = (s.flags & kSegFlagCompressed)
                                        ? s.memorySize : s.storedSize;
            if (ph.filesz > extent || d > extent - ph.filesz) continue;
            seg = &s; segIdx = j; delta = d;
            break;
        }
        if (!seg) {
            // Named refusal with the numbers the next fix needs: which
            // phdr starved, and what the entry table actually offered.
            std::string ids;
            char b[40];
            const uint32_t shown =
                std::min<uint32_t>(res.segments.size(), 12);
            for (uint32_t j = 0; j < shown; ++j) {
                const SegmentInfo& s = res.segments[j];
                std::snprintf(b, sizeof(b), "%s%u:0x%llX%s%u",
                              j ? " " : "", j,
                              (unsigned long long)s.flags,
                              (s.flags & kSegFlagBlocked) ? "+id" : "~id",
                              SegId(s.flags));
                ids += b;
            }
            char head[128];
            std::snprintf(head, sizeof(head),
                          "phdr[%u] (type=0x%X filesz=%llu) has no covering "
                          "Blocked SELF segment",
                          i, ph.type, (unsigned long long)ph.filesz);
            return bail(std::string(head) + " (entries=" +
                        std::to_string(res.segments.size()) +
                        " ids=[" + ids +
                        (shown < res.segments.size() ? " ..more" : "") +
                        "])");
        }

        const size_t dataPos = out.size();
        if (seg->flags & kSegFlagCompressed) {
            if (seg->storedSize == 0 || seg->memorySize == 0 ||
                seg->storedSize > kMaxSegmentBytes ||
                seg->memorySize > kMaxSegmentBytes) {
                return bail("segment " + std::to_string(segIdx) +
                            " (backing phdr " +
                            std::to_string(SegId(seg->flags)) +
                            ") compressed with implausible sizes (stored=" +
                            std::to_string(seg->storedSize) + " mem=" +
                            std::to_string(seg->memorySize) + ")");
            }
            if (seg->fileOffset >= fileLimit ||
                seg->storedSize > fileLimit - seg->fileOffset) {
                return bail("segment " + std::to_string(segIdx) +
                            " extent exceeds file");
            }
            std::vector<uint8_t> full(
                static_cast<size_t>(seg->memorySize));
            std::string zerr;
            if (!InflateSegment(data + seg->fileOffset, seg->storedSize,
                                full.data(), seg->memorySize, &zerr)) {
                return bail("segment " + std::to_string(segIdx) +
                            " inflate failed: " + zerr);
            }
            ++inflated;
            out.insert(out.end(), full.begin() + static_cast<size_t>(delta),
                       full.begin() + static_cast<size_t>(delta + ph.filesz));
        } else {
            if (seg->storedSize != seg->memorySize) {
                return bail("segment " + std::to_string(segIdx) +
                            " raw but stored=" +
                            std::to_string(seg->storedSize) + " mem=" +
                            std::to_string(seg->memorySize));
            }
            if (seg->fileOffset >= fileLimit ||
                delta > fileLimit - seg->fileOffset ||
                ph.filesz > fileLimit - seg->fileOffset - delta) {
                return bail("segment " + std::to_string(segIdx) +
                            " extent exceeds file (pull beyond fileLimit)");
            }
            out.insert(out.end(),
                       data + seg->fileOffset + delta,
                       data + seg->fileOffset + delta + ph.filesz);
        }
        ++extracted;

        // Point this phdr at its reassembled bytes.
        Elf64PhdrRaw patchedPh = ph;
        patchedPh.offset = dataPos;
        memcpy(out.data() + phOutPos, &patchedPh, sizeof(patchedPh));
    }

    if (extracted == 0) {
        return bail("no phdr payload was reassembled");
    }
    if (out.size() > kMaxStreamBytes) {
        return bail("rebuilt image exceeds safety bound");
    }

    res.extractedSegments = extracted;
    res.inflatedSegments  = inflated;
    {
        char b[64];
        std::snprintf(b, sizeof(b), " served=%u inflated=%u", extracted,
                      inflated);
        facts += b;
    }
    res.elfBytes = std::move(out);
    res.elfOffset = 0;
    res.ok = true;
    res.headerFacts = facts;
    return res;
}

} // namespace PX5::SelfExtract
