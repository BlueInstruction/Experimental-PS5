// SPDX-License-Identifier: GPL-2.0-only
// PX5 — real audio output: an AAudio-backed AudioSink factory for the
// sceAudioOut HLE.
//
// WHAT THIS IS
//   The Android half of the audio chain the old stub described and never
//   built: AAudio_createStreamBuilder -> setDataCallback ->
//   requestStart, with a bounded guest-side ring buffer per port. The
//   sceAudioOut HLE (audio_out_hle.cpp) paces the guest against the ring's
//   fill level; this TU owns the device that actually drains it.
//
//   Every port the guest opens gets its own stream at the port's rate /
//   channel count / format (PCM_I16 for the S16 formats, FLOAT for the F32
//   formats). A stream that fails to open leaves the port sinkless — the
//   HLE's virtual-queue clock paces the guest instead (the AnyPS5 model),
//   and the failure is counted here, never hidden.
#include "audio.h"

#include <aaudio/AAudio.h>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

#include "../audio/audio_out_hle.h"
#include "../utils/logger.h"

namespace PX5 {
namespace {

// Ring-buffer depth in buffer-frames (queue-fill headroom the HLE's pacing
// target of 40 ms stays well inside).
constexpr size_t kRingBufferDepths = 8;
// Queue() blocks at most this long for ring space before dropping a block
// (a wedged callback must not wedge the guest's mixer thread forever).
constexpr int64_t kQueueTimeoutMs = 2000;

std::atomic<float> g_masterVolume{1.0f};
std::atomic<uint64_t> g_streamOpenFailures{0};
std::atomic<uint64_t> g_queueStalls{0};

// One BoundedRing per port, format-typed at open time.
class BoundedRing {
public:
    void Reset(size_t bytes) {
        std::lock_guard<std::mutex> lock(m_);
        buf_.assign(bytes, 0);
        read_ = write_ = fill_ = 0;
    }

    size_t Fill() const {
        std::lock_guard<std::mutex> lock(m_);
        return fill_;
    }

    // Blocking push with a timeout; returns false on timeout (caller drops).
    bool Push(const void* data, size_t bytes) {
        std::unique_lock<std::mutex> lock(m_);
        if (!cv_.wait_for(lock, std::chrono::milliseconds(kQueueTimeoutMs),
                          [&] { return bytes <= buf_.size() - fill_; })) {
            return false;
        }
        const auto* src = static_cast<const uint8_t*>(data);
        const size_t cap = buf_.size();
        for (size_t i = 0; i < bytes; ++i) {
            buf_[write_] = src[i];
            write_ = (write_ + 1) % cap;
        }
        fill_ += bytes;
        return true;
    }

    // Callback-side pull; never blocks (the callback owns real-time rules).
    size_t Pull(void* dst, size_t bytes) {
        std::lock_guard<std::mutex> lock(m_);
        const size_t n = std::min(bytes, fill_);
        const size_t cap = buf_.size();
        auto* out = static_cast<uint8_t*>(dst);
        for (size_t i = 0; i < n; ++i) {
            out[i] = buf_[read_];
            read_ = (read_ + 1) % cap;
        }
        if (n < bytes) std::memset(out + n, 0, bytes - n);  // underrun: silence
        fill_ -= n;
        return n;
    }

    std::mutex m_;
    std::condition_variable cv_;
    std::vector<uint8_t> buf_;
    size_t read_ = 0, write_ = 0, fill_ = 0;
};

// The AAudio sink: one stream + one ring per opened port.
class AAudioSink final : public SceAudioOutHle::AudioSink {
public:
    AAudioSink() = default;
    ~AAudioSink() override { Close(); }

