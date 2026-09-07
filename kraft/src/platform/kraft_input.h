#pragma once

#include <core/kraft_core.h>
#include <core/kraft_math.h>

#define KRAFT_DEFINE_KEYCODE(key, code) KEY_##key = code

namespace kraft {

// Codes are same as in GLFW3
enum Keys : u32
{
    /* Printable keys */
    KRAFT_DEFINE_KEYCODE(SPACE, 32),
    KRAFT_DEFINE_KEYCODE(APOSTROPHE, 39), /* ' */
    KRAFT_DEFINE_KEYCODE(COMMA, 44),      /* , */
    KRAFT_DEFINE_KEYCODE(MINUS, 45),      /* - */
    KRAFT_DEFINE_KEYCODE(PERIOD, 46),     /* . */
    KRAFT_DEFINE_KEYCODE(SLASH, 47),      /* / */
    KRAFT_DEFINE_KEYCODE(0, 48),
    KRAFT_DEFINE_KEYCODE(1, 49),
    KRAFT_DEFINE_KEYCODE(2, 50),
    KRAFT_DEFINE_KEYCODE(3, 51),
    KRAFT_DEFINE_KEYCODE(4, 52),
    KRAFT_DEFINE_KEYCODE(5, 53),
    KRAFT_DEFINE_KEYCODE(6, 54),
    KRAFT_DEFINE_KEYCODE(7, 55),
    KRAFT_DEFINE_KEYCODE(8, 56),
    KRAFT_DEFINE_KEYCODE(9, 57),
    KRAFT_DEFINE_KEYCODE(SEMICOLON, 59), /* , */
    KRAFT_DEFINE_KEYCODE(EQUAL, 61),     /* = */
    KRAFT_DEFINE_KEYCODE(A, 65),
    KRAFT_DEFINE_KEYCODE(B, 66),
    KRAFT_DEFINE_KEYCODE(C, 67),
    KRAFT_DEFINE_KEYCODE(D, 68),
    KRAFT_DEFINE_KEYCODE(E, 69),
    KRAFT_DEFINE_KEYCODE(F, 70),
    KRAFT_DEFINE_KEYCODE(G, 71),
    KRAFT_DEFINE_KEYCODE(H, 72),
    KRAFT_DEFINE_KEYCODE(I, 73),
    KRAFT_DEFINE_KEYCODE(J, 74),
    KRAFT_DEFINE_KEYCODE(K, 75),
    KRAFT_DEFINE_KEYCODE(L, 76),
    KRAFT_DEFINE_KEYCODE(M, 77),
    KRAFT_DEFINE_KEYCODE(N, 78),
    KRAFT_DEFINE_KEYCODE(O, 79),
    KRAFT_DEFINE_KEYCODE(P, 80),
    KRAFT_DEFINE_KEYCODE(Q, 81),
    KRAFT_DEFINE_KEYCODE(R, 82),
    KRAFT_DEFINE_KEYCODE(S, 83),
    KRAFT_DEFINE_KEYCODE(T, 84),
    KRAFT_DEFINE_KEYCODE(U, 85),
    KRAFT_DEFINE_KEYCODE(V, 86),
    KRAFT_DEFINE_KEYCODE(W, 87),
    KRAFT_DEFINE_KEYCODE(X, 88),
    KRAFT_DEFINE_KEYCODE(Y, 89),
    KRAFT_DEFINE_KEYCODE(Z, 90),
    KRAFT_DEFINE_KEYCODE(LEFT_BRACKET, 91),  /* [ */
    KRAFT_DEFINE_KEYCODE(BACKSLASH, 92),     /* \ */
    KRAFT_DEFINE_KEYCODE(RIGHT_BRACKET, 93), /* ] */
    KRAFT_DEFINE_KEYCODE(GRAVE_ACCENT, 96),  /* ` */
    KRAFT_DEFINE_KEYCODE(WORLD_1, 161),      /* non-US #1 */
    KRAFT_DEFINE_KEYCODE(WORLD_2, 162),      /* non-US #2 */

