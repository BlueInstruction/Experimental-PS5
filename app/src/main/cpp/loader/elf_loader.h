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
        uint32_t phdrIndex = 0; // v1.41: the phdr index this PT_LOAD had in
                                // the parse loop — crash attribution names
                                // segments exactly as the loader log did
        // v1.41 — evidence: SHA-256 of this segment's file bytes
        // ([fileOffset, fileOffset+filesz) in the hashed stream). The user
        // can reproduce it with dd + sha256sum against their own file.
        char     sha256Hex[65] = {};
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

    // v1.41 — evidence identity. sha256Hex hashes the byte stream this
    // struct was parsed from (the file for plain ELFs, the extracted inner
    // ELF buffer for SELF images); containerSha256Hex hashes the on-disk
    // file (equals sha256Hex for plain ELFs, the SELF container otherwise).
    // A user hashes their own file with sha256sum and compares — no agent
    // in the loop.
    char                 sha256Hex[65] = {};
    char                 containerSha256Hex[65] = {};
    uint64_t             streamSize = 0;
    // v1.42 — on-disk container size (== streamSize for plain ELFs; the
    // SELF container's size otherwise). Ledger fact for offline verifier.
    uint64_t             containerSize = 0;
    uint64_t             entryFileOff = 0;   // file offset of the entry
                                             // bytes in the hashed stream
    bool                 entryProofMatch = false; // mem bytes == file bytes
    char                 entryBytesSha256[65] = {}; // hash of the 32 bytes
                                                    // at entry (memory side)

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

    // v1.43 — PT_DYNAMIC relocation processing. The vc42 device session
    // died INSIDE the game's first compiled block (SIGSEGV si_addr=0x0,
    // LDAXR [x27=0] in a JIT block — a lock-ed RMW through a null guest
    // pointer) while the loader log honestly said "PT_DYNAMIC: parsed,
    // not yet processed". A DYN-style (e_type 0xFE10) ORBIS image keeps
    // every absolute data pointer unrelocated until DT_RELA is applied —
    // 119,961 R_X86_64_RELATIVE entries on the vc42 image alone. These
    // fields carry what was processed and what was refused, each count
    // produced by a real loop over real table bytes.
    bool                 dynProcessed = false;
    uint64_t             dynVa = 0;          // guest VA of the dynamic table
    uint64_t             dynStreamOff = 0;   // its offset in the hashed stream
    size_t               dynFilesz = 0;
    uint64_t             relaVa = 0;         // DT_RELA (link-time VA)
    uint64_t             relaStreamOff = 0;  // translated stream offset
    size_t               relaEntries = 0;    // DT_RELASZ / DT_RELAENT
    uint64_t             relocApplied = 0;   // R_X86_64_RELATIVE writes done
    uint64_t             relocUnresolvedImports = 0; // sym-based relocs whose
                              // symbol is UNDEF in this image (DT_RELA and
                              // DT_JMPREL both count) — the HLE/NID gate
                              // worklist (vc42 image: 815 = 341 R_64 +
                              // 31 GLOB_DAT + 443 JUMP_SLOT)
    uint64_t             relocSkippedOther = 0;      // named-otherwise types
    uint64_t             relocWriteRefused = 0;      // target not in a
                              // writable mapped segment — refused, not done

    size_t TotalMemSize() const { return imageHighVa - imageLowVa; }
};

class ElfLoader {
public:
    // Parses + maps a real x86-64 ELF into the guest window.
    static bool LoadElfFile(const std::string& filePath, LoadedElfImage& out);

    // Same parse/map work for an ELF image already held in memory — the
    // path the SELF extractor feeds (its inner ELF never touches disk).
    // `origin` is only used for logs/errors. `containerSha256Hex` (v1.41)
    // is the hash of the on-disk file the stream came from when the caller
    // knows it (SELF path); it is stored verbatim for the evidence layer.
    static bool LoadElfFromMemory(const uint8_t* data, size_t size,
                                  const std::string& origin,
                                  LoadedElfImage& out,
                                  const char* containerSha256Hex = nullptr,
                                  bool fromSelfContainer = false,
                                  uint64_t containerSizeBytes = 0);

    // SELF containers: parsed by the real extractor (loader/self_extract.cpp).
    // Unencrypted/fake-signed dumps load their inner ELF for real; segments
    // that are ENCRYPTED are counted and refused by name — we do not have
    // Sony keys and will not pretend otherwise.
    static bool LoadSelf(const std::string& filePath, LoadedElfImage& out);
};

} // namespace PX5

#endif // PX5_ELF_LOADER_H
