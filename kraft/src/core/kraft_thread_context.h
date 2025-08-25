#pragma once

namespace kraft {

struct ThreadContext
{
    // Double-buffered scratch arenas
    ArenaAllocator* scratch_arenas[2];
};

kraft_internal ThreadContext*  CreateThreadContext();
kraft_internal void            DestroyThreadContext(ThreadContext* ctx);
kraft_internal void            SetCurrentThreadContext(ThreadContext* ctx);
kraft_internal ArenaAllocator* GetScratchArena(ArenaAllocator** conflicting_arenas, u64 conflicting_arenas_count);

} // namespace kraft
