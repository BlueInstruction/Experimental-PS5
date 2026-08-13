#ifndef PX5_AUDIO_H
#define PX5_AUDIO_H

#include <cstdint>

namespace PX5 {

class AudioEngine {
public:
    static AudioEngine& GetInstance();
    bool Initialize();
    void Shutdown();
    void SetVolume(float volume);

private:
    AudioEngine() = default;
    ~AudioEngine() = default;
    bool m_initialized = false;
};

} // namespace PX5

#endif // PX5_AUDIO_H
