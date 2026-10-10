// PX5 v1.48 — ORBIS video-out HLE implementation. See videoout_hle.h for
// the contract and the honesty boundary (validation and status accounting
// are real; presentation is a later device-validated milestone).
//
// Validation ORDER and error codes follow AnyPS5
// core/libs/prx/libSceVideoOut (GPL-2.0-only): handle/address checks
// first, then value checks, then state checks. Guest pointers arrive as
// VAs and go through the memory window — an untranslated pointer is a
// named refusal, never a guess.
#include "videoout_hle.h"

#include "../memory/memory.h"
#include "../utils/logger.h"

#include <cstring>

namespace PX5::SceVideoOutHle {

namespace {

// The window translation every guest pointer goes through. len==0 is
// normalized to 1 so null-range checks still validate the base address.
void* ResolveWindowPtr(uint64_t va, size_t len) {
    auto& mm = MemoryManager::GetInstance();
    if (!mm.IsValidAddress(va, len ? len : 1)) return nullptr;
    return mm.GetHostPointer(va);
}

} // namespace

VideoOutHle& VideoOutHle::Instance() {
    static VideoOutHle inst;
    return inst;
}

bool VideoOutHle::IsOpen(int32_t handle) const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_out.opened && handle == m_nextHandle - 1 && handle > 0;
}

void VideoOutHle::ResetForTest() {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_out = Output{};
    m_nextHandle = 1;
}

int32_t VideoOutHle::Open(int32_t userId, int32_t busType, int32_t index,
                          uint64_t paramVa) {
    // AnyPS5 validateOpenParam: a present param must be window-readable
    // and carry the ABI's first word (16).
    if (paramVa != 0) {
        const auto* p = static_cast<const VideoOutOpenParam*>(
            ResolveWindowPtr(paramVa, sizeof(VideoOutOpenParam)));
        if (!p) {
            PX5_LOGW(LogCategory::GPU,
                     "VIDEOOUT: open param unreadable at 0x%llx",
                     (unsigned long long)paramVa);
            return VIDEO_OUT_ERROR_INVALID_ADDRESS;
        }
        if (p->firstWord != 16) {
            return VIDEO_OUT_ERROR_INVALID_VALUE;
        }
    }
    if (userId != 255 && userId != 0) {
        return VIDEO_OUT_ERROR_INVALID_VALUE;
    }
    if (busType != VIDEO_OUT_BUS_TYPE_MAIN &&
        busType != VIDEO_OUT_BUS_TYPE_OVERLAY &&
        busType != VIDEO_OUT_BUS_TYPE_SUB) {
        return VIDEO_OUT_ERROR_INVALID_VALUE;
    }
    if (index != 0) {
        return VIDEO_OUT_ERROR_INVALID_VALUE;
    }
    std::lock_guard<std::mutex> lk(m_mutex);
    // PX5 presents ONE main surface (the app's swapchain); a second open
    // is the same RESOURCE_BUSY a single-display device answers.
    if (m_out.opened) {
        return VIDEO_OUT_ERROR_RESOURCE_BUSY;
    }
    m_out = Output{};
    m_out.opened = true;
    const int32_t handle = m_nextHandle++;   // handles never repeat (AnyPS5
                                             // reopen-changes-handle rule)
    PX5_LOGI(LogCategory::GPU,
             "VIDEOOUT: opened handle=%d bus=%d (default %ux%u)",
             handle, busType, m_out.width, m_out.height);
    return handle;
}

