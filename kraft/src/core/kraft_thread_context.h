#pragma once

namespace kraft {

struct ThreadContext
{
    // Double-buffered scratch arenas
    ArenaAllocator* scratch_arenas[2];
};

kraft_internal ThreadContext*  CreateThreadContext();
kraft_internal ThreadContext*  CreateThreadContext(u64 scratch_size); // per arena, and there are two
kraft_internal void            DestroyThreadContext(ThreadContext* ctx);
kraft_internal void            SetCurrentThreadContext(ThreadContext* ctx);
kraft_internal ArenaAllocator* GetScratchArena(ArenaAllocator** conflicting_arenas, u64 conflicting_arenas_count);

} // namespace kraft
