#pragma once

#include "core/kraft_core.h"

#define KRAFT_DEFINE_KEYCODE(key, code) KEY_##key = code

namespace kraft
{

// Codes are same as in GLFW3
enum Keys
{
    /* Printable keys */
    KRAFT_DEFINE_KEYCODE(SPACE,             32),
    KRAFT_DEFINE_KEYCODE(APOSTROPHE,        39),  /* ' */
    KRAFT_DEFINE_KEYCODE(COMMA,             44),  /* , */
    KRAFT_DEFINE_KEYCODE(MINUS,             45),  /* - */
    KRAFT_DEFINE_KEYCODE(PERIOD,            46),  /* . */
    KRAFT_DEFINE_KEYCODE(SLASH,             47),  /* / */
    KRAFT_DEFINE_KEYCODE(0,                 48),
    KRAFT_DEFINE_KEYCODE(1,                 49),
    KRAFT_DEFINE_KEYCODE(2,                 50),
    KRAFT_DEFINE_KEYCODE(3,                 51),
    KRAFT_DEFINE_KEYCODE(4,                 52),
    KRAFT_DEFINE_KEYCODE(5,                 53),
    KRAFT_DEFINE_KEYCODE(6,                 54),
    KRAFT_DEFINE_KEYCODE(7,                 55),
    KRAFT_DEFINE_KEYCODE(8,                 56),
    KRAFT_DEFINE_KEYCODE(9,                 57),
    KRAFT_DEFINE_KEYCODE(SEMICOLON,         59),  /* , */
    KRAFT_DEFINE_KEYCODE(EQUAL,             61),  /* = */
    KRAFT_DEFINE_KEYCODE(A,                 65),
    KRAFT_DEFINE_KEYCODE(B,                 66),
    KRAFT_DEFINE_KEYCODE(C,                 67),
    KRAFT_DEFINE_KEYCODE(D,                 68),
    KRAFT_DEFINE_KEYCODE(E,                 69),
    KRAFT_DEFINE_KEYCODE(F,                 70),
    KRAFT_DEFINE_KEYCODE(G,                 71),
    KRAFT_DEFINE_KEYCODE(H,                 72),
    KRAFT_DEFINE_KEYCODE(I,                 73),
    KRAFT_DEFINE_KEYCODE(J,                 74),
    KRAFT_DEFINE_KEYCODE(K,                 75),
    KRAFT_DEFINE_KEYCODE(L,                 76),
    KRAFT_DEFINE_KEYCODE(M,                 77),
    KRAFT_DEFINE_KEYCODE(N,                 78),
    KRAFT_DEFINE_KEYCODE(O,                 79),
    KRAFT_DEFINE_KEYCODE(P,                 80),
    KRAFT_DEFINE_KEYCODE(Q,                 81),
    KRAFT_DEFINE_KEYCODE(R,                 82),
    KRAFT_DEFINE_KEYCODE(S,                 83),
    KRAFT_DEFINE_KEYCODE(T,                 84),
    KRAFT_DEFINE_KEYCODE(U,                 85),
    KRAFT_DEFINE_KEYCODE(V,                 86),
    KRAFT_DEFINE_KEYCODE(W,                 87),
    KRAFT_DEFINE_KEYCODE(X,                 88),
    KRAFT_DEFINE_KEYCODE(Y,                 89),
    KRAFT_DEFINE_KEYCODE(Z,                 90),
    KRAFT_DEFINE_KEYCODE(LEFT_BRACKET,      91),  /* [ */
    KRAFT_DEFINE_KEYCODE(BACKSLASH,         92),  /* \ */
    KRAFT_DEFINE_KEYCODE(RIGHT_BRACKET,     93),  /* ] */
    KRAFT_DEFINE_KEYCODE(GRAVE_ACCENT,      96),  /* ` */
    KRAFT_DEFINE_KEYCODE(WORLD_1,           161), /* non-US #1 */
    KRAFT_DEFINE_KEYCODE(WORLD_2,           162), /* non-US #2 */

