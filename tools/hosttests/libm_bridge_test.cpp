// PX5 v1.66 — libm/XMM-bridge host test.
//
// Locks the two directions of the v1.66 XMM spill at the HLE boundary:
//   * FP arguments: the test fills ImportCall.xmm directly (the same state
//     the syscall entry publishes from the live frame on device), the math
//     HLE reads them through ReadXmmDouble/ReadXmmFloat,
//   * FP returns: the HLE publishes the SysV XMM0 bit pattern through the
//     bridge's return channel (uint64_t xmmRet), exactly what the epilogue
//     writes back into the guest frame,
//   * printf %f consumes its OWN XMM sequence and formats the value.
// A missing XMM snapshot refuses named (-ENODATA) instead of computing on
// garbage — also locked here.
#include "kernel/libc_hle.h"
#include "kernel/syscalls.h"
#include "kernel/sce_kernel_hle.h"   // SymbolEntry
#include "loader/runtime_linker.h"   // RuntimeLinker, ImportTrapEntry, range
#include "memory/memory.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using PX5::SceKernelHle::LibcHle;
using PX5::ImportCall;
using Mgr = PX5::MemoryManager;

static int g_failures = 0;
#define CHECK(cond, msg)                                                    \
    do {                                                                    \
        if (!(cond)) {                                                      \
            ++g_failures;                                                   \
            std::printf("  FAIL: %s (line %d)\n", msg, __LINE__);           \
        }                                                                   \
    } while (0)

static uint64_t Dbits(double d) {
    uint64_t b = 0;
    std::memcpy(&b, &d, sizeof b);
    return b;
}
static double BitsD(uint64_t b) {
    double d = 0;
    std::memcpy(&d, &b, sizeof d);
    return d;
}

// builds an ImportCall with integer args + XMM0/XMM1 set to raw doubles
static ImportCall MakeCall(std::initializer_list<uint64_t> ints, double x0,
                           double x1 = 0.0, bool setXmm = true) {
    ImportCall c{};
    size_t i = 0;
    for (uint64_t a : ints) c.args[i++] = a;
    if (setXmm) {
        c.xmm[0][0] = Dbits(x0);
        c.xmm[1][0] = Dbits(x1);
        c.xmmValid = true;   // the device bridge sets this from the live frame
    }
    return c;
}

