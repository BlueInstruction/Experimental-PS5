#include "media_codec_hle.h"
#include "../utils/logger.h"
#include <cstring>

namespace PX5 {

MediaCodecHle& MediaCodecHle::GetInstance() {
    static MediaCodecHle instance;
    return instance;
}

bool MediaCodecHle::InitializeDecoder(const MediaDecoderConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
    m_initialized = true;

    PX5_LOGI(LogCategory::CORE, "PS5 MediaCodec HLE initialized: Resolution %ux%u @ %u FPS (HW Acceleration: %s)",
             config.width, config.height, config.frameRate, config.hardwareAccelerated ? "ENABLED" : "DISABLED");
    return true;
}

bool MediaCodecHle::DecodeFrame(const uint8_t* compressedData, size_t dataSize, uint8_t* outYuvBuffer, size_t outSize) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized || !compressedData || !outYuvBuffer) return false;

    // HLE Software/Hardware frame pass-through fallback
    size_t copyLen = std::min(dataSize, outSize);
    std::memcpy(outYuvBuffer, compressedData, copyLen);
    return true;
}

void MediaCodecHle::Flush() {
    std::lock_guard<std::mutex> lock(m_mutex);
    PX5_LOGI(LogCategory::CORE, "PS5 MediaCodec HLE flushed pipeline");
}

void MediaCodecHle::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_initialized = false;
    PX5_LOGI(LogCategory::CORE, "PS5 MediaCodec HLE shut down successfully");
}

} // namespace PX5
