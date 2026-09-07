#include "kraft_input.h"

#include <core/kraft_events.h>
#include <core/kraft_log.h>
#include <core/kraft_memory.h>
#include <core/kraft_strings.h>

namespace kraft {

bool             InputSystem::Initialized = false;
InputSystemState InputSystem::State = {};

bool InputSystem::Init()
{
    if (Initialized)
    {
        KERROR("[InputSystem::Init]: Already initialized!");
        return false;
    }

    MemZero(&State, sizeof(InputSystemState));
    Initialized = true;

    return true;
}

bool InputSystem::Shutdown()
{
    Initialized = false;
    return true;
}

void InputSystem::Update()
{
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
    State.drop_path_count = 0;
    State.drop_path_storage_used = 0;
    State.event_count = 0;
}

u32 InputSystem::CurrentModifiers()
{
    u32 modifiers = 0;
    if (State.shift_down) modifiers |= INPUT_MODIFIER_SHIFT;
    if (State.ctrl_down) modifiers |= INPUT_MODIFIER_CONTROL;
    if (State.alt_down) modifiers |= INPUT_MODIFIER_ALT;
    if (State.super_down) modifiers |= INPUT_MODIFIER_SUPER;

    return modifiers;
}

InputEvent* InputSystem::PushEvent(InputEventKind kind)
{
    if (State.event_count >= KRAFT_MAX_INPUT_EVENTS)
        return nullptr;

    InputEvent* event = &State.events[State.event_count++];
    MemZero(event, sizeof(InputEvent));
    event->kind = kind;
    event->mouse_position = State.mouse_position;
    event->modifiers = CurrentModifiers();

    return event;
}

void InputSystem::ProcessKey(int keycode, bool pressed, bool repeat, u32 modifiers)
{
    if (keycode < 0 || keycode >= KRAFT_KEY_COUNT)
        return;

    State.shift_down = (modifiers & INPUT_MODIFIER_SHIFT) != 0;
    State.ctrl_down = (modifiers & INPUT_MODIFIER_CONTROL) != 0;
    State.alt_down = (modifiers & INPUT_MODIFIER_ALT) != 0;
    State.super_down = (modifiers & INPUT_MODIFIER_SUPER) != 0;

    EventData data;
    data.Int32Value[0] = keycode;
    if (pressed)
    {
        State.key_pressed[keycode] = true;
        if (!State.key_down[keycode])
        {
            State.key_down[keycode] = true;
            EventSystem::Dispatch(EventType::EVENT_TYPE_KEY_DOWN, data, 0);
        }
    }
    else
    {
        State.key_released[keycode] = true;
        if (State.key_down[keycode])
        {
            State.key_down[keycode] = false;
            EventSystem::Dispatch(EventType::EVENT_TYPE_KEY_UP, data, 0);
        }
    }

    InputEvent* event = PushEvent(pressed ? INPUT_EVENT_KEY_PRESS : INPUT_EVENT_KEY_RELEASE);
    if (event)
    {
        event->key = (Keys)keycode;
        event->repeat = repeat;
        event->modifiers = modifiers;
    }
}

void InputSystem::ProcessKeyboard(int keycode, bool pressed)
{
    ProcessKey(keycode, pressed, false, CurrentModifiers());
}

void InputSystem::ProcessText(u32 codepoint)
{
    if (State.text_count < KRAFT_MAX_TEXT_INPUT)
        State.text[State.text_count++] = codepoint;

    InputEvent* event = PushEvent(INPUT_EVENT_TEXT);
    if (event)
        event->codepoint = codepoint;
}

void InputSystem::ProcessMouseButton(int button, bool pressed, u32 modifiers)
{
    if (button < 0 || button >= MOUSE_BUTTON_COUNT)
        return;

    if (pressed)
        State.mouse_pressed[button] = true;
    else
        State.mouse_released[button] = true;

    if (State.mouse_down[button] != pressed)
    {
        State.mouse_down[button] = pressed;
        EventData data;
        data.Int32Value[0] = button;
        EventSystem::Dispatch(pressed ? EventType::EVENT_TYPE_MOUSE_DOWN : EventType::EVENT_TYPE_MOUSE_UP, data, 0);
    }

    if ((button == MOUSE_BUTTON_LEFT || button == MOUSE_BUTTON_RIGHT) && !pressed && State.dragging)
    {
        State.dragging = false;
        EventData drag_data;
        drag_data.Int32Value[0] = (i32)State.mouse_position.x;
        drag_data.Int32Value[1] = (i32)State.mouse_position.y;
        EventSystem::Dispatch(EventType::EVENT_TYPE_MOUSE_DRAG_END, drag_data, 0);
    }

    InputEvent* event = PushEvent(pressed ? INPUT_EVENT_MOUSE_PRESS : INPUT_EVENT_MOUSE_RELEASE);
    if (event)
    {
        event->button = (MouseButtons)button;
        event->modifiers = modifiers;
    }
}

void InputSystem::ProcessMouseButton(int button, bool pressed)
{
    ProcessMouseButton(button, pressed, CurrentModifiers());
}

void InputSystem::ProcessMouseMove(f64 x, f64 y)
{
    Vec2f new_position((f32)x, (f32)y);
    if (new_position.x == State.mouse_position.x && new_position.y == State.mouse_position.y)
        return;

    EventData data;
    data.Int32Value[0] = (i32)x;
    data.Int32Value[1] = (i32)y;
    if (State.mouse_down[MOUSE_BUTTON_LEFT] || State.mouse_down[MOUSE_BUTTON_RIGHT])
    {
        if (!State.dragging)
        {
            State.dragging = true;
            EventSystem::Dispatch(EventType::EVENT_TYPE_MOUSE_DRAG_START, data, 0);
        }
        else
        {
            EventSystem::Dispatch(EventType::EVENT_TYPE_MOUSE_DRAG_DRAGGING, data, 0);
        }
    }

    State.mouse_delta = State.mouse_delta + (new_position - State.mouse_position);
    State.mouse_position = new_position;
    EventSystem::Dispatch(EventType::EVENT_TYPE_MOUSE_MOVE, data, 0);

    PushEvent(INPUT_EVENT_MOUSE_MOVE);
}

void InputSystem::ProcessMouseMove(int x, int y)
{
    ProcessMouseMove((f64)x, (f64)y);
}

void InputSystem::SetMousePosition(f64 x, f64 y)
{
    State.mouse_position = Vec2f((f32)x, (f32)y);
    State.mouse_position_previous = State.mouse_position;
}

void InputSystem::ProcessScroll(f64 x, f64 y)
{
    State.scroll = State.scroll + Vec2f((f32)x, (f32)y);

    EventData data;
    data.Float64Value[0] = x;
    data.Float64Value[1] = y;
    EventSystem::Dispatch(EventType::EVENT_TYPE_SCROLL, data, 0);

    InputEvent* event = PushEvent(INPUT_EVENT_SCROLL);
    if (event)
        event->scroll = Vec2f((f32)x, (f32)y);
}

void InputSystem::ProcessWindowResize(i32 width, i32 height, bool maximized)
{
    InputEvent* event = PushEvent(maximized ? INPUT_EVENT_WINDOW_MAXIMIZE : INPUT_EVENT_WINDOW_RESIZE);
    if (event)
    {
        event->width = width;
        event->height = height;
        event->maximized = maximized;
    }
}

void InputSystem::ProcessFramebufferResize(i32 width, i32 height)
{
    InputEvent* event = PushEvent(INPUT_EVENT_FRAMEBUFFER_RESIZE);
    if (event)
    {
        event->width = width;
        event->height = height;
    }
}

void InputSystem::ProcessDrop(int count, const char** paths)
{
    State.drop_path_count = 0;
    State.drop_path_storage_used = 0;
    for (int path_index = 0; path_index < count && State.drop_path_count < KRAFT_MAX_DROP_PATHS; path_index++)
    {
        u64 length = CStringLength((u8*)paths[path_index]);
        if (State.drop_path_storage_used + length + 1 > KRAFT_DROP_PATH_STORAGE)
            break;

        u8* destination = State.drop_path_storage + State.drop_path_storage_used;
        MemCpy(destination, paths[path_index], length);
        destination[length] = 0;
        State.drop_paths[State.drop_path_count++] = String8{ { destination }, length };
        State.drop_path_storage_used += (u32)(length + 1);
    }

    InputEvent* event = PushEvent(INPUT_EVENT_DROP);
    if (event)
    {
        event->paths = State.drop_paths;
        event->path_count = State.drop_path_count;
    }
}

} // namespace kraft
