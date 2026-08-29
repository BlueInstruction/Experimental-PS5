// SPDX-License-Identifier: MIT
// PX5 Phase C milestone 2a — SELF extractor self-test (implementation).

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

// ---- Synthetic ELF64 image (a real parseable header, x86-64, 2 phdrs) ----
std::vector<uint8_t> MakeInnerElf() {
    std::vector<uint8_t> e(0x200, 0);
    e[0] = 0x7f; e[1] = 'E'; e[2] = 'L'; e[3] = 'F';
    e[4] = 2;    // ELFCLASS64
    e[5] = 1;    // ELFDATA2LSB
    // machine = EM_X86_64 (62) @ ehdr+0x12
    const uint16_t machine = 62;
    memcpy(e.data() + 0x12, &machine, 2);
    // phoff @ 0x20, entry @ 0x18
    const uint64_t entry = 0x1000ull, phoff = 0x40ull;
    memcpy(e.data() + 0x18, &entry, 8);
    memcpy(e.data() + 0x20, &phoff, 8);
    // phentsize=0x38 phnum=2 @ 0x36/0x38
    const uint16_t phentsize = 0x38, phnum = 2;
    memcpy(e.data() + 0x36, &phentsize, 2);
    memcpy(e.data() + 0x38, &phnum, 2);
    // Mark each phdr as PT_LOAD-ish content for realism (type=1).
    for (int p = 0; p < 2; ++p) {
        const uint32_t ptype = 1;
        memcpy(e.data() + phoff + p * phentsize, &ptype, 4);
    }
    return e;
}

std::vector<uint8_t> Deflate(const std::vector<uint8_t>& in) {
    uLongf cap = compressBound(static_cast<uLong>(in.size()));
    std::vector<uint8_t> out(cap);
    compress(out.data(), &cap, in.data(), static_cast<uLong>(in.size()));
    out.resize(cap);
    return out;
}

// ---- Builder per the documented layout ------------------------------------
// Moved to loader/self_fixtures.cpp in milestone 3: the foundation
// self-test (emulator.cpp step 5b) must wrap the SAME fixture format,
// so one shared builder feeds both proofs.
using SelfFixtures::BuiltSelf;
using SelfFixtures::BuildSelfContainer;

BuiltSelf BuildSelf(const std::vector<std::vector<uint8_t>>& payloads,
                    const std::vector<uint64_t>& flags,
                    const std::vector<uint64_t>& memSizes) {
    return BuildSelfContainer(payloads, flags, memSizes);
}

// ---- Subtests --------------------------------------------------------------

SubResult TestPlainSegmentRoundTrip() {
    SubResult r{"self_plain_segment", false, ""};
    const std::vector<uint8_t> elf = MakeInnerElf();
    const BuiltSelf s = BuildSelf({elf}, {kSegFlagSigned}, {elf.size()});
    const ExtractResult res = ExtractInnerElf(s.bytes.data(), s.bytes.size());
    if (!res.ok) { r.detail = "extract failed: " + res.error; return r; }
    if (res.elfOffset != 0) { r.detail = "ELF not at stream start"; return r; }
    if (res.elfBytes.size() - res.elfOffset < elf.size()) {
        r.detail = "extracted bytes short"; return r;
    }
    if (memcmp(res.elfBytes.data(), elf.data(), 0x40) != 0) {
        r.detail = "ELF bytes mismatch"; return r;
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
    const std::vector<uint8_t> elf = MakeInnerElf();
    const std::vector<uint8_t> zlibSeg = Deflate(elf);
    const BuiltSelf s = BuildSelf({zlibSeg},
                                  {kSegFlagSigned | kSegFlagCompressed},
                                  {elf.size()});
    const ExtractResult res = ExtractInnerElf(s.bytes.data(), s.bytes.size());
    if (!res.ok) { r.detail = "extract failed: " + res.error; return r; }
    if (res.inflatedSegments != 1) { r.detail = "inflate not counted"; return r; }
    if (memcmp(res.elfBytes.data(), elf.data(), 0x40) != 0) {
        r.detail = "inflated bytes mismatch"; return r;
    }
    r.pass = true;
    return r;
}

SubResult TestEncryptedSegmentRefused() {
    SubResult r{"self_encrypted_refused", false, ""};
    const std::vector<uint8_t> elf = MakeInnerElf();
    const BuiltSelf s = BuildSelf({elf},
                                  {kSegFlagSigned | kSegFlagEncrypted},
                                  {elf.size()});
    const ExtractResult res = ExtractInnerElf(s.bytes.data(), s.bytes.size());
    // Honest refusal: the encrypted-only container yields nothing.
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
