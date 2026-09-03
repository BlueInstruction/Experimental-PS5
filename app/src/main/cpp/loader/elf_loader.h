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
        uint64_t fileOffset = 0; // v1.40: p_offset in the parsed buffer
                                 // (rebuilt-buffer offset for SELF-extracted
                                 // inner ELFs) — decides where the phdr
                                 // table bytes actually live in VA terms
        size_t   filesz;
        size_t   memsz;
        uint32_t flags;         // MemoryFlags bits
    };

    std::string          path;
    uint64_t             entryPoint = 0;
    uint64_t             imageLowVa = ~0ull;
    uint64_t             imageHighVa = 0;     // program break suggestion
    std::vector<Segment> segments;
    bool                 isSelf = false;      // image came out of a SELF
                                              // container (extracted inner
                                              // ELF) or was detected as SELF
    std::string          error;

    // v1.40 — parse-truth header facts. The vc40 device session proved the
    // mapped image is segment CONTENT only: for SELF-extracted inner ELFs
    // the header/phdr table never enters guest VA, so re-reading them from
    // the map returned text bytes (AT_PHDR=0x4c1e... garbage). Anything
    // that needs ELF facts reads them from HERE now.
    uint64_t             phoff = 0;
    uint32_t             phnum = 0;
    uint32_t             phentsize = 0;
    uint64_t             auxPhdrVa = 0;      // guest VA where the phdr table
                                             // is readable (copy page or
                                             // imageLowVa when covered)

    // v1.40 — PT_TLS as parsed (ORBIS kernel contract: FSBASE is set
    // before entry from this; the guest crt never issues arch_prctl).
    bool                 hasTls = false;
    uint64_t             tlsVa = 0;          // guest VA of the TLS init image
    size_t               tlsFilesz = 0;
    size_t               tlsMemsz = 0;
    uint64_t             tlsAlign = 0;

    size_t TotalMemSize() const { return imageHighVa - imageLowVa; }
};

class ElfLoader {
public:
    // Parses + maps a real x86-64 ELF into the guest window.
    static bool LoadElfFile(const std::string& filePath, LoadedElfImage& out);

    // Same parse/map work for an ELF image already held in memory — the
    // path the SELF extractor feeds (its inner ELF never touches disk).
    // `origin` is only used for logs/errors.
    static bool LoadElfFromMemory(const uint8_t* data, size_t size,
                                  const std::string& origin,
                                  LoadedElfImage& out);

    // SELF containers: parsed by the real extractor (loader/self_extract.cpp).
    // Unencrypted/fake-signed dumps load their inner ELF for real; segments
    // that are ENCRYPTED are counted and refused by name — we do not have
    // Sony keys and will not pretend otherwise.
    static bool LoadSelf(const std::string& filePath, LoadedElfImage& out);
};

} // namespace PX5

#endif // PX5_ELF_LOADER_H
