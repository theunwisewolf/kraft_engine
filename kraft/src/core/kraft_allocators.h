#pragma once

#define KRAFT_SIZE_KB(Size) Size * 1024
#define KRAFT_SIZE_MB(Size) KRAFT_SIZE_KB(Size) * 1024

#define KRAFT_MEMORY_DEBUG 1

namespace kraft {

enum MemoryTag : int;

struct ArenaCreateOptions
{
    u64       ChunkSize = 64;
    u64       Alignment = 64;
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

#if KRAFT_MEMORY_DEBUG
    ~TempArena()
    {
        KASSERTM(arena->position == position, "ScratchEnd not called!");

        // Set the original region to some garbage so we never read legit values from it
        for (u64 i = 0; i < arena->position; i += 4)
        {
            arena->base_ptr[position + i + 0] = 0xfc;
            arena->base_ptr[position + i + 1] = 0xfc;
            arena->base_ptr[position + i + 2] = 0xfc;
            arena->base_ptr[position + i + 3] = 0xfc;
        }
    }
#endif
};

ArenaAllocator* CreateArena(ArenaCreateOptions options);
void            DestroyArena(ArenaAllocator* arena);
u64             ArenaPosition(ArenaAllocator* arena);

void    ArenaPop(ArenaAllocator* arena, u64 size);
void    ArenaPopToPosition(ArenaAllocator* arena, u64 position);
char*   ArenaPushString(ArenaAllocator* arena, const char* src, uint64 length);
String8 ArenaPushString8Empty(ArenaAllocator* arena, u64 size);
String8 ArenaPushString8Copy(ArenaAllocator* arena, String8 str);
void    ArenaPopString8(ArenaAllocator* arena, String8 str);

#define ArenaPushArrayAligned(arena, T, count)       (T*)arena->Push(sizeof(T) * count, AlignOf(T), true)
#define ArenaPushArrayAlignedNoZero(arena, T, count) (T*)arena->Push(sizeof(T) * count, AlignOf(T), false)
#define ArenaPushArray(arena, T, count)              ArenaPushArrayAligned(arena, T, count)
#define ArenaPushArrayNoZero(arena, T, count)        ArenaPushArrayAlignedNoZero(arena, T, count)
#define ArenaPush(arena, T)                          ArenaPushArray(arena, T, 1)
#define ArenaAlloc()                                 CreateArena({ .ChunkSize = KRAFT_SIZE_MB(64), .Alignment = KRAFT_SIZE_KB(64) })

TempArena TempBegin(ArenaAllocator* arena);
void      TempEnd(TempArena temp);

#define ScratchBegin(conflicting_arenas, conflicting_arenas_count) kraft::TempBegin(kraft::GetScratchArena(conflicting_arenas, conflicting_arenas_count))
#define ScratchEnd(scratch_arena)                                  kraft::TempEnd(scratch_arena);

} // namespace kraft