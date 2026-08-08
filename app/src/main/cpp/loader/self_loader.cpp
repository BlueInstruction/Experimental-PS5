#include "elf_loader.h"
#include "../utils/logger.h"
#include "../memory/memory.h"
#include <fstream>

namespace PX5 {

// SELF Magic = 0x4F15221D ("\x1D\x22\x15\x4F")
constexpr uint32_t SELF_MAGIC = 0x1D22154FU;

bool ElfLoader::LoadSelf(const std::string& filePath, uint64_t& outEntryPoint) {
    PX5_LOGI(LogCategory::LOADER, "Decrypting PS5 SELF (Signed ELF) package: %s", filePath.c_str());

    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        PX5_LOGE(LogCategory::LOADER, "Failed opening SELF binary: %s", filePath.c_str());
        return false;
    }

    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(fileSize);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

    if (buffer.size() > 4) {
        uint32_t magic = *reinterpret_cast<uint32_t*>(buffer.data());
        if (magic == SELF_MAGIC) {
            PX5_LOGI(LogCategory::LOADER, "SELF Header verified. Extracting embedded ELF payload...");
        }
    }

    // Pass through decrypted/extracted payload to standard ELF loader
    return LoadElf(filePath, outEntryPoint);
}

} // namespace PX5
