// SPDX-License-Identifier: GPL-2.0-only
// PX5 — ORBIS sceAudioOut HLE, implementation. See audio_out_hle.h.
// Ported from AnyPS5 libSceAudioOut/src/AudioOut.cpp (GPL-2.0-only): the
// port table, format decode, volume/Std-channel-mix prepare path, the
// latency-target pacing for real sinks, and the virtual-queue clock for
// sinkless ports — every refusal carries the verbatim ORBIS error code.
#include "audio_out_hle.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <limits>
#include <mutex>
#include <thread>
#include <vector>

#include "../utils/logger.h"

namespace PX5::SceAudioOutHle {
namespace {

// Pacing constants (verbatim from the reference).
constexpr uint64_t kTargetLatencyUs = 40000;
constexpr uint64_t kDrainTimeoutUs = 200000;
constexpr uint64_t kDrainSleepUs = 1000;
// The 8ch *Std channel map (verbatim: PS5 std order -> the port's L/R-first
// interleaved order).
constexpr uint32_t kStd8chMap[8] = {0, 1, 2, 3, 6, 7, 4, 5};

class RealClock final : public Clock {
public:
    uint64_t NowUs() override {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count());
    }
    void WaitUs(uint64_t us) override {
        std::this_thread::sleep_for(std::chrono::microseconds(us));
    }
};

struct Port {
    bool used = false;
    int type = 0;
    uint32_t samplesNum = 0;
    uint32_t freq = 0;
    Format format = Format::Unknown;
    int channels = 0;
    std::array<int32_t, 8> volume{};
    int32_t mixLevel = kDefaultVolume;
    uint64_t lastDataOutputTime = 0;
    uint64_t virtualQueueEnd = 0;
    std::unique_ptr<AudioSink> sink;   // null => virtual pacing
};

uint32_t BytesPerSample(Format f) {
    return FormatIsFloat(f) ? sizeof(float) : sizeof(int16_t);
}

// Verbatim prepare path: per-channel volume + mixlevel, Std channel map,
// clamped for S16. Returns `data` untouched when nothing applies.
const void* PrepareBuffer(const Port& port, const void* data,
                          std::vector<uint8_t>& buf) {
    const uint32_t frames = port.samplesNum;
    const uint32_t ch = static_cast<uint32_t>(port.channels);
    const uint32_t bps = BytesPerSample(port.format);
    const uint32_t size = frames * ch * bps;

    bool volumeChanged = port.mixLevel != kDefaultVolume;
    for (uint32_t i = 0; i < ch; i++) {
        if (port.volume[i] != kDefaultVolume) {
            volumeChanged = true;
            break;
        }
    }
    if (!volumeChanged && !FormatIsStd(port.format)) return data;

    buf.resize(size);
    const bool isStd = FormatIsStd(port.format) && ch == 8;
    if (FormatIsFloat(port.format)) {
        auto* dst = reinterpret_cast<float*>(buf.data());
        const auto* src = static_cast<const float*>(data);
        const float mix = static_cast<float>(port.mixLevel) /
                          static_cast<float>(kDefaultVolume);
        for (uint32_t fr = 0; fr < frames; fr++) {
            for (uint32_t c = 0; c < ch; c++) {
                const uint32_t srcCh = isStd ? kStd8chMap[c] : c;
                dst[fr * ch + c] =
                    src[fr * ch + srcCh] *
                    (static_cast<float>(port.volume[c]) /
                     static_cast<float>(kDefaultVolume)) * mix;
            }
        }
    } else {
        auto* dst = reinterpret_cast<int16_t*>(buf.data());
        const auto* src = static_cast<const int16_t*>(data);
        for (uint32_t fr = 0; fr < frames; fr++) {
            for (uint32_t c = 0; c < ch; c++) {
                const uint32_t srcCh = isStd ? kStd8chMap[c] : c;
                int64_t s = static_cast<int64_t>(src[fr * ch + srcCh]) *
                            port.volume[c] * port.mixLevel /
                            (static_cast<int64_t>(kDefaultVolume) *
                             kDefaultVolume);
                s = std::clamp<int64_t>(
                    s, std::numeric_limits<int16_t>::min(),
                    std::numeric_limits<int16_t>::max());
                dst[fr * ch + c] = static_cast<int16_t>(s);
            }
        }
    }
    return buf.data();
}

}  // namespace

uint32_t ChannelsForFormat(Format f) {
    switch (f) {
    case Format::S16Mono:
    case Format::F32Mono:      return 1;
    case Format::S16Stereo:
    case Format::F32Stereo:    return 2;
    case Format::S16_8Ch:
    case Format::F32_8Ch:
    case Format::S16_8ChStd:
    case Format::F32_8ChStd:   return 8;
    default:                   return 0;
    }
}

