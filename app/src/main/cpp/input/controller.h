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

/**
 * PS5 DualSense button bit masks.
 */
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

/**
 * Input manager for DualSense controller state (lock-free atomics).
 */
class InputManager {
public:
    /**
     * Returns the singleton InputManager instance.
     * @return Reference to singleton
     */
    static InputManager& GetInstance();

    /**
     * Legacy DualSense state structure for blob updates.
     */
    struct DualSenseState {
        uint32_t buttons;       ///< Button bit mask
        float lx, ly;           ///< Left stick axes
        float rx, ry;           ///< Right stick axes
        float l2, r2;           ///< Trigger values [0..1]
        bool touchPadPressed;   ///< Touchpad press state
    };

    /**
     * Sets a button state (press or release).
     * @param bit Button bit mask (from PadButtons enum)
     * @param pressed true for press, false for release
     */
    void SetButton(uint32_t bit, bool pressed);

    /**
     * Sets left analog stick position.
     * @param lx X axis [-1..1]
     * @param ly Y axis [-1..1]
     */
    void SetLeftStick(float lx, float ly);

    /**
     * Sets right analog stick position.
     * @param rx X axis [-1..1]
     * @param ry Y axis [-1..1]
     */
    void SetRightStick(float rx, float ry);

    /**
     * Sets trigger values.
     * @param l2 L2 trigger [0..1]
     * @param r2 R2 trigger [0..1]
     */
    void SetTriggers(float l2, float r2);

    /**
     * Sets touchpad press state.
     * @param pressed true if pressed, false otherwise
     */
    void TouchpadPressed(bool pressed);

    /**
     * Returns current button bit mask.
     * @return Button mask (combination of PadButtons bits)
     */
    uint32_t ButtonMask() const { return m_buttons.load(std::memory_order_relaxed); }

    /**
     * Returns left stick X axis.
     * @return Lx value [-1..1]
     */
    float Lx() const { return m_lx.load(); }

    /**
     * Returns left stick Y axis.
     * @return Ly value [-1..1]
     */
    float Ly() const { return m_ly.load(); }

    /**
     * Returns right stick X axis.
     * @return Rx value [-1..1]
     */
    float Rx() const { return m_rx.load(); }

    /**
     * Returns right stick Y axis.
     * @return Ry value [-1..1]
     */
    float Ry() const { return m_ry.load(); }

    /**
     * Returns L2 trigger value.
     * @return L2 value [0..1]
     */
    float L2() const { return m_l2.load(); }

    /**
     * Returns R2 trigger value.
     * @return R2 value [0..1]
     */
    float R2() const { return m_r2.load(); }

    /**
     * Returns milliseconds since epoch of most recent input event.
     * @return Last event timestamp (readback evidence)
     */
    uint64_t LastEventMs() const { return m_lastEventMs.load(); }

    /**
     * Returns human-readable summary of current input state.
     * @return Summary string with buttons, axes, timestamp
     */
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
