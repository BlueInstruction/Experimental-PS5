// SPDX-License-Identifier: MIT
// PX5 Phase C milestone 3 — shared synthetic SELF container builder.
// See self_fixtures.h for the why and the consumers.

#include "loader/self_fixtures.h"

#include <cstring>

namespace PX5::SelfFixtures {

namespace {

#pragma pack(push, 1)
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
#pragma pack(pop)

} // namespace

BuiltSelf BuildSelfContainer(
        const std::vector<uint8_t>& innerEhdr,
        const std::vector<uint8_t>& innerPhdrs,
        const std::vector<std::vector<uint8_t>>& payloads,
        const std::vector<uint64_t>& flags,
        const std::vector<uint64_t>& memSizes) {
    BuiltSelf b;
    const uint32_t count = static_cast<uint32_t>(payloads.size());
    constexpr uint64_t dataStart = 0x100ull;   // header + table + inner ELF

    // Segment payload area begins at dataStart; entries are sequential.
    std::vector<uint8_t> data;
    uint64_t off = dataStart;
    for (size_t i = 0; i < payloads.size(); ++i) {
        b.entries.push_back({flags[i], off, payloads[i].size(),
                             memSizes[i]});
        data.insert(data.end(), payloads[i].begin(), payloads[i].end());
        off += payloads[i].size();
    }

    b.bytes.resize(static_cast<size_t>(dataStart) + data.size(), 0);

    // self_header @0x00 (orbis field map, shadPS4-verified).
    const uint32_t magic = PX5::SelfExtract::kSelfMagic;
    memcpy(b.bytes.data() + 0x00, &magic, 4);
    b.bytes[0x04] = 0x00;   // version
    b.bytes[0x05] = 0x01;   // mode
    b.bytes[0x06] = 0x01;   // endian = little
    b.bytes[0x07] = 0x12;   // attributes (shadPS4 expectation)
    b.bytes[0x08] = 0x01;   // category
    b.bytes[0x09] = 0x01;   // program_type
    const uint32_t fileSize = static_cast<uint32_t>(b.bytes.size());
    memcpy(b.bytes.data() + 0x10, &fileSize, 4);
    const uint16_t segCount = static_cast<uint16_t>(count);
    memcpy(b.bytes.data() + 0x18, &segCount, 2);
    const uint16_t unknown1A = 0x22;
    memcpy(b.bytes.data() + 0x1A, &unknown1A, 2);

    // self_segment_header[count] @0x20.
    for (uint32_t i = 0; i < count; ++i) {
        uint8_t* e = b.bytes.data() + 0x20 + i * 0x20;
        memcpy(e + 0x00, &b.entries[i].flags, 8);
        memcpy(e + 0x08, &b.entries[i].fileOffset, 8);
        memcpy(e + 0x10, &b.entries[i].storedSize, 8);
        memcpy(e + 0x18, &b.entries[i].memorySize, 8);
    }

    // Inner ELF header + phdr table directly after the segment table.
    const size_t innerPos = 0x20 + static_cast<size_t>(count) * 0x20;
    memcpy(b.bytes.data() + innerPos, innerEhdr.data(), innerEhdr.size());
    if (!innerPhdrs.empty()) {
        memcpy(b.bytes.data() + innerPos + innerEhdr.size(),
               innerPhdrs.data(), innerPhdrs.size());
    }

    // Segment payloads at their absolute offsets.
    memcpy(b.bytes.data() + dataStart, data.data(), data.size());
    return b;
}

BuiltSelf BuildSelfFromWholeElf(const std::vector<uint8_t>& elfFile,
                                const std::vector<uint64_t>& flags) {
    if (elfFile.size() < sizeof(Elf64HeaderRaw)) {
        return {};
    }
    Elf64HeaderRaw ehdr{};
    memcpy(&ehdr, elfFile.data(), sizeof(ehdr));
    if (memcmp(ehdr.ident, "\x7f""ELF", 4) != 0 || ehdr.phnum == 0 ||
        ehdr.phnum != flags.size()) {
        return {};
    }

    // Split: inner header, inner phdr table, one payload per PT_LOAD.
    std::vector<uint8_t> innerEhdr(
        elfFile.begin(), elfFile.begin() + sizeof(Elf64HeaderRaw));
    std::vector<uint8_t> innerPhdrs;
    std::vector<std::vector<uint8_t>> payloads;
    std::vector<uint64_t> memSizes;
    for (uint16_t i = 0; i < ehdr.phnum; ++i) {
        const size_t phPos = static_cast<size_t>(ehdr.phoff) +
                             static_cast<size_t>(i) * ehdr.phentsize;
        if (phPos + sizeof(Elf64PhdrRaw) > elfFile.size()) return {};
        Elf64PhdrRaw ph{};
        memcpy(&ph, elfFile.data() + phPos, sizeof(ph));
        innerPhdrs.insert(innerPhdrs.end(),
                          elfFile.begin() + phPos,
                          elfFile.begin() + phPos + sizeof(Elf64PhdrRaw));
        if (ph.type != 1 /*PT_LOAD*/) continue;   // carried verbatim only
        if (static_cast<uint64_t>(ph.offset) + ph.filesz >
            elfFile.size()) return {};
        payloads.emplace_back(
            elfFile.begin() + ph.offset,
            elfFile.begin() + ph.offset + ph.filesz);
        memSizes.push_back(ph.memsz);
    }

    // The container indexes segments by PHDR order; payloads currently
    // hold only the PT_LOAD subset. For fixture purposes the inner ELF
    // is expected to be all-PT_LOAD (true for the test guests); anything
    // else is a caller error and yields an empty build.
    if (payloads.size() != flags.size()) return {};

    return BuildSelfContainer(innerEhdr, innerPhdrs, payloads, flags,
                              memSizes);
}

} // namespace PX5::SelfFixtures
