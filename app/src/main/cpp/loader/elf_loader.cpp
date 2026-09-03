#include "elf_loader.h"
#include "self_extract.h"
#include "../utils/logger.h"
#include "../utils/breadcrumbs.h"
#include "../utils/evidence.h"
#include "../memory/memory.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>

namespace PX5 {

namespace {

constexpr uint32_t PT_LOAD     = 1;

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

uint32_t ProtFromFlags(uint32_t pFlags) {
    uint32_t out = MemoryFlags::PAGE_NONE;
    if (pFlags & 4) out |= MemoryFlags::PAGE_READ;   // PF_R
    if (pFlags & 2) out |= MemoryFlags::PAGE_WRITE;  // PF_W
    if (pFlags & 1) out |= MemoryFlags::PAGE_EXEC;   // PF_X
    return out;
}

// v1.29: first-16-bytes evidence. The vc29 session ended on "bad ELF
// magic" for a 7.7 MB eboot.bin with zero bytes named — the single most
// expensive missing clue we have produced. Any format verdict now names
// the bytes it rejected.
std::string HexDumpHead(const uint8_t* p, size_t avail) {
    const size_t n = std::min<size_t>(avail, 16);
    char b[3 * 16 + 1];
    size_t o = 0;
    for (size_t i = 0; i < n; ++i)
        o += static_cast<size_t>(snprintf(b + o, sizeof(b) - o,
                                          "%s%02X", i ? " " : "", p[i]));
    return std::string(b);
}

std::string ReadWholeFile(const std::string& path, std::vector<uint8_t>& data) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return "cannot open file: " + path + " (" +
                               strerror(errno) + ")";
    const std::streamsize sz = f.tellg();
    if (sz <= 0 || sz > (1ull << 31)) return "unreasonable file size: " + path;
    f.seekg(0);
    data.resize(static_cast<size_t>(sz));
    if (!f.read(reinterpret_cast<char*>(data.data()), sz))
        return "read failed: " + path;
    return {};
}

// v1.41 — the trust-review session (2026-09-04) ruled that a log alone
// cannot carry the load: every claim needs evidence a third party can
// verify WITHOUT the agent. The loader therefore computes SHA-256 over the
// parsed stream and logs it; Emulator::LoadExecutable binds it into the
// evidence layer and ledger (px5_evidence.log). The user hashes their own
// file on a PC and compares. The loader stays caller-agnostic: it labels
// its lines [GUEST]/[SYNTH] from the evidence session flag, defaulting to
// SYNTH so an unlabeled context never masquerades as game execution.
const char* SessionTag() {
    return Evidence::SessionIsRealGuest() ? "[GUEST]" : "[SYNTH]";
}

} // namespace

