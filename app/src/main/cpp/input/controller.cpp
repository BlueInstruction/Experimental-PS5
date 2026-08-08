#include "controller.h"
#include "../utils/logger.h"

namespace PX5 {

InputManager& InputManager::GetInstance() {
    static InputManager instance;
    return instance;
}

void InputManager::UpdateState(const DualSenseState& state) {
    m_state = state;
}

DualSenseState InputManager::GetState() const {
    return m_state;
}

} // namespace PX5