    /* Function keys */
    KRAFT_DEFINE_KEYCODE(ESCAPE,            256),
    KRAFT_DEFINE_KEYCODE(ENTER,             257),
    KRAFT_DEFINE_KEYCODE(TAB,               258),
    KRAFT_DEFINE_KEYCODE(BACKSPACE,         259),
    KRAFT_DEFINE_KEYCODE(INSERT,            260),
    KRAFT_DEFINE_KEYCODE(DELETE,            261),
    KRAFT_DEFINE_KEYCODE(RIGHT,             262),
    KRAFT_DEFINE_KEYCODE(LEFT,              263),
    KRAFT_DEFINE_KEYCODE(DOWN,              264),
    KRAFT_DEFINE_KEYCODE(UP,                265),
    KRAFT_DEFINE_KEYCODE(PAGE_UP,           266),
    KRAFT_DEFINE_KEYCODE(PAGE_DOWN,         267),
    KRAFT_DEFINE_KEYCODE(HOME,              268),
    KRAFT_DEFINE_KEYCODE(END,               269),
    KRAFT_DEFINE_KEYCODE(CAPS_LOCK,         280),
    KRAFT_DEFINE_KEYCODE(SCROLL_LOCK,       281),
    KRAFT_DEFINE_KEYCODE(NUM_LOCK,          282),
    KRAFT_DEFINE_KEYCODE(PRINT_SCREEN,      283),
    KRAFT_DEFINE_KEYCODE(PAUSE,             284),
    KRAFT_DEFINE_KEYCODE(F1,                290),
    KRAFT_DEFINE_KEYCODE(F2,                291),
    KRAFT_DEFINE_KEYCODE(F3,                292),
    KRAFT_DEFINE_KEYCODE(F4,                293),
    KRAFT_DEFINE_KEYCODE(F5,                294),
    KRAFT_DEFINE_KEYCODE(F6,                295),
    KRAFT_DEFINE_KEYCODE(F7,                296),
    KRAFT_DEFINE_KEYCODE(F8,                297),
    KRAFT_DEFINE_KEYCODE(F9,                298),
    KRAFT_DEFINE_KEYCODE(F10,               299),
    KRAFT_DEFINE_KEYCODE(F11,               300),
    KRAFT_DEFINE_KEYCODE(F12,               301),
    KRAFT_DEFINE_KEYCODE(F13,               302),
    KRAFT_DEFINE_KEYCODE(F14,               303),
    KRAFT_DEFINE_KEYCODE(F15,               304),
    KRAFT_DEFINE_KEYCODE(F16,               305),
    KRAFT_DEFINE_KEYCODE(F17,               306),
    KRAFT_DEFINE_KEYCODE(F18,               307),
    KRAFT_DEFINE_KEYCODE(F19,               308),
    KRAFT_DEFINE_KEYCODE(F20,               309),
    KRAFT_DEFINE_KEYCODE(F21,               310),
    KRAFT_DEFINE_KEYCODE(F22,               311),
    KRAFT_DEFINE_KEYCODE(F23,               312),
    KRAFT_DEFINE_KEYCODE(F24,               313),
    KRAFT_DEFINE_KEYCODE(F25,               314),
    KRAFT_DEFINE_KEYCODE(KP_0,              320),
    KRAFT_DEFINE_KEYCODE(KP_1,              321),
    KRAFT_DEFINE_KEYCODE(KP_2,              322),
    KRAFT_DEFINE_KEYCODE(KP_3,              323),
    KRAFT_DEFINE_KEYCODE(KP_4,              324),
    KRAFT_DEFINE_KEYCODE(KP_5,              325),
    KRAFT_DEFINE_KEYCODE(KP_6,              326),
    KRAFT_DEFINE_KEYCODE(KP_7,              327),
    KRAFT_DEFINE_KEYCODE(KP_8,              328),
    KRAFT_DEFINE_KEYCODE(KP_9,              329),
    KRAFT_DEFINE_KEYCODE(KP_DECIMAL,        330),
    KRAFT_DEFINE_KEYCODE(KP_DIVIDE,         331),
    KRAFT_DEFINE_KEYCODE(KP_MULTIPLY,       332),
    KRAFT_DEFINE_KEYCODE(KP_SUBTRACT,       333),
    KRAFT_DEFINE_KEYCODE(KP_ADD,            334),
    KRAFT_DEFINE_KEYCODE(KP_ENTER,          335),
    KRAFT_DEFINE_KEYCODE(KP_EQUAL,          336),
    KRAFT_DEFINE_KEYCODE(LEFT_SHIFT,        340),
    KRAFT_DEFINE_KEYCODE(LEFT_CONTROL,      341),
    KRAFT_DEFINE_KEYCODE(LEFT_ALT,          342),
    KRAFT_DEFINE_KEYCODE(LEFT_SUPER,        343),
    KRAFT_DEFINE_KEYCODE(RIGHT_SHIFT,       344),
    KRAFT_DEFINE_KEYCODE(RIGHT_CONTROL,     345),
    KRAFT_DEFINE_KEYCODE(RIGHT_ALT,         346),
    KRAFT_DEFINE_KEYCODE(RIGHT_SUPER,       347),
    KRAFT_DEFINE_KEYCODE(MENU,              348),
};

enum MouseButtons
{
    MOUSE_BUTTON_1      = 0,
    MOUSE_BUTTON_2,
    MOUSE_BUTTON_3,
    MOUSE_BUTTON_4,
    MOUSE_BUTTON_5,
    MOUSE_BUTTON_6,
    MOUSE_BUTTON_7,
    MOUSE_BUTTON_8,
    MOUSE_BUTTON_LAST   = MOUSE_BUTTON_8,
    MOUSE_BUTTON_LEFT   = MOUSE_BUTTON_1,
    MOUSE_BUTTON_RIGHT  = MOUSE_BUTTON_2,
    MOUSE_BUTTON_MIDDLE = MOUSE_BUTTON_3,
};

struct KeyboardState
{
    bool Keys[512];
};

struct MousePosition
{
    int x;
    int y;
};

struct MouseState
{
    MousePosition Position;
    bool Buttons[16];
};

struct InputSystemState
{
    KeyboardState PreviousKeyboardState;
    KeyboardState CurrentKeyboardState;
    MouseState PreviousMouseState;
    MouseState CurrentMouseState;
};

struct KRAFT_API InputSystem
{
    static InputSystemState State;
    static bool Initialized;

    static bool Init();
    static bool Shutdown();
    static void Update(float64 dt);
    static void ProcessKeyboard(int keycode, bool pressed);
    static void ProcessMouseButton(int button, bool pressed);
    static void ProcessMouseMove(int x, int y);
    static void ProcessScroll(float64 x, float64 y);

    KRAFT_INLINE static bool IsKeyDown(Keys key)
    {
        return State.CurrentKeyboardState.Keys[key];
    }

    KRAFT_INLINE static bool WasKeyDown(Keys key)
    {
        return State.PreviousKeyboardState.Keys[key];
    }

    KRAFT_INLINE static bool IsMouseButtonDown(MouseButtons button)
    {
        return State.CurrentMouseState.Buttons[button];
    }

    KRAFT_INLINE static bool WasMouseButtonDown(MouseButtons button)
    {
        return State.PreviousMouseState.Buttons[button];
    }

    KRAFT_INLINE static MousePosition GetMousePosition()
    {
        return State.CurrentMouseState.Position;
    }

    KRAFT_INLINE static MousePosition GetPreviousMousePosition()
    {
        return State.PreviousMouseState.Position;
    }
};

}