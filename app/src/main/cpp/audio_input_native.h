#ifndef PX5_AUDIO_INPUT_NATIVE_H
#define PX5_AUDIO_INPUT_NATIVE_H

#include <string>
#include <cstdint>

class AudioInputNative {
public:
    static AudioInputNative& getInstance();

    bool initAAudioStream();
    bool sendDualSenseHapticFeedback(uint8_t leftMotor, uint8_t rightMotor);
    std::string getAudioInputStatus() const;

private:
    AudioInputNative() = default;
    bool m_aaudioActive = false;
    uint32_t m_sampleRate = 48000;
    uint32_t m_channelCount = 8; // PS5 7.1 Surround Sound output
};

#endif // PX5_AUDIO_INPUT_NATIVE_H
