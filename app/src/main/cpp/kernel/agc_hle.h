// PX5 v1.68 — the sceAgc surface (189 imports, device-import-driven).
//
// WHAT THIS IS
//   The game pulls 189 functions from PS5 libagc (the PS5 GPU command
//   builder: ACB command construction, shader state, patch helpers, the
//   driver submit family). This tranche:
//     * resolves the WHOLE surface through the import dispatch
//       (route=hle, counters move — the device session's hle-calls gate),
//     * implements AgcInit (state/version record) and the get-size class
//       honestly where the contract is a plain constant,
//     * hands everything else to a named default: first hit per symbol
//       lands on the import ledger with its name, and the return is 0.
//   The ledger from the next device session IS the tranche-2 worklist:
//   which of the 189 the boot path actually calls, hottest first.
//
// REFERENCE
//   KytyPS5 src/libs/agc.cpp (GPL-2.0) — the mature PS5 AGC
//   implementation. Command-packet semantics for tranche 2 come from
//   there; no code is copied in this tranche.
//
// HONEST BOUNDARIES
//   * No ACB command encoding yet: functions that build GPU packets
//     return 0 without writing to the guest's command buffers. A game
//     that submits will produce empty submissions — the GPU counters
//     will show submits with 0 dwords, which is the truth of this
//     tranche, not a renderer claim.
#ifndef PX5_KERNEL_AGC_HLE_H
#define PX5_KERNEL_AGC_HLE_H

#include <cstdint>
#include <string>
#include <vector>

#include "../loader/runtime_linker.h"   // ImportCall

namespace PX5::SceKernelHle {

struct SymbolEntry;

class AgcHle {
public:
    static AgcHle& Instance();

    void CollectSymbols(std::vector<SymbolEntry>* tableOut);
    bool HasSymbol(const std::string& symbol) const;

    uint64_t Invoke(const std::string& symbol, const ImportCall& call,
                    uint64_t* xmmRetOut = nullptr);

    void ResetForSession();

private:
    AgcHle() = default;
};

} // namespace PX5::SceKernelHle

#endif // PX5_KERNEL_AGC_HLE_H
