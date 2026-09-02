#include "elf_loader.h"
#include "self_extract.h"
#include "../utils/logger.h"
#include "../utils/breadcrumbs.h"
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
                                      filePath + " [SELF-extracted]", out);
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
    return LoadElfFromMemory(buffer.data(), buffer.size(), filePath, out);
}

bool ElfLoader::LoadElfFromMemory(const uint8_t* data, size_t size,
                                  const std::string& origin,
                                  LoadedElfImage& out) {
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

    PX5_LOGI(LogCategory::LOADER,
             "ELF %s: type=%u entry=0x%llx phnum=%u",
             origin.c_str(), ehdr.type,
             (unsigned long long)ehdr.entry, ehdr.phnum);
    Breadcrumb::Set("elf: parse ok phnum=%u", (unsigned)ehdr.phnum);

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

        if (ph.type != PT_LOAD) continue;
        if (ph.memsz == 0)      continue;

        Breadcrumb::Set("elf: PT_LOAD#%u va=0x%llx sz=%llu",
                        (unsigned)i, (unsigned long long)ph.vaddr,
                        (unsigned long long)ph.memsz);
        LoadedElfImage::Segment seg{};
        seg.vaddr  = ph.vaddr;
        seg.filesz = static_cast<size_t>(ph.filesz);
        seg.memsz  = static_cast<size_t>(ph.memsz);
        seg.flags  = ProtFromFlags(ph.flags);

        if (!mem.MapMemory(seg.vaddr, seg.memsz, seg.flags,
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
        }
        // memsz > filesz region stays zeroed (anonymous reservation).

        out.segments.push_back(seg);
        out.imageLowVa  = std::min(out.imageLowVa, seg.vaddr);
        out.imageHighVa = std::max(out.imageHighVa, seg.vaddr + seg.memsz);
        mappedAny = true;

        PX5_LOGI(LogCategory::LOADER,
                 "  PT_LOAD[%u] va=0x%llx filesz=%zu memsz=%zu flags=R%sW%sX%s",
                 i, (unsigned long long)seg.vaddr, seg.filesz, seg.memsz,
                 (seg.flags & MemoryFlags::PAGE_READ) ? "+" : "-",
                 (seg.flags & MemoryFlags::PAGE_WRITE) ? "+" : "-",
                 (seg.flags & MemoryFlags::PAGE_EXEC) ? "+" : "-");
    }

    if (!mappedAny || out.segments.empty()) {
        out.error = "no PT_LOAD segments found";
        return false;
    }

    out.entryPoint = ehdr.entry ? ehdr.entry : out.imageLowVa;

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
    mem.SetProgramBreak(out.imageHighVa);

    PX5_LOGI(LogCategory::LOADER,
             "ELF loaded honestly: image=[0x%llx..0x%llx] entry=0x%llx",
             (unsigned long long)out.imageLowVa,
             (unsigned long long)out.imageHighVa,
             (unsigned long long)out.entryPoint);
    return true;
}

} // namespace PX5
