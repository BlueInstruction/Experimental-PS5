// SPDX-License-Identifier: MIT
// PX5 Phase C milestone 2a — SELF container extractor (implementation).
// v1.29: layout corrected to the orbis map shadPS4's production parser
// uses (see self_extract.h header comment for the field-by-field source).

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

} // namespace

ExtractResult ExtractInnerElf(const uint8_t* data, size_t size) {
    ExtractResult res;

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
        res.headerFacts = b;
    }

    // file_size is a u32 some dumps leave zero; the buffer the caller
    // read IS the file, so an absent field trusts the real length while
    // a present one is honored (refusal stays named on truncation).
    uint64_t fileLimit = size;
    if (hdr.fileSize != 0) {
        if (hdr.fileSize > size) {
            res.error = "SELF fileSize (" + std::to_string(hdr.fileSize) +
                        ") exceeds buffer (" + std::to_string(size) + ")";
            return res;
        }
        fileLimit = hdr.fileSize;
    }

    if (hdr.segmentCount == 0 || hdr.segmentCount > kMaxSegments) {
        res.error = "segment count " + std::to_string(hdr.segmentCount) +
                    " outside [1.." + std::to_string(kMaxSegments) + "]";
        return res;
    }

    // --- inner ELF header directly after the segment table --------------
    const size_t innerElfPos = 0x20 + static_cast<size_t>(hdr.segmentCount) *
                                        sizeof(SegmentEntryRaw);
    Elf64HeaderRaw ehdr{};
    if (!ReadStruct(data, size, innerElfPos, &ehdr)) {
        res.error = "inner ELF header out of bounds (table ends at 0x" +
                    Hex32(static_cast<uint32_t>(innerElfPos)) + ")";
        return res;
    }
    if (memcmp(ehdr.ident, "\x7f""ELF", 4) != 0 ||
        ehdr.ident[4] != 2 /*ELFCLASS64*/ || ehdr.ident[5] != 1 /*LSB*/) {
        res.error = "inner image is not an ELF64-LSB (ident[0..5]="
                    + std::to_string(ehdr.ident[0]) + ","
                    + std::to_string(ehdr.ident[1]) + ","
                    + std::to_string(ehdr.ident[2]) + ","
                    + std::to_string(ehdr.ident[3]) + ","
                    + std::to_string(ehdr.ident[4]) + ","
                    + std::to_string(ehdr.ident[5]) + ")";
        return res;
    }
    res.innerEntry = ehdr.entry;
    if (ehdr.phnum == 0 || ehdr.phnum > kMaxSegments) {
        res.error = "inner phnum " + std::to_string(ehdr.phnum) +
                    " outside [1.." + std::to_string(kMaxSegments) + "]";
        return res;
    }
    if (ehdr.phentsize < sizeof(Elf64PhdrRaw)) {
        res.error = "inner phentsize " + std::to_string(ehdr.phentsize) +
                    " smaller than a 64-bit phdr (56)";
        return res;
    }
    // shadPS4 locates the tables at elf_header_pos + e_phoff/e_shoff —
    // the inner ELF's own offsets, relative to where its header sits.
    const size_t phStride = ehdr.phentsize;
    const size_t phdrPos = innerElfPos + ehdr.phoff;
    if (phdrPos + static_cast<size_t>(ehdr.phnum) * phStride > size) {
        res.error = "inner phdr table out of bounds";
        return res;
    }
    res.innerPhdrs = ehdr.phnum;

    // Segment table + phdr sanity shared by the extraction loop.
    if (hdr.segmentCount < ehdr.phnum) {
        res.error = "SELF has " + std::to_string(hdr.segmentCount) +
                    " segments but inner ELF declares " +
                    std::to_string(ehdr.phnum) +
                    " phdrs (shadPS4 matches them by index)";
        return res;
    }

    // --- parse the SELF segment table ------------------------------------
    res.segments.reserve(hdr.segmentCount);
    for (uint32_t i = 0; i < hdr.segmentCount; ++i) {
        SegmentEntryRaw ent{};
        const size_t off = 0x20 + static_cast<size_t>(i) *
                                    sizeof(SegmentEntryRaw);
        if (!ReadStruct(data, size, off, &ent)) {
            res.error = "segment entry " + std::to_string(i) +
                        " out of bounds";
            return res;
        }
        res.segments.push_back({ent.flags, ent.fileOffset,
                                ent.fileSize, ent.memorySize});

        if (ent.flags & kSegFlagEncrypted) {
            // HONEST boundary: we do not decrypt. Counted by name, and
            // (unlike v1.28) the WHOLE extraction fails — a partially
            // mapped image would pretend the guest can run.
            ++res.refusedEncrypted;
            continue;
        }
        if (ent.fileSize == 0 || ent.memorySize == 0 ||
            ent.fileSize > kMaxSegmentBytes ||
            ent.memorySize > kMaxSegmentBytes) {
            res.error = "segment " + std::to_string(i) +
                        " size fields implausible (stored=" +
                        std::to_string(ent.fileSize) + " mem=" +
                        std::to_string(ent.memorySize) + ")";
            return res;
        }
        if (ent.fileOffset + ent.fileSize > fileLimit) {
            res.error = "segment " + std::to_string(i) +
                        " extent exceeds file";
            return res;
        }
    }
    if (res.refusedEncrypted > 0) {
        res.error = std::to_string(res.refusedEncrypted) +
                    " encrypted segment(s) refused — nothing will load " +
                    "without Sony keys";
        return res;
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
        Elf64PhdrRaw ph{};
        memcpy(&ph, data + phdrPos + static_cast<size_t>(i) * phStride,
               sizeof(ph));

        // Copy the phdr verbatim first (patches below for PT_LOAD).
        const size_t phOutPos = sizeof(Elf64HeaderRaw) +
                                static_cast<size_t>(i) * phStride;
        memcpy(out.data() + phOutPos, &ph, sizeof(ph));
        if (ph.type != 1 /*PT_LOAD*/) continue;

        const SegmentInfo& seg = res.segments[i];
        if (seg.storedSize == 0 || seg.memorySize == 0) {
            res.error = "PT_LOAD " + std::to_string(i) +
                        " maps to an empty/refused SELF segment";
            return res;
        }

        const size_t dataPos = out.size();
        if (seg.flags & kSegFlagCompressed) {
            out.resize(dataPos + seg.memorySize);
            std::string zerr;
            if (!InflateSegment(data + seg.fileOffset, seg.storedSize,
                                out.data() + dataPos, seg.memorySize,
                                &zerr)) {
                res.error = "segment " + std::to_string(i) +
                            " inflate failed: " + zerr;
                return res;
            }
            ++inflated;
        } else {
            if (seg.storedSize != seg.memorySize) {
                res.error = "segment " + std::to_string(i) +
                            " raw but stored=" +
                            std::to_string(seg.storedSize) + " mem=" +
                            std::to_string(seg.memorySize);
                return res;
            }
            out.resize(dataPos + seg.storedSize);
            memcpy(out.data() + dataPos, data + seg.fileOffset,
                   static_cast<size_t>(seg.storedSize));
        }
        ++extracted;

        // Point this phdr at the reassembled payload.
        Elf64PhdrRaw patchedPh = ph;
        patchedPh.offset = dataPos;
        memcpy(out.data() + phOutPos, &patchedPh, sizeof(patchedPh));
    }

    if (extracted == 0) {
        res.error = "no PT_LOAD payload was reassembled";
        return res;
    }
    if (out.size() > kMaxStreamBytes) {
        res.error = "rebuilt image exceeds safety bound";
        return res;
    }

    res.extractedSegments = extracted;
    res.inflatedSegments  = inflated;
    res.elfBytes = std::move(out);
    res.elfOffset = 0;
    res.ok = true;
    return res;
}

} // namespace PX5::SelfExtract
