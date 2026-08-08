#ifndef PX5_MEDIA_CODEC_HLE_H
#define PX5_MEDIA_CODEC_HLE_H

#include <cstdint>
#include <string>
#include <vector>
#include <mutex>

namespace PX5 {

enum class VideoCodecType {
    AVC_H264,
    HEVC_H265,
    VP9,
    AV1
};

struct MediaDecoderConfig {
    VideoCodecType codec = VideoCodecType::HEVC_H265;
    uint32_t width = 3840;
    uint32_t height = 2160;
    uint32_t frameRate = 60;
    bool hardwareAccelerated = true;
};

class MediaCodecHle {
public:
    static MediaCodecHle& GetInstance();

    bool InitializeDecoder(const MediaDecoderConfig& config);
    bool DecodeFrame(const uint8_t* compressedData, size_t dataSize, uint8_t* outYuvBuffer, size_t outSize);
    void Flush();
    void Shutdown();

private:
    MediaCodecHle() = default;
    ~MediaCodecHle() = default;

    std::mutex m_mutex;
    bool m_initialized = false;
    MediaDecoderConfig m_config{};
};

} // namespace PX5

#endif // PX5_MEDIA_CODEC_HLE_H
