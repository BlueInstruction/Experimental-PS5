#ifndef PX5_CONTROLLER_H
#define PX5_CONTROLLER_H

#include <cstdint>

namespace PX5 {

struct DualSenseState {
    uint32_t buttons;
    float lx, ly;
    float rx, ry;
    float l2, r2;
    bool touchPadPressed;
};

class InputManager {
public:
    static InputManager& GetInstance();
    void UpdateState(const DualSenseState& state);
    DualSenseState GetState() const;

private:
    InputManager() = default;
    ~InputManager() = default;
    DualSenseState m_state{};
};

} // namespace PX5

#endif // PX5_CONTROLLER_H
