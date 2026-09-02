// SPDX-License-Identifier: MIT
// PX5 v1.31 — runtime linker self-test body. Pure C++, no engine deps:
// runs for real on both ABIs (CI x86_64 smoke + on-device diagnostics).

#include "runtime_linker_selftest.h"

#include "runtime_linker.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace PX5 {

namespace {

// ---------------------------------------------------------------------------
// Synthetic ELF64 image with a real PT_DYNAMIC (strings + SCE-range tag).
// Layout: ehdr @0, phdrs @0x40 (PT_LOAD whole file, PT_DYNAMIC), then
// dynamic entries, then the string table.
// ---------------------------------------------------------------------------
struct DynFixture {
    std::vector<uint8_t> bytes;
    uint64_t dynEntries = 0;
};

void Put16(std::vector<uint8_t>& v, size_t off, uint16_t x) {
    v[off]     = (uint8_t)(x & 0xFF);
    v[off + 1] = (uint8_t)(x >> 8);
}
void Put32(std::vector<uint8_t>& v, size_t off, uint32_t x) {
    for (int i = 0; i < 4; ++i) v[off + i] = (uint8_t)(x >> (8 * i));
}
void Put64(std::vector<uint8_t>& v, size_t off, uint64_t x) {
    for (int i = 0; i < 8; ++i) v[off + i] = (uint8_t)(x >> (8 * i));
}

constexpr uint64_t kFixtureVaddrBase = 0x200000ull;

DynFixture BuildDynFixture(const std::vector<std::pair<uint64_t, uint64_t>>& extraTags) {
    DynFixture f;
    // NOTE: built with explicit std::string(1,'\0') separators — C-string
    // concatenation ("a\0" + "b") truncates at the first NUL and would
    // silently corrupt the table (caught by this test's own host run).
    const std::string strtab =
        std::string(1, '\0') + std::string("libpx5dyn.so") +
        std::string(1, '\0') + std::string("libsce_test.so") +
        std::string(1, '\0');
    // offsets inside strtab
    const uint64_t sonameOff = 1;                       // libpx5dyn.so
    const uint64_t neededOff = 1 + 12 + 1;              // libsce_test.so
    const size_t strOff = 0x40 + 2 * 56;                // after ehdr + 2 phdrs
    const size_t strPadded = (strtab.size() + 7) & ~size_t(7);
    const size_t dynOff = strOff + strPadded;
    std::vector<std::pair<uint64_t, uint64_t>> dyn = {
        {5,  kFixtureVaddrBase + strOff},               // DT_STRTAB
        {10, strtab.size()},                            // DT_STRSZ
        {14, sonameOff},                                // DT_SONAME
        {1,  neededOff},                                // DT_NEEDED
    };
    dyn.insert(dyn.end(), extraTags.begin(), extraTags.end());
    dyn.emplace_back(0, 0);                             // DT_NULL
    f.dynEntries = dyn.size() - 1;

    const size_t total = dynOff + dyn.size() * 16;
    f.bytes.assign(total, 0);

    // ehdr
    const uint8_t ident[16] = {0x7F, 'E', 'L', 'F', 2, 1, 1, 0,
                               0, 0, 0, 0, 0, 0, 0, 0};
    memcpy(f.bytes.data(), ident, 16);
    Put16(f.bytes, 16, 3);              // e_type = ET_DYN
    Put16(f.bytes, 18, 0x3E);           // e_machine = x86-64
    Put32(f.bytes, 20, 1);              // e_version
    Put64(f.bytes, 24, 0);              // e_entry
    Put64(f.bytes, 32, 0x40);           // e_phoff
    Put16(f.bytes, 54, 56);             // e_phentsize
    Put16(f.bytes, 56, 2);              // e_phnum
    // phdr[0]: PT_LOAD covering the whole image
    Put32(f.bytes, 0x40 + 0,  1);                       // p_type
    Put64(f.bytes, 0x40 + 8,  0);                       // p_offset
    Put64(f.bytes, 0x40 + 16, kFixtureVaddrBase);       // p_vaddr
    Put64(f.bytes, 0x40 + 32, total);                   // p_filesz
    // phdr[1]: PT_DYNAMIC
    Put32(f.bytes, 0x40 + 56 + 0, 2);                   // p_type
    Put64(f.bytes, 0x40 + 56 + 8,  dynOff);             // p_offset
    Put64(f.bytes, 0x40 + 56 + 16, kFixtureVaddrBase + dynOff);
    Put64(f.bytes, 0x40 + 56 + 32, dyn.size() * 16);    // p_filesz
    // dynamic entries
    for (size_t i = 0; i < dyn.size(); ++i) {
        Put64(f.bytes, dynOff + i * 16,      dyn[i].first);
        Put64(f.bytes, dynOff + i * 16 + 8,  dyn[i].second);
    }
    // string table
    memcpy(f.bytes.data() + strOff, strtab.data(), strtab.size());
    return f;
}

} // namespace

