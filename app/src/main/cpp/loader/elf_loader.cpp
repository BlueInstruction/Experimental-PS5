#include "elf_loader.h"
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
    // HONEST behavior — replaces v1 which logged "Decrypting..." and then
    // silently passed the ENCRYPTED file to the ELF loader.
    std::vector<uint8_t> data;
    std::string err = ReadWholeFile(filePath, data);
    if (!err.empty()) {
        PX5_LOGE(LogCategory::LOADER, "SELF: %s", err.c_str());
        out.error = err;
        return false;
    }
    if (data.size() >= 4 &&
        *reinterpret_cast<const uint32_t*>(data.data()) == SELF_MAGIC) {
        out.isSelf = true;
        out.error = "PS5 SELF container detected: decryption/extract pipeline "
                    "is a Phase-C milestone and is NOT implemented. Provide "
                    "a decrypted ELF for foundation testing.";
        PX5_LOGW(LogCategory::LOADER,
                 "SELF image detected but decryption not implemented (honest "
                 "rejection): %s", filePath.c_str());
        return false;
    }
    // Not actually a SELF: fall through to normal ELF handling so callers
    // with plain ELFs misnamed as .self still work.
    return LoadElfFile(filePath, out);
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
    if (buffer.size() < sizeof(Elf64HeaderRaw)) {
        out.error = "file smaller than ELF header";
        return false;
    }

    Elf64HeaderRaw ehdr{};
    memcpy(&ehdr, buffer.data(), sizeof(ehdr));

    if (memcmp(ehdr.ident, "\x7f""ELF", 4) != 0) {
        out.error = "bad ELF magic";
        PX5_LOGE(LogCategory::LOADER, "%s: bad magic", filePath.c_str());
        return false;
    }
    if (ehdr.ident[4] != 2 /*ELFCLASS64*/) { out.error = "not ELF64";   return false; }
    if (ehdr.ident[5] != 1 /*ELFDATA2LSB*/) { out.error = "not little-endian"; return false; }
    if (ehdr.machine != 62 /*EM_X86_64*/) {
        out.error = "machine is not EM_X86_64";
        PX5_LOGE(LogCategory::LOADER, "%s: unsupported machine %u",
                 filePath.c_str(), ehdr.machine);
        return false;
    }
    if (ehdr.phnum == 0 || ehdr.phoff == 0 ||
        ehdr.phoff + static_cast<uint64_t>(ehdr.phnum) * ehdr.phentsize >
            buffer.size()) {
        out.error = "program header table missing/corrupt";
        return false;
    }

    auto& mem = MemoryManager::GetInstance();

    PX5_LOGI(LogCategory::LOADER,
             "ELF %s: type=%u entry=0x%llx phnum=%u",
             filePath.c_str(), ehdr.type,
             (unsigned long long)ehdr.entry, ehdr.phnum);

    bool mappedAny = false;
    for (uint16_t i = 0; i < ehdr.phnum; ++i) {
        const size_t off = static_cast<size_t>(ehdr.phoff) +
                           static_cast<size_t>(i) * ehdr.phentsize;
        if (off + sizeof(Elf64PhdrRaw) > buffer.size()) {
            out.error = "phdr truncated";
            return false;
        }
        Elf64PhdrRaw ph{};
        memcpy(&ph, buffer.data() + off, sizeof(ph));

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
            if (static_cast<uint64_t>(ph.offset) + seg.filesz > buffer.size()) {
                out.error = "segment file extent exceeds file";
                return false;
            }
            memcpy(hostVa, buffer.data() + ph.offset, seg.filesz);
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
