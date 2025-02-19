#include "kraft_events.h"

#include <containers/kraft_carray.h>
#include <core/kraft_log.h>
#include <core/kraft_memory.h>

namespace kraft {

bool             EventSystem::Initialized = false;
EventSystemState EventSystem::State;

bool EventSystem::Init()
{
    if (Initialized)
    {
        KERROR("EventSystem already initialized!");
        return false;
    }

    MemZero(&State, sizeof(EventSystemState));
    Initialized = true;

    return true;
}

bool EventSystem::Shutdown()
{
    for (int i = 0; i < EventType::EVENT_TYPE_NUM_COUNT; i++)
    {
        Event* events = State.EventEntries[i].Events;
        arrfree(events);

        State.EventEntries[i].Events = nullptr;
    }

    return true;
}

bool EventSystem::Listen(EventType type, void* listener, EventCallback callback)
{
    Event* events = State.EventEntries[type].Events;

    // Check for duplicate events
    for (int i = 0; i < arrlen(events); ++i)
    {
        if (events[i].Listener == listener)
        {
            // TODO (amn): Fix this
            // KERROR("[EventSystem::Listen]: An event entry with the same listener already exists for event type %d", type);
            // return false;
        }
    }

    Event e;
    e.Listener = listener;
    e.Callback = callback;

    arrpush(events, e);
    State.EventEntries[type].Events = events;
    return true;
}

bool EventSystem::Unlisten(EventType type, void* listener, EventCallback callback)
{
    Event* events = State.EventEntries[type].Events;
    for (int i = 0; i < arrlen(events); i++)
    {
        Event event = events[i];
        if (event.Listener == listener && event.Callback == callback)
        {
            arrdel(events, i);
            return true;
        }
    }

    return false;
}

void EventSystem::Dispatch(EventType type, EventData data, void* sender)
{
    Event* events = State.EventEntries[type].Events;
    for (int i = 0; i < arrlen(events); ++i)
    {
        Event e = events[i];
        if (e.Callback(type, sender, e.Listener, data))
        {
            return;
        }
    }
}

}