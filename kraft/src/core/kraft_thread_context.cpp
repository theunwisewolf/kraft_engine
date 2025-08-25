namespace kraft {

kraft_thread_internal ThreadContext* thread_context_local;

kraft_internal ThreadContext* CreateThreadContext()
{
    ArenaAllocator* arena = CreateArena({
        .ChunkSize = KRAFT_SIZE_MB(64),
        .Alignment = KRAFT_SIZE_KB(64),
    });

    ThreadContext* thread_context = ArenaPush(arena, ThreadContext);
    thread_context->scratch_arenas[0] = arena;
    thread_context->scratch_arenas[1] = CreateArena({
        .ChunkSize = KRAFT_SIZE_MB(64),
        .Alignment = KRAFT_SIZE_KB(64),
    });

    return thread_context;
}

kraft_internal void DestroyThreadContext(ThreadContext* ctx)
{
    DestroyArena(ctx->scratch_arenas[0]);
    DestroyArena(ctx->scratch_arenas[1]);
}

kraft_internal void SetCurrentThreadContext(ThreadContext* ctx)
{
    thread_context_local = ctx;
}

kraft_internal ArenaAllocator* GetScratchArena(ArenaAllocator** conflicting_arenas, u64 conflicting_arenas_count)
{
    ThreadContext*  ctx = thread_context_local;
    ArenaAllocator* out = nullptr;
    for (u32 i = 0; i < KRAFT_C_ARRAY_SIZE(ctx->scratch_arenas); i++)
    {
        bool has_conflict = false;
        for (u32 j = 0; j < conflicting_arenas_count; j++)
        {
            if (ctx->scratch_arenas[i] == conflicting_arenas[j])
            {
                has_conflict = true;
                break;
            }
        }

        if (!has_conflict)
        {
            out = ctx->scratch_arenas[i];
            break;
        }
    }

    return out;
}

} // namespace kraft