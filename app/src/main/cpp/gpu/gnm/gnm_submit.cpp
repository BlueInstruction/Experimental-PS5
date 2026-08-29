// SPDX-License-Identifier: MIT
// PX5 Phase C milestone 2b — SceGnmDriver submission shim (implementation).
//
// Address resolution: on device the default path resolves guest VAs through
// the MemoryManager window (raw-host-pointer fallback mirrors the kernel
// HLE evidence-test policy). Host unit tests compile with PX5_HOST_TEST
// (identity fallback) and may inject a resolver via
// SetAddressResolverForTest; device behavior is byte-identical code.

#include "gpu/gnm/gnm_submit.h"

#include <cstdio>

#ifndef PX5_HOST_TEST
#include <memory/memory.h>
#endif
#include <utils/logger.h>

namespace PX5::Gnm {

GnmSubmit& GnmSubmit::GetInstance() {
    static GnmSubmit inst;
    return inst;
}

void GnmSubmit::SetAddressResolverForTest(AddressResolver fn) {
    m_testResolver = std::move(fn);
}

const void* GnmSubmit::ResolveDefault(uint64_t addr, size_t bytes) const {
#ifdef PX5_HOST_TEST
    (void)bytes;
    return reinterpret_cast<const void*>(addr);
#else
    auto& mem = MemoryManager::GetInstance();
    if (mem.IsValidAddress(addr, bytes ? bytes : 1)) {
        return mem.GetHostPointer(addr);
    }
    // NO fallback. The previous "evidence-test fallback" reinterpreted the
    // raw guest VA as a host pointer and the first dereference killed the
    // whole process on real devices (SIGSEGV, no named error, no evidence).
    // An address outside the guest window is simply unresolvable: nullptr
    // -> caller emits a named EFAULT. Tests that legitimately hold HOST
    // pointers inject identity resolution via SetAddressResolverForTest.
    return nullptr;
#endif
}

const void* GnmSubmit::Resolve(uint64_t addr, size_t bytes) const {
    if (m_testResolver) return m_testResolver(addr, bytes);
    return ResolveDefault(addr, bytes);
}

void GnmSubmit::ResetForTest() {
    m_state.Reset();
    m_cumulative.Reset();
    m_submissions = 0;
    m_dwordsDecoded = 0;
    m_errors.clear();
    m_logTag = "";
}

bool GnmSubmit::SubmitBuffer(const uint32_t* dwords, size_t dwordCount,
                             const char* tag, std::string* errOut) {
    const char* name = tag ? tag : "submit";
    if (!dwords || dwordCount == 0) {
        const std::string what = std::string(name) + ": empty buffer";
        if (errOut) *errOut = what;
        if (m_errors.size() < 16) m_errors.push_back(what);
        return false;
    }
    if (dwordCount > kMaxSubmitDwords) {
        const std::string what = std::string(name) + ": buffer too large (" +
                                 std::to_string(dwordCount) + " dwords)";
        if (errOut) *errOut = what;
        if (m_errors.size() < 16) m_errors.push_back(what);
        return false;
    }

    // Decode into the owned state; run stats fold directly into the
    // cumulative evidence (counters add, opcode maps accumulate).
    std::vector<StreamError> errors;
    Pm4Decoder d;
    d.Decode(dwords, dwordCount, m_state, m_cumulative, &errors);

    ++m_submissions;
    m_dwordsDecoded += dwordCount;

    for (const auto& e : errors) {
        const std::string line = std::string(name) +
                                 " @" + std::to_string(e.dwordOffset) +
                                 ": " + e.what;
        if (m_errors.size() < 16) m_errors.push_back(line);
    }
    // Stream errors are evidence, not fatal: partial decode of a malformed
    // buffer still yields real stats. The error ring carries the reasons.
    return true;
}

int64_t GnmSubmit::SubmitDescriptors(uint64_t count, uint64_t descriptorsPtr,
                                     uint64_t userDataPtr,
                                     std::string* errOut) {
    (void)userDataPtr;  // parsed; semantics land in later phases

    if (count == 0 || count > 32) {
        const std::string what = "sceGnmSubmitCommandBuffers: count=" +
                                 std::to_string(count) + " outside [1..32]";
        if (errOut) *errOut = what;
        if (m_errors.size() < 16) m_errors.push_back(what);
        return -static_cast<int64_t>(0x16 /*EINVAL*/);
    }

    const size_t arrayBytes = static_cast<size_t>(count) * sizeof(uint64_t);
    const void* descHost = Resolve(descriptorsPtr, arrayBytes);
    if (!descHost) {
        const std::string what =
            "sceGnmSubmitCommandBuffers: descriptor array unresolvable @0x" +
            [](uint64_t v) {
                char b[32]; std::snprintf(b, sizeof(b), "%llX",
                                          (unsigned long long)v);
                return std::string(b);
            }(descriptorsPtr);
        if (errOut) *errOut = what;
        if (m_errors.size() < 16) m_errors.push_back(what);
        return -static_cast<int64_t>(0xE /*EFAULT*/);
    }

    const uint64_t* descs = static_cast<const uint64_t*>(descHost);

    // Evidence-first: hex-log raw descriptors of every submission
    // (bounded). If the provisional packing is wrong, THIS is what
    // corrects it from real-device evidence.
    {
        std::string hex;
        for (uint64_t i = 0; i < count && i < 8; ++i) {
            char b[32];
            std::snprintf(b, sizeof(b), "%s%016llX", i ? " " : "",
                          (unsigned long long)descs[i]);
            hex += b;
        }
        PX5_LOGI(LogCategory::GPU,
                 "%ssceGnmSubmitCommandBuffers: count=%llu descriptors=[%s%s]",
                 m_logTag,
                 (unsigned long long)count, hex.c_str(),
                 count > 8 ? " ..." : "");
    }

    int64_t rc = 0;
    for (uint64_t i = 0; i < count; ++i) {
        const uint64_t desc    = descs[i];
        const uint64_t addr    = desc & kDescAddrMask;
        const uint64_t ndwords = desc >> 48;

        if (ndwords == 0 || ndwords > kMaxSubmitDwords) {
            const std::string what = "descriptor " + std::to_string(i) +
                                     ": dword count " +
                                     std::to_string(ndwords) +
                                     " implausible (packing may be wrong — "
                                     "raw desc logged above)";
            if (errOut) *errOut = what;
            if (m_errors.size() < 16) m_errors.push_back(what);
            rc = -static_cast<int64_t>(0x16 /*EINVAL*/);
            continue;
        }

        const size_t bytes = static_cast<size_t>(ndwords) * sizeof(uint32_t);
        const void* host = Resolve(addr, bytes);
        if (!host) {
            const std::string what = "descriptor " + std::to_string(i) +
                                     ": buffer address unresolvable @0x" +
                                     [](uint64_t v) {
                                         char b[32];
                                         std::snprintf(b, sizeof(b), "%llX",
                                                       (unsigned long long)v);
                                         return std::string(b);
                                     }(addr);
            if (errOut) *errOut = what;
            if (m_errors.size() < 16) m_errors.push_back(what);
            rc = -static_cast<int64_t>(0xE /*EFAULT*/);
            continue;
        }

        std::string subErr;
        SubmitBuffer(static_cast<const uint32_t*>(host),
                     static_cast<size_t>(ndwords),
                     ("cb[" + std::to_string(i) + "]").c_str(), &subErr);
    }
    return rc;
}

std::string GnmSubmit::GetStatsString() const {
    char head[192];
    std::snprintf(head, sizeof(head),
                  "submits=%llu dwords=%llu draws=%llu dispatches=%llu "
                  "regWrites=%llu streamErrors=%llu",
                  (unsigned long long)m_submissions,
                  (unsigned long long)m_dwordsDecoded,
                  (unsigned long long)m_cumulative.drawCalls,
                  (unsigned long long)m_cumulative.dispatches,
                  (unsigned long long)m_state.TotalRegisterWrites(),
                  (unsigned long long)m_cumulative.streamErrors);
    return std::string(head);
}

} // namespace PX5::Gnm
