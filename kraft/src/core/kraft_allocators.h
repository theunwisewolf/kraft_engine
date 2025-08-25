#pragma once

#define KRAFT_SIZE_KB(Size) Size * 1024
#define KRAFT_SIZE_MB(Size) KRAFT_SIZE_KB(Size) * 1024

namespace kraft {

enum MemoryTag : int;

struct ArenaCreateOptions
{
    uint64    ChunkSize = 64;
    uint64    Alignment = 64;
    MemoryTag Tag = (MemoryTag)0;
};

// TODO: Arena->Next
struct ArenaAllocator
{
    MemoryTag          tag;
    u8*                base_ptr;
    u64                capacity = 0;
    u64                base_position;
    u64                position;
    ArenaCreateOptions options;

    void* Push(u64 size, u64 alignment, bool zero);
};

struct TempArena
{
    ArenaAllocator* arena;
    u64             position;
};

kraft_internal ArenaAllocator* CreateArena(ArenaCreateOptions options);
kraft_internal void            DestroyArena(ArenaAllocator* arena);
kraft_internal u64             ArenaPosition(ArenaAllocator* arena);

kraft_internal void    ArenaPop(ArenaAllocator* arena, u64 size);
kraft_internal void    ArenaPopToPosition(ArenaAllocator* arena, u64 position);
kraft_internal char*   ArenaPushString(ArenaAllocator* arena, const char* src, uint64 length);
kraft_internal string8 ArenaPushString8Copy(ArenaAllocator* arena, string8 str);
kraft_internal void    ArenaPopString8(ArenaAllocator* arena, string8 str);

#define ArenaPushArrayAligned(arena, T, count)       (T*)arena->Push(sizeof(T) * count, AlignOf(T), true)
#define ArenaPushArrayAlignedNoZero(arena, T, count) (T*)arena->Push(sizeof(T) * count, AlignOf(T), false)
#define ArenaPushArray(arena, T, count)              ArenaPushArrayAligned(arena, T, count)
#define ArenaPushArrayNoZero(arena, T, count)        ArenaPushArrayAlignedNoZero(arena, T, count)
#define ArenaPush(arena, T)                          ArenaPushArray(arena, T, 1)

kraft_internal TempArena TempBegin(ArenaAllocator* arena);
kraft_internal void      TempEnd(TempArena temp);

#define ScratchBegin(conflicting_arenas, conflicting_arenas_count) kraft::TempBegin(kraft::GetScratchArena(conflicting_arenas, conflicting_arenas_count))
#define ScratchEnd(scratch_arena)                                  kraft::TempEnd(scratch_arena);

} // namespace kraft