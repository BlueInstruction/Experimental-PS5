// SPDX-License-Identifier: GPL-2.0-only
// PX5 — ORBIS scePad HLE (app-owned service).
//
// WHAT THIS IS
//   The libScePad surface a guest title calls, ported from AnyPS5's
//   libScePad (Export.cpp + SceTypes.hpp, GPL-2.0-only) onto PX5's input
//   stack: the verbatim ORBIS PadData layout (the struct a game's
//   scePadRead buffer expects), the ORBIS button bits, the handle/port
//   validation rules, the error codes, and the read semantics (Read
//   reports ONE current state per call — the title drains by polling).
//   The state source is PX5's InputManager atomics (the virtual pad +
//   Android controller bridge); the HLE never polls SDL or any device.
//
// WHAT THIS IS NOT
//   No NID resolution yet (imports arrive with the import-thunk
//   milestone). Motion/touch are honest zeros with connected=true — a
//   game reading only buttons/sticks/triggers gets real data; a game
//   requiring motion gets the documented absence, not fabricated motion.
#ifndef PX5_INPUT_PAD_HLE_H
#define PX5_INPUT_PAD_HLE_H

#include <cstdint>
#include <string>

namespace PX5::ScePadHle {

// Verbatim ORBIS constants (AnyPS5 Pad.hpp).
constexpr int kOk = 0;
constexpr int kErrorInvalidArg    = -2137915391;
constexpr int kErrorInvalidHandle = -2137915389;
constexpr int kErrorNotConnected  = static_cast<int>(0x80920007u);
constexpr int kErrorNoHandle      = static_cast<int>(0x80920008u);

constexpr int kPortTypeStandard = 0;
constexpr int kPortTypeSpecial  = 2;
constexpr int kPortTypeRemote   = 16;
constexpr int kUserIdSystem     = 0xff;
constexpr int kHandle           = 1;

constexpr int kConnectionTypeLocal = 0;
constexpr int kDeviceClassStandard = 0;
constexpr int kDeviceTypeDualSense = 3;

// ORBIS PadData button bits (verbatim; CREATE included at its real bit).
namespace PadButton {
constexpr uint32_t L3       = 0x0002u;
constexpr uint32_t R3       = 0x0004u;
constexpr uint32_t Options  = 0x0008u;
constexpr uint32_t Up       = 0x0010u;
constexpr uint32_t Right    = 0x0020u;
constexpr uint32_t Down     = 0x0040u;
constexpr uint32_t Left     = 0x0080u;
constexpr uint32_t L2       = 0x0100u;
constexpr uint32_t R2       = 0x0200u;
constexpr uint32_t L1       = 0x0400u;
constexpr uint32_t R1       = 0x0800u;
constexpr uint32_t Triangle = 0x1000u;
constexpr uint32_t Circle   = 0x2000u;
constexpr uint32_t Cross    = 0x4000u;
constexpr uint32_t Square   = 0x8000u;
constexpr uint32_t Create   = 0x10000u;
constexpr uint32_t TouchPad = 0x100000u;
}  // namespace PadButton

// The ORBIS PadData layout, verbatim field order from the reference
// (SceTypes.hpp). A guest buffer written through scePadRead has exactly
// these offsets for every field a title reads.
struct PadData {
    uint32_t buttons;
    uint8_t left_stick_x;
    uint8_t left_stick_y;
    uint8_t right_stick_x;
    uint8_t right_stick_y;
    uint8_t analog_buttons_l2;
    uint8_t analog_buttons_r2;
    uint8_t padding[2];
    float orientation_x;
    float orientation_y;
    float orientation_z;
    float orientation_w;
    float acceleration_x;
    float acceleration_y;
    float acceleration_z;
    float angular_velocity_x;
    float angular_velocity_y;
    float angular_velocity_z;
    uint8_t touch_data_touch_num;
    uint8_t touch_data_reserve[3];
    uint32_t touch_data_reserve1;
    uint16_t touch_data_touch0_x;
    uint16_t touch_data_touch0_y;
    uint8_t touch_data_touch0_id;
    uint8_t touch_data_touch0_reserve[3];
    uint16_t touch_data_touch1_x;
    uint16_t touch_data_touch1_y;
    uint8_t touch_data_touch1_id;
    uint8_t touch_data_touch1_reserve[3];
    bool connected;
    uint64_t timestamp;
    uint32_t extension_unit_data_extension_unit_id;
    uint8_t extension_unit_data_reserve[1];
    uint8_t extension_unit_data_data_length;
    uint8_t extension_unit_data_data[10];
    uint8_t connected_count;
    uint8_t reserve[2];
    uint8_t device_unique_data_len;
    uint8_t device_unique_data[12];
};

// Verbatim controller-information shapes (the reference's fills included).
struct PadTouchPadInformation {
    float pixelDensity;
    struct { uint16_t x; uint16_t y; } resolution;
};
struct PadStickInformation {
    uint8_t deadZoneLeft;
    uint8_t deadZoneRight;
};
struct PadControllerInformation {
    PadTouchPadInformation touchPadInfo;
    PadStickInformation stickInfo;
    uint8_t connectionType;
    uint8_t connectedCount;
    bool connected;
    uint8_t pad[3];
    int32_t deviceClass;
    uint8_t reserve[8];
};
struct PadVibrationParam { uint8_t large_motor; uint8_t small_motor; };
struct PadLightBarParam { uint8_t r; uint8_t g; uint8_t b; };

struct PadInfo {
    uint8_t maxConnectCount;
    uint8_t connectedCount[2];
    uint8_t reserved0[2];
    struct PadTypeInfo {
        uint8_t connectPort;
        uint8_t status;
        uint8_t deviceType;
        uint8_t connectType;
        uint8_t bluetoothMacAddress[6];
    } padTypeInfo[8];
    uint8_t reserved1[4];
};

struct PadStats {
    uint64_t opens = 0;
    uint64_t closes = 0;
    uint64_t reads = 0;        // Read()/ReadState() calls
    uint64_t vibrations = 0;
    uint64_t failures = 0;     // refused calls

    void Reset() { *this = PadStats{}; }
    std::string SummaryString() const;
};

class PadHle {
public:
    static PadHle& Instance();

    PadHle(const PadHle&) = delete;
    PadHle& operator=(const PadHle&) = delete;

    // The ORBIS surface (semantics verbatim from the reference Export.cpp).
    int32_t Init();
    int32_t Open(int32_t userId, int32_t type, int32_t index,
                 const void* param);
    int32_t Close(int32_t handle);
    int32_t Read(int32_t handle, PadData* data, int32_t num);
    int32_t ReadState(int32_t handle, PadData* data);
    int32_t GetControllerInformation(int32_t handle,
                                     PadControllerInformation* info);
    int32_t GetInfo(PadInfo* info);
    int32_t SetVibration(int32_t handle, const PadVibrationParam* param);
    int32_t SetLightBar(int32_t handle, const PadLightBarParam* param);
    int32_t ResetLightBar(int32_t handle);
    int32_t ResetOrientation(int32_t handle);
    int32_t SetMotionSensorState(int32_t handle, bool enable);
    int32_t SetProcessPrivilege(int32_t privilege);
    int32_t SetParticularMode(bool mode);

    PadStats Stats() const;
    std::string GetSummaryString() const;

private:
    PadHle();
    ~PadHle();
    struct Impl;
    Impl* impl_ = nullptr;
};

}  // namespace PX5::ScePadHle

#endif  // PX5_INPUT_PAD_HLE_H
