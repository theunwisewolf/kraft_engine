#pragma once

#include <containers/kraft_chunked_array.h>

namespace kraft {

struct ArenaAllocator;

enum EventType
{
    EVENT_TYPE_WINDOW_RESIZE,
    EVENT_TYPE_WINDOW_MAXIMIZE,
    EVENT_TYPE_KEY_UP,
    EVENT_TYPE_KEY_DOWN,
    EVENT_TYPE_MOUSE_UP,
    EVENT_TYPE_MOUSE_DOWN,
    EVENT_TYPE_MOUSE_MOVE,
    EVENT_TYPE_MOUSE_DRAG_START,
    EVENT_TYPE_MOUSE_DRAG_DRAGGING,
    EVENT_TYPE_MOUSE_DRAG_END,
    EVENT_TYPE_SCROLL,
    EVENT_TYPE_WINDOW_DRAG_DROP,

    EVENT_TYPE_NUM_COUNT
};

struct EventDataResize
{
    u32  width;
    u32  height;
    bool maximized;
};

struct EventData
{
    union
    {
        char CharValue[16];
        i8   Int8Value[16];
        i16  Int16Value[8];
        i32  Int32Value[4];
        i64  Int64Value[2];

        u8  UInt8Value[16];
        u16 UInt16Value[8];
        u32 UInt32Value[4];
        u64 UInt64Value[2];

        f32 Float32Value[4];
        f64 Float64Value[2];
    };
};

typedef bool (*EventCallback)(EventType type, void* sender, void* listener, EventData data);

struct EventListener
{
    void*         listener;
    EventCallback callback;
};

#define KRAFT_EVENT_LISTENERS_PER_CHUNK 16

struct EventEntry
{
    ChunkedArray<EventListener, KRAFT_EVENT_LISTENERS_PER_CHUNK> listeners;
};

struct EventSystemState
{
    ArenaAllocator* arena;
    EventEntry      event_entries[EventType::EVENT_TYPE_NUM_COUNT];
};

struct KRAFT_API EventSystem
{
    static EventSystemState state;
    static bool             initialized;

    static bool Init(ArenaAllocator* arena);
    static bool Shutdown();
    static bool Listen(EventType type, void* listener, EventCallback callback);
    static bool Unlisten(EventType type, void* listener, EventCallback callback);
    static void Dispatch(EventType type, EventData data, void* sender);
};

} // namespace kraft
