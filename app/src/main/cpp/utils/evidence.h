// SPDX-License-Identifier: MIT
// PX5 — Evidence layer (v1.41).
//
// WHY THIS EXISTS — the vc40/vc41 trust review (user + external reviewer,
// 2026-09-04) ruled: foundation-suite PASS lines prove the ENGINE, they do
// not prove a real PS5 game executed. And a log alone cannot distinguish
// honest diagnostics from wishful claims. The answer is not assurances —
// it is evidence a third party can verify WITHOUT the agent in the loop:
//
//   1. IMAGE IDENTITY: every loaded image is SHA-256-bound (the whole
//      parsed byte stream + every PT_LOAD's file bytes). The user hashes
//      their own eboot.bin/.self on a PC and compares with the log.
//   2. ENTRY PROOF: the 32 bytes the JIT will execute are hashed from BOTH
//      sides — mapped guest memory AND the source byte stream at the
//      matching file offset. match=1 means "the dispatch target is byte-
//      identical to the game file", printed BEFORE any instruction runs.
//   3. LEDGER: an append-only px5_evidence.log (separate from the main
//      log) collects load/dispatch/syscall/trap events, each carrying the
//      image hash — a compact chain a reviewer reads in one minute.
//   4. CRASH ATTRIBUTION: any guest RIP in a crash report is attributed to
//      its PT_LOAD (index, offset-in-segment, FILE offset in the hashed
//      byte stream). The user opens their file at that offset in a hex
//      editor and compares with the logged guest bytes. If they match,
//      execution was genuinely inside the game's code.
//
// HONESTY LIMIT (stated, not hidden): the app writes its own logs, so on-
// device output can never be cryptographic proof of correctness. What this
// layer removes is the ability to claim progress WITHOUT leaving byte-level
// traces a user can falsify offline. The final acceptance criterion remains
// external to any logger: the game visibly runs.
//
// Concurrency: BindImage is called once per successful load, BEFORE any
// dispatch; GetImage is async-signal-safe (plain BSS read, no locks) for
// the crash handler. Ledger writes take a mutex and fsync — never call
// from a signal handler.
#ifndef PX5_EVIDENCE_H
#define PX5_EVIDENCE_H

#include <cstddef>
#include <cstdint>
#include <string>

namespace PX5 {
namespace Evidence {

constexpr int kMaxSegments = 16;

// Byte-stream kind a hash/file-offset refers to. The distinction matters:
// for a plain ELF the parsed stream IS the on-disk file; for a SELF
// container the executed code comes from the extracted INNER ELF stream.
enum class Stream : uint32_t {
    None = 0,
    File = 1,        // the container/file on disk (plain ELF, or SELF)
    InnerElf = 2,    // the extracted inner ELF byte stream (SELF images)
};

struct ImageIdentity {
    bool     valid = false;
    Stream   stream = Stream::None;      // stream the segment offsets refer to
    char     sha256[65];                 // hash of the executed byte stream
    char     containerSha256[65];        // hash of the on-disk file (SELF);
                                         // equals sha256 for plain ELFs
    uint64_t streamSize = 0;
    uint64_t containerSize = 0;   // v1.42: on-disk container size (differs
                                  // from streamSize for SELF containers)
    char     path[256];
    uint64_t entry = 0;
    bool     isSelf = false;
    int      segCount = 0;
    struct Segment {
        uint64_t va;
        uint64_t memsz;
        uint64_t fileOff;    // offset of this segment's file bytes in the
                             // hashed stream
        uint64_t filesz;
        uint32_t prot;       // MemoryFlags bits as declared
        uint32_t phdrIndex;  // phdr index (matches the loader log lines)
        char     sha256[65]; // hash of the segment's file bytes
    } segs[kMaxSegments];
    // Entry proof (v1.41): memory bytes at entry vs source-stream bytes.
    bool     entryProven = false;
    bool     entryMatch = false;
    uint64_t entryFileOff = 0;
    char     entrySha256[65];  // hash of the 32 entry bytes (both sides when
                               // entryMatch; memory side otherwise)
};

// SHA-256 — self-contained, no deps. FIPS 180-4. `out` needs 65 bytes
// (64 hex chars + NUL), lowercase.
void Sha256Hex(const void* data, size_t len, char out[65]);
void Sha256(const void* data, size_t len, uint8_t digest[32]);

// Known-answer vectors ("", "abc", the 56-byte block boundary vector).
// Runs in the foundation suite so the device itself proves the evidence
// primitive before any image is hashed with it.
bool SelfTest();

// Bind the image the loader just mapped (normal execution context, once
// per load, before dispatch). Copies everything into BSS.
void BindImage(const ImageIdentity& img);
// Async-signal-safe snapshot of the bound image (crash handler).
// Returns false when no image was bound this session.
bool GetImage(ImageIdentity& out);

// Log namespace (v1.41, the trust-review requirement): the log itself must
// separate SYNTHETIC foundation-suite activity from REAL-guest activity so
// no reader ever mistakes an exit42 fixture PASS for game execution.
// The top-level flows set the flag (SelfTestFoundation -> false,
// LoadExecutable -> true); the loader/syscall layers only read it.
// Default is FALSE: anything logged from an unset context is labeled
// synthetic — the safe direction for an honesty tool.
void SetSessionRealGuest(bool real);
bool SessionIsRealGuest();

// Append-only evidence ledger (one line per event, fsynced). Set the path
// once at startup (same dir as the main log). Events carry the bound image
// hash automatically when one is bound.
void SetLedgerPath(const std::string& path);
void AppendLedger(const char* format, ...) __attribute__((format(printf, 1, 2)));

// First-K guest syscall evidence (called from the syscall bridge; capped
// internally so a chatty crt cannot flood the ledger).
void NoteGuestSyscall(uint32_t nr, uint64_t a0, uint64_t a1);

} // namespace Evidence
} // namespace PX5

#endif // PX5_EVIDENCE_H
