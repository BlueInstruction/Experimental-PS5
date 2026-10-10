// Host regression test for the v1.48 ORBIS video-out HLE: the surface a
// game drives to open a display, register its swap chain and submit
// flips. Semantics ported from AnyPS5 libSceVideoOut (GPL-2.0) — verbatim
// error codes, validation order, the 1920x1080 open default (games
// allocate against it before any buffer exists), and the flip-status
// accounting. The honesty boundary (flips retire, no display engine yet)
// is asserted too: pending never accumulates until the presentation
// bridge exists, and the buffer records are what a later GPU path reads.
#include "gpu/videoout_hle.h"
#include "kernel/sce_kernel_hle.h"
#include "loader/nid.h"
#include "memory/memory.h"

#include <cstdio>
#include <cstring>

using namespace PX5;
using namespace PX5::SceVideoOutHle;

static int fails = 0;
static void chk(bool ok, const char* what) {
    printf("  [%s] %s\n", ok ? "OK  " : "FAIL", what);
    if (!ok) ++fails;
}

namespace {

struct GuestFrame {
    // A small mapped window region playing every struct role.
    VideoOutOpenParam openParam;
    VideoOutBufferAttribute2 attribute;
    VideoOutBuffers buffers[4];
};

} // namespace

int main() {
    auto& mm = MemoryManager::GetInstance();
    if (!mm.Initialize(4096)) {
        printf("window init failed\n");
        return 2;
    }
    const uint64_t base = mm.GetGuestBase();
    const uint64_t frameVa = base + 0x31000000ull;
    if (!mm.MapMemory(frameVa, 4096,
                      MemoryFlags::PAGE_READ | MemoryFlags::PAGE_WRITE,
                      "videoout_hle_test") ||
        !mm.MapMemory(frameVa + 0x10000, 64,
                      MemoryFlags::PAGE_READ | MemoryFlags::PAGE_WRITE,
                      "videoout_status")) {
        printf("window map failed\n");
        return 2;
    }
    auto& vo = VideoOutHle::Instance();
    vo.ResetForTest();

    GuestFrame gf{};
    gf.openParam.firstWord = 16;
    gf.attribute.width = 1280;
    gf.attribute.height = 720;
    gf.attribute.pitch_in_pixel = 1280;
    gf.attribute.pixel_format = 0x80000022ULL;   // A8R8G8B8 sRGB class
    alignas(64) uint8_t pixels[64] = {};
    gf.buffers[0].data = pixels;
    gf.buffers[1].data = pixels + 32;
    const uint64_t paramVa = frameVa;
    const uint64_t attributeVa = frameVa + sizeof(VideoOutOpenParam);
    const uint64_t buffersVa = attributeVa + sizeof(VideoOutBufferAttribute2);
    const uint64_t statusVa = frameVa + 0x10000;
    if (!mm.WriteGuestMemory(frameVa, &gf, sizeof gf)) {
        printf("guest frame write failed\n");
        return 2;
    }

    // --- Open validation (AnyPS5 order) -----------------------------------
    printf("\nOpen validation:\n");
    chk(vo.Open(1, VIDEO_OUT_BUS_TYPE_MAIN, 0, 0) ==
            VIDEO_OUT_ERROR_INVALID_VALUE,
        "userId other than 0/255 refused");
    chk(vo.Open(255, 7, 0, 0) == VIDEO_OUT_ERROR_INVALID_VALUE,
        "unknown bus type refused");
    chk(vo.Open(255, VIDEO_OUT_BUS_TYPE_MAIN, 1, 0) ==
            VIDEO_OUT_ERROR_INVALID_VALUE,
        "non-zero index refused");
    const VideoOutOpenParam badParam{17, 0, 0, 0, 0};
    mm.WriteGuestMemory(frameVa, &badParam, sizeof badParam);
    chk(vo.Open(255, VIDEO_OUT_BUS_TYPE_MAIN, 0, paramVa) ==
            VIDEO_OUT_ERROR_INVALID_VALUE,
        "param with wrong first word refused");
    const VideoOutOpenParam goodParam{16, 0, 0, 0, 0};
    mm.WriteGuestMemory(frameVa, &goodParam, sizeof goodParam);
    chk(vo.Open(255, VIDEO_OUT_BUS_TYPE_MAIN, 0, base + 0x3E000000ull) ==
            VIDEO_OUT_ERROR_INVALID_ADDRESS,
        "unmapped param VA refused");
    mm.WriteGuestMemory(frameVa, &gf, sizeof gf);   // restore the good param
    const int32_t handle = vo.Open(255, VIDEO_OUT_BUS_TYPE_MAIN, 0, paramVa);
    chk(handle > 0, "open returns a positive handle");
    chk(vo.Open(255, VIDEO_OUT_BUS_TYPE_MAIN, 0, paramVa) ==
            VIDEO_OUT_ERROR_RESOURCE_BUSY,
        "second open refused (single main surface)");

    // --- Resolution default (AnyPS5 guest test pins 1920x1080) ------------
    printf("\nResolution default:\n");
    VideoOutResolutionStatus res{};
    mm.WriteGuestMemory(statusVa, &res, sizeof res);
    chk(vo.GetResolutionStatus(handle, statusVa) == 0,
        "resolution status reads");
    mm.ReadGuestMemory(statusVa, &res, sizeof res);
    chk(res.fullWidth == 1920 && res.fullHeight == 1080 &&
            res.paneWidth == 1920 && res.paneHeight == 1080,
        "default resolution 1920x1080 before any buffers");
    chk(res.refreshRate == VIDEO_OUT_REFRESH_RATE_59_94HZ,
        "default refresh 59.94Hz encoding");
    chk(vo.GetResolutionStatus(99, statusVa) ==
            VIDEO_OUT_ERROR_INVALID_HANDLE,
        "stale handle refused");
    chk(vo.GetResolutionStatus(handle, base + 0x3E000000ull) ==
            VIDEO_OUT_ERROR_INVALID_ADDRESS,
        "unmapped status VA refused");

    // --- RegisterBuffers2 chain -------------------------------------------
    printf("\nRegisterBuffers2 validation chain:\n");
    chk(vo.RegisterBuffers2(handle, 0, 0, 0, 2, attributeVa, 0, 0) ==
            VIDEO_OUT_ERROR_INVALID_ADDRESS,
        "null buffers refused");
    chk(vo.RegisterBuffers2(handle, 0, 0, buffersVa, 2, 0, 0, 0) ==
            VIDEO_OUT_ERROR_INVALID_OPTION,
        "null attribute refused");
    chk(vo.RegisterBuffers2(handle, 0, 0, buffersVa, 2, attributeVa, 0,
                            attributeVa) ==
            VIDEO_OUT_ERROR_INVALID_OPTION,
        "non-null option refused");
    chk(vo.RegisterBuffers2(handle, 4, 0, buffersVa, 2, attributeVa, 0, 0) ==
            VIDEO_OUT_ERROR_INVALID_VALUE,
        "setIndex beyond the group table refused");
    chk(vo.RegisterBuffers2(handle, 0, 0, buffersVa, 0, attributeVa, 0, 0) ==
            VIDEO_OUT_ERROR_INVALID_VALUE,
        "zero buffer count refused");
    chk(vo.RegisterBuffers2(handle, 0, 15, buffersVa, 2, attributeVa, 0,
                            0) ==
            VIDEO_OUT_ERROR_INVALID_VALUE,
        "range past the slot table refused");
    chk(vo.RegisterBuffers2(handle, 0, 0, buffersVa, 2, attributeVa, 2, 0) ==
            VIDEO_OUT_ERROR_INVALID_CATEGORY,
        "unknown category refused");
    chk(vo.RegisterBuffers2(handle, 0, 0, base + 0x3E000000ull, 2,
                            attributeVa, 0, 0) ==
            VIDEO_OUT_ERROR_INVALID_ADDRESS,
        "unmapped buffers VA refused");
    chk(vo.RegisterBuffers2(handle, 0, 0, buffersVa, 2, attributeVa, 0, 0) ==
            0,
        "happy registration accepted");
    chk(vo.RegisterBuffers2(handle, 0, 0, buffersVa, 2, attributeVa, 0, 0) ==
            VIDEO_OUT_ERROR_INVALID_INDEX,
        "re-registering the same set refused");
    chk(vo.RegisterBuffers2(handle, 1, 1, buffersVa, 2, attributeVa, 0, 0) ==
            VIDEO_OUT_ERROR_SLOT_OCCUPIED,
        "overlap with occupied slots refused");
    chk(vo.GetResolutionStatus(handle, statusVa) == 0,
        "resolution readable after registration");
    mm.ReadGuestMemory(statusVa, &res, sizeof res);
    chk(res.fullWidth == 1280 && res.fullHeight == 720,
        "registration updates the reported resolution");

    // --- SubmitFlip + status accounting ------------------------------------
    printf("\nSubmitFlip and flip status:\n");
    chk(vo.SubmitFlip(handle, 0, 0, 0) == VIDEO_OUT_ERROR_INVALID_VALUE,
        "flip mode 0 refused");
    chk(vo.SubmitFlip(handle, 0, 7, 0) == VIDEO_OUT_ERROR_INVALID_VALUE,
        "flip mode beyond 6 refused");
    chk(vo.SubmitFlip(handle, 16, VIDEO_OUT_FLIP_MODE_VSYNC, 0) ==
            VIDEO_OUT_ERROR_INVALID_INDEX,
        "buffer index past the table refused");
    chk(vo.SubmitFlip(handle, 5, VIDEO_OUT_FLIP_MODE_VSYNC, 0) ==
            VIDEO_OUT_ERROR_INVALID_INDEX,
        "unoccupied slot flip refused");
    chk(vo.SubmitFlip(handle, VIDEO_OUT_BUFFER_INDEX_BLACK,
                      VIDEO_OUT_FLIP_MODE_VSYNC, 0x77) == 0,
        "special BLACK index accepted");
    chk(vo.SubmitFlip(handle, 0, VIDEO_OUT_FLIP_MODE_VSYNC, 0xAB) == 0,
        "registered slot flip accepted");
    VideoOutFlipStatus flip{};
    chk(vo.GetFlipStatus(handle, statusVa) == 0,
        "flip status readable after flips");
    mm.ReadGuestMemory(statusVa, &flip, sizeof flip);
    chk(flip.count == 2, "flip count advanced by exactly the submissions");
    chk(flip.flipArg == 0xAB, "last flipArg recorded");
    chk(flip.currentBuffer == 0, "currentBuffer names the last slot");
    chk(flip.flipPendingNum == 0,
        "pending retired immediately (no display engine yet — honest)");
    chk(vo.IsFlipPending(handle) == 0, "IsFlipPending agrees");
    chk(vo.GetFlipStatus(42, statusVa) == VIDEO_OUT_ERROR_INVALID_HANDLE,
        "flip status on a stale handle refused");

    // --- Close / reopen lifecycle ------------------------------------------
    printf("\nClose and reopen lifecycle:\n");
    chk(vo.Close(handle) == 0, "close accepted");
    chk(vo.Close(handle) == VIDEO_OUT_ERROR_INVALID_HANDLE,
        "double close refused");
    const int32_t handle2 =
        vo.Open(0, VIDEO_OUT_BUS_TYPE_MAIN, 0, paramVa);
    chk(handle2 > 0 && handle2 != handle, "reopen mints a NEW handle");
    VideoOutFlipStatus fresh{};
    chk(vo.GetFlipStatus(handle2, statusVa) == 0,
        "flip status readable on the new handle");
    mm.ReadGuestMemory(statusVa, &fresh, sizeof fresh);
    chk(fresh.count == 0, "new handle starts with a fresh flip status");
    chk(vo.SubmitFlip(handle, 0, VIDEO_OUT_FLIP_MODE_VSYNC, 0) ==
            VIDEO_OUT_ERROR_INVALID_HANDLE,
        "old handle is dead after close");

    // --- KernelHle wiring + NID resolution ----------------------------------
    printf("\nKernelHle route + NID:\n");
    vo.ResetForTest();
    auto& hle = SceKernelHle::KernelHle::GetInstance();
    hle.RegisterAll();
    const uint64_t openRc = hle.InvokeByName("sceVideoOutOpen", 255, 0, 0);
    chk(static_cast<int32_t>(openRc) > 0,
        "InvokeByName routes sceVideoOutOpen");
    const std::string nidOpen =
        Nid::NidRegistry::Instance().Resolve(
            Nid::ComputeNidString("sceVideoOutOpen"));
    chk(nidOpen == "sceVideoOutOpen",
        "video-out symbol NID-resolvable (guest NID-string imports land)");

    printf("\n%s\n", fails == 0 ? "videoout_hle_test: ALL PASS"
                                : "videoout_hle_test: FAILURES PRESENT");
    return fails == 0 ? 0 : 1;
}
