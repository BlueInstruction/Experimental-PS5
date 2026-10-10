// PX5 v1.47 — PS5 function-NID computation and the symbol registry.
//
// WHY THIS EXISTS: a PS5 ELF does not import "scePadOpen" by that name.
// Function imports are named by an 11-character NID string derived from
// the symbol name by a fixed one-way algorithm. The guest dynstr hands us
// that string through the import-trap ledger; to route the call into the
// HLE surface we need the inverse direction, and the only honest inverse
// is a registry: every symbol PX5 implements is registered here by name,
// its NID string computed at registration, and dispatch-time lookups map
// guest NID strings back to the symbol the HLE table knows.
//
// ALGORITHM: ported from AnyPS5 core/libs/nid (NidCompute.cpp + Sha1.cpp,
// GPL-2.0-only, pinned at c72d5ce1) — SHA-1 over (symbolName || 16-byte
// suffix), digest bytes [7..0] reversed, the first 8 bytes encoded in the
// custom base64S alphabet into exactly 11 characters. The fixed suffix and
// alphabet are constants of the PS5 toolchain, not secrets of this repo.
#ifndef PX5_LOADER_NID_H
#define PX5_LOADER_NID_H

#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>

namespace PX5::Nid {

/**
 * Computes the 11-character PS5 function-NID string of a symbol name.
 * Deterministic; empty names return an empty string (callers treat that
 * as "not a NID" rather than throwing — the loader must never abort on
 * a malformed dynstr).
 */
std::string ComputeNidString(const std::string& symbolName);

/**
 * True when s is exactly 11 characters drawn from the base64S alphabet
 * (the shape of a function import on a real PS5 image). Readable names
 * (AnyPS5-built test images, fixture ELFs) fail this on purpose.
 */
bool IsNidString(const std::string& s);

/**
 * Symbol registry: NID string -> implemented symbol name.
 * Seeded once from the HLE surface at registration time; guest threads
 * only ever Resolve() against the finished map. A mutex keeps the lazy
 * seed (first RegisterAll) safe against an early guest dispatch.
 */
class NidRegistry {
public:
    static NidRegistry& Instance();

    /// Registers a symbol by name; computes and stores its NID string.
    /// Re-registering the same symbol is a no-op (idempotent seed).
    void RegisterSymbol(const std::string& symbolName);

    /// Resolves a guest NID string to the implemented symbol name,
    /// or "" when this NID is not something PX5 implements.
    std::string Resolve(const std::string& nidString) const;

    /// Distinct registered symbols.
    size_t Size() const;

private:
    NidRegistry() = default;

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, std::string> m_nidToSymbol;
};

} // namespace PX5::Nid

#endif // PX5_LOADER_NID_H
