// SPDX-License-Identifier: MIT
// PX5 Phase C milestone 2a — SELF container extractor (implementation).

#include "loader/self_extract.h"

#include <zlib.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace PX5::SelfExtract {

namespace {

struct SelfHeaderRaw {          // 0x20 @ 0x00
    uint32_t magic;
    uint32_t version;
    uint16_t flags;
    uint16_t type;
    uint32_t metaSize;
    uint64_t headerSize;
    uint64_t fileSize;
};
static_assert(sizeof(SelfHeaderRaw) == 0x20, "SELF header must be 0x20");

struct MetadataHeaderRaw {      // 0x20 @ 0x20
    uint64_t signatureSize;
    uint64_t reserved0;
    uint32_t reserved1;
    uint32_t segmentCount;
    uint32_t reserved2;
    uint32_t reserved3;
};
static_assert(sizeof(MetadataHeaderRaw) == 0x20, "metadata hdr must be 0x20");

struct SegmentEntryRaw {        // 0x20 each
    uint64_t flags;
    uint64_t fileOffset;
    uint64_t storedSize;
    uint64_t memorySize;
};
static_assert(sizeof(SegmentEntryRaw) == 0x20, "segment entry must be 0x20");

template <typename T>
bool ReadStruct(const uint8_t* data, size_t size, size_t offset, T* out) {
    if (offset + sizeof(T) > size) return false;
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
        res.error = "bad SELF magic 0x" + [&] {
            char b[16]; std::snprintf(b, sizeof(b), "%X", hdr.magic);
            return std::string(b);
        }() + " (want 0x1D22154F)";
        return res;
    }
    if (hdr.fileSize != size) {
        // Tolerate trailing garbage but refuse truncation.
        if (hdr.fileSize < size) {
            // fileSize smaller than buffer is fine (trailing bytes ignored).
        } else {
            res.error = "SELF fileSize (" + std::to_string(hdr.fileSize) +
                        ") exceeds buffer (" + std::to_string(size) + ")";
            return res;
        }
    }
    const uint64_t fileLimit = std::min<uint64_t>(hdr.fileSize, size);
    if (hdr.headerSize > fileLimit) {
        res.error = "SELF headerSize exceeds file size";
        return res;
    }
    if (0x20 + hdr.metaSize > hdr.headerSize) {
        res.error = "metadata region exceeds header region";
        return res;
    }

    MetadataHeaderRaw meta{};
    if (!ReadStruct(data, size, 0x20, &meta)) {
        res.error = "metadata header out of bounds";
        return res;
    }
    if (meta.segmentCount == 0 || meta.segmentCount > kMaxSegments) {
        res.error = "segment count " + std::to_string(meta.segmentCount) +
                    " outside [1.." + std::to_string(kMaxSegments) + "]";
        return res;
    }
    const uint64_t tableBytes =
        0x20ull * meta.segmentCount;
    if (0x20 + tableBytes > hdr.metaSize) {
        res.error = "segment table exceeds metadata region";
        return res;
    }

    res.segmentCount = meta.segmentCount;

    std::vector<uint8_t> out;
    out.reserve(1 << 20);

    for (uint32_t i = 0; i < meta.segmentCount; ++i) {
        SegmentEntryRaw ent{};
        const size_t off = 0x20 + sizeof(MetadataHeaderRaw) +
                           static_cast<size_t>(i) * sizeof(SegmentEntryRaw);
        if (!ReadStruct(data, size, off, &ent)) {
            res.error = "segment entry " + std::to_string(i) +
                        " out of bounds";
            return res;
        }
        res.segments.push_back({ent.flags, ent.fileOffset,
                                ent.storedSize, ent.memorySize});

        if (ent.flags & kSegFlagEncrypted) {
            // HONEST boundary: we do not decrypt. Counted by name.
            ++res.refusedEncrypted;
            continue;
        }
        if (ent.storedSize == 0 || ent.memorySize == 0 ||
            ent.storedSize > kMaxSegmentBytes ||
            ent.memorySize > kMaxSegmentBytes) {
            res.error = "segment " + std::to_string(i) +
                        " size fields implausible (stored=" +
                        std::to_string(ent.storedSize) + " mem=" +
                        std::to_string(ent.memorySize) + ")";
            return res;
        }
        if (ent.fileOffset + ent.storedSize > fileLimit) {
            res.error = "segment " + std::to_string(i) +
                        " extent exceeds file";
            return res;
        }

        const size_t before = out.size();
        if (ent.flags & kSegFlagCompressed) {
            out.resize(before + ent.memorySize);
            std::string zerr;
            if (!InflateSegment(data + ent.fileOffset, ent.storedSize,
                                out.data() + before, ent.memorySize, &zerr)) {
                res.error = "segment " + std::to_string(i) +
                            " inflate failed: " + zerr;
                return res;
            }
            ++res.inflatedSegments;
        } else {
            out.resize(before + ent.storedSize);
            memcpy(out.data() + before, data + ent.fileOffset,
                   static_cast<size_t>(ent.storedSize));
        }
        ++res.extractedSegments;
    }

    if (out.empty()) {
        res.error = std::to_string(res.refusedEncrypted) +
                    " encrypted segment(s) refused, nothing extractable";
        return res;
    }
    if (out.size() > kMaxStreamBytes) {
        res.error = "extracted stream exceeds safety bound";
        return res;
    }

    // Locate the inner ELF: bounded scan for a validated ELF64-LSB magic.
    constexpr uint8_t kElfMagic[4] = {0x7f, 'E', 'L', 'F'};
    for (size_t i = 0; i + 6 <= out.size(); i += 4) {
        if (memcmp(out.data() + i, kElfMagic, 4) == 0 &&
            out[i + 4] == 2 /*ELFCLASS64*/ && out[i + 5] == 1 /*LSB*/) {
            res.elfOffset = i;
            res.ok = true;
            break;
        }
    }
    if (!res.ok) {
        res.error = "no ELF64 image found in " +
                    std::to_string(out.size()) + " extracted bytes";
        return res;
    }

    res.elfBytes = std::move(out);
    return res;
}

} // namespace PX5::SelfExtract
