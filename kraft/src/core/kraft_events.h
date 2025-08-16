#pragma once

#include "core/kraft_core.h"

// inspiration from the kohi engine

namespace kraft {

// Different event types
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
    uint32 width;
    uint32 height;
    bool   maximized;
};

// Data associated with any event
struct EventData
{
    // a 128 bit struct
    union
    {
        char  CharValue[16];
        int8  Int8Value[16];
        int16 Int16Value[8];
        int32 Int32Value[4];
        int64 Int64Value[2];

        uint8  UInt8Value[16];
        uint16 UInt16Value[8];
        uint32 UInt32Value[4];
        uint64 UInt64Value[2];

        float32 Float32Value[4];
        float64 Float64Value[2];
    };
};

// Adding event type here can be useful because we might want to have a single
// listener that is handling multiple event types
typedef bool (*EventCallback)(EventType type, void* sender, void* listener, EventData data);

struct EventListener
{
    void*         Listener;
    EventCallback Callback;
};

struct EventEntry
{
    EventListener* Events;
};

struct EventSystemState
{
    EventEntry EventEntries[EventType::EVENT_TYPE_NUM_COUNT];
};

struct KRAFT_API EventSystem
{
    static EventSystemState State;
    static bool             Initialized;

    static bool Init();
    static bool Shutdown();
    static bool Listen(EventType type, void* listener, EventCallback callback);
    static bool Unlisten(EventType type, void* listener, EventCallback callback);
    static void Dispatch(EventType type, EventData data, void* sender);
};

} // namespace kraft