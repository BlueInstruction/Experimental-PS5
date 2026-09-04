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

/**
 * Byte-stream kind for hash/file-offset references.
 * Distinguishes plain ELF (stream=disk file) from SELF (stream=extracted inner ELF).
 */
enum class Stream : uint32_t {
    None = 0,
    File = 1,        ///< Container/file on disk (plain ELF or SELF)
    InnerElf = 2,    ///< Extracted inner ELF byte stream (SELF images)
};

/**
 * Image identity for evidence binding and crash attribution.
 * Contains SHA-256 hashes of loaded image and its segments, plus entry proof.
 */
struct ImageIdentity {
    bool     valid = false;               ///< Whether identity is valid
    Stream   stream = Stream::None;       ///< Stream kind segment offsets refer to
    char     sha256[65];                  ///< Hash of executed byte stream
    char     containerSha256[65];         ///< Hash of on-disk file (SELF); equals sha256 for ELF
    uint64_t streamSize = 0;              ///< Executed stream size
    uint64_t containerSize = 0;           ///< On-disk container size
    char     path[256];                   ///< File path
    uint64_t entry = 0;                   ///< Entry point address
    bool     isSelf = false;              ///< Whether image came from SELF
    int      segCount = 0;                ///< Number of segments

    /**
     * Per-segment evidence with hash and file offset.
     */
    struct Segment {
        uint64_t va;                      ///< Virtual address
        uint64_t memsz;                   ///< Memory size
        uint64_t fileOff;                 ///< Offset in hashed stream
        uint64_t filesz;                  ///< File size
        uint32_t prot;                    ///< Protection flags
        uint32_t phdrIndex;               ///< Program header index
        char     sha256[65];              ///< Hash of segment file bytes
    } segs[kMaxSegments];

    bool     entryProven = false;         ///< Whether entry proof was performed
    bool     entryMatch = false;          ///< Whether entry bytes match source
    uint64_t entryFileOff = 0;            ///< Entry point file offset
    char     entrySha256[65];             ///< Hash of 32 entry bytes
};

/**
 * Computes SHA-256 hash and outputs as lowercase hex string.
 * Self-contained, no deps. FIPS 180-4.
 * @param data Input data
 * @param len Data length
 * @param out Output buffer (needs 65 bytes: 64 hex + NUL)
 */
void Sha256Hex(const void* data, size_t len, char out[65]);

/**
 * Computes SHA-256 hash and outputs raw digest.
 * @param data Input data
 * @param len Data length
 * @param digest Output buffer (32 bytes)
 */
void Sha256(const void* data, size_t len, uint8_t digest[32]);

/**
 * Runs SHA-256 known-answer test vectors (FIPS 180-4).
 * Tests empty string, "abc", 56-byte block boundary.
 * Runs in foundation suite to prove evidence primitive on device.
 * @return true if all KAT vectors passed, false otherwise
 */
bool SelfTest();

/**
 * Binds image identity after loader maps it (before dispatch).
 * Copies identity into BSS for async-signal-safe access.
 * @param img Image identity to bind
 */
void BindImage(const ImageIdentity& img);

/**
 * Gets snapshot of bound image identity (async-signal-safe, for crash handler).
 * @param out Output parameter receiving image identity
 * @return false if no image bound this session, true otherwise
 */
bool GetImage(ImageIdentity& out);

/**
 * Sets log namespace flag: SYNTHETIC (foundation suite) vs REAL-GUEST.
 * Separates test fixture activity from real game execution in logs.
 * Default FALSE (synthetic) — safe direction for honesty tool.
 * @param real true for real-guest session, false for synthetic/test
 */
void SetSessionRealGuest(bool real);

/**
 * Returns whether current session is real-guest (vs. synthetic).
 * @return true if real-guest session, false if synthetic
 */
bool SessionIsRealGuest();

/**
 * Sets path for append-only evidence ledger.
 * One line per event, fsynced. Set once at startup (same dir as main log).
 * @param path Ledger file path
 */
void SetLedgerPath(const std::string& path);

/**
 * Appends event to evidence ledger with printf-style formatting.
 * Events carry bound image hash automatically when image is bound.
 * @param format Printf-style format string
 * @param ... Format arguments
 */
void AppendLedger(const char* format, ...) __attribute__((format(printf, 1, 2)));

/**
 * Records guest syscall in evidence ledger (first K calls, capped).
 * Called from syscall bridge; prevents chatty crt from flooding ledger.
 * @param nr Syscall number
 * @param a0 First argument
 * @param a1 Second argument
 */
void NoteGuestSyscall(uint32_t nr, uint64_t a0, uint64_t a1);

} // namespace Evidence
} // namespace PX5

#endif // PX5_EVIDENCE_H
