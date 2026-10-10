// PX5 v1.48 — ORBIS video-out HLE: the presentation-facing service games
// call to open a display, register swap-chain buffers and submit flips.
//
// SEMANTICS PORTED FROM AnyPS5 core/libs/prx/libSceVideoOut (GPL-2.0-only,
// pin c72d5ce1): verbatim ORBIS error codes, the RegisterBuffers2
// validation chain, the flip-status layout and the 1920x1080 default
// resolution (AnyPS5's own guest test pins fullWidth==1920 right after
// Open, before any buffers exist — games allocate against it).
//
// HONESTY BOUNDARY: PX5 has no display engine behind this surface yet.
// The handle table, buffer records, attribute validation and flip-status
// accounting are real; the flip itself RETIRES immediately (pending
// decays at once) and vblank waits return without blocking — logged once.
// The guest-buffer -> screen presentation bridge is a later,
// device-validated milestone; nothing here claims pixels were shown.
#ifndef PX5_GPU_VIDEOOUT_HLE_H
#define PX5_GPU_VIDEOOUT_HLE_H

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace PX5::SceVideoOutHle {

// --- ORBIS constants (verbatim from AnyPS5 libSceVideoOut) ---------------
constexpr int VIDEO_OUT_ERROR_INVALID_VALUE          = -2144796671;
constexpr int VIDEO_OUT_ERROR_INVALID_ADDRESS        = -2144796670;
constexpr int VIDEO_OUT_ERROR_INVALID_HANDLE         = -2144796661;
constexpr int VIDEO_OUT_ERROR_INVALID_INDEX          = -2144796662;
constexpr int VIDEO_OUT_ERROR_INVALID_OPTION         = -2144796646;
constexpr int VIDEO_OUT_ERROR_INVALID_CATEGORY       = -2144796643;
constexpr int VIDEO_OUT_ERROR_SLOT_OCCUPIED          = -2144796656;
constexpr int VIDEO_OUT_ERROR_RESOURCE_BUSY          = -2144796663;
constexpr int VIDEO_OUT_ERROR_FLIP_QUEUE_FULL        = -2144796654;
constexpr int VIDEO_OUT_ERROR_UNSUPPORTED_OUTPUT_MODE = -2144796650;
constexpr int VIDEO_OUT_ERROR_UNAVAILABLE_OUTPUT_MODE = -2144796647;
constexpr int VIDEO_OUT_ERROR_UNKNOWN_OUTPUT_MODE    = -2144796642;

constexpr int VIDEO_OUT_BUS_TYPE_MAIN    = 0;
constexpr int VIDEO_OUT_BUS_TYPE_OVERLAY = 1;
constexpr int VIDEO_OUT_BUS_TYPE_SUB     = 2;

constexpr uint32_t VIDEO_OUT_DEFAULT_WIDTH  = 1920;
constexpr uint32_t VIDEO_OUT_DEFAULT_HEIGHT = 1080;

constexpr int VIDEO_OUT_BUFFER_NUM_MAX           = 16;
constexpr int VIDEO_OUT_BUFFER_ATTRIBUTE_NUM_MAX = 4;
constexpr size_t VIDEO_OUT_FLIP_QUEUE_CAPACITY   = 16;

constexpr int VIDEO_OUT_BUFFER_ATTRIBUTE_CATEGORY_UNCOMPRESSED = 0;
constexpr int VIDEO_OUT_BUFFER_ATTRIBUTE_CATEGORY_COMPRESSED   = 1;

constexpr int VIDEO_OUT_FLIP_MODE_VSYNC       = 1;
constexpr int VIDEO_OUT_FLIP_MODE_VSYNC_MULTI = 4;

constexpr int VIDEO_OUT_BUFFER_INDEX_BLACK = -2;
constexpr int VIDEO_OUT_BUFFER_INDEX_BLANK = -1;

constexpr uint64_t VIDEO_OUT_OUTPUT_MODE_DEFAULT   = 0x0000000000000001ULL;
constexpr uint64_t VIDEO_OUT_OUTPUT_MODE_119_88HZ  = 0x000000000000000FULL;
constexpr uint64_t VIDEO_OUT_REFRESH_RATE_59_94HZ  = 3;
constexpr uint64_t VIDEO_OUT_REFRESH_RATE_119_88HZ = 13;

// --- Guest ABI layouts (locked against AnyPS5 SceTypes.hpp) --------------

struct VideoOutBufferAttribute2 {
    uint32_t reserved0;
    uint32_t tiling_mode;
    uint32_t aspect_ratio;
    uint32_t width;
    uint32_t height;
    uint32_t pitch_in_pixel;
    uint64_t option;
    uint64_t pixel_format;
    uint64_t dcc_cb_register_clear_color;
    uint32_t dcc_control;
    uint32_t pad0;
    uint64_t reserved1[3];
};
static_assert(sizeof(VideoOutBufferAttribute2) == 80);
static_assert(offsetof(VideoOutBufferAttribute2, option) == 24);
static_assert(offsetof(VideoOutBufferAttribute2, pixel_format) == 32);

struct VideoOutBuffers {
    const void* data;
    const void* metadata;
    const void* reserved[2];
};
static_assert(sizeof(VideoOutBuffers) == 32);

struct VideoOutFlipStatus {
    uint64_t count = 0;
    uint64_t processTime = 0;
    uint64_t reserved0 = 0;
    int64_t flipArg = 0;
    uint64_t reserved1 = 0;
    uint64_t processTimeCounter = 0;
    int32_t gcQueueNum = 0;
    int32_t flipPendingNum = 0;
    int32_t currentBuffer = 0;
    uint32_t reserved2 = 0;
    uint64_t submitProcessTimeCounter = 0;
    uint64_t reserved3[7] = {};
};
static_assert(sizeof(VideoOutFlipStatus) == 128);
static_assert(offsetof(VideoOutFlipStatus, flipArg) == 24);
static_assert(offsetof(VideoOutFlipStatus, gcQueueNum) == 48);

struct VideoOutVblankStatus {
    uint64_t count = 0;
    uint64_t processTime = 0;
    uint64_t reserved = 0;
    uint64_t processTimeCounter = 0;
    uint8_t flags = 0;
    uint8_t phase = 0;
    uint8_t pad1[6] = {};
};
static_assert(sizeof(VideoOutVblankStatus) == 40);

struct VideoOutOutputStatus {
    uint32_t resolution = 0;
    uint32_t dynamicRange = 0;
    uint64_t refreshRate = 0;
    uint64_t flags = 0;
    uint64_t reserved[3] = {};
};
static_assert(sizeof(VideoOutOutputStatus) == 48);

struct VideoOutResolutionStatus {
    uint32_t fullWidth = 0;
    uint32_t fullHeight = 0;
    uint32_t paneWidth = 0;
    uint32_t paneHeight = 0;
    uint64_t refreshRate = 0;
    float screenSizeInInch = 0.0f;
    uint16_t flags = 0;
    uint16_t reserved0 = 0;
    uint32_t reserved1[3] = {};
};
static_assert(sizeof(VideoOutResolutionStatus) == 48);
static_assert(offsetof(VideoOutResolutionStatus, refreshRate) == 16);
static_assert(offsetof(VideoOutResolutionStatus, screenSizeInInch) == 24);

struct VideoOutOpenParam {
    uint32_t firstWord;
    uint32_t setPriority;
    int32_t priority;
    uint32_t setAffinity;
    uint64_t affinity;
};
static_assert(sizeof(VideoOutOpenParam) == 24);
static_assert(offsetof(VideoOutOpenParam, affinity) == 16);

// One registered swap-chain buffer (what the GPU path will read later).
struct VideoOutBufferRecord {
    int setIndex = -1;       ///< attribute group this buffer belongs to
    uint64_t dataVa = 0;     ///< guest VA of the pixel data
    uint64_t metadataVa = 0; ///< guest VA of the metadata (may be 0)
    bool occupied = false;
};

/**
 * ORBIS video-out HLE surface. Single main output, verbatim validation
 * order, nothing fabricated past the honesty boundary above.
 */
class VideoOutHle {
public:
    static VideoOutHle& Instance();

    // --- service surface (all return ORBIS int32 codes) -------------------

    int32_t Open(int32_t userId, int32_t busType, int32_t index,
                 uint64_t paramVa);
    int32_t Close(int32_t handle);

    int32_t RegisterBuffers2(int32_t handle, int32_t setIndex,
                             int32_t bufferIndexStart, uint64_t buffersVa,
                             int32_t bufferNum, uint64_t attributeVa,
                             int32_t category, uint64_t optionVa);

    int32_t SubmitFlip(int32_t handle, int32_t index, int32_t flipMode,
                       int64_t flipArg);

    int32_t GetFlipStatus(int32_t handle, uint64_t statusVa);
    int32_t GetVblankStatus(int32_t handle, uint64_t statusVa);
    int32_t GetOutputStatus(int32_t handle, uint64_t statusVa);
    int32_t GetResolutionStatus(int32_t handle, uint64_t statusVa);
    int32_t SetFlipRate(int32_t handle, int32_t rate);
    int32_t IsFlipPending(int32_t handle);
    int32_t WaitVblank(int32_t handle);
    int32_t AllowOutputResolutionWqhdDetection(int32_t handle);

    /// AnyPS5-faithful: the only supported mode is the default one.
    int32_t IsOutputSupported(int32_t handle, uint64_t mode,
                              uint64_t optionsVa);
    int32_t ConfigureOutput(int32_t handle, uint64_t mode,
                            uint64_t optionsVa);

    /// True when handle names the opened output.
    bool IsOpen(int32_t handle) const;

    /// Host-gate seam: closes everything so tests start from a clean slate.
    void ResetForTest();

private:
    VideoOutHle() = default;

    struct Output {
        bool opened = false;
        bool closing = false;
        uint32_t width = VIDEO_OUT_DEFAULT_WIDTH;
        uint32_t height = VIDEO_OUT_DEFAULT_HEIGHT;
        uint64_t outputMode = VIDEO_OUT_OUTPUT_MODE_DEFAULT;
        int flipRate = 0;
        bool wqhdAllowed = false;
        VideoOutFlipStatus flipStatus;
        VideoOutVblankStatus vblankStatus;
        VideoOutBufferAttribute2 attributes[VIDEO_OUT_BUFFER_ATTRIBUTE_NUM_MAX];
        bool attributeOccupied[VIDEO_OUT_BUFFER_ATTRIBUTE_NUM_MAX] = {};
        VideoOutBufferRecord buffers[VIDEO_OUT_BUFFER_NUM_MAX];
    };

    // m_mutex guards the single Output record; methods copy status out
    // under it and never hold it across logging.
    mutable std::mutex m_mutex;
    Output m_out;
    int32_t m_nextHandle = 1;

    uint64_t m_vblankOnceLog = 0;   ///< one-shot "no vblank engine" marker
};

} // namespace PX5::SceVideoOutHle

#endif // PX5_GPU_VIDEOOUT_HLE_H
