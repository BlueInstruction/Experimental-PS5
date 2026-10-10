// SPDX-License-Identifier: GPL-2.0-only
// PX5 host gate — sceAudioOut HLE locked behaviour (port table, format
// decode, volume/Std-mix prepare math, pacing, error codes).
#include "audio/audio_out_hle.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace PX5::SceAudioOutHle;

namespace {

int g_failures = 0;

void Check(bool ok, const std::string& what) {
    if (ok) {
        std::printf("  ok  %s\n", what.c_str());
        return;
    }
    ++g_failures;
    std::printf("FAIL  %s\n", what.c_str());
}

// Manual clock: NowUs is scriptable; WaitUs advances the virtual time.
class FakeClock final : public Clock {
public:
    uint64_t now = 1'000'000;
    uint64_t waits = 0;
    uint64_t waitedUs = 0;
    uint64_t NowUs() override { return now; }
    void WaitUs(uint64_t us) override {
        ++waits;
        waitedUs += us;
        now += us;
    }
};

// Scripted sink: records everything the HLE queues.
class MockSink final : public AudioSink {
public:
    bool openResult = true;
    uint32_t openFreq = 0, openChannels = 0;
    Format openFormat = Format::Unknown;
    std::vector<uint8_t> queued;
    size_t queuedBytesValue = 0;   // QueuedBytes returns this (drivable)
    size_t clears = 0, closes = 0;

    bool Open(uint32_t freq, uint32_t channels, Format format) override {
        openFreq = freq;
        openChannels = channels;
        openFormat = format;
        return openResult;
    }
    void Queue(const void* data, uint32_t bytes) override {
        const auto* p = static_cast<const uint8_t*>(data);
        queued.insert(queued.end(), p, p + bytes);
    }
    size_t QueuedBytes() override { return queuedBytesValue; }
    void Clear() override { ++clears; queued.clear(); }
    void Close() override { ++closes; }
};

}  // namespace

