// SPDX-License-Identifier: MIT
// PX5 Phase C milestone 3 — shared synthetic SELF container builder.
// See self_fixtures.h for the why and the consumers.

#include "loader/self_fixtures.h"

#include <cstring>

namespace PX5::SelfFixtures {

BuiltSelf BuildSelfContainer(
        const std::vector<std::vector<uint8_t>>& payloads,
        const std::vector<uint64_t>& flags,
        const std::vector<uint64_t>& memSizes) {
    BuiltSelf b;
    const uint32_t count = static_cast<uint32_t>(payloads.size());
    constexpr uint64_t headerSize = 0x100ull;   // header + meta + table + slack

    // Segment data area begins at headerSize.
    std::vector<uint8_t> data;
    uint64_t off = headerSize;
    for (size_t i = 0; i < payloads.size(); ++i) {
        b.entries.push_back({flags[i], off, payloads[i].size(), memSizes[i]});
        data.insert(data.end(), payloads[i].begin(), payloads[i].end());
        off += payloads[i].size();
    }

    b.bytes.resize(static_cast<size_t>(headerSize) + data.size(), 0);
    // SelfHeader @0x00
    const uint32_t magic = PX5::SelfExtract::kSelfMagic;
    memcpy(b.bytes.data() + 0x00, &magic, 4);
    uint32_t version = 0;
    memcpy(b.bytes.data() + 0x04, &version, 4);
    const uint32_t metaSize = 0x20 + 0x20ull * count;
    memcpy(b.bytes.data() + 0x0C, &metaSize, 4);
    const uint64_t hs = headerSize, fs = b.bytes.size();
    memcpy(b.bytes.data() + 0x10, &hs, 8);
    memcpy(b.bytes.data() + 0x18, &fs, 8);
    // MetadataHeader @0x20
    uint64_t sig = 0;
    memcpy(b.bytes.data() + 0x20, &sig, 8);
    memcpy(b.bytes.data() + 0x34, &count, 4);
    // SegmentEntry table @0x40
    for (uint32_t i = 0; i < count; ++i) {
        uint8_t* e = b.bytes.data() + 0x40 + i * 0x20;
        memcpy(e + 0x00, &b.entries[i].flags, 8);
        memcpy(e + 0x08, &b.entries[i].fileOffset, 8);
        memcpy(e + 0x10, &b.entries[i].storedSize, 8);
        memcpy(e + 0x18, &b.entries[i].memorySize, 8);
    }
    // Data
    memcpy(b.bytes.data() + headerSize, data.data(), data.size());
    return b;
}

} // namespace PX5::SelfFixtures
