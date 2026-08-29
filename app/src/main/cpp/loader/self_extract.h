// SPDX-License-Identifier: MIT
// PX5 Phase C milestone 2a — SELF container extractor (real, bounded).
//
// WHAT THIS IS
//   The first real step of the SELF pipeline: parse the SCE container and
//   extract the inner ELF. This matches what working PS4 emulators do for
//   practical dumps: fake-signed / already-decrypted SELF files carry
//   unencrypted (optionally zlib-compressed) segments, and the loader's
//   job is container parsing + segment extraction — not cryptanalysis.
//
// HONEST BOUNDARIES (all enforced in code):
//   * ENCRYPTED segments are never touched: counted + refused BY NAME.
//     We do not have Sony keys and will not pretend to.
//   * Signatures are not verified (skipped); the fields are parsed and
//     bounds-checked but crypto validation is out of scope.
//   * Field layout follows public reverse-engineering lineage
//     (shadPS4/GPCS4-era SELF parsing). Where public sources leave room
//     for doubt, every assumption is a NAMED constant below and every
//     mismatch produces a named validation error naming the field — so a
//     real dump that disagrees tells us exactly which field to fix.
//   * The extractor is platform-independent C++ with its own synthetic
//     round-trip self-test (loader/self_extract_selftest.cpp) — the
//     mechanics are proven before any real dump arrives.
//
// LAYOUT implemented here (provisional, source-annotated):
//   SelfHeader @0x00 (0x20 bytes):
//     +0x00 u32 magic        == kSelfMagic (0x1D22154F, PS5/PS4 SCE family)
//     +0x04 u32 version
//     +0x08 u16 flags
//     +0x0A u16 type
//     +0x0C u32 metaSize     (metadata region size, starts at 0x20)
//     +0x10 u64 headerSize   (total header region; segment data starts here)
//     +0x18 u64 fileSize
//   MetadataHeader @0x20 (0x20 bytes):
//     +0x00 u64 signatureSize
//     +0x08 u64 reserved0
//     +0x10 u32 reserved1
//     +0x14 u32 segmentCount
//     +0x18 u32 reserved2
//     +0x1C u32 reserved3
//   SegmentEntry[i] @ (0x20 + 0x20) + i*0x20 (0x20 bytes each):
//     +0x00 u64 flags        (bit0 encrypted, bit1 signed, bit2 compressed)
//     +0x08 u64 fileOffset
//     +0x10 u64 storedSize   (compressed size when compressed)
//     +0x18 u64 memorySize   (uncompressed size)
//
// After extraction the inner ELF is located by a bounded magic scan of the
// reassembled segment stream (ELF64 LSB validation) — the same artifact
// fake-signed dumps carry. If no ELF is found the result fails with a
// named reason; nothing is guessed.

#ifndef PX5_LOADER_SELF_EXTRACT_H
#define PX5_LOADER_SELF_EXTRACT_H

#include <cstdint>
#include <string>
#include <vector>

namespace PX5::SelfExtract {

// Same constant elf_loader.cpp already uses for detection.
constexpr uint32_t kSelfMagic = 0x1D22154FU;

// Segment flag bits (provisional per public RE; named for falsifiability).
constexpr uint64_t kSegFlagEncrypted  = 1ull << 0;
constexpr uint64_t kSegFlagSigned     = 1ull << 1;
constexpr uint64_t kSegFlagCompressed = 1ull << 2;

// Hard safety bounds (real SELF segments are far below these; anything
// larger is treated as corruption, not decoded).
constexpr uint64_t kMaxSegments      = 64;
constexpr uint64_t kMaxSegmentBytes  = 256ull * 1024 * 1024;   // per segment
constexpr uint64_t kMaxStreamBytes   = 512ull * 1024 * 1024;   // total out

struct SegmentInfo {
    uint64_t flags;
    uint64_t fileOffset;
    uint64_t storedSize;
    uint64_t memorySize;
};

struct ExtractResult {
    bool        ok = false;
    std::string error;              // non-empty when !ok (named reason)
    // Reassembled extracted bytes; `elfOffset` is where a validated
    // ELF64-LSB magic was found (elfBytes not sliced to keep evidence).
    std::vector<uint8_t> elfBytes;
    uint64_t elfOffset = 0;

    uint32_t segmentCount       = 0;
    uint32_t extractedSegments  = 0;
    uint32_t refusedEncrypted   = 0;
    uint32_t inflatedSegments   = 0;
    std::vector<SegmentInfo> segments;
};

// Extracts the inner ELF from a SELF container image held in memory.
// Never reads out of bounds; every structural mismatch is a named error.
ExtractResult ExtractInnerElf(const uint8_t* data, size_t size);

} // namespace PX5::SelfExtract

#endif // PX5_LOADER_SELF_EXTRACT_H
