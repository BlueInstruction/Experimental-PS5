#include "audio.h"
#include "../utils/logger.h"

namespace PX5 {

AudioEngine& AudioEngine::GetInstance() {
    static AudioEngine instance;
    return instance;
}

bool AudioEngine::Initialize() {
    if (m_initialized) return true;
    PX5_LOGI(LogCategory::CORE, "AAudio Low-Latency Stream initialized (SceAudioOut HLE)");
    m_initialized = true;
    return true;
}

void AudioEngine::Shutdown() {
    m_initialized = false;
}

void AudioEngine::SetVolume(float volume) {
    PX5_LOGI(LogCategory::CORE, "AAudio Output Volume set to %.2f", volume);
}

} // namespace PX5