bool ElfLoader::LoadSelf(const std::string& filePath, LoadedElfImage& out) {
    // Milestone 3: the container is parsed by the REAL extractor. v1 of
    // this file logged "Decrypting..." and fed encrypted bytes onward;
    // v2 refused everything with a "NOT implemented" banner; v3 does the
    // honest, useful thing — extract what the container genuinely carries.
    out = LoadedElfImage{};
    out.path = filePath;

    std::vector<uint8_t> data;
    std::string err = ReadWholeFile(filePath, data);
    if (!err.empty()) {
        PX5_LOGE(LogCategory::LOADER, "SELF: %s", err.c_str());
        out.error = err;
        return false;
    }
    Breadcrumb::Set("self: read %zu bytes", data.size());
    if (!(data.size() >= 4 &&
          *reinterpret_cast<const uint32_t*>(data.data()) ==
          SelfExtract::kSelfMagic)) {
        // Not a SELF container (magic 0x1D3D154F, orbis/shadPS4-verified):
        // fall through to normal ELF handling so plain ELFs misnamed as
        // .self still work. The ELF path carries the byte evidence when
        // this file is neither format.
        Breadcrumb::Set("self: no SELF magic -> ELF path");
        return LoadElfFile(filePath, out);
    }

    using PX5::SelfExtract::ExtractResult;
    Breadcrumb::Set("self: extract begin");
    const ExtractResult ex =
        PX5::SelfExtract::ExtractInnerElf(data.data(), data.size());
    Breadcrumb::Set("self: extract done ok=%d segs=%u",
                    ex.ok ? 1 : 0, ex.segmentCount);

    // v1.41 — bind the CONTAINER first: this is the hash the user can
    // reproduce directly (sha256sum their .self/eboot file on a PC).
    char containerSha[65] = {};
    Evidence::Sha256Hex(data.data(), data.size(), containerSha);
    PX5_LOGI(LogCategory::LOADER,
             "%s SELF container sha256=%s size=%zu",
             SessionTag(), containerSha, data.size());

    // Log the container facts either way — a dump that disagrees with the
    // parser must leave named evidence in the log, not a bare false.
    PX5_LOGI(LogCategory::LOADER,
             "SELF %s: segments=%u extracted=%u inflated=%u encryptedRefused=%u "
             "innerPhdrs=%u innerEntry=0x%llx [%s]",
             filePath.c_str(), ex.segmentCount, ex.extractedSegments,
             ex.inflatedSegments, ex.refusedEncrypted,
             ex.innerPhdrs, (unsigned long long)ex.innerEntry,
             ex.headerFacts.c_str());

    if (!ex.ok) {
        out.isSelf = true;
        out.error = "SELF container not loadable: " + ex.error +
                    " [segments=" + std::to_string(ex.segmentCount) +
                    " extracted=" + std::to_string(ex.extractedSegments) +
                    " encryptedRefused=" + std::to_string(ex.refusedEncrypted) +
                    "]";
        PX5_LOGE(LogCategory::LOADER, "SELF extract failed: %s",
                 out.error.c_str());
        return false;
    }

    const uint8_t* elf = ex.elfBytes.data() + ex.elfOffset;
    const size_t   elfSize = ex.elfBytes.size() - ex.elfOffset;
    // NOTE: LoadElfFromMemory resets `out`, so isSelf is set on the loaded
    // image only after that call — and on the failed-extract path above,
    // where no reset happened.
    Breadcrumb::Set("self: map inner elf (%zu bytes)", elfSize);
    const bool ok = LoadElfFromMemory(elf, elfSize,
                                      filePath + " [SELF-extracted]", out,
                                      containerSha);
    if (ok) {
        out.isSelf = true;
        PX5_LOGI(LogCategory::LOADER,
                 "SELF inner ELF mapped: image=[0x%llx..0x%llx] entry=0x%llx "
                 "(container carried %u/%u usable segments)",
                 (unsigned long long)out.imageLowVa,
                 (unsigned long long)out.imageHighVa,
                 (unsigned long long)out.entryPoint,
                 ex.extractedSegments, ex.segmentCount);
    }
    return ok;
}

bool ElfLoader::LoadElfFile(const std::string& filePath, LoadedElfImage& out) {
    out = LoadedElfImage{};
    out.path = filePath;

    std::vector<uint8_t> buffer;
    if (std::string err = ReadWholeFile(filePath, buffer); !err.empty()) {
        PX5_LOGE(LogCategory::LOADER, "%s", err.c_str());
        out.error = err;
        return false;
    }
    // v1.41 — for a plain ELF the parsed stream IS the file, so container
    // hash and stream hash coincide; computed here and passed through so
    // the evidence ledger names one byte stream everywhere.
    char containerSha[65] = {};
    Evidence::Sha256Hex(buffer.data(), buffer.size(), containerSha);
    return LoadElfFromMemory(buffer.data(), buffer.size(), filePath, out,
                             containerSha);
}

