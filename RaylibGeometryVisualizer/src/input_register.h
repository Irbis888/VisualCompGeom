#pragma once

#include "raylib.h"

#include <array>
#include <cstddef>

struct InputButtonState {
    bool pressed = false;
    bool repeated = false;
    bool down = false;
    bool released = false;
};

// A complete snapshot of the controls used by the application during one frame.
// It contains physical input only; application meanings are assigned below.
struct InputRegister {
    static constexpr std::size_t KeyboardKeyCapacity = 512;

    Vector2 pointerPosition{};
    Vector2 pointerDelta{};
    float wheelDelta = 0.0F;
    bool closeRequested = false;

    std::array<InputButtonState, KeyboardKeyCapacity> keys{};
    InputButtonState primaryPointer;
    InputButtonState secondaryPointer;

    const InputButtonState& Key(int raylibKey) const;
};

struct ApplicationActions {
    Vector2 pointerPosition{};
    Vector2 pointerDelta{};
    float zoomDelta = 0.0F;
    bool primaryPressed = false;
    bool primaryDown = false;
    bool primaryReleased = false;
    bool secondaryPressed = false;
    bool secondaryDown = false;
    bool secondaryReleased = false;
    bool addPointModifier = false;
    bool nextAlgorithm = false;
    int selectedAlgorithm = -1;
    bool load = false;
    bool save = false;
    bool clear = false;
    bool runAlgorithm = false;
    bool fitView = false;
    bool restartPlayback = false;
    bool toggleForwardPlayback = false;
    bool toggleBackwardPlayback = false;
    bool stepForward = false;
    bool stepBackward = false;
    bool jumpToStart = false;
    bool jumpToEnd = false;
    bool increaseSpeed = false;
    bool decreaseSpeed = false;
};

InputRegister CollectInputRegister();
ApplicationActions MapInputToApplicationActions(const InputRegister& input);
