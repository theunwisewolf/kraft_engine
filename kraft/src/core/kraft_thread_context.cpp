namespace kraft {

kraft_thread_internal ThreadContext* thread_context_local;

kraft_internal ThreadContext* CreateThreadContext(u64 scratch_size) {
    // CreateArena commits and zeroes the whole chunk up front, so a pool of workers cannot each
    // afford the 64 MB default: sixteen of them would touch 2 GB before running a single task.
    ArenaCreateOptions options = {};
    options.ChunkSize = scratch_size;
    options.Alignment = KRAFT_SIZE_KB(64);

    ArenaAllocator* arena = CreateArena(options);
    ThreadContext* thread_context = ArenaPush(arena, ThreadContext);
    thread_context->scratch_arenas[0] = arena;
    thread_context->scratch_arenas[1] = CreateArena(options);

    return thread_context;
}

kraft_internal ThreadContext* CreateThreadContext() {
    return CreateThreadContext(KRAFT_SIZE_MB(64));
}

kraft_internal void DestroyThreadContext(ThreadContext* ctx) {
    // The context itself lives inside scratch_arenas[0]; grab both pointers before freeing anything
    ArenaAllocator* first_arena = ctx->scratch_arenas[0];
    ArenaAllocator* second_arena = ctx->scratch_arenas[1];
    DestroyArena(second_arena);
    DestroyArena(first_arena);
}

kraft_internal void SetCurrentThreadContext(ThreadContext* ctx) {
    thread_context_local = ctx;
}

kraft_internal ArenaAllocator* GetScratchArena(ArenaAllocator** conflicting_arenas, u64 conflicting_arenas_count) {
    ThreadContext* ctx = thread_context_local;
    ArenaAllocator* out = nullptr;
    for (u32 i = 0; i < KRAFT_C_ARRAY_SIZE(ctx->scratch_arenas); i++) {
        bool has_conflict = false;
        for (u32 j = 0; j < conflicting_arenas_count; j++) {
            if (ctx->scratch_arenas[i] == conflicting_arenas[j]) {
                has_conflict = true;
                break;
            }
        }

        if (!has_conflict) {
            out = ctx->scratch_arenas[i];
            break;
        }
    }

    return out;
}

} // namespace kraft