    /* Function keys */
    KRAFT_DEFINE_KEYCODE(ESCAPE, 256),
    KRAFT_DEFINE_KEYCODE(ENTER, 257),
    KRAFT_DEFINE_KEYCODE(TAB, 258),
    KRAFT_DEFINE_KEYCODE(BACKSPACE, 259),
    KRAFT_DEFINE_KEYCODE(INSERT, 260),
    KRAFT_DEFINE_KEYCODE(DELETE, 261),
    KRAFT_DEFINE_KEYCODE(RIGHT, 262),
    KRAFT_DEFINE_KEYCODE(LEFT, 263),
    KRAFT_DEFINE_KEYCODE(DOWN, 264),
    KRAFT_DEFINE_KEYCODE(UP, 265),
    KRAFT_DEFINE_KEYCODE(PAGE_UP, 266),
    KRAFT_DEFINE_KEYCODE(PAGE_DOWN, 267),
    KRAFT_DEFINE_KEYCODE(HOME, 268),
    KRAFT_DEFINE_KEYCODE(END, 269),
    KRAFT_DEFINE_KEYCODE(CAPS_LOCK, 280),
    KRAFT_DEFINE_KEYCODE(SCROLL_LOCK, 281),
    KRAFT_DEFINE_KEYCODE(NUM_LOCK, 282),
    KRAFT_DEFINE_KEYCODE(PRINT_SCREEN, 283),
    KRAFT_DEFINE_KEYCODE(PAUSE, 284),
    KRAFT_DEFINE_KEYCODE(F1, 290),
    KRAFT_DEFINE_KEYCODE(F2, 291),
    KRAFT_DEFINE_KEYCODE(F3, 292),
    KRAFT_DEFINE_KEYCODE(F4, 293),
    KRAFT_DEFINE_KEYCODE(F5, 294),
    KRAFT_DEFINE_KEYCODE(F6, 295),
    KRAFT_DEFINE_KEYCODE(F7, 296),
    KRAFT_DEFINE_KEYCODE(F8, 297),
    KRAFT_DEFINE_KEYCODE(F9, 298),
    KRAFT_DEFINE_KEYCODE(F10, 299),
    KRAFT_DEFINE_KEYCODE(F11, 300),
    KRAFT_DEFINE_KEYCODE(F12, 301),
    KRAFT_DEFINE_KEYCODE(F13, 302),
    KRAFT_DEFINE_KEYCODE(F14, 303),
    KRAFT_DEFINE_KEYCODE(F15, 304),
    KRAFT_DEFINE_KEYCODE(F16, 305),
    KRAFT_DEFINE_KEYCODE(F17, 306),
    KRAFT_DEFINE_KEYCODE(F18, 307),
    KRAFT_DEFINE_KEYCODE(F19, 308),
    KRAFT_DEFINE_KEYCODE(F20, 309),
    KRAFT_DEFINE_KEYCODE(F21, 310),
    KRAFT_DEFINE_KEYCODE(F22, 311),
    KRAFT_DEFINE_KEYCODE(F23, 312),
    KRAFT_DEFINE_KEYCODE(F24, 313),
    KRAFT_DEFINE_KEYCODE(F25, 314),
    KRAFT_DEFINE_KEYCODE(KP_0, 320),
    KRAFT_DEFINE_KEYCODE(KP_1, 321),
    KRAFT_DEFINE_KEYCODE(KP_2, 322),
    KRAFT_DEFINE_KEYCODE(KP_3, 323),
    KRAFT_DEFINE_KEYCODE(KP_4, 324),
    KRAFT_DEFINE_KEYCODE(KP_5, 325),
    KRAFT_DEFINE_KEYCODE(KP_6, 326),
    KRAFT_DEFINE_KEYCODE(KP_7, 327),
    KRAFT_DEFINE_KEYCODE(KP_8, 328),
    KRAFT_DEFINE_KEYCODE(KP_9, 329),
    KRAFT_DEFINE_KEYCODE(KP_DECIMAL, 330),
    KRAFT_DEFINE_KEYCODE(KP_DIVIDE, 331),
    KRAFT_DEFINE_KEYCODE(KP_MULTIPLY, 332),
    KRAFT_DEFINE_KEYCODE(KP_SUBTRACT, 333),
    KRAFT_DEFINE_KEYCODE(KP_ADD, 334),
    KRAFT_DEFINE_KEYCODE(KP_ENTER, 335),
    KRAFT_DEFINE_KEYCODE(KP_EQUAL, 336),
    KRAFT_DEFINE_KEYCODE(LEFT_SHIFT, 340),
    KRAFT_DEFINE_KEYCODE(LEFT_CONTROL, 341),
    KRAFT_DEFINE_KEYCODE(LEFT_ALT, 342),
    KRAFT_DEFINE_KEYCODE(LEFT_SUPER, 343),
    KRAFT_DEFINE_KEYCODE(RIGHT_SHIFT, 344),
    KRAFT_DEFINE_KEYCODE(RIGHT_CONTROL, 345),
    KRAFT_DEFINE_KEYCODE(RIGHT_ALT, 346),
    KRAFT_DEFINE_KEYCODE(RIGHT_SUPER, 347),
    KRAFT_DEFINE_KEYCODE(MENU, 348),
};

enum MouseButtons
{
    MOUSE_BUTTON_1 = 0,
    MOUSE_BUTTON_2,
    MOUSE_BUTTON_3,
    MOUSE_BUTTON_4,
    MOUSE_BUTTON_5,
    MOUSE_BUTTON_6,
    MOUSE_BUTTON_7,
    MOUSE_BUTTON_8,
    MOUSE_BUTTON_COUNT,
    MOUSE_BUTTON_LAST = MOUSE_BUTTON_8,
    MOUSE_BUTTON_LEFT = MOUSE_BUTTON_1,
    MOUSE_BUTTON_RIGHT = MOUSE_BUTTON_2,
    MOUSE_BUTTON_MIDDLE = MOUSE_BUTTON_3,
};

#define KRAFT_KEY_COUNT         512
#define KRAFT_MAX_INPUT_EVENTS  256
#define KRAFT_MAX_TEXT_INPUT    64
#define KRAFT_MAX_DROP_PATHS    16
#define KRAFT_DROP_PATH_STORAGE 4096

enum InputEventKind
{
    INPUT_EVENT_NONE,
    INPUT_EVENT_KEY_PRESS,
    INPUT_EVENT_KEY_RELEASE,
    INPUT_EVENT_TEXT,
    INPUT_EVENT_MOUSE_PRESS,
    INPUT_EVENT_MOUSE_RELEASE,
    INPUT_EVENT_MOUSE_MOVE,
    INPUT_EVENT_SCROLL,
    INPUT_EVENT_WINDOW_RESIZE,
    INPUT_EVENT_WINDOW_MAXIMIZE,
    INPUT_EVENT_FRAMEBUFFER_RESIZE,
    INPUT_EVENT_DROP,
};

// Same bit values as GLFW_MOD_*
enum InputModifiers : u32
{
    INPUT_MODIFIER_SHIFT = 0x0001,
    INPUT_MODIFIER_CONTROL = 0x0002,
    INPUT_MODIFIER_ALT = 0x0004,
    INPUT_MODIFIER_SUPER = 0x0008,
};

struct InputEvent
{
    InputEventKind kind;
    Keys           key;
    MouseButtons   button;
    u32            codepoint;
    u32            modifiers;
    bool           repeat;
    bool           handled;
    Vec2f          mouse_position;
    Vec2f          scroll;
    i32            width;
    i32            height;
    bool           maximized;
    String8*       paths;
    u32            path_count;
};

struct MousePosition
{
    int x;
    int y;
};

struct InputSystemState
{
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
    bool  dragging;