bool ElfLoader::LoadElfFromMemory(const uint8_t* data, size_t size,
                                  const std::string& origin,
                                  LoadedElfImage& out,
                                  const char* containerSha256Hex) {
    out = LoadedElfImage{};
    out.path = origin;
    if (!data || size < sizeof(Elf64HeaderRaw)) {
        out.error = "image smaller than ELF header";
        return false;
    }

    Elf64HeaderRaw ehdr{};
    memcpy(&ehdr, data, sizeof(ehdr));

    if (memcmp(ehdr.ident, "\x7f""ELF", 4) != 0) {
        // Named evidence instead of a bare verdict: these bytes decide
        // the next fix (v1.28's constant was wrong and we could not see
        // it from the log).
        out.error = "bad ELF magic — first bytes: " +
                    HexDumpHead(data, size);
        PX5_LOGE(LogCategory::LOADER, "%s: bad magic [%s]",
                 origin.c_str(), out.error.c_str());
        return false;
    }
    if (ehdr.ident[4] != 2 /*ELFCLASS64*/) { out.error = "not ELF64";   return false; }
    if (ehdr.ident[5] != 1 /*ELFDATA2LSB*/) { out.error = "not little-endian"; return false; }
    if (ehdr.machine != 62 /*EM_X86_64*/) {
        out.error = "machine is not EM_X86_64";
        PX5_LOGE(LogCategory::LOADER, "%s: unsupported machine %u",
                 origin.c_str(), ehdr.machine);
        return false;
    }
    if (ehdr.phnum == 0 || ehdr.phoff == 0 ||
        ehdr.phoff + static_cast<uint64_t>(ehdr.phnum) * ehdr.phentsize >
            size) {
        out.error = "program header table missing/corrupt";
        return false;
    }

    auto& mem = MemoryManager::GetInstance();

    // v1.41 — stream identity. Hash FIRST (before any mapping work), log
    // with the session tag, and store on the image struct. For a plain ELF
    // the user compares against `sha256sum <file>`; for a SELF they compare
    // the container hash logged by LoadSelf and read the inner hash as the
    // extraction fingerprint.
    Evidence::Sha256Hex(data, size, out.sha256Hex);
    out.streamSize = size;
    snprintf(out.containerSha256Hex, sizeof(out.containerSha256Hex), "%s",
             containerSha256Hex ? containerSha256Hex : out.sha256Hex);
    PX5_LOGI(LogCategory::LOADER,
             "%s image stream sha256=%s size=%zu (container %s)",
             SessionTag(), out.sha256Hex, size, out.containerSha256Hex);

    PX5_LOGI(LogCategory::LOADER,
             "ELF %s: type=%u entry=0x%llx phnum=%u",
             origin.c_str(), ehdr.type,
             (unsigned long long)ehdr.entry, ehdr.phnum);
    Breadcrumb::Set("elf: parse ok phnum=%u", (unsigned)ehdr.phnum);

    // v1.40 — parse-truth facts ride on the image struct. Consumers (auxv
    // builder, TLS setup) must never re-derive them from mapped memory:
    // for SELF-extracted images the header/phdr table is NOT part of any
    // PT_LOAD mapping (the vc40 session's AT_PHDR garbage proved it).
    out.phoff     = ehdr.phoff;
    out.phnum     = ehdr.phnum;
    out.phentsize = ehdr.phentsize;

    // v1.32 — THE GAME-LOAD BLOCKER FIX (vc32 device evidence: six boot
    // attempts, all identical). Real PS5 eboot.bin inner ELFs are SONY
    // DYN-style: e_type 0xFE10 (65040) with RELATIVE p_vaddr values — the
    // vc32 log's inner facts were innerType=65040 entry=0x70 phdr#0
    // VA=0x0 size=0x2e70bc. Mapping raw p_vaddr rejects at VA=0x0
    // ("segment mapping rejected ... mapped=0.00 MiB") and the boot dies
    // before a single byte is mapped. Policy: DYN-style images are based
    // at the guest-window anchor — every segment lands at base+p_vaddr,
    // the entry becomes base+e_entry, and the loaded image stays inside
    // the manager's window contract.
    constexpr uint16_t kEtDyn        = 3;       // standard PIE/shared
    constexpr uint16_t kEtSceDynamic = 0xFE10;  // 65040 — SONY DYN-style exec
    const bool dynStyle = ehdr.type == kEtDyn || ehdr.type == kEtSceDynamic;
    const uint64_t loadBase = dynStyle ? mem.GetGuestBase() : 0;
    if (dynStyle) {
        PX5_LOGI(LogCategory::LOADER,
                 "ELF %s: DYN-style image (type=%u) — basing at guest "
                 "window anchor 0x%llx (segments at base+p_vaddr, entry "
                 "base+e_entry)",
                 origin.c_str(), ehdr.type,
                 (unsigned long long)loadBase);
        Breadcrumb::Set("elf: dyn-style base=0x%llx",
                        (unsigned long long)loadBase);
    }

    bool mappedAny = false;
    for (uint16_t i = 0; i < ehdr.phnum; ++i) {
        const size_t off = static_cast<size_t>(ehdr.phoff) +
                           static_cast<size_t>(i) * ehdr.phentsize;
        if (off + sizeof(Elf64PhdrRaw) > size) {
            out.error = "phdr truncated";
            return false;
        }
        Elf64PhdrRaw ph{};
        memcpy(&ph, data + off, sizeof(ph));

        // v1.40 — full phdr table evidence. 14 phdrs but only PT_LOADs
        // were ever named in the log; PT_TLS/PT_DYNAMIC (the TLS and the
        // dynamic sections a real crt needs) stayed invisible. One INFO
        // line per phdr — the next session reads the image anatomy
        // directly instead of inferring it from crash addresses.
        PX5_LOGI(LogCategory::LOADER,
                 "  phdr[%2u] type=0x%-6X flags=0x%X off=0x%-6llx va=0x%-6llx "
                 "filesz=%-7llu memsz=%-7llu align=0x%llx",
                 (unsigned)i, ph.type, ph.flags,
                 (unsigned long long)ph.offset,
                 (unsigned long long)ph.vaddr,
                 (unsigned long long)ph.filesz,
                 (unsigned long long)ph.memsz,
                 (unsigned long long)ph.align);

        // v1.40 — ORBIS kernel contract inputs: the TLS init image and
        // the dynamic table, parsed from the same phdr loop. FSBASE is
        // pre-set from PT_TLS before dispatch (the guest crt never calls
        // arch_prctl — vc40 logged zero arch_prctl lines with fs_base=0).
        constexpr uint32_t PT_DYNAMIC_ = 2;
        constexpr uint32_t PT_TLS_     = 7;
        if (ph.type == PT_TLS_) {
            out.hasTls    = true;
            out.tlsVa     = loadBase + ph.vaddr;
            out.tlsFilesz = static_cast<size_t>(ph.filesz);
            out.tlsMemsz  = static_cast<size_t>(ph.memsz);
            out.tlsAlign  = ph.align ? ph.align : 16;
            PX5_LOGI(LogCategory::LOADER,
                     "  PT_TLS: va=0x%llx filesz=%zu memsz=%zu align=0x%llx "
                     "(FSBASE will be pre-set for dispatch)",
                     (unsigned long long)out.tlsVa, out.tlsFilesz,
                     out.tlsMemsz, (unsigned long long)out.tlsAlign);
            continue;
        }
        if (ph.type == PT_DYNAMIC_) {
            PX5_LOGI(LogCategory::LOADER,
                     "  PT_DYNAMIC: va=0x%llx filesz=%llu (parsed, not yet "
                     "processed)",
                     (unsigned long long)(loadBase + ph.vaddr),
                     (unsigned long long)ph.filesz);
            continue;
        }

        if (ph.type != PT_LOAD) continue;
        if (ph.memsz == 0)      continue;

        Breadcrumb::Set("elf: PT_LOAD#%u va=0x%llx sz=%llu",
                        (unsigned)i, (unsigned long long)ph.vaddr,
                        (unsigned long long)ph.memsz);
        LoadedElfImage::Segment seg{};
        seg.vaddr  = loadBase + ph.vaddr;
        seg.fileOffset = ph.offset;   // v1.40 — where this payload sits in
                                      // the parsed buffer (rebuilt-buffer
                                      // offset for SELF-extracted images)
        seg.filesz = static_cast<size_t>(ph.filesz);
        seg.memsz  = static_cast<size_t>(ph.memsz);
        seg.flags  = ProtFromFlags(ph.flags);
        seg.phdrIndex = i;            // v1.41 — attribution names segments
                                      // exactly as the log lines do

        // v1.34 — TWO-PHASE LOAD (the vc34 lesson, Dreaming Sarah
        // PPSA02929). The real eboot's first PT_LOAD is R-only
        // (prot=4, 2.9 MB of text at p_vaddr=0). v1.32 mapped the
        // FINAL protection up front and then memcpy'd the segment
        // contents host-side: a guaranteed write into a read-only
        // page — SIGSEGV ACCERR si_addr=0x140000000 before a single
        // game instruction ran. Every fixture we ship is RWX, which
        // is why the whole self-test suite stayed green while the
        // first real image died on byte one of the copy.
        //   phase 1: map R|W|X, copy the file bytes in
        //   phase 2: re-protect to the segment's ELF-declared flags
        const uint32_t kLoadProt = MemoryFlags::PAGE_READ |
                                   MemoryFlags::PAGE_WRITE |
                                   MemoryFlags::PAGE_EXEC;
        if (!mem.MapMemory(seg.vaddr, seg.memsz, kLoadProt,
                           "PT_LOAD#" + std::to_string(i))) {
            out.error = "segment mapping rejected: VA=0x" +
                        [&]{ char b[24]; snprintf(b, sizeof(b), "%llx",
                             (unsigned long long)ph.vaddr); return std::string(b); }() +
                        " size=0x" +
                        [&]{ char b[24]; snprintf(b, sizeof(b), "%zx",
                             seg.memsz); return std::string(b); }() +
                        " (" + mem.GetWindowInfoString() + ")";
            PX5_LOGE(LogCategory::LOADER,
                     "MapMemory FAILED for segment #%u VA=0x%llx — %s",
                     i, (unsigned long long)ph.vaddr, out.error.c_str());
            return false;
        }

        void* hostVa = mem.GetHostPointer(seg.vaddr);
        if (!hostVa) {
            out.error = "host bridge lost after mapping";
            return false;
        }
        if (seg.filesz > 0) {
            if (static_cast<uint64_t>(ph.offset) + seg.filesz > size) {
                out.error = "segment file extent exceeds image";
                return false;
            }
            memcpy(hostVa, data + ph.offset, seg.filesz);
            // v1.41 — segment file-content hash (reproducible with
            // `dd if=<file> skip=<p_offset> count=<filesz> | sha256sum`
            // for plain ELFs).
            Evidence::Sha256Hex(data + ph.offset, seg.filesz,
                                seg.sha256Hex);
        }
        // memsz > filesz region stays zeroed (anonymous reservation).

        // Phase 2 — seal the segment at its ELF-declared protection.
        // ProtectMemory also fires the JIT invalidation contract when
        // the W bit comes off an executable range.
        //
        // v1.38 — PS5 XOM segments. PS5 game images declare their text
        // execute-only (PF_X without PF_R — the console enforces XOM in
        // hardware). AArch64 enforces it too: the CPU refuses DATA loads
        // from an exec-only page (SEGV_ACCERR), and FEXCore's decoder
        // READS guest instruction bytes (Frontend.cpp PeekByte). The
        // vc38 session died on exactly that fetch at the eboot entry
        // (si_addr=0x140000070, DecodeInstructionsAtEntry). Never seal a
        // guest page below-read; the W bit is honored as declared.
        const uint32_t sealFlags = MemoryFlags::HostReadableExec(seg.flags);
        if (sealFlags != seg.flags) {
            PX5_LOGI(LogCategory::LOADER,
                     "  XOM: PT_LOAD#%u declared exec-only (flags=0x%x) — "
                     "sealed R|X (0x%x) so the JIT can fetch bytes",
                     i, seg.flags, sealFlags);
        }
        if (sealFlags != kLoadProt) {
            if (!mem.ProtectMemory(seg.vaddr, seg.memsz, sealFlags)) {
                out.error = "segment re-protect failed: VA=0x" +
                            [&]{ char b[24]; snprintf(b, sizeof(b), "%llx",
                                 (unsigned long long)seg.vaddr); return std::string(b); }() +
                            " prot=0x" +
                            [&]{ char b[8]; snprintf(b, sizeof(b), "%x",
                                 sealFlags); return std::string(b); }();
                PX5_LOGE(LogCategory::LOADER,
                         "ProtectMemory FAILED for segment #%u VA=0x%llx — %s",
                         i, (unsigned long long)seg.vaddr, out.error.c_str());
                Breadcrumb::Set("elf: PT_LOAD#%u re-protect FAILED",
                                (unsigned)i);
                return false;
            }
            Breadcrumb::Set("elf: PT_LOAD#%u sealed prot=0x%x",
                            (unsigned)i, sealFlags);
        }

        out.segments.push_back(seg);
        out.imageLowVa  = std::min(out.imageLowVa, seg.vaddr);
        out.imageHighVa = std::max(out.imageHighVa, seg.vaddr + seg.memsz);
        mappedAny = true;

        PX5_LOGI(LogCategory::LOADER,
                 "  PT_LOAD[%u] va=0x%llx filesz=%zu memsz=%zu flags=R%sW%sX%s "
                 "sha256=%.16s%s",
                 i, (unsigned long long)seg.vaddr, seg.filesz, seg.memsz,
                 (seg.flags & MemoryFlags::PAGE_READ) ? "+" : "-",
                 (seg.flags & MemoryFlags::PAGE_WRITE) ? "+" : "-",
                 (seg.flags & MemoryFlags::PAGE_EXEC) ? "+" : "-",
                 seg.sha256Hex, seg.filesz ? "…" : " (no file bytes)");
    }

    if (!mappedAny || out.segments.empty()) {
        out.error = "no PT_LOAD segments found";
        return false;
    }

    // v1.32: entry is an IMAGE-OFFSET for DYN-style images (the vc32
    // eboot carried entry=0x70) — the executable address is base+entry.
    // The entryMapped gate below then validates against the ABSOLUTE
    // segment extents, exactly as before, so a bogus entry still refuses
    // to dispatch instead of running zeroed pages.
    out.entryPoint = ehdr.entry ? (loadBase + ehdr.entry) : out.imageLowVa;

    // v1.29 hard gate — the vc29 lesson. The foundation fixture carried
    // entry = image base + 0x80 while the segment held only 46 bytes, so
    // the JIT executed zero-filled memory (x86 00 00 = add byte [rax],al)
    // and the guest null-store became a host SIGSEGV that killed the
    // whole in-process suite. An entry outside every mapped segment is
    // now a named loader error, never a dispatch.
    bool entryMapped = false;
    for (const auto& s : out.segments) {
        if (out.entryPoint >= s.vaddr &&
            out.entryPoint <  s.vaddr + s.memsz) { entryMapped = true; break; }
    }
    if (!entryMapped) {
        out.error =
            "entry 0x" + [&]{ char b[24]; snprintf(b, sizeof(b), "%llx",
                (unsigned long long)out.entryPoint); return std::string(b); }() +
            " outside every PT_LOAD segment [0x" +
            [&]{ char b[24]; snprintf(b, sizeof(b), "%llx",
                (unsigned long long)out.imageLowVa); return std::string(b); }() +
            "..0x" +
            [&]{ char b[24]; snprintf(b, sizeof(b), "%llx",
                (unsigned long long)out.imageHighVa); return std::string(b); }() +
            "] — refusing to execute";
        PX5_LOGE(LogCategory::LOADER, "%s: %s",
                 origin.c_str(), out.error.c_str());
        return false;
    }

    // v1.40 — the phdr table must be readable in GUEST VA (the AT_PHDR
    // auxv contract; a crt that honors AT_PHDR iterates it for TLS and
    // module facts). SELF-extracted inner ELFs carry the table only in
    // the host-side rebuild buffer — mapped PT_LOADs start at segment
    // payloads — so when no segment maps the header's file range, map a
    // one-page copy just above the image (the brk area) and point AT_PHDR
    // there. Plain real-ELF layouts keep the header inside PT_LOAD#0
    // (p_offset=0) and need no copy.
    uint64_t brkStart = (out.imageHighVa + 4095) & ~uint64_t{4095};
    {
        const uint64_t hdrBytes = out.phoff +
            static_cast<uint64_t>(out.phnum) * out.phentsize;
        // v1.40 fix-on-fix: coverage must be decided in FILE-OFFSET space,
        // not VA space. The vc40 game image's PT_LOAD#0 spans
        // [0x140000000..0x1402e8000) and a naive VA check would claim
        // loadBase+phoff=0x140000040 is "covered" — while the bytes at
        // that VA are text (the segment maps file offset dataPos>0, the
        // header lives at rebuilt-buffer offset 0x40 and is mapped
        // NOWHERE). The header bytes are guest-readable iff some segment
        // maps the file range [phoff, phoff+hdrBytes); the VA follows
        // from that segment's (fileOffset -> vaddr) translation.
        bool covered = false;
        uint64_t coveredVa = 0;
        for (const auto& s : out.segments) {
            if (out.phoff >= s.fileOffset &&
                hdrBytes > 0 &&
                out.phoff + hdrBytes <= s.fileOffset + s.filesz) {
                covered = true;
                coveredVa = s.vaddr + (out.phoff - s.fileOffset);
                break;
            }
        }
        if (covered) {
            out.auxPhdrVa = coveredVa;
            PX5_LOGI(LogCategory::LOADER,
                     "  phdr table readable inside mapped segment — "
                     "AT_PHDR=0x%llx",
                     (unsigned long long)out.auxPhdrVa);
        } else {
            constexpr uint64_t kPageSize_ = 4096;
            const uint64_t hdrPageVa =
                (out.imageHighVa + kPageSize_ - 1) & ~(kPageSize_ - 1);
            bool copyPlaced = false;
            if (hdrBytes > kPageSize_) {
                PX5_LOGW(LogCategory::LOADER,
                         "  phdr table (%llu bytes) exceeds one page — "
                         "AT_PHDR copy skipped (crt auxv consumers may "
                         "fail; image stays honest)",
                         (unsigned long long)hdrBytes);
            } else if (!mem.MapMemory(hdrPageVa, kPageSize_,
                                      MemoryFlags::PAGE_READ,
                                      "phdr_table_copy")) {
                PX5_LOGW(LogCategory::LOADER,
                         "  phdr copy map FAILED at 0x%llx — AT_PHDR left 0 "
                         "rather than naming unmapped memory",
                         (unsigned long long)hdrPageVa);
            } else {
                void* h = mem.GetHostPointer(hdrPageVa);
                if (h) memcpy(h, data, static_cast<size_t>(hdrBytes));
                // brk starts ABOVE the copy page — a later brk allocation
                // must never overwrite the table the auxv points at.
                brkStart = hdrPageVa + kPageSize_;
                copyPlaced = true;
                PX5_LOGI(LogCategory::LOADER,
                         "  phdr table copy mapped at 0x%llx (%llu bytes, "
                         "R) — AT_PHDR=0x%llx",
                         (unsigned long long)hdrPageVa,
                         (unsigned long long)hdrBytes,
                         (unsigned long long)(hdrPageVa + out.phoff));
            }
            // v1.40 honesty rule: a copy that could not be placed leaves
            // AT_PHDR at 0 (an auxv entry the crt can test) instead of a
            // pointer into unmapped memory it would silently dereference.
            out.auxPhdrVa = copyPlaced ? hdrPageVa + out.phoff : 0;
        }
    }

    // v1.40 — entry-bytes evidence BEFORE dispatch. The vc40 crash chain
    // (LDAR w3,[x11] x11=0) was interpreted blind: nobody ever saw the
    // first guest instruction. 32 bytes at entry, read through the
    // already-mapped image — if this line shows sane x86-64 code the
    // mapping is right; garbage bytes indict the extraction instead.
    //
    // v1.41 — the bytes are no longer just displayed, they are BOUND: the
    // memory bytes are hashed AND the same 32 bytes are hashed from the
    // source stream at the matching file offset. match=1 pins the dispatch
    // target to the game file itself — the user can open their own file at
    // entry_file_off in a hex editor and compare with the logged hex.
    {
        uint8_t entryBytes[32] {};
        if (mem.ReadGuestMemory(out.entryPoint, entryBytes,
                                sizeof entryBytes)) {
            char hex[3 * 32 + 1];
            size_t o = 0;
            for (size_t b = 0; b < sizeof entryBytes; ++b)
                o += static_cast<size_t>(snprintf(hex + o, sizeof(hex) - o,
                                                  "%s%02X", b ? " " : "",
                                                  entryBytes[b]));
            PX5_LOGI(LogCategory::LOADER,
                     "  entry bytes @0x%llx: %s",
                     (unsigned long long)out.entryPoint, hex);

            // v1.41 — entry proof. The entry lives in some PT_LOAD (the
            // entryMapped gate above refused anything else); translate the
            // VA back to the source stream offset via that segment.
            for (const auto& s : out.segments) {
                if (out.entryPoint >= s.vaddr &&
                    out.entryPoint <  s.vaddr + s.memsz) {
                    const uint64_t off = s.fileOffset +
                        (out.entryPoint - s.vaddr);
                    out.entryFileOff = off;
                    char memSha[65] = {}, fileSha[65] = {};
                    Evidence::Sha256Hex(entryBytes, sizeof entryBytes,
                                        memSha);
                    bool haveFile = off + sizeof entryBytes <= size;
                    if (haveFile) {
                        Evidence::Sha256Hex(data + off, sizeof entryBytes,
                                            fileSha);
                    }
                    out.entryProofMatch =
                        haveFile && memcmp(memSha, fileSha, 65) == 0;
                    memcpy(out.entryBytesSha256, memSha,
                           sizeof out.entryBytesSha256);
                    PX5_LOGI(LogCategory::LOADER,
                             "  entry proof: file_off=0x%llx "
                             "mem_sha256=%s file_sha256=%s match=%d "
                             "(verify: open YOUR file at file_off and "
                             "compare the hex above)",
                             (unsigned long long)off, memSha,
                             haveFile ? fileSha : "(out of stream)",
                             out.entryProofMatch ? 1 : 0);
                    break;
                }
            }
        } else {
            PX5_LOGW(LogCategory::LOADER,
                     "  entry bytes @0x%llx: UNREADABLE through the memory "
                     "manager (mapping table and host bridge disagree?)",
                     (unsigned long long)out.entryPoint);
        }
    }

    // v1.40 — brk starts above the phdr copy page (brkStart was raised
    // by the copy branch above; covered images keep the classic value).
    mem.SetProgramBreak(brkStart);

    PX5_LOGI(LogCategory::LOADER,
             "%s ELF loaded honestly: image=[0x%llx..0x%llx] entry=0x%llx "
             "sha256=%.16s…",
             SessionTag(),
             (unsigned long long)out.imageLowVa,
             (unsigned long long)out.imageHighVa,
             (unsigned long long)out.entryPoint,
             out.sha256Hex);

    // v1.41 — evidence binding happens ONLY on the real-guest path. The
    // foundation suite (fixtures) loads through this same function but
    // never binds: GetImage() stays invalid, the ledger's img column stays
    // "-", and no fixture can ever impersonate a bound game image.
    if (Evidence::SessionIsRealGuest()) {
        Evidence::ImageIdentity id{};
        id.valid = true;
        id.stream = out.isSelf ? Evidence::Stream::InnerElf
                               : Evidence::Stream::File;
        memcpy(id.sha256, out.sha256Hex, sizeof id.sha256);
        memcpy(id.containerSha256, out.containerSha256Hex,
               sizeof id.containerSha256);
        id.streamSize = out.streamSize;
        snprintf(id.path, sizeof(id.path), "%s", out.path.c_str());
        id.entry = out.entryPoint;
        id.isSelf = out.isSelf;
        id.segCount = 0;
        for (const auto& s : out.segments) {
            if (id.segCount >= Evidence::kMaxSegments) break;
            auto& d = id.segs[id.segCount];
            d.va = s.vaddr;
            d.memsz = s.memsz;
            d.fileOff = s.fileOffset;
            d.filesz = s.filesz;
            d.prot = s.flags;
            d.phdrIndex = s.phdrIndex;
            memcpy(d.sha256, s.sha256Hex, sizeof d.sha256);
            ++id.segCount;
        }
        id.entryProven = out.entryFileOff != 0 || out.entryProofMatch;
        id.entryMatch = out.entryProofMatch;
        id.entryFileOff = out.entryFileOff;
        memcpy(id.entrySha256, out.entryBytesSha256, sizeof id.entrySha256);
        Evidence::BindImage(id);
    }
    return true;
}

} // namespace PX5
