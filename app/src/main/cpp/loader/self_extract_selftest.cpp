// SPDX-License-Identifier: MIT
// PX5 Phase C milestone 2a — SELF extractor self-test (implementation).
// v1.29: subtests rebuilt around the orbis/shadPS4-verified container
// layout and the standalone-ELF rebuild contract.

#include "loader/self_extract_selftest.h"

#include <zlib.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "loader/self_extract.h"
#include "loader/self_fixtures.h"

namespace PX5::SelfExtract {

namespace {

struct SubResult {
    std::string name;
    bool pass = false;
    std::string detail;
};

// ---- A real, loadable-shape whole ELF64 image (ehdr+phdr+payload) ------
// Deliberately shaped like the engine's test guests: PT_LOAD whose file
// payload sits at p_offset=0x80 (NOT at 0x40) so the rebuild contract
// (p_offset rewritten into the standalone image) is exercised for real.
struct GuestElf {
    std::vector<uint8_t> image;   // whole file
    std::vector<uint8_t> payload; // the PT_LOAD file bytes
};

GuestElf MakeGuestElf() {
    GuestElf g;
    // payload: a tiny x86-64-ish blob, must survive byte-exact.
    const uint8_t code[] = {0xB8, 0x01, 0x00, 0x00, 0x00, 0x0F, 0x05, 0xF4};
    g.payload.assign(std::begin(code), std::end(code));

    constexpr uint64_t kPayloadOff = 0x80;
    const uint64_t entry = 0x1000ull;

    std::vector<uint8_t> img(static_cast<size_t>(kPayloadOff) +
                             g.payload.size(), 0);
    uint8_t* p = img.data();
    memcpy(p, "\x7f""ELF\x02\x01\x01\x00", 8);          // ELF64 LSB
    const uint16_t machine = 62;                          // EM_X86_64
    memcpy(p + 0x12, &machine, 2);
    memcpy(p + 0x18, &entry, 8);                          // e_entry
    const uint64_t phoff = 0x40;
    memcpy(p + 0x20, &phoff, 8);                          // e_phoff
    const uint16_t phentsize = 0x38, phnum = 1;
    memcpy(p + 0x36, &phentsize, 2);                      // e_phentsize
    memcpy(p + 0x38, &phnum, 2);                          // e_phnum
    // phdr[0]: PT_LOAD RWX, payload at 0x80, vaddr 0x1000.
    uint8_t* ph = p + 0x40;
    const uint32_t ptype = 1, pflags = 7;
    memcpy(ph + 0x00, &ptype, 4);
    memcpy(ph + 0x04, &pflags, 4);
    const uint64_t poffset = kPayloadOff, vaddr = 0x1000;
    memcpy(ph + 0x08, &poffset, 8);
    memcpy(ph + 0x10, &vaddr, 8);
    memcpy(ph + 0x18, &vaddr, 8);                         // paddr
    const uint64_t filesz = g.payload.size();
    memcpy(ph + 0x20, &filesz, 8);                        // p_filesz
    memcpy(ph + 0x28, &filesz, 8);                        // p_memsz
    const uint64_t align = 0x1000;
    memcpy(ph + 0x30, &align, 8);                         // p_align

    memcpy(img.data() + kPayloadOff, g.payload.data(), g.payload.size());
    g.image = std::move(img);
    return g;
}

std::vector<uint8_t> Deflate(const std::vector<uint8_t>& in) {
    uLongf cap = compressBound(static_cast<uLong>(in.size()));
    std::vector<uint8_t> out(cap);
    compress(out.data(), &cap, in.data(), static_cast<uLong>(in.size()));
    out.resize(cap);
    return out;
}

// ---- Subtests --------------------------------------------------------------

SubResult TestPlainSegmentRoundTrip() {
    SubResult r{"self_plain_segment", false, ""};
    const GuestElf g = MakeGuestElf();
    const SelfFixtures::BuiltSelf s =
        SelfFixtures::BuildSelfFromWholeElf(g.image, {kSegFlagSigned});
    const ExtractResult res = ExtractInnerElf(s.bytes.data(), s.bytes.size());
    if (!res.ok) { r.detail = "extract failed: " + res.error; return r; }
    if (res.elfOffset != 0) { r.detail = "rebuild must start at 0"; return r; }

    // Rebuilt ehdr must carry the inner header verbatim (fixture has no
    // section headers, so the shoff/shnum patching is a no-op here).
    if (memcmp(res.elfBytes.data(), g.image.data(), 0x40) != 0) {
        r.detail = "rebuilt ELF header mismatch"; return r;
    }
    // phdr[0].offset must point at the appended payload (0x40 + 0x38).
    const uint64_t rebuiltOff = 0x40 + 0x38;
    if (res.elfBytes.size() < rebuiltOff + g.payload.size()) {
        r.detail = "rebuilt image short"; return r;
    }
    if (memcmp(res.elfBytes.data() + rebuiltOff, g.payload.data(),
               g.payload.size()) != 0) {
        r.detail = "payload bytes mismatch"; return r;
    }
    if (res.extractedSegments != 1 || res.refusedEncrypted != 0 ||
        res.inflatedSegments != 0) {
        r.detail = "accounting wrong"; return r;
    }
    r.pass = true;
    return r;
}

SubResult TestCompressedSegmentRoundTrip() {
    SubResult r{"self_compressed_segment", false, ""};
    const GuestElf g = MakeGuestElf();
    const std::vector<uint8_t> z = Deflate(g.payload);
    // Split the whole ELF into inner header + phdr table by hand and
    // feed the deflated payload through the generic builder.
    const std::vector<uint8_t> ehdr(g.image.begin(), g.image.begin() + 0x40);
    const std::vector<uint8_t> phdrs(g.image.begin() + 0x40,
                                     g.image.begin() + 0x78);
    const SelfFixtures::BuiltSelf s = SelfFixtures::BuildSelfContainer(
        ehdr, phdrs, {z}, {kSegFlagSigned | kSegFlagCompressed},
        {g.payload.size()});
    const ExtractResult res = ExtractInnerElf(s.bytes.data(), s.bytes.size());
    if (!res.ok) { r.detail = "extract failed: " + res.error; return r; }
    if (res.inflatedSegments != 1) { r.detail = "inflate not counted"; return r; }
    const uint64_t rebuiltOff = 0x40 + 0x38;
    if (memcmp(res.elfBytes.data() + rebuiltOff, g.payload.data(),
               g.payload.size()) != 0) {
        r.detail = "inflated payload mismatch"; return r;
    }
    r.pass = true;
    return r;
}

SubResult TestEncryptedSegmentRefused() {
    SubResult r{"self_encrypted_refused", false, ""};
    const GuestElf g = MakeGuestElf();
    const SelfFixtures::BuiltSelf s =
        SelfFixtures::BuildSelfFromWholeElf(g.image,
                                            {kSegFlagSigned |
                                             kSegFlagEncrypted});
    const ExtractResult res = ExtractInnerElf(s.bytes.data(), s.bytes.size());
    // Honest refusal: an encrypted segment fails the WHOLE extraction —
    // a partially mapped image would be a lie.
    if (res.ok) { r.detail = "encrypted segment was NOT refused"; return r; }
    if (res.refusedEncrypted != 1) { r.detail = "refusal not counted"; return r; }
    if (res.error.find("encrypted") == std::string::npos) {
        r.detail = "error not named: " + res.error; return r;
    }
    r.pass = true;
    return r;
}

SubResult TestBadMagicRefused() {
    SubResult r{"self_bad_magic", false, ""};
    std::vector<uint8_t> junk(0x100, 0xAB);
    const ExtractResult res = ExtractInnerElf(junk.data(), junk.size());
    if (res.ok) { r.detail = "junk accepted"; return r; }
    if (res.error.find("bad SELF magic") == std::string::npos) {
        r.detail = "error not named: " + res.error; return r;
    }
    r.pass = true;
    return r;
}

} // namespace

bool RunSelfExtractSelfTest(std::string* report) {
    const SubResult results[] = {
        TestPlainSegmentRoundTrip(),
        TestCompressedSegmentRoundTrip(),
        TestEncryptedSegmentRefused(),
        TestBadMagicRefused(),
    };

    size_t passed = 0;
    std::string out;
    for (const auto& r : results) {
        passed += r.pass ? 1 : 0;
        out += r.pass ? "  [ok]   " : "  [FAIL] ";
        out += r.name;
        if (!r.pass && !r.detail.empty()) out += " — " + r.detail;
        out += "\n";
    }

    char head[128];
    std::snprintf(head, sizeof(head), "%s (%zu/%zu subtests passed)\n",
                  passed == sizeof(results) / sizeof(results[0])
                      ? "PASS" : "FAIL",
                  passed, sizeof(results) / sizeof(results[0]));
    out = "SELF extractor self-test: " + std::string(head) + out;

    if (report) *report = out;
    return passed == sizeof(results) / sizeof(results[0]);
}

} // namespace PX5::SelfExtract
