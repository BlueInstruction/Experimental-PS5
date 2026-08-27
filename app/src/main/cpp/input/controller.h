#ifndef PX5_CONTROLLER_H
#define PX5_CONTROLLER_H

#include <cstdint>
#include <atomic>
#include <string>

namespace PX5 {

// ---------------------------------------------------------------------------
// NativeInput — the honest shared state between touch UI and (future) guest
// pad endpoints.
//
// Kotlin writes button mask changes / axis values through JNI; these land in
// lock-free atomics that an HLE gamepad service will poll inside emulation
// threads later. The UI also reads back a summary + last-event latency to
// PROVE on screen that the pipeline really runs end-to-end.
// ---------------------------------------------------------------------------
enum PadButtons : uint32_t {
    PAD_CROSS     = 1u << 0,
    PAD_CIRCLE    = 1u << 1,
    PAD_SQUARE    = 1u << 2,
    PAD_TRIANGLE  = 1u << 3,
    PAD_DPAD_UP   = 1u << 4,
    PAD_DPAD_DOWN = 1u << 5,
    PAD_DPAD_LEFT = 1u << 6,
    PAD_DPAD_RIGHT= 1u << 7,
    PAD_L1        = 1u << 8,
    PAD_R1        = 1u << 9,
    PAD_OPTIONS   = 1u << 10,
    PAD_SHARE     = 1u << 11,
    PAD_PS_HOME   = 1u << 12,
};

class InputManager {
public:
    static InputManager& GetInstance();

    // Legacy blob update kept for any existing callers.
    struct DualSenseState {
        uint32_t buttons;
        float lx, ly;
        float rx, ry;
        float l2, r2;
        bool touchPadPressed;
    };

    void SetButton(uint32_t bit, bool pressed);
    void SetLeftStick(float lx, float ly);
    void SetRightStick(float rx, float ry);
    void SetTriggers(float l2, float r2);
    void TouchpadPressed(bool pressed);

    uint32_t ButtonMask() const { return m_buttons.load(std::memory_order_relaxed); }
    float Lx() const { return m_lx.load(); }
    float Ly() const { return m_ly.load(); }
    float Rx() const { return m_rx.load(); }
    float Ry() const { return m_ry.load(); }
    float L2() const { return m_l2.load(); }
    float R2() const { return m_r2.load(); }

    // ms since epoch of most recent event (any source) — readback evidence.
    uint64_t LastEventMs() const { return m_lastEventMs.load(); }

    std::string GetSummaryString() const;

private:
    InputManager() = default;

    std::atomic<uint32_t> m_buttons{0};
    std::atomic<float> m_lx{0}, m_ly{0}, m_rx{0}, m_ry{0};
    std::atomic<float> m_l2{0}, m_r2{0};
    std::atomic<bool> m_touchpad{false};
    std::atomic<uint64_t> m_lastEventMs{0};
};

} // namespace PX5

#endif // PX5_CONTROLLER_H
