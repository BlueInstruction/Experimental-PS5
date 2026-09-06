// SPDX-License-Identifier: MIT
// PX5 Phase C milestone 2a — SELF container extractor (real, bounded).
//
// WHAT THIS IS
//   Parse the SCE SELF container and hand the loader a runnable inner
//   ELF. Fake-signed / already-decrypted dump containers carry plaintext
//   (optionally zlib-compressed) segments; the loader's job is container
//   parsing + segment extraction — not cryptanalysis.
//
// HONEST BOUNDARIES (all enforced in code):
//   * ENCRYPTED segments are never touched: counted + refused BY NAME,
//     and ANY encrypted segment fails the whole extraction (a partially
//     mapped image would be a lie). We do not have Sony keys.
//   * Signatures are not verified (skipped); the fields are parsed and
//     bounds-checked but crypto validation is out of scope.
//   * Layout below is the ORBIS (PS4) layout VERIFIED against shadPS4's
//     production parser (src/core/loader/elf.{h,cpp}, self_header::
//     signature = 0x1D3D154F). v1.28 carried 0x1D22154F and a different
//     field map — that guess is RETIRED; the vc29 session proved it
//     against a real eboot.bin (the file fell through to the ELF path
//     and died on "bad magic" with no container evidence). Where doubt
//     remains (PS5-specific fields), every assumption is a named
//     constant below and every mismatch names the field in the error.
//
// LAYOUT implemented here (orbis, shadPS4-verified):
//   self_header @0x00 (0x20 bytes):
//     +0x00 u32 magic        == kSelfMagic (0x1D3D154F)
//     +0x04 u8  version      (shadPS4 expects 0x00; logged, not enforced)
//     +0x05 u8  mode         (shadPS4 expects 0x01; logged)
//     +0x06 u8  endian       (shadPS4 expects 0x01 little; logged)
//     +0x07 u8  attributes   (shadPS4 expects 0x12; logged)
//     +0x08 u8  category     (shadPS4 expects 0x01; logged)
//     +0x09 u8  programType  (shadPS4 expects 0x01; logged)
//     +0x0A u16 padding1
//     +0x0C u16 headerSize   (as stored; segment table still located
//                             structurally — see below)
//     +0x0E u16 metaSize
//     +0x10 u32 fileSize
//     +0x14 u32 padding2
//     +0x18 u16 segmentCount
//     +0x1A u16 unknown1A    (shadPS4: observed always 0x22; logged)
//     +0x1C u32 padding3
//   self_segment_header[i] @0x20 + i*0x20 (0x20 bytes each):
//     +0x00 u64 flags        (bit0 ordered, bit1 ENCRYPTED, bit2 signed,
//                             bit3 COMPRESSED, bit11 blocked,
//                             id = (flags>>20)&0xFFF — shadPS4 mapping)
//     +0x08 u64 fileOffset   (absolute offset in the SELF file)
//     +0x10 u64 fileSize     (stored bytes; compressed size when bit3)
//     +0x18 u64 memorySize   (uncompressed size)
//   inner ELF64 header immediately after the segment table
//     (@0x20 + segmentCount*0x20; shadPS4 Elf::Open reads it from the
//     stream position right after the segment headers), then phdrs at
//     innerElfPos + e_phoff.
//   segment data: at self_segment_header[i].fileOffset (absolute).
//
// EXTRACTION CONTRACT (v1.30 — corrected against shadPS4's REAL
// resolution, verified in src/core/loader/elf.cpp Elf::LoadSegment +
// src/core/module.cpp LoadModuleToMemory):
//   The extractor rebuilds a STANDALONE ELF64: inner header + phdrs,
//   each data-carrying phdr's p_offset rewritten to point at its
//   reassembled bytes appended below (the classic self2elf shape, no
//   decryption). The rebuilt image then flows through the ordinary ELF
//   loader + mapper, so nothing downstream special-cases SELF.
//   RESOLUTION: the SELF entry table and the inner ELF's phdr table are
//   INDEPENDENT counts — real eboot.bin carries 12 entries vs 14 phdrs
//   (vc30 device log), and shadPS4 never compares them. A phdr's bytes
//   live in the Blocked entry (flags bit 11) whose id field
//   ((flags >> 20) & 0xFFF) names the backing phdr index; the pull is
//   seg.fileOffset + (p_offset - idPhdr.p_offset). phdrs served for
//   data: PT_LOAD, PT_DYNAMIC, PT_SCE_DYNLIBDATA, PT_SCE_RELRO (the
//   exact set shadPS4 routes through LoadSegment). v1.29 matched
//   segment j to phdr j BY ORDER and rejected count mismatches — that
//   gate was a v1.29 invention, refuted by the vc30 session, removed.

