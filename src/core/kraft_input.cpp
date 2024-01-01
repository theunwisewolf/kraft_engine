#include "kraft_input.h"

#include "core/kraft_log.h"
#include "core/kraft_memory.h"
#include "core/kraft_events.h"

namespace kraft
{

bool                InputSystem::Initialized = false;
InputSystemState    InputSystem::State;

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
    return true;
}

void InputSystem::Update(float64 dt)
{
    MemCpy(&State.PreviousKeyboardState, &State.CurrentKeyboardState, sizeof(KeyboardState));
    MemCpy(&State.PreviousMouseState, &State.CurrentMouseState, sizeof(MouseState));
}

void InputSystem::ProcessKeyboard(int keycode, bool pressed)
{
    if (State.CurrentKeyboardState.Keys[keycode] != pressed)
    {
        State.CurrentKeyboardState.Keys[keycode] = pressed;

        EventData data;
        data.Int32[0] = keycode;
        EventSystem::Dispatch(pressed ? EventType::EVENT_TYPE_KEY_DOWN : EventType::EVENT_TYPE_KEY_UP, data, 0);
    }
}

void InputSystem::ProcessMouseButton(int button, bool pressed)
{
    if (State.CurrentMouseState.Buttons[button] != pressed)
    {
        State.CurrentMouseState.Buttons[button] = pressed;

        EventData data;
        data.Int32[0] = button;
        EventSystem::Dispatch(pressed ? EventType::EVENT_TYPE_MOUSE_DOWN : EventType::EVENT_TYPE_MOUSE_UP, data, 0);
    }
}

void InputSystem::ProcessMouseMove(int x, int y)
{
    if (State.CurrentMouseState.Position.x != x || State.CurrentMouseState.Position.y != y)
    {
        State.CurrentMouseState.Position.x = x;
        State.CurrentMouseState.Position.y = y;

        EventData data;
        data.Int32[0] = x;
        data.Int32[1] = y;
        EventSystem::Dispatch(EventType::EVENT_TYPE_MOUSE_MOVE, data, 0);
    }
}

void InputSystem::ProcessScroll(float64 x, float64 y)
{
    EventData data;
    data.Float64[0] = x;
    data.Float64[1] = y;
    EventSystem::Dispatch(EventType::EVENT_TYPE_SCROLL, data, 0);
}


}