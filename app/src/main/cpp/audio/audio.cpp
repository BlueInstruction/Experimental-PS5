#include "audio.h"
#include "../utils/logger.h"

namespace PX5 {

AudioEngine& AudioEngine::GetInstance() {
    static AudioEngine instance;
    return instance;
}

// NOT IMPLEMENTED, and it says so.
//
// The previous version logged "AAudio Low-Latency Stream initialized
// (SceAudioOut HLE)" and returned true without calling a single AAudio
// function. Callers, the status string, and the README all inherited that
// claim. There is no output stream, no callback, no SceAudioOut HLE.
//
// Wiring this up means AAudio_createStreamBuilder ->
// AAudioStreamBuilder_setDataCallback -> AAudioStream_requestStart, plus a
// guest-side ring buffer fed by the SceAudioOut HLE entry points. Until that
// exists, Initialize() reports failure so nothing downstream can claim audio
// works.
bool AudioEngine::Initialize() {
    if (m_initialized) return true;
    PX5_LOGW(LogCategory::CORE,
             "audio: NOT IMPLEMENTED - no AAudio stream is opened and no "
             "SceAudioOut HLE exists; guest audio output is silent");
    return false;
}

void AudioEngine::Shutdown() {
    m_initialized = false;
}

void AudioEngine::SetVolume(float volume) {
    PX5_LOGW(LogCategory::CORE,
             "audio: SetVolume(%.2f) ignored - no audio stream exists",
             volume);
}

bool AudioEngine::IsInitialized() const {
    return m_initialized;
}

std::string AudioEngine::GetStatusString() const {
    return "audio: NOT IMPLEMENTED (no AAudio stream, no SceAudioOut HLE)";
}

} // namespace PX5
