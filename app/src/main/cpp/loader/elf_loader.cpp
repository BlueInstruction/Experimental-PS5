#include "elf_loader.h"
#include "self_extract.h"
#include "../utils/logger.h"
#include "../memory/memory.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fstream>

namespace PX5 {

namespace {

constexpr uint32_t SELF_MAGIC  = 0x1D22154FU;   // PS5 Signed ELF
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
    if (!(data.size() >= 4 &&
          *reinterpret_cast<const uint32_t*>(data.data()) == SELF_MAGIC)) {
        // Not actually a SELF: fall through to normal ELF handling so
        // callers with plain ELFs misnamed as .self still work.
        return LoadElfFile(filePath, out);
    }

    using PX5::SelfExtract::ExtractResult;
    const ExtractResult ex =
        PX5::SelfExtract::ExtractInnerElf(data.data(), data.size());

    // Log the container facts either way — a dump that disagrees with the
    // parser must leave named evidence in the log, not a bare false.
    PX5_LOGI(LogCategory::LOADER,
             "SELF %s: segments=%u extracted=%u inflated=%u encryptedRefused=%u",
             filePath.c_str(), ex.segmentCount, ex.extractedSegments,
             ex.inflatedSegments, ex.refusedEncrypted);

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
        out.error = "bad ELF magic";
        PX5_LOGE(LogCategory::LOADER, "%s: bad magic", origin.c_str());
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

        LoadedElfImage::Segment seg{};
        seg.vaddr  = ph.vaddr;
        seg.filesz = static_cast<size_t>(ph.filesz);
        seg.memsz  = static_cast<size_t>(ph.memsz);
        seg.flags  = ProtFromFlags(ph.flags);

        if (!mem.MapMemory(seg.vaddr, seg.memsz, seg.flags,
                           "PT_LOAD#" + std::to_string(i))) {
            out.error = "segment mapping rejected by memory manager (outside window?)";
            PX5_LOGE(LogCategory::LOADER,
                     "MapMemory FAILED for segment #%u VA=0x%llx",
                     i, (unsigned long long)ph.vaddr);
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
    mem.SetProgramBreak(out.imageHighVa);

    PX5_LOGI(LogCategory::LOADER,
             "ELF loaded honestly: image=[0x%llx..0x%llx] entry=0x%llx",
             (unsigned long long)out.imageLowVa,
             (unsigned long long)out.imageHighVa,
             (unsigned long long)out.entryPoint);
    return true;
}

} // namespace PX5