bool FormatIsFloat(Format f) {
    return f == Format::F32Mono || f == Format::F32Stereo ||
           f == Format::F32_8Ch || f == Format::F32_8ChStd;
}

bool FormatIsStd(Format f) {
    return f == Format::S16_8ChStd || f == Format::F32_8ChStd;
}

bool FormatFromParam(uint32_t param, Format& out) {
    const uint32_t sel = param & kFormatMask;
    if (sel > static_cast<uint32_t>(Format::F32_8ChStd)) return false;
    out = static_cast<Format>(sel);
    return true;
}

std::string AudioOutStats::SummaryString() const {
    char b[160];
    snprintf(b, sizeof(b),
             "opens=%llu closes=%llu outputs=%llu queued=%llu waits=%llu "
             "failures=%llu",
             static_cast<unsigned long long>(opens),
             static_cast<unsigned long long>(closes),
             static_cast<unsigned long long>(outputs),
             static_cast<unsigned long long>(bytesQueued),
             static_cast<unsigned long long>(pacedWaits),
             static_cast<unsigned long long>(failures));
    return std::string(b);
}

struct AudioOutHle::Impl {
    std::mutex mutex;
    std::mutex outputMutex;   // serializes Output()/Outputs()/Close()
    std::array<Port, kPortsMax> ports{};
    bool initialized = false;
    AudioOutStats stats;
    std::function<std::unique_ptr<AudioSink>(uint32_t, uint32_t, Format)>
        sinkFactory;
    Clock* clock = nullptr;   // null => the real clock
    RealClock realClock;

    Clock& Clk() {
        if (clock != nullptr) return *clock;
        return realClock;
    }

    Port* GetPortLocked(int32_t handle) {
        const int idx = handle - 1;
        if (idx < 0 || idx >= kPortsMax || !ports[idx].used) return nullptr;
        return &ports[idx];
    }

    // Virtual-queue pacing (the sinkless port model, verbatim): the port
    // advances a clock of "consumed" microseconds; Output sleeps until the
    // queue would drain to the latency target, then extends it.
    void PaceVirtualLocked(Port& port, bool hasData) {
        Clock& clk = Clk();
        const uint64_t now = clk.NowUs();
        const int64_t queued =
            static_cast<int64_t>(port.virtualQueueEnd - now);
        if (queued <= 0) port.virtualQueueEnd = now;
        if (!hasData) {
            if (queued > 0) {
                ++stats.pacedWaits;
                clk.WaitUs(static_cast<uint64_t>(queued));
            }
            return;
        }
        if (queued > static_cast<int64_t>(kTargetLatencyUs)) {
            ++stats.pacedWaits;
            clk.WaitUs(static_cast<uint64_t>(queued) - kTargetLatencyUs);
        }
        port.virtualQueueEnd +=
            (1000000ULL * port.samplesNum) / (port.freq ? port.freq : 1u);
    }

    // Real-sink path (verbatim shape): wait until the queue drains to the
    // latency target (bounded), then queue the prepared buffer.
    void QueueLocked(Port& port, const void* data) {
        std::vector<uint8_t> prepared;
        const void* preparedData = PrepareBuffer(port, data, prepared);
        const uint32_t preparedSize =
            port.samplesNum * static_cast<uint32_t>(port.channels) *
            BytesPerSample(port.format);

        const uint64_t bufferUs =
            port.freq != 0 ? (1000000ULL * port.samplesNum) / port.freq : 0;
        const uint32_t buffers =
            bufferUs != 0
                ? static_cast<uint32_t>(
                      (kTargetLatencyUs + bufferUs - 1) / bufferUs)
                : 2u;
        const size_t minQueued =
            static_cast<size_t>(preparedSize) *
            std::clamp(buffers, 2u, 16u);
        Clock& clk = Clk();
        const uint64_t waitStart = clk.NowUs();
        while (port.sink->QueuedBytes() > minQueued) {
            if (clk.NowUs() - waitStart > kDrainTimeoutUs) {
                port.sink->Clear();
                break;
            }
            ++stats.pacedWaits;
            clk.WaitUs(kDrainSleepUs);
        }
        port.sink->Queue(preparedData, preparedSize);
        stats.bytesQueued += preparedSize;
    }
};

AudioOutHle& AudioOutHle::Instance() {
    static AudioOutHle instance;
    return instance;
}

AudioOutHle::AudioOutHle() : impl_(new Impl{}) {}
AudioOutHle::~AudioOutHle() { delete impl_; }

void AudioOutHle::SetSinkFactory(
    std::function<std::unique_ptr<AudioSink>(uint32_t, uint32_t, Format)>
        factory) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->sinkFactory = std::move(factory);
}