int main() {
    std::printf("[libm_bridge_test] window init\n");
    auto& mm = Mgr::GetInstance();
    CHECK(mm.Initialize() && mm.GetGuestBase() != 0, "window initialized");

    auto& libc = LibcHle::Instance();
    CHECK(libc.HasSymbol("sin") && libc.HasSymbol("pow") &&
              libc.HasSymbol("sinf") && libc.HasSymbol("atan2"),
          "the game's decoded math surface is present");

    // ------------------------------------------------------------------
    // one-arg doubles
    // ------------------------------------------------------------------
    std::printf("[libm_bridge_test] one-arg doubles\n");
    {
        uint64_t ret = 0;
        auto c = MakeCall({}, 1.0);
        CHECK(libc.Invoke("sin", c, &ret) == 0, "sin returns via RAX=0");
        CHECK(ret == Dbits(std::sin(1.0)), "sin(1.0) bit-exact in XMM0 slot");
    }
    {
        uint64_t ret = 0;
        auto c = MakeCall({}, 2.0);
        libc.Invoke("log", c, &ret);
        CHECK(BitsD(ret) == std::log(2.0), "log(2.0) exact");
        c = MakeCall({}, 10.0);
        libc.Invoke("exp", c, &ret);
        CHECK(BitsD(ret) == std::exp(10.0), "exp(10.0) exact");
        c = MakeCall({}, 8.0);
        libc.Invoke("log2", c, &ret);
        CHECK(BitsD(ret) == 3.0, "log2(8.0) == 3");
        c = MakeCall({}, 100.0);
        libc.Invoke("log10", c, &ret);
        CHECK(BitsD(ret) == 2.0, "log10(100.0) == 2");
        c = MakeCall({}, 0.5);
        libc.Invoke("asin", c, &ret);
        CHECK(BitsD(ret) == std::asin(0.5), "asin exact");
        libc.Invoke("acos", c, &ret);
        CHECK(BitsD(ret) == std::acos(0.5), "acos exact");
        libc.Invoke("atan", c, &ret);
        CHECK(BitsD(ret) == std::atan(0.5), "atan exact");
        libc.Invoke("tan", c, &ret);
        CHECK(BitsD(ret) == std::tan(0.5), "tan exact");
        libc.Invoke("round", c, &ret);
        CHECK(BitsD(ret) == 1.0, "round(0.5) == 1");
    }

    // ------------------------------------------------------------------
    // two-arg doubles
    // ------------------------------------------------------------------
    std::printf("[libm_bridge_test] two-arg doubles\n");
    {
        uint64_t ret = 0;
        auto c = MakeCall({}, 2.0, 10.0);
        libc.Invoke("pow", c, &ret);
        CHECK(BitsD(ret) == 1024.0, "pow(2,10) == 1024");
        c = MakeCall({}, 7.0, 3.0);
        libc.Invoke("fmod", c, &ret);
        CHECK(BitsD(ret) == std::fmod(7.0, 3.0), "fmod exact");
        c = MakeCall({}, 1.0, 2.0);
        libc.Invoke("atan2", c, &ret);
        CHECK(BitsD(ret) == std::atan2(1.0, 2.0), "atan2 exact");
    }

    // ------------------------------------------------------------------
    // float forms: result is the SysV float return (XMM0 low 32 bits)
    // ------------------------------------------------------------------
    std::printf("[libm_bridge_test] float forms\n");
    {
        uint64_t ret = 0;
        ImportCall c = MakeCall({}, 0.25f);
        // set the FLOAT view of XMM0 explicitly
        const float in = 0.25f;
        uint32_t ib = 0;
        std::memcpy(&ib, &in, sizeof ib);
        c.xmm[0][0] = ib;
        libc.Invoke("sinf", c, &ret);
        const float got = std::sin(in);
        uint32_t gb = 0;
        std::memcpy(&gb, &got, sizeof gb);
        CHECK((ret & 0xFFFFFFFFull) == gb, "sinf returns float bits in XMM0[31:0]");
        libc.Invoke("cosf", c, &ret);
        const float gotc = std::cos(in);
        std::memcpy(&gb, &gotc, sizeof gb);
        CHECK((ret & 0xFFFFFFFFull) == gb, "cosf returns float bits in XMM0[31:0]");
    }

    // ------------------------------------------------------------------
    // missing XMM snapshot refuses named (no fabricated zero)
    // ------------------------------------------------------------------
    std::printf("[libm_bridge_test] named refusal\n");
    {
        uint64_t ret = 0xDEADBEEFull;
        auto c = MakeCall({}, 1.0, 0.0, /*setXmm=*/false);
        CHECK(libc.Invoke("sin", c, &ret) ==
                  static_cast<uint64_t>(-61),   // -ENODATA
              "missing XMM -> -ENODATA through RAX");
    }

    // ------------------------------------------------------------------
    // printf %f: the FP conversion consumes its own XMM sequence
    // ------------------------------------------------------------------
    std::printf("[libm_bridge_test] printf %%f\n");
    {
        // dst + format string live in the guest window; snprintf(dst, cap,
        // fmt, ...) takes fmt at a2 with varargs from a3 (XMM sequence).
        const uint64_t va = mm.GetGuestBase() + 0x30001000;
        mm.MapMemory(va & ~0xFFFull, 0x2000,
                     PX5::MemoryFlags::PAGE_READ | PX5::MemoryFlags::PAGE_WRITE,
                     "libm_test");
        auto* host = static_cast<char*>(mm.GetHostPointer(va));
        std::memset(host, 0, 0x2000);
        const char* fmt = "v=%f!";
        const uint64_t fmtVa = va;
        const uint64_t dstVa = va + 0x100;
        std::memcpy(host, fmt, std::strlen(fmt) + 1);

        ImportCall c{};
        c.args[0] = dstVa;                    // dst
        c.args[1] = 64;                       // cap
        c.args[2] = fmtVa;                    // fmt
        c.xmm[0][0] = Dbits(3.5);             // the float vararg (own sequence)
        c.xmmValid = true;
        libc.Invoke("snprintf", c, nullptr);
        const char* got = static_cast<const char*>(
            mm.GetHostPointer(dstVa));
        CHECK(got && std::string(got) == "v=3.500000!",
              "snprintf %%f formatted the XMM argument for real");
    }

    // printf: same truth through the console path
    {
        const uint64_t va = mm.GetGuestBase() + 0x30001000;
        auto* host = static_cast<char*>(mm.GetHostPointer(va));
        const char* fmt = "p=%g\n";
        std::memcpy(host, fmt, std::strlen(fmt) + 1);
        ImportCall c{};
        c.args[0] = va;
        c.xmm[0][0] = Dbits(0.125);
        c.xmmValid = true;
        libc.Invoke("printf", c, nullptr);
        const std::string captured = PX5::GuestSyscalls::TakeOutput();
        CHECK(captured.find("p=0.125") != std::string::npos,
              "printf %%g formatted the XMM argument");
        CHECK(captured.find("<f>") == std::string::npos,
              "no <f> placeholder remains");
    }

    // ------------------------------------------------------------------
    // v1.69 — the FULL device chain through the REAL dispatch path:
    // entry snapshot -> GuestSyscalls::Dispatch (import-call range) ->
    // RuntimeLinker::DispatchImportCall -> KernelHle -> libc libm ->
    // explicit FP-return claim -> GuestBridge return channel -> what the
    // syscall epilogue writes into the guest frame's XMM0.
    // Before v1.69 the HLE wrote xmmRet and Dispatch DROPPED it —
    // PublishXmmReturn had no call site and the epilogue channel was dead.
    // ------------------------------------------------------------------
    std::printf("[libm_bridge_test] full dispatch chain (pow via the bridge)\n");
    {
        using PX5::RuntimeLinker;
        using PX5::ImportTrapEntry;
        using PX5::GuestSyscalls;
        namespace GB = PX5::GuestBridge;
        // forces KernelHle::RegisterAll, which installs the production
        // import route (the same seam the device loader resolves through)
        CHECK(PX5::SceKernelHle::KernelHle::GetInstance().HasSymbol("pow"),
              "production route registered (pow resolves)");
        ImportTrapEntry e{};
        e.name = "pow";
        e.isPlt = true;
        RuntimeLinker::GetInstance().SetImportTraps(
            PX5::kPx5ImportCallBase, PX5::kPx5ImportCallEnd, {e});

        // syscall entry: the handler publishes the caller's XMM0..7
        GB::XmmArgs xa{};
        xa.v[0][0] = Dbits(2.0);
        xa.v[1][0] = Dbits(10.0);
        GB::PublishXmmArgs(xa);

        const uint64_t rax = GuestSyscalls::Dispatch(
            PX5::kPx5ImportCallBase + 0, 0, 0, 0, 0, 0, 0, 0);
        CHECK(rax == 0, "pow dispatch returns RAX=0 (result rides XMM0)");

        uint64_t xmm[2] = {0, 0};
        CHECK(GB::TakeXmmReturn(xmm),
              "epilogue channel carries the FP return (was dead before v1.69)");
        CHECK(BitsD(xmm[0]) == 1024.0,
              "guest-visible XMM0 == pow(2,10) == 1024 (bit-exact)");
    }

    if (g_failures == 0) {
        std::printf("[libm_bridge_test] ALL PASS\n");
        return 0;
    }
    std::printf("[libm_bridge_test] FAILURES: %d\n", g_failures);
    return 1;
}
