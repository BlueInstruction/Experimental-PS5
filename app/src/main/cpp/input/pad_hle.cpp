// SPDX-License-Identifier: GPL-2.0-only
// PX5 — ORBIS scePad HLE, implementation. See pad_hle.h.
// Ported from AnyPS5 libScePad Export.cpp (GPL-2.0-only). The state source
// is PX5's InputManager: the Android virtual pad + controller bridge feed
// lock-free atomics; this service maps them into the verbatim ORBIS PadData
// every time the title polls.
#include "pad_hle.h"

#include <chrono>
#include <cstring>
#include <mutex>

#include "../input/controller.h"
#include "../utils/logger.h"

namespace PX5::ScePadHle {
namespace {

bool ValidPort(int userId, int type, int index) {
    const bool personalPort =
        type == kPortTypeStandard || type == kPortTypeSpecial;
    const bool systemRemote = userId == kUserIdSystem &&
                              type == kPortTypeRemote;
    return index == 0 && (personalPort || systemRemote);
}

// InputManager's Android-side bit -> ORBIS PadData bit. The InputManager
// enum is PX5's private UI encoding (input/controller.h); the ORBIS bits
// are the ABI the guest sees. PS_HOME has no ORBIS counterpart in this
// table and stays unmapped (0) — honest absence, not a fake bit.
uint32_t MapButtons(uint32_t in) {
    using PX5::PAD_CROSS; using PX5::PAD_CIRCLE;
    using PX5::PAD_SQUARE; using PX5::PAD_TRIANGLE;
    using PX5::PAD_DPAD_UP; using PX5::PAD_DPAD_DOWN;
    using PX5::PAD_DPAD_LEFT; using PX5::PAD_DPAD_RIGHT;
    using PX5::PAD_L1; using PX5::PAD_R1;
    using PX5::PAD_OPTIONS; using PX5::PAD_SHARE;
    using PX5::PAD_L3; using PX5::PAD_R3;
    using PX5::PAD_TOUCHPAD;
    uint32_t out = 0;
    if (in & PAD_CROSS)     out |= PadButton::Cross;
    if (in & PAD_CIRCLE)    out |= PadButton::Circle;
    if (in & PAD_SQUARE)    out |= PadButton::Square;
    if (in & PAD_TRIANGLE)  out |= PadButton::Triangle;
    if (in & PAD_DPAD_UP)   out |= PadButton::Up;
    if (in & PAD_DPAD_DOWN) out |= PadButton::Down;
    if (in & PAD_DPAD_LEFT) out |= PadButton::Left;
    if (in & PAD_DPAD_RIGHT) out |= PadButton::Right;
    if (in & PAD_L1)        out |= PadButton::L1;
    if (in & PAD_R1)        out |= PadButton::R1;
    if (in & PAD_OPTIONS)   out |= PadButton::Options;
    if (in & PAD_SHARE)     out |= PadButton::Create;
    if (in & PAD_L3)        out |= PadButton::L3;
    if (in & PAD_R3)        out |= PadButton::R3;
    if (in & PAD_TOUCHPAD)  out |= PadButton::TouchPad;
    return out;
}

// Float axis [-1..1] -> ORBIS stick byte [0..255], centre 128, clamped.
uint8_t StickByte(float v) {
    float s = (v + 1.0f) * 127.5f;
    if (s < 0.0f) s = 0.0f;
    if (s > 255.0f) s = 255.0f;
    return static_cast<uint8_t>(s + 0.5f);
}

// Float trigger [0..1] -> ORBIS analog byte [0..255], clamped.
uint8_t TriggerByte(float v) {
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    return static_cast<uint8_t>(v * 255.0f + 0.5f);
}

uint64_t NowUs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

}  // namespace

std::string PadStats::SummaryString() const {
    char b[128];
    snprintf(b, sizeof(b),
             "opens=%llu closes=%llu reads=%llu vibrations=%llu failures=%llu",
             static_cast<unsigned long long>(opens),
             static_cast<unsigned long long>(closes),
             static_cast<unsigned long long>(reads),
             static_cast<unsigned long long>(vibrations),
             static_cast<unsigned long long>(failures));
    return std::string(b);
}

struct PadHle::Impl {
    std::mutex mutex;
    bool opened = false;
    PadStats stats;
    uint8_t lastLightBar[3]{};
    bool lightBarValid = false;
};

PadHle& PadHle::Instance() {
    static PadHle instance;
    return instance;
}

PadHle::PadHle() : impl_(new Impl{}) {}
PadHle::~PadHle() { delete impl_; }

int32_t PadHle::Init() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return kOk;   // verbatim: the reference's Init always succeeds
}

int32_t PadHle::Open(int32_t userId, int32_t type, int32_t index,
                     const void* param) {
    (void)param;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!ValidPort(userId, type, index)) {
        ++impl_->stats.failures;
        return kErrorInvalidArg;
    }
    impl_->opened = true;
    ++impl_->stats.opens;
    return kHandle;
}

int32_t PadHle::Close(int32_t handle) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (handle != kHandle) { ++impl_->stats.failures; return kErrorInvalidHandle; }
    impl_->opened = false;
    ++impl_->stats.closes;
    return kOk;
}

