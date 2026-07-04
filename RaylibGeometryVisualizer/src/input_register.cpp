#include "input_register.h"

#include <array>

const InputButtonState& InputRegister::Key(int raylibKey) const
{
    static const InputButtonState emptyState;
    if (raylibKey < 0 || static_cast<std::size_t>(raylibKey) >= keys.size()) {
        return emptyState;
    }
    return keys[static_cast<std::size_t>(raylibKey)];
}

InputRegister CollectInputRegister()
{
    InputRegister input;
    input.pointerPosition = GetMousePosition();
    input.pointerDelta = GetMouseDelta();
    input.wheelDelta = GetMouseWheelMove();
    input.closeRequested = WindowShouldClose();

    for (std::size_t key = 0; key < input.keys.size(); ++key) {
        const int raylibKey = static_cast<int>(key);
        input.keys[key] = {
            IsKeyPressed(raylibKey),
            IsKeyPressedRepeat(raylibKey),
            IsKeyDown(raylibKey),
            IsKeyReleased(raylibKey)
        };
    }

    input.primaryPointer = {
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
        false,
        IsMouseButtonDown(MOUSE_BUTTON_LEFT),
        IsMouseButtonReleased(MOUSE_BUTTON_LEFT)
    };
    input.secondaryPointer = {
        IsMouseButtonPressed(MOUSE_BUTTON_RIGHT),
        false,
        IsMouseButtonDown(MOUSE_BUTTON_RIGHT),
        IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)
    };
    return input;
}

ApplicationActions MapInputToApplicationActions(const InputRegister& input)
{
    ApplicationActions actions;
    actions.pointerPosition = input.pointerPosition;
    actions.pointerDelta = input.pointerDelta;
    actions.zoomDelta = input.wheelDelta;

    actions.primaryPressed = input.primaryPointer.pressed;
    actions.primaryDown = input.primaryPointer.down;
    actions.primaryReleased = input.primaryPointer.released;
    actions.secondaryPressed = input.secondaryPointer.pressed;
    actions.secondaryDown = input.secondaryPointer.down;
    actions.secondaryReleased = input.secondaryPointer.released;
    actions.addPointModifier = input.Key(KEY_LEFT_SHIFT).down ||
        input.Key(KEY_RIGHT_SHIFT).down;

    actions.nextAlgorithm = input.Key(KEY_TAB).pressed;
    constexpr std::array<int, 9> digitKeys = {
        KEY_ONE,
        KEY_TWO,
        KEY_THREE,
        KEY_FOUR,
        KEY_FIVE,
        KEY_SIX,
        KEY_SEVEN,
        KEY_EIGHT,
        KEY_NINE
    };
    for (std::size_t i = 0; i < digitKeys.size(); ++i) {
        if (input.Key(digitKeys[i]).pressed) {
            actions.selectedAlgorithm = static_cast<int>(i);
        }
    }

    actions.load = input.Key(KEY_L).pressed;
    actions.save = input.Key(KEY_S).pressed;
    actions.clear = input.Key(KEY_C).pressed;
    actions.runAlgorithm = input.Key(KEY_ENTER).pressed;
    actions.fitView = input.Key(KEY_F).pressed;
    actions.restartPlayback = input.Key(KEY_R).pressed;
    actions.toggleForwardPlayback = input.Key(KEY_SPACE).pressed;
    actions.toggleBackwardPlayback = input.Key(KEY_B).pressed;
    const InputButtonState& right = input.Key(KEY_RIGHT);
    const InputButtonState& left = input.Key(KEY_LEFT);
    actions.stepForward = right.pressed || right.repeated;
    actions.stepBackward = left.pressed || left.repeated;
    actions.jumpToStart = input.Key(KEY_HOME).pressed;
    actions.jumpToEnd = input.Key(KEY_END).pressed;
    actions.increaseSpeed = input.Key(KEY_UP).pressed;
    actions.decreaseSpeed = input.Key(KEY_DOWN).pressed;
    return actions;
}