int main() {
    // ---- 1. format table -----------------------------------------------------
    Check(ChannelsForFormat(Format::S16Mono) == 1 &&
              ChannelsForFormat(Format::F32Mono) == 1,
          "mono formats have 1 channel");
    Check(ChannelsForFormat(Format::S16Stereo) == 2 &&
              ChannelsForFormat(Format::F32Stereo) == 2,
          "stereo formats have 2 channels");
    Check(ChannelsForFormat(Format::S16_8Ch) == 8 &&
              ChannelsForFormat(Format::F32_8Ch) == 8 &&
              ChannelsForFormat(Format::S16_8ChStd) == 8 &&
              ChannelsForFormat(Format::F32_8ChStd) == 8,
          "8ch formats have 8 channels");
    Check(ChannelsForFormat(Format::Unknown) == 0, "unknown has 0 channels");
    Check(FormatIsFloat(Format::F32Mono) && FormatIsFloat(Format::F32Stereo) &&
              FormatIsFloat(Format::F32_8Ch) &&
              FormatIsFloat(Format::F32_8ChStd) &&
              !FormatIsFloat(Format::S16Stereo),
          "float format predicate");
    Check(FormatIsStd(Format::S16_8ChStd) && FormatIsStd(Format::F32_8ChStd) &&
              !FormatIsStd(Format::F32_8Ch),
          "std format predicate");
    Check(static_cast<uint32_t>(Format::S16Mono) == 0 &&
              static_cast<uint32_t>(Format::S16Stereo) == 1 &&
              static_cast<uint32_t>(Format::S16_8Ch) == 2 &&
              static_cast<uint32_t>(Format::F32Mono) == 3 &&
              static_cast<uint32_t>(Format::F32Stereo) == 4 &&
              static_cast<uint32_t>(Format::F32_8Ch) == 5 &&
              static_cast<uint32_t>(Format::S16_8ChStd) == 6 &&
              static_cast<uint32_t>(Format::F32_8ChStd) == 7,
          "format enum values ARE the param selectors");
    {
        Format f = Format::Unknown;
        Check(FormatFromParam(4, f) && f == Format::F32Stereo,
              "param 4 decodes to F32Stereo");
        Check(!FormatFromParam(0xAB, f) && !FormatFromParam(8, f),
              "unknown selectors refused");
    }

    // ---- 2. open validation + port table --------------------------------------
    auto& hle = AudioOutHle::Instance();
    auto& clock = *new FakeClock();
    hle.SetClock(&clock);
    int factoryCalls = 0;
    std::vector<MockSink*> made;
    hle.SetSinkFactory([&](uint32_t freq, uint32_t channels, Format format)
                           -> std::unique_ptr<AudioSink> {
        ++factoryCalls;
        auto s = std::make_unique<MockSink>();
        made.push_back(s.get());
        return s;
    });

    Check(hle.Open(0, 5, 0, 256, 48000, 1) == kErrInvalidPort,
          "port type 5 refused (verbatim error)");
    Check(hle.Open(0, kPortTypeMain, 0, 0, 48000, 1) == kErrInvalidLen,
          "len 0 refused");
    Check(hle.Open(0, kPortTypeMain, 0, 256, 0, 1) == kErrInvalidFreq,
          "freq 0 refused");
    Check(hle.Open(0, kPortTypeMain, 1, 256, 48000, 1) == kErrInvalidPort,
          "index != 0 refused");
    Check(hle.Open(0, kPortTypeMain, 0, 256, 48000, 0x8F) == kErrInvalidParam,
          "unknown format selector refused");
    const int32_t h1 = hle.Open(0, kPortTypeMain, 0, 256, 48000, 1);
    Check(h1 == 1, "first port gets handle 1");
    Check(factoryCalls == 1, "factory called exactly once for a real port");
    Check(made[0]->openFreq == 48000 && made[0]->openChannels == 2 &&
              made[0]->openFormat == Format::S16Stereo,
          "sink opened with the port's rate/channels/format");
    const int32_t h2 = hle.Open(0, kPortTypeVibration, 0, 256, 48000, 0);
    Check(h2 == 2 && factoryCalls == 1,
          "vibration port opens sinkless (factory not called)");
    {
        AudioOutPortState st{};
        Check(hle.GetPortState(h2, &st) == 0 && st.output == 4 &&
                  st.channel == 1,
              "vibration port state (output 4, channel 1)");
    }

    // ---- 3. sinkless pacing ------------------------------------------------------
    {
        // h2 is the SINKLESS vibration port: 256 samples at 48 kHz = 5333 us
        // per Output. Ten data outputs push the virtual queue to 53.3 ms —
        // the next one crosses the 40 ms latency target and must pace.
        int16_t samples[256] = {};
        for (int i = 0; i < 10; ++i) hle.Output(h2, samples);
        const uint64_t waitsBefore = clock.waits;
        const int32_t rc = hle.Output(h2, samples);
        Check(rc == 256, "Output returns the port's samplesNum");
        Check(clock.waits > waitsBefore,
              "sinkless pacing engages past the latency target");
        Check(clock.waitedUs > 0, "pacing actually waited");
        // Null data drains the queue (a wait) without extending it.
        const uint64_t w = clock.waits;
        hle.Output(h2, nullptr);
        Check(clock.waits > w, "null-data Output drains the virtual queue");
        const AudioOutStats st = hle.Stats();
        Check(st.pacedWaits > 0 && st.outputs == 11,
              "11 data outputs counted; pacing recorded");
    }

    // ---- 4. real-sink path + volume math ------------------------------------------
    {
        int16_t samples[512];
        for (int i = 0; i < 512; ++i) samples[i] = static_cast<int16_t>(i % 7);
        // Default volume: passthrough, exactly len*ch*bps bytes.
        MockSink& sink = *made[0];
        const int32_t rc = hle.Output(h1, samples);
        Check(rc == 256, "Output (data) returns samplesNum");
        Check(sink.queued.size() == 256 * 2 * sizeof(int16_t),
              "sink received len*ch*bps bytes");
        Check(std::memcmp(sink.queued.data(), samples, sink.queued.size()) == 0,
              "default volume passthrough is byte-identical");

        // Half volume on both channels: samples scaled by 0.5, clamped s16.
        int32_t vol[8] = {16384, 16384};
        Check(hle.SetVolume(h1, 0x3u, vol) == 0, "SetVolume ok");
        sink.queued.clear();
        hle.Output(h1, samples);
        const auto* out = reinterpret_cast<const int16_t*>(sink.queued.data());
        bool scaled = true;
        for (int i = 0; i < 512; ++i) {
            const int16_t expected = static_cast<int16_t>(
                (static_cast<int64_t>(samples[i]) * 16384 * 32768) /
                (32768 * 32768));
            if (out[i] != expected) { scaled = false; break; }
        }
        Check(scaled, "half-volume s16 scaling exact");
        vol[0] = 0; vol[1] = 0;
        hle.SetVolume(h1, 0x3u, vol);
    }

    // ---- 5. Std channel map (F32_8ChStd, param 7) -----------------------------------
    {
        const int32_t h = hle.Open(0, kPortTypeMain, 0, 4, 48000, 7);
        Check(h == 3, "third port gets handle 3");
        float samples[4 * 8];
        for (int i = 0; i < 32; ++i) samples[i] = static_cast<float>(i);
        MockSink& sink = *made[1];   // factory call #2 = this port
        hle.Output(h, samples);
        Check(sink.queued.size() == 4 * 8 * sizeof(float),
              "8ch std queued size");
        const auto* out = reinterpret_cast<const float*>(sink.queued.data());
        static const uint32_t map[8] = {0, 1, 2, 3, 6, 7, 4, 5};
        bool mapped = true;
        for (int fr = 0; fr < 4 && mapped; ++fr) {
            for (int c = 0; c < 8; ++c) {
                if (out[fr * 8 + c] != samples[fr * 8 + static_cast<int>(map[c])])
                    { mapped = false; break; }
            }
        }
        Check(mapped, "Std 8ch channel map applied (0,1,2,3,6,7,4,5)");
        hle.Close(h);
    }

    // ---- 6. drain-wait path with a backed-up sink ------------------------------------
    {
        MockSink& sink = *made[0];
        sink.queuedBytesValue = 1u << 20;   // pretend the device is backed up
        const uint64_t before = clock.now;
        int16_t samples[512] = {};
        hle.Output(h1, samples);
        // The wait is bounded by the 200 ms drain timeout, then the sink is
        // cleared and the data queued anyway.
        Check(clock.now - before >= 200000,
              "backed-up sink waits the drain timeout");
        Check(sink.clears == 1, "drain timeout cleared the sink");
        Check(sink.queued.size() == 256 * 2 * sizeof(int16_t),
              "data queued after the clear");
        sink.queuedBytesValue = 0;
    }

    // ---- 7. port state table + padspk mix level ---------------------------------------
    {
        AudioOutPortState st{};
        Check(hle.GetPortState(h1, &st) == 0 && st.output == 1 &&
                  st.channel == 2,
              "main port state (output 1, channel 2)");
        const int32_t hv = hle.Open(0, kPortTypeVoice, 0, 256, 48000, 0);
        Check(hle.GetPortState(hv, &st) == 0 && st.output == 0x40 &&
                  st.channel == 1,
              "voice port state (output 0x40, channel 1)");
        const int32_t hp = hle.Open(0, kPortTypePadspk, 0, 256, 48000, 0);
        Check(hle.GetPortState(hp, &st) == 0 && st.output == 4 &&
                  st.channel == 1,
              "padspk port state (output 4, channel 1)");
        const int32_t ha = hle.Open(0, kPortTypeAux, 0, 256, 48000, 0);
        Check(hle.GetPortState(ha, &st) == 0 && st.output == 0x80 &&
                  st.channel == 0,
              "aux port state (output 0x80, channel 0)");
        Check(hle.SetMixLevelPadSpk(h1, 1000) == kErrInvalidPort,
              "mix level on a non-padspk port refused");
        Check(hle.SetMixLevelPadSpk(hp, kDefaultVolume + 1) == kErrMixLevel,
              "mix level above full scale refused");
        Check(hle.SetMixLevelPadSpk(hp, 1000) == 0, "padspk mix level set");
        AudioOutPortState bogus{};
        Check(hle.GetPortState(999, &bogus) == kErrInvalidHandle,
              "invalid handle refused");
        Check(hle.GetPortState(hv, nullptr) == kErrInvalidParam,
              "null state refused");
    }

    // ---- 8. Outputs (multi-port) + close semantics ---------------------------------------
    {
        int16_t samples[512] = {};
        AudioOutHle::OutputParam params[2] = {
            {h1, samples}, {h2, samples}};
        const int32_t rc = hle.Outputs(params, 2);
        Check(rc == 256, "Outputs returns the FIRST port's samplesNum");
        AudioOutHle::OutputParam bad[1] = {{999, samples}};
        Check(hle.Outputs(bad, 1) == kErrInvalidHandle,
              "Outputs refuses an unknown handle");
        Check(hle.Outputs(nullptr, 1) == kErrInvalidParam &&
                  hle.Outputs(params, 0) == kErrInvalidParam,
              "Outputs refuses null/zero params");
        Check(hle.Close(h1) == 0, "close ok");
        Check(hle.Output(h1, samples) == kErrInvalidHandle,
              "closed handle refused");
        {
            // First-fit reuse: the table must eventually exhaust exactly.
            int opened = 0;
            while (hle.Open(0, kPortTypeAux, 0, 256, 48000, 0) > 0) ++opened;
            Check(opened == 28,
                  "port table exhausts after the remaining 28 slots");
            Check(hle.Open(0, kPortTypeAux, 0, 256, 48000, 0) == kErrNoFreePorts,
                  "exhausted table refused (verbatim error)");
        }
    }

    if (g_failures == 0) {
        std::printf("audio_out_hle_test: ALL PASS\n");
        return 0;
    }
    std::printf("audio_out_hle_test: %d FAILURES\n", g_failures);
    return 1;
}
