#pragma once

#include <core/kraft_core.h>
#include <core/kraft_math.h>
#include <core/kraft_memory.h>
#include <platform/kraft_keys.h>

#define KRAFT_MAX_WINDOW_EVENTS 256
#define KRAFT_MAX_DROP_PATHS 16
#define KRAFT_DROP_PATH_STORAGE_SIZE 4096

namespace kraft {

enum WindowEventKind {
    WINDOW_EVENT_NONE,
    WINDOW_EVENT_KEY_PRESS,
    WINDOW_EVENT_KEY_RELEASE,
    WINDOW_EVENT_TEXT,
    WINDOW_EVENT_MOUSE_PRESS,
    WINDOW_EVENT_MOUSE_RELEASE,
    WINDOW_EVENT_MOUSE_MOVE,
    WINDOW_EVENT_SCROLL,
    WINDOW_EVENT_RESIZE,
    WINDOW_EVENT_MAXIMIZE,
    WINDOW_EVENT_FRAMEBUFFER_RESIZE,
    WINDOW_EVENT_DROP,
};

struct WindowEvent {
    WindowEventKind kind;
    Keys key;
    MouseButtons button;
    u32 codepoint;
    u32 modifiers;
    bool repeat;
    bool handled;
    Vec2f cursor_position;
    Vec2f scroll;
    i32 width;
    i32 height;
    bool maximized;
    String8* paths;
    u32 path_count;
};

// Everything a window reported since its last poll, in order. Cleared when the next poll begins.
struct WindowEventQueue {
    WindowEvent events[KRAFT_MAX_WINDOW_EVENTS];
    u32 count;

    Vec2f cursor_position;
    u32 modifiers;

    String8 drop_paths[KRAFT_MAX_DROP_PATHS];
    u32 drop_path_count;
    u8 drop_path_storage[KRAFT_DROP_PATH_STORAGE_SIZE];
    u32 drop_path_storage_used;

    KRAFT_INLINE void Clear() {
        count = 0;
        drop_path_count = 0;
        drop_path_storage_used = 0;
    }

    KRAFT_INLINE WindowEvent* Push(WindowEventKind kind) {
        if (count >= KRAFT_MAX_WINDOW_EVENTS)
            return nullptr;

        WindowEvent* event = &events[count++];
        MemZero(event, sizeof(WindowEvent));
        event->kind = kind;
        event->cursor_position = cursor_position;
        event->modifiers = modifiers;

        return event;
    }

    WindowEvent* PushDrop(int path_count, const char** paths);
};

} // namespace kraft
