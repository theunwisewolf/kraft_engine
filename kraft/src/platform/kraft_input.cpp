#include "kraft_input.h"

#include <core/kraft_events.h>
#include <core/kraft_log.h>
#include <core/kraft_memory.h>

namespace kraft {

bool InputSystem::Initialized = false;
InputSystemState InputSystem::State = {};

bool InputSystem::Init() {
    if (Initialized) {
        KERROR("[InputSystem::Init]: Already initialized!");
        return false;
    }

    MemZero(&State, sizeof(InputSystemState));
    Initialized = true;

    return true;
}

bool InputSystem::Shutdown() {
    Initialized = false;
    return true;
}

void InputSystem::Update() {
    MemCpy(State.key_down_previous, State.key_down, sizeof(State.key_down));
    MemZero(State.key_pressed, sizeof(State.key_pressed));
    MemZero(State.key_released, sizeof(State.key_released));

    MemCpy(State.mouse_down_previous, State.mouse_down, sizeof(State.mouse_down));
    MemZero(State.mouse_pressed, sizeof(State.mouse_pressed));
    MemZero(State.mouse_released, sizeof(State.mouse_released));

    State.mouse_position_previous = State.mouse_position;
    State.mouse_delta = Vec2f(0.0f, 0.0f);
    State.scroll = Vec2f(0.0f, 0.0f);
    State.text_count = 0;
}

void InputSystem::SetMousePosition(f64 x, f64 y) {
    State.mouse_position = Vec2f((f32)x, (f32)y);
    State.mouse_position_previous = State.mouse_position;
}

static void ApplyModifiers(u32 modifiers) {
    InputSystem::State.shift_down = (modifiers & INPUT_MODIFIER_SHIFT) != 0;
    InputSystem::State.ctrl_down = (modifiers & INPUT_MODIFIER_CONTROL) != 0;
    InputSystem::State.alt_down = (modifiers & INPUT_MODIFIER_ALT) != 0;
    InputSystem::State.super_down = (modifiers & INPUT_MODIFIER_SUPER) != 0;
}

static void ApplyKey(const WindowEvent* event) {
    u32 keycode = (u32)event->key;
    if (keycode >= KRAFT_KEY_COUNT) {
        return;
    }

    InputSystemState* state = &InputSystem::State;
    ApplyModifiers(event->modifiers);

    EventData data;
    data.Int32Value[0] = (i32)keycode;
    if (event->kind == WINDOW_EVENT_KEY_PRESS) {
        state->key_pressed[keycode] = true;
        if (!state->key_down[keycode]) {
            state->key_down[keycode] = true;
            EventSystem::Dispatch(EventType::EVENT_TYPE_KEY_DOWN, data, 0);
        }
    } else {
        state->key_released[keycode] = true;
        if (state->key_down[keycode]) {
            state->key_down[keycode] = false;
            EventSystem::Dispatch(EventType::EVENT_TYPE_KEY_UP, data, 0);
        }
    }
}

static void ApplyText(const WindowEvent* event) {
    InputSystemState* state = &InputSystem::State;
    if (state->text_count < KRAFT_MAX_TEXT_INPUT) {
        state->text[state->text_count++] = event->codepoint;
    }
}

static void ApplyMouseButton(const WindowEvent* event) {
    u32 button = (u32)event->button;
    if (button >= MOUSE_BUTTON_COUNT) {
        return;
    }

    InputSystemState* state = &InputSystem::State;
    ApplyModifiers(event->modifiers);
    bool pressed = event->kind == WINDOW_EVENT_MOUSE_PRESS;

    if (pressed)
        state->mouse_pressed[button] = true;
    else
        state->mouse_released[button] = true;

    if (state->mouse_down[button] != pressed) {
        state->mouse_down[button] = pressed;
        EventData data;
        data.Int32Value[0] = (i32)button;
        EventSystem::Dispatch(pressed ? EventType::EVENT_TYPE_MOUSE_DOWN : EventType::EVENT_TYPE_MOUSE_UP, data, 0);
    }

    if ((button == MOUSE_BUTTON_LEFT || button == MOUSE_BUTTON_RIGHT) && !pressed && state->dragging) {
        state->dragging = false;
        EventData drag_data;
        drag_data.Int32Value[0] = (i32)state->mouse_position.x;
        drag_data.Int32Value[1] = (i32)state->mouse_position.y;
        EventSystem::Dispatch(EventType::EVENT_TYPE_MOUSE_DRAG_END, drag_data, 0);
    }
}

static void ApplyMouseMove(const WindowEvent* event) {
    InputSystemState* state = &InputSystem::State;
    Vec2f new_position = event->cursor_position;
    if (new_position.x == state->mouse_position.x && new_position.y == state->mouse_position.y) {
        return;
    }

    EventData data;
    data.Int32Value[0] = (i32)new_position.x;
    data.Int32Value[1] = (i32)new_position.y;
    if (state->mouse_down[MOUSE_BUTTON_LEFT] || state->mouse_down[MOUSE_BUTTON_RIGHT]) {
        if (!state->dragging) {
            state->dragging = true;
            EventSystem::Dispatch(EventType::EVENT_TYPE_MOUSE_DRAG_START, data, 0);
        } else {
            EventSystem::Dispatch(EventType::EVENT_TYPE_MOUSE_DRAG_DRAGGING, data, 0);
        }
    }

    state->mouse_delta = state->mouse_delta + (new_position - state->mouse_position);
    state->mouse_position = new_position;
    EventSystem::Dispatch(EventType::EVENT_TYPE_MOUSE_MOVE, data, 0);
}

static void ApplyScroll(const WindowEvent* event) {
    InputSystemState* state = &InputSystem::State;
    state->scroll = state->scroll + event->scroll;

    EventData data;
    data.Float64Value[0] = (f64)event->scroll.x;
    data.Float64Value[1] = (f64)event->scroll.y;
    EventSystem::Dispatch(EventType::EVENT_TYPE_SCROLL, data, 0);
}

void InputSystem::ProcessEvents(const WindowEventQueue* queue) {
    for (u32 event_index = 0; event_index < queue->count; event_index++) {
        const WindowEvent* event = &queue->events[event_index];
        switch (event->kind) {
        case WINDOW_EVENT_KEY_PRESS:
        case WINDOW_EVENT_KEY_RELEASE:   ApplyKey(event); break;
        case WINDOW_EVENT_TEXT:          ApplyText(event); break;
        case WINDOW_EVENT_MOUSE_PRESS:
        case WINDOW_EVENT_MOUSE_RELEASE: ApplyMouseButton(event); break;
        case WINDOW_EVENT_MOUSE_MOVE:    ApplyMouseMove(event); break;
        case WINDOW_EVENT_SCROLL:        ApplyScroll(event); break;
        default:                         break;
        }
    }
}

} // namespace kraft