int32_t VideoOutHle::Close(int32_t handle) {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (!m_out.opened || handle != m_nextHandle - 1) {
        return VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    m_out = Output{};
    return 0;
}

int32_t VideoOutHle::RegisterBuffers2(int32_t handle, int32_t setIndex,
                                      int32_t bufferIndexStart,
                                      uint64_t buffersVa, int32_t bufferNum,
                                      uint64_t attributeVa, int32_t category,
                                      uint64_t optionVa) {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (!m_out.opened || handle != m_nextHandle - 1) {
        return VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    if (buffersVa == 0) {
        return VIDEO_OUT_ERROR_INVALID_ADDRESS;
    }
    if (attributeVa == 0) {
        return VIDEO_OUT_ERROR_INVALID_OPTION;
    }
    if (optionVa != 0) {
        return VIDEO_OUT_ERROR_INVALID_OPTION;
    }
    if (setIndex < 0 || setIndex >= VIDEO_OUT_BUFFER_ATTRIBUTE_NUM_MAX ||
        bufferIndexStart < 0 ||
        bufferIndexStart >= VIDEO_OUT_BUFFER_NUM_MAX || bufferNum < 1 ||
        bufferNum > VIDEO_OUT_BUFFER_NUM_MAX ||
        bufferIndexStart + bufferNum > VIDEO_OUT_BUFFER_NUM_MAX) {
        return VIDEO_OUT_ERROR_INVALID_VALUE;
    }
    if (category != VIDEO_OUT_BUFFER_ATTRIBUTE_CATEGORY_UNCOMPRESSED &&
        category != VIDEO_OUT_BUFFER_ATTRIBUTE_CATEGORY_COMPRESSED) {
        return VIDEO_OUT_ERROR_INVALID_CATEGORY;
    }
    auto* buffers = static_cast<const VideoOutBuffers*>(
        ResolveWindowPtr(buffersVa,
                         static_cast<size_t>(bufferNum) *
                             sizeof(VideoOutBuffers)));
    if (!buffers) {
        return VIDEO_OUT_ERROR_INVALID_ADDRESS;
    }
    auto* attribute = static_cast<const VideoOutBufferAttribute2*>(
        ResolveWindowPtr(attributeVa, sizeof(VideoOutBufferAttribute2)));
    if (!attribute) {
        return VIDEO_OUT_ERROR_INVALID_OPTION;
    }
    if (m_out.attributeOccupied[setIndex]) {
        return VIDEO_OUT_ERROR_INVALID_INDEX;
    }
    for (int32_t i = 0; i < bufferNum; ++i) {
        if (m_out.buffers[bufferIndexStart + i].occupied) {
            return VIDEO_OUT_ERROR_SLOT_OCCUPIED;
        }
    }
    m_out.attributeOccupied[setIndex] = true;
    m_out.attributes[setIndex] = *attribute;
    for (int32_t i = 0; i < bufferNum; ++i) {
        auto& rec = m_out.buffers[bufferIndexStart + i];
        rec.setIndex = setIndex;
        rec.dataVa = reinterpret_cast<uint64_t>(buffers[i].data);
        rec.metadataVa = reinterpret_cast<uint64_t>(buffers[i].metadata);
        rec.occupied = true;
    }
    m_out.width = attribute->width;
    m_out.height = attribute->height;
    PX5_LOGI(LogCategory::GPU,
             "VIDEOOUT: buffers set=%d start=%d num=%d %ux%u fmt=0x%llx",
             setIndex, bufferIndexStart, bufferNum, attribute->width,
             attribute->height,
             (unsigned long long)attribute->pixel_format);
    return 0;
}

int32_t VideoOutHle::SubmitFlip(int32_t handle, int32_t index,
                                int32_t flipMode, int64_t flipArg) {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (!m_out.opened || handle != m_nextHandle - 1) {
        return VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    // Every mode is presented at the next vsync; HSYNC/WINDOW variants
    // only change tearing behaviour (AnyPS5 accepts 1..6).
    if (flipMode < VIDEO_OUT_FLIP_MODE_VSYNC || flipMode > 6) {
        return VIDEO_OUT_ERROR_INVALID_VALUE;
    }
    if (index < VIDEO_OUT_BUFFER_INDEX_BLACK ||
        index >= VIDEO_OUT_BUFFER_NUM_MAX) {
        return VIDEO_OUT_ERROR_INVALID_INDEX;
    }
    const bool special = (index == VIDEO_OUT_BUFFER_INDEX_BLANK ||
                          index == VIDEO_OUT_BUFFER_INDEX_BLACK);
    if (!special && !m_out.buffers[index].occupied) {
        return VIDEO_OUT_ERROR_INVALID_INDEX;
    }
    // No display engine yet: the flip retires at once (count advances,
    // pending returns to zero — a real queue bound can never trip until
    // the presentation bridge lands, so FLIP_QUEUE_FULL stays untestable
    // and unclaimed).
    auto& fs = m_out.flipStatus;
    ++fs.count;
    fs.flipArg = flipArg;
    fs.currentBuffer = index;
    fs.flipPendingNum = 0;
    if (m_vblankOnceLog == 0) {
        m_vblankOnceLog = 1;
        PX5_LOGW(LogCategory::GPU,
                 "VIDEOOUT: flip queued (count=%llu arg=0x%llx) — no "
                 "display engine yet, flips retire immediately",
                 (unsigned long long)fs.count,
                 (unsigned long long)flipArg);
    }
    return 0;
}

int32_t VideoOutHle::GetFlipStatus(int32_t handle, uint64_t statusVa) {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (!m_out.opened || handle != m_nextHandle - 1) {
        return VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    auto* status = static_cast<VideoOutFlipStatus*>(
        ResolveWindowPtr(statusVa, sizeof(VideoOutFlipStatus)));
    if (!status) {
        return VIDEO_OUT_ERROR_INVALID_ADDRESS;
    }
    *status = m_out.flipStatus;
    return 0;
}

int32_t VideoOutHle::GetVblankStatus(int32_t handle, uint64_t statusVa) {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (!m_out.opened || handle != m_nextHandle - 1) {
        return VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    auto* status = static_cast<VideoOutVblankStatus*>(
        ResolveWindowPtr(statusVa, sizeof(VideoOutVblankStatus)));
    if (!status) {
        return VIDEO_OUT_ERROR_INVALID_ADDRESS;
    }
    *status = m_out.vblankStatus;
    return 0;
}

int32_t VideoOutHle::GetOutputStatus(int32_t handle, uint64_t statusVa) {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (!m_out.opened || handle != m_nextHandle - 1) {
        return VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    auto* status = static_cast<VideoOutOutputStatus*>(
        ResolveWindowPtr(statusVa, sizeof(VideoOutOutputStatus)));
    if (!status) {
        return VIDEO_OUT_ERROR_INVALID_ADDRESS;
    }
    *status = VideoOutOutputStatus{};
    status->resolution = (m_out.width >= 3840 || m_out.height >= 2160) ? 2u
                                                                       : 1u;
    status->dynamicRange = 1;
    status->refreshRate =
        (m_out.outputMode == VIDEO_OUT_OUTPUT_MODE_119_88HZ)
            ? VIDEO_OUT_REFRESH_RATE_119_88HZ
            : VIDEO_OUT_REFRESH_RATE_59_94HZ;
    return 0;
}

int32_t VideoOutHle::GetResolutionStatus(int32_t handle, uint64_t statusVa) {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (!m_out.opened || handle != m_nextHandle - 1) {
        return VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    auto* status = static_cast<VideoOutResolutionStatus*>(
        ResolveWindowPtr(statusVa, sizeof(VideoOutResolutionStatus)));
    if (!status) {
        return VIDEO_OUT_ERROR_INVALID_ADDRESS;
    }
    *status = VideoOutResolutionStatus{};
    status->fullWidth = m_out.width;
    status->fullHeight = m_out.height;
    status->paneWidth = m_out.width;
    status->paneHeight = m_out.height;
    status->refreshRate =
        (m_out.outputMode == VIDEO_OUT_OUTPUT_MODE_119_88HZ)
            ? VIDEO_OUT_REFRESH_RATE_119_88HZ
            : VIDEO_OUT_REFRESH_RATE_59_94HZ;
    return 0;
}

int32_t VideoOutHle::SetFlipRate(int32_t handle, int32_t rate) {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (!m_out.opened || handle != m_nextHandle - 1) {
        return VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    if (rate < 0 || rate > 2) {
        return VIDEO_OUT_ERROR_INVALID_VALUE;
    }
    m_out.flipRate = rate;
    return 0;
}

int32_t VideoOutHle::IsFlipPending(int32_t handle) {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (!m_out.opened || handle != m_nextHandle - 1) {
        return VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    return m_out.flipStatus.flipPendingNum;
}

int32_t VideoOutHle::WaitVblank(int32_t handle) {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (!m_out.opened || handle != m_nextHandle - 1) {
        return VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    // No vblank engine: an honest immediate return (logged once, on the
    // first flip the guest already saw the marker). A blocking wait here
    // would stall the guest on an event PX5 does not produce yet.
    return 0;
}

int32_t VideoOutHle::AllowOutputResolutionWqhdDetection(int32_t handle) {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (!m_out.opened || handle != m_nextHandle - 1) {
        return VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    m_out.wqhdAllowed = true;
    return 0;
}

int32_t VideoOutHle::IsOutputSupported(int32_t handle, uint64_t mode,
                                       uint64_t optionsVa) {
    (void)optionsVa;   // ORBIS options are opaque here; the mode decides
    std::lock_guard<std::mutex> lk(m_mutex);
    if (!m_out.opened || handle != m_nextHandle - 1) {
        return VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    if (mode == VIDEO_OUT_OUTPUT_MODE_DEFAULT) return 1;
    if (mode == VIDEO_OUT_OUTPUT_MODE_119_88HZ) return 0;
    return VIDEO_OUT_ERROR_UNKNOWN_OUTPUT_MODE;
}

int32_t VideoOutHle::ConfigureOutput(int32_t handle, uint64_t mode,
                                     uint64_t optionsVa) {
    (void)optionsVa;   // ORBIS options are opaque here; the mode decides
    std::lock_guard<std::mutex> lk(m_mutex);
    if (!m_out.opened || handle != m_nextHandle - 1) {
        return VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    if (mode == VIDEO_OUT_OUTPUT_MODE_DEFAULT) {
        m_out.outputMode = mode;
        return 0;
    }
    if (mode == VIDEO_OUT_OUTPUT_MODE_119_88HZ) {
        return VIDEO_OUT_ERROR_UNAVAILABLE_OUTPUT_MODE;
    }
    return VIDEO_OUT_ERROR_UNKNOWN_OUTPUT_MODE;
}

} // namespace PX5::SceVideoOutHle
