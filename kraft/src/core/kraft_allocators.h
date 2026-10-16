#pragma once

#define KRAFT_SIZE_KB(Size) ((u64)(Size) * 1024)
#define KRAFT_SIZE_MB(Size) (KRAFT_SIZE_KB(Size) * 1024)

#ifndef KRAFT_MEMORY_DEBUG
#ifdef KRAFT_DEBUG
#define KRAFT_MEMORY_DEBUG 1
#else
#define KRAFT_MEMORY_DEBUG 0
#endif
#endif

namespace kraft {

enum MemoryTag : int;

struct ArenaCreateOptions {
    u64 ChunkSize = 64;
    u64 Alignment = 64;
    u64 CommitSize = 0;
    u64 DecommitThreshold = 0;
    MemoryTag Tag = (MemoryTag)0;
};

struct ArenaAllocator {
    MemoryTag tag;
    u8* base_ptr;
    u64 capacity = 0;
    u64 base_position;
    u64 position;

    // header plus the chunk rounded to pages
    u64 capacity_reserved;
    
    // bytes committed from the start of the reservation, header included
    u64 committed;
    
    // furthest position ever used; committed pages above it have never been written
    u64 high_water;
    ArenaCreateOptions options;

    void* Push(u64 size, u64 alignment, bool zero);
};

struct TempArena {
    ArenaAllocator* arena;
    u64 position;

#if KRAFT_MEMORY_DEBUG
    // Poisoning the region happens in ArenaPopToPosition, which knows both ends of it and can stay
    // inside the committed pages.
    ~TempArena() {
        KASSERTM(arena->position == position, "ScratchEnd not called!");
    }
#endif
};

ArenaAllocator* CreateArena(ArenaCreateOptions options);
void DecommitArenaAbove(ArenaAllocator* arena, u64 position); // give back everything past a point
void DestroyArena(ArenaAllocator* arena);
u64 ArenaPosition(ArenaAllocator* arena);

void ArenaPop(ArenaAllocator* arena, u64 size);
void ArenaPopToPosition(ArenaAllocator* arena, u64 position);
void ArenaClear(ArenaAllocator* arena);
char* ArenaPushString(ArenaAllocator* arena, const char* src, u64 length);
String8 ArenaPushString8Empty(ArenaAllocator* arena, u64 size);
String8 ArenaPushString8Copy(ArenaAllocator* arena, String8 str);
void ArenaPopString8(ArenaAllocator* arena, String8 str);

#define ArenaPushArrayAligned(arena, T, count) (T*)(arena)->Push(sizeof(T) * (u64)(count), AlignOf(T), true)
#define ArenaPushArrayAlignedNoZero(arena, T, count) (T*)(arena)->Push(sizeof(T) * (u64)(count), AlignOf(T), false)
#define ArenaPushArray(arena, T, count) ArenaPushArrayAligned(arena, T, count)
#define ArenaPushArrayNoZero(arena, T, count) ArenaPushArrayAlignedNoZero(arena, T, count)
#define ArenaPush(arena, T) ArenaPushArray(arena, T, 1)
#define ArenaAlloc() CreateArena({.ChunkSize = KRAFT_SIZE_MB(64), .Alignment = KRAFT_SIZE_KB(64)})

TempArena TempBegin(ArenaAllocator* arena);
void TempEnd(TempArena temp);

#define ScratchBegin(conflicting_arenas, conflicting_arenas_count) kraft::TempBegin(kraft::GetScratchArena(conflicting_arenas, conflicting_arenas_count))
#define ScratchEnd(scratch_arena) kraft::TempEnd(scratch_arena);

} // namespace kraft