void AudioOutHle::SetClock(Clock* clock) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->clock = clock;
}

int32_t AudioOutHle::Init() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->initialized = true;
    return 0;   // verbatim: the reference's Init always succeeds
}

int32_t AudioOutHle::Open(int32_t userId, int32_t type, int32_t index,
                          uint32_t len, uint32_t freq, uint32_t param) {
    (void)userId;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    const bool typeValid =
        (type >= kPortTypeMain && type <= kPortTypePadspk) ||
        type == kPortTypeVibration || type == kPortTypeAudio3d ||
        type == kPortTypeAux;
    if (!typeValid) {
        ++impl_->stats.failures;
        return kErrInvalidPort;
    }
    if (len == 0) { ++impl_->stats.failures; return kErrInvalidLen; }
    if (freq == 0) { ++impl_->stats.failures; return kErrInvalidFreq; }
    if (index != 0) { ++impl_->stats.failures; return kErrInvalidPort; }

    Format format = Format::Unknown;
    if (!FormatFromParam(param, format)) {
        ++impl_->stats.failures;
        PX5_LOGW(LogCategory::CORE,
                 "AudioOutHle::Open: unknown format selector %u",
                 param & kFormatMask);
        return kErrInvalidParam;
    }

    for (int i = 0; i < kPortsMax; i++) {
        Port& port = impl_->ports[i];
        if (port.used) continue;
        port.used = true;
        port.type = type;
        port.samplesNum = len;
        port.freq = freq;
        port.format = format;
        port.channels = static_cast<int>(ChannelsForFormat(format));
        port.lastDataOutputTime = 0;
        port.virtualQueueEnd = impl_->Clk().NowUs();
        port.mixLevel = (type == kPortTypePadspk) ? kDefaultPadspkMixLevel
                                                  : kDefaultVolume;
        for (int c = 0; c < port.channels; c++) port.volume[c] = kDefaultVolume;
        if (type != kPortTypeVibration && impl_->sinkFactory) {
            port.sink = impl_->sinkFactory(freq,
                                           static_cast<uint32_t>(port.channels),
                                           format);
            if (port.sink && !port.sink->Open(freq,
                                              static_cast<uint32_t>(port.channels),
                                              format)) {
                // Verbatim model: a device that would not open leaves the
                // port sinkless — the virtual clock paces the guest.
                port.sink.reset();
            }
        }
        ++impl_->stats.opens;
        return i + 1;
    }
    ++impl_->stats.failures;
    return kErrNoFreePorts;
}

int32_t AudioOutHle::Close(int32_t handle) {
    std::lock_guard<std::mutex> outputLock(impl_->outputMutex);
    std::lock_guard<std::mutex> lock(impl_->mutex);
    Port* port = impl_->GetPortLocked(handle);
    if (port == nullptr) { ++impl_->stats.failures; return kErrInvalidHandle; }
    if (port->sink) port->sink->Close();
    *port = Port{};
    ++impl_->stats.closes;
    return 0;
}

int32_t AudioOutHle::Output(int32_t handle, const void* ptr) {
    std::lock_guard<std::mutex> outputLock(impl_->outputMutex);
    std::lock_guard<std::mutex> lock(impl_->mutex);
    Port* port = impl_->GetPortLocked(handle);
    if (port == nullptr) { ++impl_->stats.failures; return kErrInvalidHandle; }

    if (!port->sink) {
        impl_->PaceVirtualLocked(*port, ptr != nullptr);
    } else if (ptr != nullptr) {
        impl_->QueueLocked(*port, ptr);
    }
    if (ptr != nullptr) {
        port->lastDataOutputTime = impl_->Clk().NowUs();
        ++impl_->stats.outputs;
    }
    return static_cast<int32_t>(port->samplesNum);
}

int32_t AudioOutHle::Outputs(const OutputParam* params, uint32_t num) {
    if (params == nullptr || num == 0) {
        ++impl_->stats.failures;
        return kErrInvalidParam;
    }
    std::lock_guard<std::mutex> outputLock(impl_->outputMutex);
    std::lock_guard<std::mutex> lock(impl_->mutex);
    for (uint32_t i = 0; i < num; i++) {
        if (impl_->GetPortLocked(params[i].handle) == nullptr) {
            ++impl_->stats.failures;
            return kErrInvalidHandle;
        }
    }
    const int32_t firstSamples =
        static_cast<int32_t>(impl_->GetPortLocked(params[0].handle)->samplesNum);
    for (uint32_t i = 0; i < num; i++) {
        Port& port = *impl_->GetPortLocked(params[i].handle);
        if (!port.sink) {
            impl_->PaceVirtualLocked(port, params[i].ptr != nullptr);
        } else if (params[i].ptr != nullptr) {
            impl_->QueueLocked(port, params[i].ptr);
        }
    }
    const uint64_t done = impl_->Clk().NowUs();
    for (uint32_t i = 0; i < num; i++) {
        if (params[i].ptr != nullptr) {
            impl_->GetPortLocked(params[i].handle)->lastDataOutputTime = done;
            ++impl_->stats.outputs;
        }
    }
    return firstSamples;
}

