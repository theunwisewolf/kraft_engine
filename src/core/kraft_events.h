#pragma once

#include "core/kraft_core.h"

// inspiration from the kohi engine

namespace kraft
{

// Different event types
enum EventType
{
    EVENT_TYPE_WINDOW_RESIZE,
    EVENT_TYPE_KEY_UP,
    EVENT_TYPE_KEY_DOWN,
    EVENT_TYPE_MOUSE_UP,
    EVENT_TYPE_MOUSE_DOWN,
    EVENT_TYPE_MOUSE_MOVE,
    EVENT_TYPE_SCROLL,
    EVENT_TYPE_WINDOW_DRAG_DROP,

    EVENT_TYPE_NUM_COUNT
};

// Data associated with any event
struct EventData
{
    // a 128 bit struct
    union
    {
        char    Char[16];
        int8    Int8[16];
        int16   Int16[8];
        int32   Int32[4];
        int64   Int64[2];

        uint8   UInt8[16];
        uint16  UInt16[8];
        uint32  UInt32[4];
        uint64  UInt64[2];

        float32 Float32[4];
        float64 Float64[2];
    };
};

// Adding event type here can be useful because we might want to have a single
// listener that is handling multiple event types
typedef bool (*EventCallback)(EventType type, void* sender, void* listener, EventData data);

struct Event
{
    void*           Listener;
    EventCallback   Callback;
};

struct EventEntry
{
    Event* Events;
};

struct EventSystemState
{
    EventEntry EventEntries[EventType::EVENT_TYPE_NUM_COUNT];
};

struct KRAFT_API EventSystem
{
    static EventSystemState State;
    static bool Initialized;

    static bool Init();
    static bool Shutdown();
    static bool Listen(EventType type, void* listener, EventCallback callback);
    static bool Unlisten(EventType type, void* listener, EventCallback callback);
    static void Dispatch(EventType type, EventData data, void* sender);
};

}