    bool shift_down;
    bool ctrl_down;
    bool alt_down;
    bool super_down;

    u32 text[KRAFT_MAX_TEXT_INPUT];
    u32 text_count;

    String8 drop_paths[KRAFT_MAX_DROP_PATHS];
    u32     drop_path_count;
    u8      drop_path_storage[KRAFT_DROP_PATH_STORAGE];
    u32     drop_path_storage_used;

    InputEvent events[KRAFT_MAX_INPUT_EVENTS];
    u32        event_count;
};

struct KRAFT_API InputSystem
{
    static InputSystemState State;
    static bool             Initialized;

    static bool Init();
    static bool Shutdown();

    // Call once at the start of every frame, before polling the window.
    static void Update();

    static void ProcessKey(int keycode, bool pressed, bool repeat, u32 modifiers);
    static void ProcessKeyboard(int keycode, bool pressed);
    static void ProcessText(u32 codepoint);
    static void ProcessMouseButton(int button, bool pressed, u32 modifiers);
    static void ProcessMouseButton(int button, bool pressed);
    static void ProcessMouseMove(f64 x, f64 y);
    static void ProcessMouseMove(int x, int y);
    static void SetMousePosition(f64 x, f64 y);
    static void ProcessScroll(f64 x, f64 y);
    static void ProcessWindowResize(i32 width, i32 height, bool maximized);
    static void ProcessFramebufferResize(i32 width, i32 height);
    static void ProcessDrop(int count, const char** paths);

    static InputEvent* PushEvent(InputEventKind kind);
    static u32         CurrentModifiers();

    KRAFT_INLINE static bool IsKeyDown(Keys key)
    {
        return State.key_down[key];
    }

    KRAFT_INLINE static bool WasKeyDown(Keys key)
    {
        return State.key_down_previous[key];
    }

    KRAFT_INLINE static bool IsKeyPressed(Keys key)
    {
        return State.key_pressed[key];
    }

    KRAFT_INLINE static bool IsKeyReleased(Keys key)
    {
        return State.key_released[key];
    }

    KRAFT_INLINE static bool IsMouseButtonDown(MouseButtons button)
    {
        return State.mouse_down[button];
    }

    KRAFT_INLINE static bool WasMouseButtonDown(MouseButtons button)
    {
        return State.mouse_down_previous[button];
    }

    KRAFT_INLINE static bool IsMouseButtonPressed(MouseButtons button)
    {
        return State.mouse_pressed[button];
    }

    KRAFT_INLINE static bool IsMouseButtonReleased(MouseButtons button)
    {
        return State.mouse_released[button];
    }

    KRAFT_INLINE static MousePosition GetMousePosition()
    {
        return MousePosition{ (int)State.mouse_position.x, (int)State.mouse_position.y };
    }

    KRAFT_INLINE static MousePosition GetPreviousMousePosition()
    {
        return MousePosition{ (int)State.mouse_position_previous.x, (int)State.mouse_position_previous.y };
    }
};

} // namespace kraft
