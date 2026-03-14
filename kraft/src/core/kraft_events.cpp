#include "kraft_events.h"

namespace kraft {

bool             EventSystem::initialized = false;
EventSystemState EventSystem::state;

bool EventSystem::Init(ArenaAllocator* arena)
{
    if (initialized)
    {
        KERROR("EventSystem already initialized!");
        return false;
    }

    MemZero(&state, sizeof(EventSystemState));
    state.arena = arena;

    for (int i = 0; i < EventType::EVENT_TYPE_NUM_COUNT; i++)
    {
        state.event_entries[i].listeners.Init(arena);
    }

    initialized = true;

    return true;
}

bool EventSystem::Shutdown()
{
    initialized = false;
    return true;
}

bool EventSystem::Listen(EventType type, void* listener, EventCallback callback)
{
    EventListener* it;
    ca_for(state.event_entries[type].listeners, it)
    {
        if (it->listener == listener)
        {
            // Duplicate listener
        }
    }

    state.event_entries[type].listeners.Push({ listener, callback });
    return true;
}

bool EventSystem::Unlisten(EventType type, void* listener, EventCallback callback)
{
    EventListener* it;
    u32            index = 0;
    ca_for(state.event_entries[type].listeners, it)
    {
        if (it->listener == listener && it->callback == callback)
        {
            state.event_entries[type].listeners.SwapRemove(index);
            return true;
        }

        index++;
    }

    return false;
}

void EventSystem::Dispatch(EventType type, EventData data, void* sender)
{
    EventListener* it;
    ca_for(state.event_entries[type].listeners, it)
    {
        if (it->callback(type, sender, it->listener, data))
        {
            return;
        }
    }
}

} // namespace kraft
