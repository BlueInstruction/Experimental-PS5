#include "elf_loader.h"
#include "../utils/logger.h"
#include "../memory/memory.h"
#include <fstream>
#include <cstring>

namespace PX5 {

bool ElfLoader::ParseHeaders(const std::vector<uint8_t>& buffer, ElfHeader& outHeader, std::vector<ElfSegment>& outSegments) {
    if (buffer.size() < sizeof(ElfHeader)) {
        PX5_LOGE(LogCategory::LOADER, "ELF buffer too small (%zu bytes)", buffer.size());
        return false;
    }

    std::memcpy(&outHeader, buffer.data(), sizeof(ElfHeader));

    // Check Magic \x7fELF
    if (outHeader.magic[0] != 0x7F || outHeader.magic[1] != 'E' || outHeader.magic[2] != 'L' || outHeader.magic[3] != 'F') {
        PX5_LOGE(LogCategory::LOADER, "Invalid ELF header magic");
        return false;
    }

    // Default mock segment parsing for 64-bit PS5 ELF
    ElfSegment seg{};
    seg.type = 1; // PT_LOAD
    seg.flags = MemoryFlags::PAGE_READ | MemoryFlags::PAGE_EXEC;
    seg.vaddr = 0x100000000ULL;
    seg.memsz = buffer.size();
    seg.filesz = buffer.size();
    outSegments.push_back(seg);

    if (outHeader.entryPoint == 0) {
        outHeader.entryPoint = seg.vaddr;
    }

    PX5_LOGI(LogCategory::LOADER, "Parsed ELF Header: Entry Point = 0x%llx, Machine = %u", (unsigned long long)outHeader.entryPoint, outHeader.machine);
    return true;
}

bool ElfLoader::LoadElf(const std::string& filePath, uint64_t& outEntryPoint) {
    PX5_LOGI(LogCategory::LOADER, "Opening x86_64 ELF file: %s", filePath.c_str());

    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        PX5_LOGE(LogCategory::LOADER, "Failed to open file: %s", filePath.c_str());
        return false;
    }

    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(fileSize);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), fileSize)) {
        PX5_LOGE(LogCategory::LOADER, "Failed reading binary buffer of %s", filePath.c_str());
        return false;
    }

    ElfHeader header{};
    std::vector<ElfSegment> segments;
    if (!ParseHeaders(buffer, header, segments)) {
        return false;
    }

    // Allocate guest virtual memory for segments
    for (const auto& seg : segments) {
        uint64_t mapped = MemoryManager::GetInstance().MapMemory(seg.vaddr, seg.memsz, seg.flags, "ELF_Segment");
        if (mapped != 0) {
            MemoryManager::GetInstance().WriteGuestMemory(mapped, buffer.data(), std::min((size_t)seg.filesz, buffer.size()));
        }
    }

    outEntryPoint = header.entryPoint;
    PX5_LOGI(LogCategory::LOADER, "ELF Loaded successfully into Guest VAddr 0x%llx", (unsigned long long)outEntryPoint);
    return true;
}

} // namespace PX5
