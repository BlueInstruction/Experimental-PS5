#include "controller.h"
#include "../utils/logger.h"

#include <chrono>
#include <cstdio>

namespace PX5 {

namespace {
inline uint64_t NowMs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
}
}

InputManager& InputManager::GetInstance() {
    static InputManager instance;
    return instance;
}

void InputManager::SetButton(uint32_t bit, bool pressed) {
    if (pressed) m_buttons.fetch_or(bit, std::memory_order_relaxed);
    else         m_buttons.fetch_and(~bit, std::memory_order_relaxed);
    m_lastEventMs.store(NowMs(), std::memory_order_relaxed);
}

void InputManager::SetLeftStick(float lx, float ly) {
    m_lx.store(lx); m_ly.store(ly);
    m_lastEventMs.store(NowMs(), std::memory_order_relaxed);
}

void InputManager::SetRightStick(float rx, float ry) {
    m_rx.store(rx); m_ry.store(ry);
    m_lastEventMs.store(NowMs(), std::memory_order_relaxed);
}

void InputManager::SetTriggers(float l2, float r2) {
    m_l2.store(l2); m_r2.store(r2);
    m_lastEventMs.store(NowMs(), std::memory_order_relaxed);
}

void InputManager::TouchpadPressed(bool pressed) {
    m_touchpad.store(pressed);
    m_lastEventMs.store(NowMs(), std::memory_order_relaxed);
}

std::string InputManager::GetSummaryString() const {
    char buf[160];
    snprintf(buf, sizeof(buf),
             "input: buttons=0x%08X | L=(%.2f,%.2f) R=(%.2f,%.2f) "
             "L2=%.2f R2=%.2f | last=%llums",
             m_buttons.load(),
             static_cast<double>(m_lx.load()), static_cast<double>(m_ly.load()),
             static_cast<double>(m_rx.load()), static_cast<double>(m_ry.load()),
             static_cast<double>(m_l2.load()), static_cast<double>(m_r2.load()),
             (unsigned long long)m_lastEventMs.load());
    return buf;
}

} // namespace PX5
