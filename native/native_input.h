#pragma once

#include <algorithm>
#include <cmath>
#include "raylib.h"

// Uses raylib's standard gamepad mapping (Xbox-style and compatible pads).
// Physical controller behavior still needs verification on the target PC.
struct NativeInputState {
    float steer = 0.0f;
    float push = 0.0f;
    float brake = 0.0f;
    bool crouch = false;
    bool jumpReleased = false;
    bool resetPressed = false;
    bool gamepad = false;
    int gamepadIndex = -1;
};

inline NativeInputState sampleNativeInput() {
    static bool previousCrouch = false;
    NativeInputState state;

    const bool left = IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT);
    const bool right = IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT);
    state.steer = static_cast<float>(right) - static_cast<float>(left);
    state.push = (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) ? 1.0f : 0.0f;
    state.brake = (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) ? 1.0f : 0.0f;
    state.crouch = IsKeyDown(KEY_SPACE);
    state.resetPressed = IsKeyPressed(KEY_R);

    for (int index = 0; index < 4; ++index) {
        if (!IsGamepadAvailable(index)) continue;

        state.gamepad = true;
        state.gamepadIndex = index;
        const auto axis = [](float value) {
            constexpr float deadzone = 0.18f;
            if (std::fabs(value) <= deadzone) return 0.0f;
            const float amount = std::min(1.0f, (std::fabs(value) - deadzone) /
                                                       (1.0f - deadzone));
            return std::copysign(amount, value);
        };
        const float x = axis(GetGamepadAxisMovement(index, GAMEPAD_AXIS_LEFT_X));
        const float y = axis(GetGamepadAxisMovement(index, GAMEPAD_AXIS_LEFT_Y));
        const bool padLeft = IsGamepadButtonDown(index, GAMEPAD_BUTTON_LEFT_FACE_LEFT);
        const bool padRight = IsGamepadButtonDown(index, GAMEPAD_BUTTON_LEFT_FACE_RIGHT);
        const bool padUp = IsGamepadButtonDown(index, GAMEPAD_BUTTON_LEFT_FACE_UP);
        const bool padDown = IsGamepadButtonDown(index, GAMEPAD_BUTTON_LEFT_FACE_DOWN);

        // A D-pad direction takes precedence over its corresponding analog axis.
        const float padSteer = (padLeft || padRight)
            ? static_cast<float>(padRight) - static_cast<float>(padLeft) : x;
        state.steer = std::clamp(state.steer + padSteer, -1.0f, 1.0f);
        state.push = std::max(state.push, padUp ? 1.0f : std::max(0.0f, -y));
        state.brake = std::max(state.brake, padDown ? 1.0f : std::max(0.0f, y));
        state.crouch = state.crouch ||
            IsGamepadButtonDown(index, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
        state.resetPressed = state.resetPressed ||
            IsGamepadButtonPressed(index, GAMEPAD_BUTTON_MIDDLE_LEFT);
        break;
    }

    // Release only after every active crouch input has been released. This keeps
    // a keyboard release from triggering an ollie while the pad button is held.
    state.jumpReleased = previousCrouch && !state.crouch;
    previousCrouch = state.crouch;
    return state;
}
