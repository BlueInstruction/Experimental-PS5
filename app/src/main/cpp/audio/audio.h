#ifndef PX5_AUDIO_H
#define PX5_AUDIO_H

#include <cstdint>
#include <string>

namespace PX5 {

// Audio output seam. NOTHING IS IMPLEMENTED YET: Initialize() returns false
// and GetStatusString() says so. The type exists so the emulator's startup
// sequence has a place to call into once a real AAudio stream and the
// SceAudioOut HLE are written -- not so the UI can display a success.
class AudioEngine {
public:
    static AudioEngine& GetInstance();

    // Returns false: no AAudio stream is opened. See audio.cpp.
    bool Initialize();
    void Shutdown();
    void SetVolume(float volume);

    bool IsInitialized() const;
    std::string GetStatusString() const;

private:
    AudioEngine() = default;
    ~AudioEngine() = default;
    bool m_initialized = false;
};

} // namespace PX5

#endif // PX5_AUDIO_H