bool RunRuntimeLinkerSelfTest(std::string* report) {
    std::vector<std::string> lines;
    bool allOk = true;
    auto fail = [&](const std::string& s) { lines.push_back("[FAIL] " + s); allOk = false; };

    // --- Subtest 1: PT_DYNAMIC parse (strings + SCE tags) -----------------
    {
        auto f = BuildDynFixture({{0x61000013ull, 2}, {0x61000047ull, 0}});
        DynamicInfo d = ParseDynamicFromElfImage(f.bytes.data(), f.bytes.size());
        bool ok = d.ok &&
                  d.needed.size() == 1 && d.needed[0] == "libsce_test.so" &&
                  d.soname == "libpx5dyn.so" &&
                  d.dynEntries == f.dynEntries &&
                  d.sceTags.size() == 2 &&
                  d.sceTags[0].first == 0x61000013 && d.sceTags[0].second == 2;
        if (ok) {
            lines.push_back("[PASS] 1. PT_DYNAMIC parse | " + d.summary);
        } else {
            fail("1. PT_DYNAMIC parse | " +
                 (d.ok ? "content mismatch (needed=" +
                         std::to_string(d.needed.size()) + ")"
                       : d.error));
        }
    }

    // --- Subtest 2: registry + gate dispatch ------------------------------
    {
        auto& rl = RuntimeLinker::GetInstance();
        rl.Reset();
        rl.RegisterHleExport("libpx5test", 0x11111111, "test_sum",
                             [](const uint64_t* args, size_t argc) -> int64_t {
                                 int64_t s = 0;
                                 for (size_t i = 0; i < argc; ++i)
                                     s += (int64_t)args[i];
                                 return s;
                             });
        rl.RegisterGuestExport(0x22222222, 0xCAFEBABEull, "guest_fn");

        const uint64_t args[2] = {7, 35};
        GateResult hle = rl.DispatchNid(0x11111111, args, 2);
        GateResult guest = rl.DispatchNid(0x22222222, args, 2);
        GateResult unknown = rl.DispatchNid(0x33333333, args, 2);

        const auto& st = rl.Stats();
        const bool ok = hle.ok && hle.value == 42 &&
                        !guest.ok &&
                        guest.error.find("guest export") != std::string::npos &&
                        !unknown.ok &&
                        unknown.error.find("not registered") != std::string::npos &&
                        st.gateCalls == 3 && st.resolvedHle == 1 &&
                        st.guestRouted == 1 && st.unresolved == 1;
        if (ok) {
            lines.push_back("[PASS] 2. registry+gate | hle(7+35)=42 "
                            "guest-refused unknown-refused | " +
                            rl.GetSummaryString());
        } else {
            char b[128];
            snprintf(b, sizeof(b), "hle.ok=%d val=%lld guest.ok=%d unk.ok=%d "
                     "stats g=%llu h=%llu gu=%llu u=%llu",
                     hle.ok ? 1 : 0, (long long)hle.value, guest.ok ? 1 : 0,
                     unknown.ok ? 1 : 0,
                     (unsigned long long)st.gateCalls,
                     (unsigned long long)st.resolvedHle,
                     (unsigned long long)st.guestRouted,
                     (unsigned long long)st.unresolved);
            fail(std::string("2. registry+gate | ") + b);
        }
        rl.Reset();
    }

    // --- Subtest 3: named refusal paths ------------------------------------
    {
        // 3a: bad magic
        std::vector<uint8_t> junk(256, 0);
        DynamicInfo d1 = ParseDynamicFromElfImage(junk.data(), junk.size());
        // 3b: no PT_DYNAMIC (a valid ELF header alone)
        std::vector<uint8_t> noDyn(64 + 56, 0);
        const uint8_t ident[16] = {0x7F, 'E', 'L', 'F', 2, 1, 1, 0,
                                   0, 0, 0, 0, 0, 0, 0, 0};
        memcpy(noDyn.data(), ident, 16);
        Put16(noDyn, 54, 56);
        Put16(noDyn, 56, 1);
        DynamicInfo d2 = ParseDynamicFromElfImage(noDyn.data(), noDyn.size());
        // 3c: dynamic segment beyond the image
        auto f = BuildDynFixture({});
        f.bytes.resize(0x40 + 2 * 56 + 8);   // truncate before the dyn data
        DynamicInfo d3 = ParseDynamicFromElfImage(f.bytes.data(), f.bytes.size());

        const bool ok = !d1.ok && d1.error == "bad ELF magic" &&
                        !d2.ok && d2.error.find("PT_DYNAMIC") != std::string::npos &&
                        !d3.ok && !d3.error.empty();
        if (ok) {
            lines.push_back("[PASS] 3. refusals by name | '" + d1.error +
                            "' / '" + d2.error + "' / '" + d3.error + "'");
        } else {
            fail("3. refusals | d1.ok=" + std::to_string(d1.ok) +
                 " d2.ok=" + std::to_string(d2.ok) +
                 " d3.ok=" + std::to_string(d3.ok));
        }
    }

    // --- Report ------------------------------------------------------------
    if (report) {
        std::string out = allOk ? "PASS" : "FAIL";
        for (const auto& l : lines) out += "\n" + l;
        *report = out;
    }
    return allOk;
}

} // namespace PX5