int32_t AudioOutHle::SetVolume(int32_t handle, uint32_t flag,
                               const int32_t* vol) {
    if (vol == nullptr) { ++impl_->stats.failures; return kErrInvalidParam; }
    std::lock_guard<std::mutex> lock(impl_->mutex);
    Port* port = impl_->GetPortLocked(handle);
    if (port == nullptr) { ++impl_->stats.failures; return kErrInvalidHandle; }
    const bool isStd = FormatIsStd(port->format);
    for (int i = 0; i < port->channels; i++, flag >>= 1u) {
        if ((flag & 1u) == 0) continue;
        int srcIdx = i;
        if (isStd) {   // verbatim Std remap
            if (i == 4) srcIdx = 6;
            else if (i == 5) srcIdx = 7;
            else if (i == 6) srcIdx = 4;
            else if (i == 7) srcIdx = 5;
        }
        port->volume[i] = vol[srcIdx];
    }
    return 0;
}

int32_t AudioOutHle::GetLastOutputTime(int32_t handle,
                                       uint64_t* outputTime) {
    if (outputTime == nullptr) {
        ++impl_->stats.failures;
        return kErrInvalidParam;
    }
    std::lock_guard<std::mutex> lock(impl_->mutex);
    Port* port = impl_->GetPortLocked(handle);
    if (port == nullptr) { ++impl_->stats.failures; return kErrInvalidHandle; }
    *outputTime = port->lastDataOutputTime;
    return 0;
}

int32_t AudioOutHle::GetPortState(int32_t handle, AudioOutPortState* state) {
    if (state == nullptr) { ++impl_->stats.failures; return kErrInvalidParam; }
    std::lock_guard<std::mutex> lock(impl_->mutex);
    Port* port = impl_->GetPortLocked(handle);
    if (port == nullptr) { ++impl_->stats.failures; return kErrInvalidHandle; }
    *state = AudioOutPortState{};
    switch (port->type) {   // verbatim output/channel table
    case kPortTypeMain:
    case kPortTypeBgm:
    case kPortTypeAudio3d:
        state->output = 1;
        state->channel = static_cast<uint8_t>(port->channels > 2
                                                  ? 2 : port->channels);
        break;
    case kPortTypeVoice:
    case kPortTypePersonal:
        state->output = 0x40;
        state->channel = 1;
        break;
    case kPortTypePadspk:
    case kPortTypeVibration:
        state->output = 4;
        state->channel = 1;
        break;
    case kPortTypeAux:
    default:
        state->output = 0x80;
        state->channel = 0;
        break;
    }
    return 0;
}

int32_t AudioOutHle::SetMixLevelPadSpk(int32_t handle, int32_t mixLevel) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    Port* port = impl_->GetPortLocked(handle);
    if (port == nullptr) { ++impl_->stats.failures; return kErrInvalidHandle; }
    if (port->type != kPortTypePadspk) {
        ++impl_->stats.failures;
        return kErrInvalidPort;
    }
    if (mixLevel > kDefaultVolume) { ++impl_->stats.failures; return kErrMixLevel; }
    if (mixLevel < 0) {
        ++impl_->stats.failures;
        PX5_LOGW(LogCategory::CORE,
                 "AudioOutHle::SetMixLevelPadSpk: negative mix level refused");
        return kErrInvalidParam;
    }
    port->mixLevel = mixLevel;
    return 0;
}

AudioOutStats AudioOutHle::Stats() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->stats;
}

std::string AudioOutHle::GetSummaryString() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    int used = 0;
    int sinked = 0;
    for (const Port& p : impl_->ports) {
        if (!p.used) continue;
        ++used;
        if (p.sink) ++sinked;
    }
    char b[160];
    snprintf(b, sizeof(b),
             "sceAudioOut HLE: ports=%d sinked=%d | %s",
             used, sinked, impl_->stats.SummaryString().c_str());
    return std::string(b);
}

void AudioOutHle::CloseAll() {
    std::lock_guard<std::mutex> outputLock(impl_->outputMutex);
    std::lock_guard<std::mutex> lock(impl_->mutex);
    for (int i = 0; i < kPortsMax; i++) {
        Port& port = impl_->ports[i];
        if (!port.used) continue;
        if (port.sink) port.sink->Close();
        port = Port{};
        ++impl_->stats.closes;
    }
}

}  // namespace PX5::SceAudioOutHle
