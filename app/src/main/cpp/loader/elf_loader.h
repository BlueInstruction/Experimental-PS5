#ifndef PX5_ELF_LOADER_H
#define PX5_ELF_LOADER_H

#include <string>
#include <vector>
#include <cstdint>

namespace PX5 {

struct ElfHeader {
    uint8_t magic[4];
    uint8_t bitClass;
    uint8_t endianness;
    uint16_t type;
    uint16_t machine;
    uint64_t entryPoint;
    uint64_t phoff;
    uint64_t shoff;
};

struct ElfSegment {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
};

class ElfLoader {
public:
    static bool LoadElf(const std::string& filePath, uint64_t& outEntryPoint);
    static bool LoadSelf(const std::string& filePath, uint64_t& outEntryPoint);
    static bool ParseHeaders(const std::vector<uint8_t>& buffer, ElfHeader& outHeader, std::vector<ElfSegment>& outSegments);
};

} // namespace PX5

#endif // PX5_ELF_LOADER_H
