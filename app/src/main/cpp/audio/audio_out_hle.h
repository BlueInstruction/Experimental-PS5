// SPDX-License-Identifier: GPL-2.0-only
// PX5 — ORBIS sceAudioOut HLE (app-owned service).
//
// WHAT THIS IS
//   The libSceAudioOut surface a guest title calls, ported from AnyPS5's
//   libSceAudioOut/src/AudioOut.cpp (GPL-2.0-only) onto PX5's layering: the
//   ORBIS port table, the 8-format table, the per-channel volume model, the
//   error codes, and BOTH pacing models — a real sink when one opened, and
//   the virtual-queue clock when it did not (exactly the AnyPS5 behaviour
//   when SDL could not open a device: the guest mixer thread is still paced
//   at the port's real sample rate so the game's timing survives).
//
//   The backend seam is AudioSink. On device, AudioEngine (audio.cpp) builds
//   AAudio-backed sinks; the host gate drives the HLE with a scripted mock
//   and a manual clock. The HLE itself never names a backend type.
//
// WHAT THIS IS NOT
//   No NID resolution yet (imports arrive with the import-thunk milestone);
//   no mixing across ports (each port queues to its own sink, matching the
//   reference). Failure policy: every refusal names its rule and carries the
//   verbatim ORBIS error code; nothing fabricates success.
#ifndef PX5_AUDIO_AUDIO_OUT_HLE_H
#define PX5_AUDIO_AUDIO_OUT_HLE_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace PX5::SceAudioOutHle {

// ORBIS port types (verbatim from the reference).
constexpr int kPortTypeMain      = 0;
constexpr int kPortTypeBgm       = 1;
constexpr int kPortTypeVoice     = 2;
constexpr int kPortTypePersonal  = 3;
constexpr int kPortTypePadspk    = 4;
constexpr int kPortTypeVibration = 10;
constexpr int kPortTypeAudio3d   = 126;
constexpr int kPortTypeAux       = 127;

constexpr int kPortsMax = 32;
constexpr int kDefaultVolume = 32768;          // 0..32768 full scale
constexpr int kDefaultPadspkMixLevel = 11626;
constexpr uint32_t kFormatMask = 0xFFu;

// Verbatim ORBIS error codes (the reference's exact negative values).
constexpr int kErrInvalidPort   = -2144993270;
constexpr int kErrInvalidFreq   = -2144993272;
constexpr int kErrInvalidLen    = -2144993274;
constexpr int kErrNoFreePorts   = -2144993275;
constexpr int kErrInvalidParam  = -2144993276;
constexpr int kErrInvalidHandle = -2144993277;
constexpr int kErrMixLevel      = -2144993260;

// The 8 ORBIS output formats. The enum values ARE the param selectors
// (param & 0xFF == value); "unknown selector" is Unknown, which no selector
// maps to. FormatFromParam is the checked decode.
enum class Format : uint32_t {
    S16Mono = 0,
    S16Stereo = 1,
    S16_8Ch = 2,
    F32Mono = 3,
    F32Stereo = 4,
    F32_8Ch = 5,
    S16_8ChStd = 6,
    F32_8ChStd = 7,
    Unknown = 0xFFFFFFFFu,
};
uint32_t ChannelsForFormat(Format f);       // 1 / 2 / 8; Unknown -> 0
bool FormatIsFloat(Format f);
bool FormatIsStd(Format f);                 // the two *Std channel maps
bool FormatFromParam(uint32_t param, Format& out);  // false = unknown selector

// The GetPortState output (fields the reference fills; layout is host-side
// for now — the import-thunk layer will place it in guest memory).
struct AudioOutPortState {
    uint8_t output = 0;
    uint8_t channel = 0;
    uint8_t flag = 0;
    uint8_t rerouteCounter = 0;
    uint16_t activeState = 0;
    uint8_t volume = 127;
    uint8_t reserved = 0;
};

// ---------------------------------------------------------------------------
// AudioSink — one per opened port. Implementations: AAudio (device), mock
// (host gate). When Open fails, the port runs on the virtual-queue clock.
// ---------------------------------------------------------------------------
class AudioSink {
public:
    virtual ~AudioSink() = default;
    virtual bool Open(uint32_t freq, uint32_t channels, Format format) = 0;
    virtual void Queue(const void* data, uint32_t bytes) = 0;
    virtual size_t QueuedBytes() = 0;
    virtual void Clear() = 0;
    virtual void Close() = 0;
};

// Monotonic microsecond clock + paced wait (virtual seams for the host gate;
// the defaults are the real CLOCK_MONOTONIC pair).
class Clock {
public:
    virtual ~Clock() = default;
    virtual uint64_t NowUs() = 0;
    virtual void WaitUs(uint64_t us) = 0;
};

struct AudioOutStats {
    uint64_t opens = 0;
    uint64_t closes = 0;
    uint64_t outputs = 0;        // Output()/Outputs() calls with data
    uint64_t bytesQueued = 0;    // through real sinks
    uint64_t pacedWaits = 0;     // virtual-clock pacing events
    uint64_t failures = 0;       // refused calls

    void Reset() { *this = AudioOutStats{}; }
    std::string SummaryString() const;
};

class AudioOutHle {
public:
    static AudioOutHle& Instance();

    AudioOutHle(const AudioOutHle&) = delete;
    AudioOutHle& operator=(const AudioOutHle&) = delete;

    // Backend wiring. `factory` may return nullptr (or Open may fail) — the
    // port then runs on the virtual-queue clock. Test hook: SetClock.
    void SetSinkFactory(
        std::function<std::unique_ptr<AudioSink>(uint32_t freq,
                                                 uint32_t channels,
                                                 Format format)> factory);
    void SetClock(Clock* clock);   // not owned; nullptr = real clock

    // The ORBIS surface. Handles are 1-based port indices (verbatim).
    int32_t Init();
    int32_t Open(int32_t userId, int32_t type, int32_t index, uint32_t len,
                 uint32_t freq, uint32_t param);
    int32_t Close(int32_t handle);
    int32_t Output(int32_t handle, const void* ptr);
    struct OutputParam { int32_t handle; const void* ptr; };
    int32_t Outputs(const OutputParam* params, uint32_t num);
    int32_t SetVolume(int32_t handle, uint32_t flag, const int32_t* vol);
    int32_t GetLastOutputTime(int32_t handle, uint64_t* outputTime);
    int32_t GetPortState(int32_t handle, AudioOutPortState* state);
    int32_t SetMixLevelPadSpk(int32_t handle, int32_t mixLevel);

    AudioOutStats Stats() const;
    std::string GetSummaryString() const;

    // Test/telemetry hook: close everything (stats survive).
    void CloseAll();

private:
    AudioOutHle();
    ~AudioOutHle();
    struct Impl;
    Impl* impl_ = nullptr;
};

}  // namespace PX5::SceAudioOutHle

#endif  // PX5_AUDIO_AUDIO_OUT_HLE_H
