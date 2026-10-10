// SPDX-License-Identifier: GPL-2.0-only
// PX5 host gate — scePad HLE locked behaviour (PadData layout, ORBIS button
// bits, stick/trigger byte conversion, read semantics, handle rules).
#include "input/pad_hle.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>

#include "input/controller.h"

using namespace PX5;
using namespace PX5::ScePadHle;

namespace {

int g_failures = 0;

void Check(bool ok, const std::string& what) {
    if (ok) {
        std::printf("  ok  %s\n", what.c_str());
        return;
    }
    ++g_failures;
    std::printf("FAIL  %s\n", what.c_str());
}

void ResetInput() {
    auto& in = InputManager::GetInstance();
    for (uint32_t bit = 1; bit != 0; bit <<= 1) in.SetButton(bit, false);
    in.SetLeftStick(0.0f, 0.0f);
    in.SetRightStick(0.0f, 0.0f);
    in.SetTriggers(0.0f, 0.0f);
    in.TouchpadPressed(false);
}

}  // namespace

int main() {
    // ---- 1. the PadData layout IS the ORBIS ABI --------------------------------
    Check(sizeof(PadData) == 120, "sizeof(PadData) == 120");
    Check(offsetof(PadData, buttons) == 0, "buttons @0");
    Check(offsetof(PadData, left_stick_x) == 4, "left_stick_x @4");
    Check(offsetof(PadData, right_stick_y) == 7, "right_stick_y @7");
    Check(offsetof(PadData, analog_buttons_l2) == 8, "l2 @8");
    Check(offsetof(PadData, analog_buttons_r2) == 9, "r2 @9");
    Check(offsetof(PadData, orientation_x) == 12, "orientation @12");
    Check(offsetof(PadData, acceleration_x) == 28, "acceleration @28");
    Check(offsetof(PadData, angular_velocity_x) == 40, "angular velocity @40");
    Check(offsetof(PadData, touch_data_touch_num) == 52, "touch_num @52");
    Check(offsetof(PadData, touch_data_touch0_x) == 60, "touch0 @60");
    Check(offsetof(PadData, touch_data_touch1_x) == 68, "touch1 @68");
    Check(offsetof(PadData, connected) == 76, "connected @76");
    Check(offsetof(PadData, timestamp) == 80, "timestamp @80");
    Check(offsetof(PadData, device_unique_data) == 108, "unique data @108");

    // ---- 2. ORBIS button bits (verbatim) ----------------------------------------
    Check(PadButton::Cross == 0x4000u && PadButton::Circle == 0x2000u &&
              PadButton::Square == 0x8000u && PadButton::Triangle == 0x1000u,
          "face buttons: Cross 0x4000 Circle 0x2000 Square 0x8000 Triangle 0x1000");
    Check(PadButton::Up == 0x10u && PadButton::Right == 0x20u &&
              PadButton::Down == 0x40u && PadButton::Left == 0x80u,
          "dpad bits (Right is 0x20 — the mapping, not the UI encoding)");
    Check(PadButton::L2 == 0x100u && PadButton::R2 == 0x200u &&
              PadButton::L1 == 0x400u && PadButton::R1 == 0x800u,
          "shoulder bits");
    Check(PadButton::Options == 0x8u && PadButton::Create == 0x10000u &&
              PadButton::TouchPad == 0x100000u && PadButton::L3 == 0x2u &&
              PadButton::R3 == 0x4u,
          "system buttons (Options 0x8, Create 0x10000, TouchPad 0x100000)");

    // ---- 3. handle / port validation ----------------------------------------------
    auto& pad = PadHle::Instance();
    Check(pad.Open(0, 5, 0, nullptr) == kErrorInvalidArg,
          "port type 5 refused");
    Check(pad.Open(0, kPortTypeStandard, 1, nullptr) == kErrorInvalidArg,
          "index != 0 refused");
    Check(pad.Open(0, kPortTypeRemote, 0, nullptr) == kErrorInvalidArg,
          "remote without the system user refused");
    Check(pad.Open(kUserIdSystem, kPortTypeRemote, 0, nullptr) == kHandle,
          "system user + remote port accepted");
    Check(pad.ReadState(2, nullptr) == kErrorInvalidHandle,
          "invalid handle refused");
    Check(pad.ReadState(kHandle, nullptr) == kErrorInvalidArg,
          "null data refused");
    {
        PadData d{};
        Check(pad.Read(kHandle, nullptr, 1) == kErrorInvalidArg,
              "Read: null data refused");
        Check(pad.Read(kHandle, &d, 0) == kErrorInvalidArg,
              "Read: zero count refused");
        Check(pad.Read(kHandle, &d, 3) == 1,
              "Read reports ONE state per call (returns 1)");
    }

    // ---- 4. state bridging: buttons ---------------------------------------------------
    {
        ResetInput();
        auto& in = InputManager::GetInstance();
        PadData d{};
        pad.ReadState(kHandle, &d);
        Check(d.buttons == 0, "neutral state: no buttons");

        in.SetButton(PAD_CROSS, true);
        in.SetButton(PAD_DPAD_RIGHT, true);
        in.SetButton(PAD_SHARE, true);
        in.SetButton(PAD_L1, true);
        in.SetButton(PAD_PS_HOME, true);   // no ORBIS counterpart
        pad.ReadState(kHandle, &d);
        Check(d.buttons == (PadButton::Cross | PadButton::Right |
                            PadButton::Create | PadButton::L1),
              "UI bits map to ORBIS bits (SHARE->Create, PS home unmapped)");
        ResetInput();
    }

    // ---- 5. state bridging: sticks + triggers -------------------------------------------
    {
        ResetInput();
        auto& in = InputManager::GetInstance();
        PadData d{};
        pad.ReadState(kHandle, &d);
        Check(d.left_stick_x == 128 && d.left_stick_y == 128 &&
                  d.right_stick_x == 128 && d.right_stick_y == 128,
              "neutral sticks are centred at 128");
        Check(d.analog_buttons_l2 == 0 && d.analog_buttons_r2 == 0,
              "neutral triggers are 0");

        in.SetLeftStick(1.0f, -1.0f);
        in.SetRightStick(0.5f, -0.5f);
        in.SetTriggers(1.0f, 0.5f);
        pad.ReadState(kHandle, &d);
        Check(d.left_stick_x == 255 && d.left_stick_y == 0,
              "+x -> 255, -y -> 0");
        Check(d.right_stick_x == 191 && d.right_stick_y == 64,
              "half deflection -> 191/64 (1.5*127.5 rounds down)");
        Check(d.analog_buttons_l2 == 255 && d.analog_buttons_r2 == 128,
              "triggers: 1.0 -> 255, 0.5 -> 128");
        ResetInput();
    }

    // ---- 6. touch + connected + timestamp ------------------------------------------------
    {
        ResetInput();
        auto& in = InputManager::GetInstance();
        PadData d{};
        in.TouchpadPressed(true);
        pad.ReadState(kHandle, &d);
        Check(d.touch_data_touch_num == 1 && d.touch_data_touch0_x == 960 &&
                  d.touch_data_touch0_y == 471,
              "touchpad press reports one centred contact");
        Check(d.connected, "connected while opened");
        Check(d.timestamp > 0, "timestamp populated");
        in.TouchpadPressed(false);
        pad.ReadState(kHandle, &d);
        Check(d.touch_data_touch_num == 0, "release clears the contact");
        Check(pad.Close(kHandle) == kOk, "close ok");
        pad.ReadState(kHandle, &d);
        Check(!d.connected, "read after close reports connected=false");
    }

    // ---- 7. controller information + info (verbatim reference fills) ---------------------
    {
        PadControllerInformation info{};
        Check(pad.GetControllerInformation(kHandle, &info) == kOk &&
                  info.touchPadInfo.pixelDensity == 44.86f &&
                  info.touchPadInfo.resolution.x == 1920 &&
                  info.touchPadInfo.resolution.y == 943 &&
                  info.stickInfo.deadZoneLeft == 2 &&
                  info.stickInfo.deadZoneRight == 2 &&
                  info.connectionType == kConnectionTypeLocal &&
                  info.connected && info.deviceClass == kDeviceClassStandard,
              "controller information fills the verbatim reference values");
        PadInfo pi{};
        Check(pad.GetInfo(&pi) == kOk && pi.maxConnectCount == 4 &&
                  pi.padTypeInfo[0].deviceType == kDeviceTypeDualSense &&
                  pi.padTypeInfo[0].bluetoothMacAddress[0] == 0xDC &&
                  pi.padTypeInfo[0].bluetoothMacAddress[5] == 0xF4,
              "pad info fills the reference identity");
        Check(pad.GetControllerInformation(9, &info) == kErrorInvalidHandle &&
                  pad.GetControllerInformation(kHandle, nullptr) ==
                      kErrorInvalidArg &&
                  pad.GetInfo(nullptr) == kErrorInvalidArg,
              "information refusals");
        Check(pad.Open(0, kPortTypeStandard, 0, nullptr) == kHandle,
              "reopen after close");
    }

    if (g_failures == 0) {
        std::printf("pad_hle_test: ALL PASS\n");
        return 0;
    }
    std::printf("pad_hle_test: %d FAILURES\n", g_failures);
    return 1;
}
