// PX5 v1.68 — the sceAgc surface implementation. See agc_hle.h.
#include "agc_hle.h"

#include "sce_kernel_hle.h"   // SymbolEntry

#include "../memory/memory.h"
#include "../utils/evidence.h"
#include "../utils/logger.h"

#include <atomic>
#include <cstring>
#include <mutex>
#include <set>
#include <string>

namespace PX5::SceKernelHle {

namespace {

constexpr int64_t kErrInval = -22;
constexpr uint64_t Retc(int64_t v) { return static_cast<uint64_t>(v); }

const char* const kSymbols[] = {
    // v1.69 — sceAgcInit IS part of the surface: its handler existed but the
    // symbol was absent from this list, so HasSymbol() refused it and the
    // route never resolved — the handler was dead code and the game's very
    // first AGC call would have fallen to the ledger as unimplemented.
    "sceAgcInit",
    "sceAgcAcbAcquireMem", "sceAgcAcbAcquireMemGetSize", "sceAgcAcbAtomicGdsGetSize",
    "sceAgcAcbAtomicMem", "sceAgcAcbAtomicMemGetSize", "sceAgcAcbCondExec",
    "sceAgcAcbCondExecGetSize", "sceAgcAcbCopyData", "sceAgcAcbCopyDataGetSize",
    "sceAgcAcbDispatchIndirect", "sceAgcAcbDispatchIndirectGetSize", "sceAgcAcbDmaData",
    "sceAgcAcbDmaDataGetSize", "sceAgcAcbEventWrite", "sceAgcAcbEventWriteGetSize",
    "sceAgcAcbJump", "sceAgcAcbJumpGetSize", "sceAgcAcbMemSemaphore",
    "sceAgcAcbPopMarker", "sceAgcAcbPrimeUtcl2", "sceAgcAcbPrimeUtcl2GetSize",
    "sceAgcAcbPushMarker", "sceAgcAcbQueueEndOfShaderActionGetSize", "sceAgcAcbResetQueue",
    "sceAgcAcbRewind", "sceAgcAcbRewindGetSize", "sceAgcAcbSetFlip",
    "sceAgcAcbSetMarker", "sceAgcAcbWaitOnAddressGetSize", "sceAgcAcbWaitRegMem",
    "sceAgcAcbWaitUntilSafeForRendering", "sceAgcAcbWriteData", "sceAgcAsyncCondExecPatchSetCommandAddress",
    "sceAgcAsyncCondExecPatchSetEnd", "sceAgcAsyncRewindPatchSetRewindState", "sceAgcBranchPatchSetCompareAddress",
    "sceAgcBranchPatchSetElseTarget", "sceAgcBranchPatchSetThenTarget", "sceAgcCbBranch",
    "sceAgcCbBranchGetSize", "sceAgcCbCondWrite", "sceAgcCbCondWriteGetSize",
    "sceAgcCbDispatch", "sceAgcCbDispatchGetSize", "sceAgcCbNop",
    "sceAgcCbNopGetSize", "sceAgcCbQueueEndOfPipeActionGetSize", "sceAgcCbReleaseMem",
    "sceAgcCbSetShRegisterRangeDirect", "sceAgcCbSetShRegisterRangeDirectGetSize", "sceAgcCbSetShRegistersDirect",
    "sceAgcCbSetShRegistersDirectGetSize", "sceAgcCbSetUcRegisterRangeDirect", "sceAgcCbSetUcRegisterRangeDirectGetSize",
    "sceAgcCbSetUcRegistersDirect", "sceAgcCbSetUcRegistersDirectGetSize", "sceAgcCondExecPatchSetCommandAddress",
    "sceAgcCondExecPatchSetEnd", "sceAgcCreatePrimState", "sceAgcCreateShader",
    "sceAgcDcbAcquireMem", "sceAgcDcbAcquireMemGetSize", "sceAgcDcbAtomicGdsGetSize",
    "sceAgcDcbAtomicMem", "sceAgcDcbAtomicMemGetSize", "sceAgcDcbBeginOcclusionQueryGetSize",
    "sceAgcDcbCondExec", "sceAgcDcbCondExecGetSize", "sceAgcDcbContextStateOpGetSize",
    "sceAgcDcbCopyData", "sceAgcDcbCopyDataGetSize", "sceAgcDcbDispatchIndirect",
    "sceAgcDcbDispatchIndirectGetSize", "sceAgcDcbDmaData", "sceAgcDcbDmaDataGetSize",
    "sceAgcDcbDrawIndex", "sceAgcDcbDrawIndexAuto", "sceAgcDcbDrawIndexAutoGetSize",
    "sceAgcDcbDrawIndexGetSize", "sceAgcDcbDrawIndexIndirect", "sceAgcDcbDrawIndexIndirectGetSize",
    "sceAgcDcbDrawIndexIndirectMulti", "sceAgcDcbDrawIndexIndirectMultiGetSize", "sceAgcDcbDrawIndexMultiInstanced",
    "sceAgcDcbDrawIndexMultiInstancedGetSize", "sceAgcDcbDrawIndexOffset", "sceAgcDcbDrawIndexOffsetGetSize",
    "sceAgcDcbDrawIndirect", "sceAgcDcbDrawIndirectGetSize", "sceAgcDcbDrawIndirectMulti",
    "sceAgcDcbDrawIndirectMultiGetSize", "sceAgcDcbEndOcclusionQueryGetSize", "sceAgcDcbEventWrite",
    "sceAgcDcbEventWriteGetSize", "sceAgcDcbGetLodStats", "sceAgcDcbGetLodStatsGetSize",
    "sceAgcDcbJump", "sceAgcDcbJumpGetSize", "sceAgcDcbMemSemaphore",
    "sceAgcDcbPopMarker", "sceAgcDcbPrimeUtcl2", "sceAgcDcbPrimeUtcl2GetSize",
    "sceAgcDcbPushMarker", "sceAgcDcbQueueEndOfShaderActionGetSize", "sceAgcDcbResetQueue",
    "sceAgcDcbRewind", "sceAgcDcbRewindGetSize", "sceAgcDcbSetBaseDispatchIndirectArgsGetSize",
    "sceAgcDcbSetBaseDrawIndirectArgsGetSize", "sceAgcDcbSetBaseIndirectArgs", "sceAgcDcbSetBoolPredicationEnableGetSize",
    "sceAgcDcbSetCxRegisterDirect", "sceAgcDcbSetCxRegisterDirectGetSize", "sceAgcDcbSetCxRegistersIndirect",
    "sceAgcDcbSetCxRegistersIndirectGetSize", "sceAgcDcbSetFlip", "sceAgcDcbSetIndexBuffer",
    "sceAgcDcbSetIndexBufferGetSize", "sceAgcDcbSetIndexCount", "sceAgcDcbSetIndexCountGetSize",
    "sceAgcDcbSetIndexIndirectArgs", "sceAgcDcbSetIndexIndirectArgsGetSize", "sceAgcDcbSetIndexSize",
    "sceAgcDcbSetIndexSizeGetSize", "sceAgcDcbSetMarker", "sceAgcDcbSetNumInstances",
    "sceAgcDcbSetNumInstancesGetSize", "sceAgcDcbSetPredication", "sceAgcDcbSetPredicationDisableGetSize",
    "sceAgcDcbSetShRegisterDirect", "sceAgcDcbSetShRegisterDirectGetSize", "sceAgcDcbSetShRegistersIndirect",
    "sceAgcDcbSetShRegistersIndirectGetSize", "sceAgcDcbSetUcRegisterDirect", "sceAgcDcbSetUcRegisterDirectGetSize",
    "sceAgcDcbSetUcRegistersIndirect", "sceAgcDcbSetUcRegistersIndirectGetSize", "sceAgcDcbSetZPassPredicationEnableGetSize",
    "sceAgcDcbStallCommandBufferParser", "sceAgcDcbStallCommandBufferParserGetSize", "sceAgcDcbWaitOnAddressGetSize",
    "sceAgcDcbWaitRegMem", "sceAgcDcbWaitUntilSafeForRendering", "sceAgcDcbWriteData",
    "sceAgcDcbWriteDataGetSize", "sceAgcDmaDataPatchSetDstAddressOrOffset", "sceAgcDmaDataPatchSetSrcAddressOrOffsetOrImmediate",
    "sceAgcDriverAddEqEvent", "sceAgcDriverAgrSubmitDcb", "sceAgcDriverAgrSubmitMultiDcbs",
    "sceAgcDriverDeleteEqEvent", "sceAgcDriverGetEqContextId", "sceAgcDriverGetEqEventType",
    "sceAgcDriverIsCaptureInProgress", "sceAgcDriverIsSubmitValidationEnabled", "sceAgcDriverIsTraceInProgress",
    "sceAgcDriverRequestCaptureStart", "sceAgcDriverRequestCaptureStop", "sceAgcDriverSubmitAcb",
    "sceAgcDriverSubmitDcb", "sceAgcDriverSubmitMultiAcbs", "sceAgcDriverSubmitMultiDcbs",
    "sceAgcDriverTriggerCapture", "sceAgcGetPacketSize", "sceAgcGetRegisterDefaults2",
    "sceAgcGetRegisterDefaults2Internal", "sceAgcJumpPatchSetTarget", "sceAgcQueueEndOfPipeActionPatchAddress",
    "sceAgcQueueEndOfPipeActionPatchData", "sceAgcQueueEndOfPipeActionPatchGcrCntl", "sceAgcQueueEndOfPipeActionPatchType",
    "sceAgcRewindPatchSetRewindState", "sceAgcSetCxRegIndirectPatchAddRegisters", "sceAgcSetCxRegIndirectPatchSetAddress",
    "sceAgcSetCxRegIndirectPatchSetNumRegisters", "sceAgcSetNop", "sceAgcSetPacketPredication",
    "sceAgcSetRangePredication", "sceAgcSetShRegIndirectPatchAddRegisters", "sceAgcSetShRegIndirectPatchSetAddress",
    "sceAgcSetShRegIndirectPatchSetNumRegisters", "sceAgcSetUcRegIndirectPatchAddRegisters", "sceAgcSetUcRegIndirectPatchSetAddress",
    "sceAgcSetUcRegIndirectPatchSetNumRegisters", "sceAgcSuspendPoint", "sceAgcUpdatePrimState",
    "sceAgcWaitRegMemPatchAddress", "sceAgcWaitRegMemPatchCompareFunction", "sceAgcWaitRegMemPatchReference",
};
constexpr size_t kSymbolCount = sizeof(kSymbols) / sizeof(kSymbols[0]);

// AgcInit(state, version): the PS5 AGC driver version contract. The state
// slot receives a handle to our session record; the version is recorded
// for the tranche-2 packet encoders (packet layout depends on it).
std::mutex g_initMutex;
uint32_t g_agcVersion = 0;
bool g_agcInitialized = false;
std::atomic<uint64_t> g_defaultHits{0};

// v1.69 — first-hit-per-symbol ledger: the device log names exactly which
// AGC entry points the boot path touched. Tranche 2 encodes the real
// worklist from this record instead of a guess.
std::mutex g_defaultMutex;
std::set<std::string> g_defaultNamed;

} // namespace

AgcHle& AgcHle::Instance() {
    static AgcHle inst;
    return inst;
}

uint64_t AgcHle::Invoke(const std::string& symbol, const ImportCall& call,
                        uint64_t* xmmRetOut) {
    (void)xmmRetOut;
    if (symbol == "sceAgcInit") {
        std::lock_guard<std::mutex> lk(g_initMutex);
        if (call.args[0]) {
            // state handle: non-null cookie (the guest treats it opaque);
            // the version lands in the ledger for tranche 2.
            auto* out = static_cast<uint64_t*>(
                ::PX5::MemoryManager::GetInstance().GetHostPointer(call.args[0]));
            if (!out) return Retc(-14);
            *out = 0xA6C00001ull;
        }
        g_agcVersion = static_cast<uint32_t>(call.args[1]);
        g_agcInitialized = true;
        PX5_LOGI(LogCategory::GPU,
                 "AGC: init version=%u (surface resolved; packet encoders "
                 "are tranche 2)",
                 g_agcVersion);
        return 0;
    }

    // Named default: the import ledger logs the first hit per symbol with
    // its name; the counter says how much of the surface the boot path
    // actually touches. Returning 0 is honest "no-op", never success
    // theater — the GPU counters will show empty submissions.
    //
    // v1.69 — every unimplemented AGC entry point says so BY NAME, once:
    // "reached the default handler" means the game called it and got a
    // no-op. The *GetSize family deserves its own warning: a real AGC
    // GetSize returns the DWORD COUNT the packet occupies in the command
    // buffer; games size their command buffers from it. Returning 0 until
    // the packet encoders land (tranche 2) means the guest sees zero-sized
    // packets — the ledger is the proof of what must be encoded first.
    {
        std::lock_guard<std::mutex> lk(g_defaultMutex);
        if (g_defaultNamed.insert(symbol).second) {
            PX5_LOGW(LogCategory::GPU,
                     "AGC: '%s' hit the default handler — UNIMPLEMENTED "
                     "(packet encoders are tranche 2); returning 0",
                     symbol.c_str());
            Evidence::AppendLedger("agc default hit '%s' (unimplemented)",
                                   symbol.c_str());
        }
    }
    g_defaultHits.fetch_add(1, std::memory_order_relaxed);
    return 0;
}

bool AgcHle::HasSymbol(const std::string& symbol) const {
    for (size_t i = 0; i < kSymbolCount; ++i)
        if (symbol == kSymbols[i]) return true;
    return false;
}

void AgcHle::CollectSymbols(std::vector<SymbolEntry>* tableOut) {
    for (size_t i = 0; i < kSymbolCount; ++i) tableOut->push_back({kSymbols[i], 0});
}

void AgcHle::ResetForSession() {
    std::lock_guard<std::mutex> lk(g_initMutex);
    g_agcVersion = 0;
    g_agcInitialized = false;
    g_defaultHits.store(0, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lk2(g_defaultMutex);
    g_defaultNamed.clear();
}

} // namespace PX5::SceKernelHle