    bool Open(uint32_t freq, uint32_t channels,
              SceAudioOutHle::Format format) override {
        isFloat_ = SceAudioOutHle::FormatIsFloat(format);
        frameBytes_ = static_cast<size_t>(channels) * (isFloat_
                                                           ? sizeof(float)
                                                           : sizeof(int16_t));
        // 4096 frames of headroom: comfortably above the HLE's drain target
        // (minQueued <= 16 * preparedSize) at any guest buffer length.
        ring_ = std::make_unique<BoundedRing>();
        ring_->Reset(frameBytes_ * 4096u);

        AAudioStreamBuilder* builder = nullptr;
        if (AAudio_createStreamBuilder(&builder) != AAUDIO_OK || !builder) {
            ++g_streamOpenFailures;
            return false;
        }
        AAudioStreamBuilder_setSampleRate(builder,
                                          static_cast<int32_t>(freq));
        AAudioStreamBuilder_setChannelCount(
            builder, static_cast<int32_t>(channels));
        AAudioStreamBuilder_setFormat(
            builder, isFloat_ ? AAUDIO_FORMAT_PCM_FLOAT
                              : AAUDIO_FORMAT_PCM_I16);
        AAudioStreamBuilder_setPerformanceMode(
            builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
        AAudioStreamBuilder_setDataCallback(
            builder,
            [](AAudioStream* /*stream*/, void* userData, void* audioData,
               int32_t numFrames) -> aaudio_data_callback_result_t {
                auto* sink = static_cast<AAudioSink*>(userData);
                const size_t bytes = static_cast<size_t>(numFrames) *
                                     sink->frameBytes_;
                sink->ring_->Pull(audioData, bytes);
                ApplyMasterVolume(sink, audioData, bytes);
                return AAUDIO_CALLBACK_RESULT_CONTINUE;
            },
            this);
        const aaudio_result_t r =
            AAudioStreamBuilder_openStream(builder, &stream_);
        AAudioStreamBuilder_delete(builder);
        if (r != AAUDIO_OK || !stream_) {
            ++g_streamOpenFailures;
            stream_ = nullptr;
            return false;
        }
        if (AAudioStream_requestStart(stream_) != AAUDIO_OK) {
            ++g_streamOpenFailures;
            AAudioStream_close(stream_);
            stream_ = nullptr;
            return false;
        }
        return true;
    }

    void Queue(const void* data, uint32_t bytes) override {
        if (!ring_ || bytes == 0) return;
        if (!ring_->Push(data, bytes)) {
            // The callback stopped draining (device vanished). Drop the
            // block, count the stall — the guest keeps running.
            ++g_queueStalls;
            PX5_LOGW_ONCE(CORE,
                          "audio: ring full for %d ms — block dropped "
                          "(stream may have died)",
                          kQueueTimeoutMs);
        }
    }

    size_t QueuedBytes() override { return ring_ ? ring_->Fill() : 0; }

    void Clear() override {
        if (!ring_) return;
        std::lock_guard<std::mutex> lock(ring_->m_);
        ring_->fill_ = 0;
        ring_->read_ = ring_->write_ = 0;
    }

    void Close() override {
        if (stream_) {
            AAudioStream_requestStop(stream_);
            AAudioStream_close(stream_);
            stream_ = nullptr;
        }
        ring_.reset();
    }

    std::unique_ptr<BoundedRing> ring_;
    AAudioStream* stream_ = nullptr;
    size_t frameBytes_ = 0;
    bool isFloat_ = false;

private:
    static void ApplyMasterVolume(AAudioSink* sink, void* audioData,
                                  size_t bytes) {
        const float vol = g_masterVolume.load(std::memory_order_relaxed);
        if (vol >= 0.999f && vol <= 1.001f) return;
        if (sink->isFloat_) {
            auto* s = static_cast<float*>(audioData);
            const size_t n = bytes / sizeof(float);
            for (size_t i = 0; i < n; ++i) s[i] *= vol;
        } else {
            auto* s = static_cast<int16_t*>(audioData);
            const size_t n = bytes / sizeof(int16_t);
            for (size_t i = 0; i < n; ++i) {
                int32_t v = static_cast<int32_t>(s[i] * vol);
                if (v > 32767) v = 32767;
                if (v < -32768) v = -32768;
                s[i] = static_cast<int16_t>(v);
            }
        }
    }
};

}  // namespace

AudioEngine& AudioEngine::GetInstance() {
    static AudioEngine instance;
    return instance;
}

// The REAL wiring this time: probe the AAudio builder path, then hand the
// HLE a factory that opens one low-latency stream per guest port. A failed
// probe is reported (false) — the HLE then runs sinkless ports on the
// virtual clock, which is honest, audible silence instead of a fake success.
bool AudioEngine::Initialize() {
    if (m_initialized) return true;

    // Probe: a builder must be creatable and failable on AAudio's terms.
    AAudioStreamBuilder* probe = nullptr;
    const aaudio_result_t r = AAudio_createStreamBuilder(&probe);
    if (r != AAUDIO_OK || !probe) {
        PX5_LOGW(LogCategory::CORE,
                 "audio: AAudio builder probe failed result=%d — no device "
                 "audio; HLE ports will run sinkless (virtual clock)",
                 static_cast<int>(r));
        return false;
    }
    AAudioStreamBuilder_delete(probe);

    SceAudioOutHle::AudioOutHle::Instance().SetSinkFactory(
        [](uint32_t freq, uint32_t channels,
           SceAudioOutHle::Format format)
            -> std::unique_ptr<SceAudioOutHle::AudioSink> {
            auto sink = std::make_unique<AAudioSink>();
            if (!sink->Open(freq, channels, format)) {
                // Sinkless port: the HLE paces on the virtual clock.
                return nullptr;
            }
            return sink;
        });

    m_initialized = true;
    PX5_LOGI(LogCategory::CORE,
             "audio: AAudio backend ready — per-port low-latency streams "
             "with bounded rings behind the sceAudioOut HLE");
    return true;
}

void AudioEngine::Shutdown() {
    if (m_initialized) {
        SceAudioOutHle::AudioOutHle::Instance().CloseAll();
    }
    m_initialized = false;
}

void AudioEngine::SetVolume(float volume) {
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    g_masterVolume.store(volume, std::memory_order_relaxed);
}

bool AudioEngine::IsInitialized() const { return m_initialized; }

std::string AudioEngine::GetStatusString() const {
    if (!m_initialized) {
        return "audio: AAudio backend not initialized (HLE ports run "
               "sinkless on the virtual clock)";
    }
    const uint64_t fails = g_streamOpenFailures.load();
    const uint64_t stalls = g_queueStalls.load();
    return "audio: AAudio ready | " +
           SceAudioOutHle::AudioOutHle::Instance().GetSummaryString() +
           " | streamOpenFailures=" + std::to_string(fails) +
           " queueStalls=" + std::to_string(stalls);
}

}  // namespace PX5