int32_t PadHle::ReadState(int32_t handle, PadData* data) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (handle != kHandle) { ++impl_->stats.failures; return kErrorInvalidHandle; }
    if (data == nullptr) { ++impl_->stats.failures; return kErrorInvalidArg; }

    // One current state, built from the live InputManager atomics.
    auto& input = PX5::InputManager::GetInstance();
    std::memset(data, 0, sizeof(*data));
    data->buttons = MapButtons(input.ButtonMask());
    data->left_stick_x = StickByte(input.Lx());
    data->left_stick_y = StickByte(input.Ly());
    data->right_stick_x = StickByte(input.Rx());
    data->right_stick_y = StickByte(input.Ry());
    data->analog_buttons_l2 = TriggerByte(input.L2());
    data->analog_buttons_r2 = TriggerByte(input.R2());
    // Motion: honest zeros (accel default per the reference's neutral state
    // is +Y gravity, but PX5 has no motion source — zeros, not fabrication).
    data->orientation_x = 0.0f;
    data->orientation_y = 0.0f;
    data->orientation_z = 0.0f;
    data->orientation_w = 1.0f;   // identity quaternion
    data->touch_data_touch_num = input.Touchpad() ? 1u : 0u;
    if (input.Touchpad()) {
        // The touchpad press arrives as a single contact at the pad centre
        // (the virtual pad has no per-contact coordinates yet — documented).
        data->touch_data_touch0_x = 960u;
        data->touch_data_touch0_y = 471u;
        data->touch_data_touch0_id = 0u;
    }
    data->connected = impl_->opened;
    data->timestamp = NowUs();
    data->connected_count = 1u;
    ++impl_->stats.reads;
    return kOk;
}

int32_t PadHle::Read(int32_t handle, PadData* data, int32_t num) {
    // The title drains the queued states; one current state is reported per
    // call (verbatim reference semantics).
    if (data == nullptr || num <= 0) {
        ++impl_->stats.failures;
        return kErrorInvalidArg;
    }
    const int32_t rc = ReadState(handle, data);
    if (rc != kOk) return rc;
    return 1;
}

int32_t PadHle::GetControllerInformation(int32_t handle,
                                         PadControllerInformation* info) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (handle != kHandle) { ++impl_->stats.failures; return kErrorInvalidHandle; }
    if (info == nullptr) { ++impl_->stats.failures; return kErrorInvalidArg; }
    std::memset(info, 0, sizeof(*info));
    info->touchPadInfo.pixelDensity = 44.86f;   // verbatim reference values
    info->touchPadInfo.resolution.x = 1920;
    info->touchPadInfo.resolution.y = 943;
    info->stickInfo.deadZoneLeft = 2;
    info->stickInfo.deadZoneRight = 2;
    info->connectionType = kConnectionTypeLocal;
    info->connectedCount = 1;
    info->connected = true;
    info->deviceClass = kDeviceClassStandard;
    return kOk;
}

int32_t PadHle::GetInfo(PadInfo* info) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (info == nullptr) { ++impl_->stats.failures; return kErrorInvalidArg; }
    std::memset(info, 0, sizeof(*info));
    info->maxConnectCount = 4;
    info->connectedCount[0] = 1;
    info->connectedCount[1] = 0;
    PadInfo::PadTypeInfo& slot = info->padTypeInfo[0];
    slot.connectPort = 0;
    slot.status = 1;
    slot.deviceType = kDeviceTypeDualSense;
    slot.connectType = 2 /* BLUETOOTH, verbatim reference fill */;
    static constexpr uint8_t kBluetoothMac[6] = {0xDC, 0x8C, 0x37, 0x51,
                                                 0x01, 0xF4};
    std::memcpy(slot.bluetoothMacAddress, kBluetoothMac, sizeof(kBluetoothMac));
    return kOk;
}

int32_t PadHle::SetVibration(int32_t handle, const PadVibrationParam* param) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (handle != kHandle) { ++impl_->stats.failures; return kErrorInvalidHandle; }
    if (param == nullptr) { ++impl_->stats.failures; return kErrorInvalidArg; }
    // No vibration actuator is wired on this device yet — the motors'
    // requested strengths are counted, not fabricated as delivered.
    ++impl_->stats.vibrations;
    PX5_LOGD(LogCategory::CORE,
             "PadHle::SetVibration large=%u small=%u (no actuator backend)",
             param->large_motor, param->small_motor);
    return kOk;
}

int32_t PadHle::SetLightBar(int32_t handle, const PadLightBarParam* param) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (handle != kHandle) { ++impl_->stats.failures; return kErrorInvalidHandle; }
    if (param == nullptr) { ++impl_->stats.failures; return kErrorInvalidArg; }
    impl_->lightBarValid = true;
    impl_->lastLightBar[0] = param->r;
    impl_->lastLightBar[1] = param->g;
    impl_->lastLightBar[2] = param->b;
    return kOk;
}

int32_t PadHle::ResetLightBar(int32_t handle) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (handle != kHandle) { ++impl_->stats.failures; return kErrorInvalidHandle; }
    impl_->lightBarValid = false;
    impl_->lastLightBar[0] = impl_->lastLightBar[1] = impl_->lastLightBar[2] = 0;
    return kOk;
}

int32_t PadHle::ResetOrientation(int32_t handle) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (handle != kHandle) { ++impl_->stats.failures; return kErrorInvalidHandle; }
    return kOk;   // no orientation source; accepted (verbatim semantics)
}

int32_t PadHle::SetMotionSensorState(int32_t handle, bool enable) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (handle != kHandle) { ++impl_->stats.failures; return kErrorInvalidHandle; }
    (void)enable;   // no motion source; accepted
    return kOk;
}

int32_t PadHle::SetProcessPrivilege(int32_t privilege) {
    (void)privilege;
    return kOk;   // verbatim no-op
}

int32_t PadHle::SetParticularMode(bool mode) {
    (void)mode;
    return kOk;   // verbatim no-op
}

PadStats PadHle::Stats() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->stats;
}

std::string PadHle::GetSummaryString() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return "scePad HLE: " +
           std::string(impl_->opened ? "opened" : "closed") + " | " +
           impl_->stats.SummaryString();
}

}  // namespace PX5::ScePadHle
