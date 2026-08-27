#ifndef PX5_ELF_LOADER_H
#define PX5_ELF_LOADER_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace PX5 {

// Result of one successful load, used by the execution bridge and the
// evidence UI. Honesty contract: every field is filled from REAL parsed
// data; on any parsing/mapping failure the call returns false and `error`
// explains why (no synthetic segments, no invented addresses).
struct LoadedElfImage {
    struct Segment {
        uint64_t vaddr;
        size_t   filesz;
        size_t   memsz;
        uint32_t flags;         // MemoryFlags bits
    };

    std::string          path;
    uint64_t             entryPoint = 0;
    uint64_t             imageLowVa = ~0ull;
    uint64_t             imageHighVa = 0;     // program break suggestion
    std::vector<Segment> segments;
    bool                 isSelf = false;      // SELF detected & NOT decrypted
    std::string          error;

    size_t TotalMemSize() const { return imageHighVa - imageLowVa; }
};

class ElfLoader {
public:
    // Parses + maps a real x86-64 ELF into the guest window.
    static bool LoadElfFile(const std::string& filePath, LoadedElfImage& out);

    // SELF containers: detected honestly. Decryption is Phase-C scope;
    // we NEVER claim to have loaded an encrypted image.
    static bool LoadSelf(const std::string& filePath, LoadedElfImage& out);
};

} // namespace PX5

#endif // PX5_ELF_LOADER_H
