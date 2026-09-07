#pragma once

#include <core/kraft_core.h>
#include <core/kraft_math.h>
#include <platform/kraft_keys.h>
#include <platform/kraft_window_events.h>

#define KRAFT_KEY_COUNT 512
#define KRAFT_MAX_TEXT_INPUT 64

namespace kraft {

struct MousePosition {
    int x;
    int y;
};

// Device state derived from the window event stream. Windows push events, ProcessEvents projects them here.
struct InputSystemState {
    bool key_down[KRAFT_KEY_COUNT];
    bool key_down_previous[KRAFT_KEY_COUNT];
    bool key_pressed[KRAFT_KEY_COUNT];
    bool key_released[KRAFT_KEY_COUNT];

    bool mouse_down[MOUSE_BUTTON_COUNT];
    bool mouse_down_previous[MOUSE_BUTTON_COUNT];
    bool mouse_pressed[MOUSE_BUTTON_COUNT];
    bool mouse_released[MOUSE_BUTTON_COUNT];

    Vec2f mouse_position;
    Vec2f mouse_position_previous;
    Vec2f mouse_delta;
    Vec2f scroll;
    bool dragging;

    bool shift_down;
    bool ctrl_down;
    bool alt_down;
    bool super_down;

    u32 text[KRAFT_MAX_TEXT_INPUT];
    u32 text_count;
};

struct KRAFT_API InputSystem {
    static InputSystemState State;
    static bool Initialized;

    static bool Init();
    static bool Shutdown();

    // Call once at the start of every frame, before the window polls.
    static void Update();

    // Called by Window after each poll with everything that happened since the previous one.
    static void ProcessEvents(const WindowEventQueue* queue);

    static void SetMousePosition(f64 x, f64 y);

    KRAFT_INLINE static bool IsKeyDown(Keys key) {
        return State.key_down[key];
    }

    KRAFT_INLINE static bool WasKeyDown(Keys key) {
        return State.key_down_previous[key];
    }

    KRAFT_INLINE static bool IsKeyPressed(Keys key) {
        return State.key_pressed[key];
    }

    KRAFT_INLINE static bool IsKeyReleased(Keys key) {
        return State.key_released[key];
    }

    KRAFT_INLINE static bool IsMouseButtonDown(MouseButtons button) {
        return State.mouse_down[button];
    }

    KRAFT_INLINE static bool WasMouseButtonDown(MouseButtons button) {
        return State.mouse_down_previous[button];
    }

    KRAFT_INLINE static bool IsMouseButtonPressed(MouseButtons button) {
        return State.mouse_pressed[button];
    }

    KRAFT_INLINE static bool IsMouseButtonReleased(MouseButtons button) {
        return State.mouse_released[button];
    }

    KRAFT_INLINE static MousePosition GetMousePosition() {
        return MousePosition{(int)State.mouse_position.x, (int)State.mouse_position.y};
    }

    KRAFT_INLINE static MousePosition GetPreviousMousePosition() {
        return MousePosition{(int)State.mouse_position_previous.x, (int)State.mouse_position_previous.y};
    }
};

} // namespace kraft