#ifndef PX5_LOADER_SELF_EXTRACT_H
#define PX5_LOADER_SELF_EXTRACT_H

#include <cstdint>
#include <string>
#include <vector>

namespace PX5::SelfExtract {

// ORBIS SELF signature — shadPS4 src/core/loader/elf.h
// `self_header::signature` (production parser, loads real dumps).
constexpr uint32_t kSelfMagic = 0x1D3D154FU;

// Segment flag bits (shadPS4 self_segment_header accessors).
constexpr uint64_t kSegFlagOrdered    = 1ull << 0;
constexpr uint64_t kSegFlagEncrypted  = 1ull << 1;
constexpr uint64_t kSegFlagSigned     = 1ull << 2;
constexpr uint64_t kSegFlagCompressed = 1ull << 3;
constexpr uint64_t kSegFlagBlocked    = 1ull << 11;

// The phdr index a Blocked entry backs lives in flags bits 20-31
// (shadPS4 self_segment_header::GetId).
constexpr uint32_t kSegIdShift = 20;
constexpr uint32_t kSegIdMask  = 0xFFFu;

/**
 * Extracts segment ID from flags (phdr index for Blocked entries).
 * @param flags SELF segment flags
 * @return Segment ID (bits 20-31)
 */
inline uint32_t SegId(uint64_t flags) {
    return static_cast<uint32_t>((flags >> kSegIdShift) & kSegIdMask);
}

// Hard safety bounds (real SELF segments are far below these; anything
// larger is treated as corruption, not decoded).
constexpr uint64_t kMaxSegments      = 64;
constexpr uint64_t kMaxSegmentBytes  = 256ull * 1024 * 1024;   // per segment
constexpr uint64_t kMaxStreamBytes   = 512ull * 1024 * 1024;   // total out

/**
 * SELF segment metadata.
 */
struct SegmentInfo {
    uint64_t flags;         ///< Segment flags (ordered, encrypted, signed, compressed, blocked)
    uint64_t fileOffset;    ///< Absolute offset in SELF file
    uint64_t storedSize;    ///< Stored bytes (compressed size if kSegFlagCompressed)
    uint64_t memorySize;    ///< Uncompressed size
};

/**
 * SELF extraction result with rebuilt ELF image.
 */
struct ExtractResult {
    bool        ok = false;             ///< Whether extraction succeeded
    std::string error;                  ///< Non-empty when !ok (named reason)
    std::vector<uint8_t> elfBytes;      ///< Rebuilt standalone ELF64 image
    uint64_t elfOffset = 0;             ///< Always 0 (rebuild IS the image)

    uint32_t segmentCount       = 0;    ///< Total SELF segments
    uint32_t extractedSegments  = 0;    ///< Successfully extracted segments
    uint32_t refusedEncrypted   = 0;    ///< Encrypted segments (refused)
    uint32_t inflatedSegments   = 0;    ///< Compressed segments (inflated)
    uint32_t innerPhdrs         = 0;    ///< Inner ELF program headers
    uint64_t innerEntry         = 0;    ///< Inner ELF entry point
    std::string headerFacts;            ///< Raw header facts (evidence)
    std::vector<SegmentInfo> segments;  ///< Segment metadata
};

/**
 * Extracts the inner ELF from a SELF container held in memory.
 * Never reads out of bounds; every structural mismatch is a named error.
 * Encrypted segments are refused (we have no Sony keys).
 * @param data Pointer to SELF container data
 * @param size Size of container in bytes
 * @return ExtractResult with success/rebuilt ELF or failure/error
 */
ExtractResult ExtractInnerElf(const uint8_t* data, size_t size);

} // namespace PX5::SelfExtract

#endif // PX5_LOADER_SELF_EXTRACT_H
